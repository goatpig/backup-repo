#include <CustomAlloc.h>

struct BufferHeader
{
	unsigned int *pinuse, linuse;
	byte *offset, move;
	unsigned int size, total;
	void* (*copy)(void*, void*);

	/***
	move is a flag for the defrag thread
		-1: don't move this buffer
		0: ready for move
		1: being moved
	***/

	void Tighten(unsigned int Z)
	{
		size = total = Z;
	}
};

class MemPool
{
	/***
	rule: the BH linking the furthest into *pool has to be the last one in the BH array
	make sure to respect this rule when defragging;
	***/
	private:
		static const int BHstep = 50;
		unsigned int totalBH;
		byte *pool;

		BufferHeader* GetBH(unsigned int size)
		{
			//check to see if a bh isn't already available
			if(size)
			{
				for(i=0; i<nBH; i++)
				{
					if(!*BH[i]->pinuse)
					{
						if(BH[i]->total>=size)
						{
							BH[i]->copy=0;
							return BH[i];
						}
					}
				}
			}

			if(nBH==totalBH)
			{
				totalBH+=BHstep;
				BufferHeader **bht;

				while(!(bht = (BufferHeader**)malloc(sizeof(BufferHeader*)*totalBH))) {}
				memcpy(bht, BH, sizeof(BufferHeader*)*nBH);
				free(BH);
				BH = bht;
			}

			while(!(bhtmp = (BufferHeader*)malloc(sizeof(BufferHeader)))) {}
			ResetBH(bhtmp);
			BH[nBH] = bhtmp;
			bhtmp->offset = 0;
			bhtmp->pinuse = &bhtmp->linuse;
			bhtmp->linuse = 1;
			nBH++;

			return bhtmp;
		}

	public:
		BufferHeader **BH;
		static const int memsize = 1024*1024; //1 mb per page
		unsigned int freemem, total, reserved, slack, fmabs, nBH;
		std::atomic_bool ab;
		byte defrag;
		unsigned int i; 
		BufferHeader *bhtmp; //global bhtmp could result in unexpected behavior, 
      //should use local bhtmp instead. However, private pools are locked and 
      //thus function sequentially. Random buffer query locks the pool as well.

		/***reserved: marks how deep the pool buffer has been assigned for. 
      GetBuffer will always assign new BH starting pool + reserved, and increment reserved by size
		PopBH will ignore it***/
		
		void ResetBH(BufferHeader *bhtmp)
		{
			memset(bhtmp, 0, sizeof(BufferHeader));
			bhtmp->pinuse = &bhtmp->linuse;
		}

		void ComputeMem()
		{
			//reset all bh that aren't inuse anymore, compute reserved based on the deepest bh in pool
			if(nBH)
			{
				unsigned int sm=0, g=0;

				for(g; g<nBH; g++)
				{
					if(*BH[g]->pinuse)
					{
						if(BH[g]->offset>BH[sm]->offset) sm=g;
					}
					else ResetBH(BH[g]);
				}

				if(BH[sm]->offset) reserved = (unsigned int)(BH[sm]->offset -pool) +BH[sm]->total;
				else reserved = 0;
				freemem = total - reserved;
			}
		}

		byte *GetPrivatePool(unsigned int size)
		{
			if(ab.exchange(1)) return 0; //lock the pool on private gets
			if(!pool)
			{
				if(size<memsize) Alloc(memsize);
				else Alloc(size);
			}

			freemem-=size;
			reserved+=size;

			return pool +reserved -size;
		}

		void ReleasePool()
		{
			ab.store(0); //unlock pool from private gets
		}

		void Alloc(unsigned int size)
		{
			while(!pool)
			{ 
				pool = (byte*)malloc(size);
			}

         mlock(pool, size);
				
			total = size;
			freemem = size;
		}
		
		MemPool::MemPool()
		{
			BH=0;
			nBH=0; totalBH=0;
			defrag=0;
			pool=0;
			freemem=0;
			total=0;
			ab=0;
			reserved=0;
		}

		MemPool::~MemPool()
		{
         if(pool)
         {
            munlock(pool, total);
			   free(pool);
			   for(i=0; i<nBH; i++)
				   free(BH[i]);
			   if(BH) free(BH);
         }
		}

		BufferHeader* GetBuffer(unsigned int size, unsigned int *sema) //needs fixing
		{
			if(ab.exchange(1)==TRUE) return 0; //pools are meant to function as single threaded
			if(!total) 
			{
				if(size<memsize) Alloc(memsize);
				else Alloc(size);
			}
			else if(size>freemem) 
			{
				ab=0;
				return 0;
			}

			GetBH(size);

			if(!bhtmp->offset) //if this bh is new (offset is null), prep it
			{
				bhtmp->offset = pool +reserved;
				bhtmp->total = size;
				reserved += size;
			}

			if(sema) bhtmp->pinuse=sema;
			bhtmp->size = size;
			bhtmp->move = 0;
			freemem-=size;
			ab.store(0);
			return bhtmp;
		}

		BufferHeader *PopBH(byte *start, unsigned int size, unsigned int *sema)
		{
			//this method is used to assign manually alligned bufferheader pointers for reserved mempools
			//it doesn't provide any garanty on buffer integrity, caller should pay attention to overrides.
			bhtmp = GetBH(size);
				
			bhtmp->pinuse = sema;
			bhtmp->offset = start;
			bhtmp->size = size;
			bhtmp->total = size;
			bhtmp->move = 0;
			/*** Do no update freemem in popbh. We assume this function is used to assign memeory that has already 
         been reserved through getprivatepool(). Use GetBuffer otherwise. The general rule would imply PopBH 
         doesn't manage any pool level information, only BH data***/

			return bhtmp;
		}

		BufferHeader *PushBH(BufferHeader *bh, unsigned int size, unsigned int *sema)
		{
			/***This function provides a bh at the end of the reserved memory. Used on private pools to get a variable length buffer 
			if *bh is null, a new bh is returned, otherwise, the one linked is used. the code doesn't verify that the bh belongs to the pool, so don't be a dick.
			each call will add size to the bh, and reduce it from the pool
			***/

			if(!bh) 
			{
				bh = GetBH(0);
				bh->offset = pool + reserved;
				bh->size = 0;
				bh->total = 0;
				bh->pinuse = sema;
				bh->move = 0;
			}

			bh->size+=size;
			bh->total+=size;

			freemem-=size;
			reserved+=size;

			return bh;
		}

		byte *GetPool()
		{
			return pool;
		}

		void Stats()
		{
			unsigned int i; 
			fmabs=total;
			slack=0;

			for(i=0; i<nBH; i++)
			{
				if(BH[i])
				{
					if(*BH[i]->pinuse)
					{
						fmabs -= BH[i]->total;
						slack+=BH[i]->total - BH[i]->size;
					}
				}
			}
		}
};


class BufferHandler
{
	/*** allocates and recycles memory used to hold transactions' raw data
	***/
	private:
		unsigned int *order, *order2, *otmp, S, I, otmpS;

		static const int poolstep = 100;
		static const int max_fetch = 10;

		std::atomic_bool ab, ordering, abu;
		std::atomic_uint orderlvl, bufferfetch, bufferfetchu;
		std::atomic_int getpoolflag, getpoolflagu;


	public:
		MemPool **MP, **MPu;
		unsigned int npools, npoolsu;
		
		BufferHandler::BufferHandler()
		{
			MP=0;
			otmp=0; otmpS=0;
			npools=0;
			order=order2=0;
			ab=ordering=0;
			orderlvl=0;
			getpoolflag=0;
			bufferfetch=0;

			MPu=0;
			npoolsu=0;
			getpoolflagu=0;
			bufferfetchu=0;
			abu=0;
		}

		BufferHandler::~BufferHandler()
		{
			for(unsigned int i=0; i<npools; i+=poolstep)
				delete[] MP[i];
			for(unsigned int i=0; i<npoolsu; i++)
				delete[] MPu[i];
			free(MP);
			free(MPu);
			free(otmp);
			free(order);
			free(order2);
		}

		BufferHeader *GetBuffer(unsigned int size, unsigned int *sema)
		{
			/*** set new lock system here ***/

			unsigned int i;
			MemPool *mp;
			BufferHeader *bh;
			
			fetchloop:
			getpoolflag.fetch_add(1);

			while((i=bufferfetch.fetch_add(1))<npools)
			{
				mp = MP[order[i]];
				if(mp->freemem>size || !mp->total) //look for available size
				{
					bh = mp->GetBuffer(size, sema); //maybe have broken this by passing sema instead of 0
					if(bh)
					{
						getpoolflag.fetch_add(-1);
						bufferfetch.store(0);
						UpdateOrder(i);
						return bh;
					}
					else UpdateOrder(i);
				}
			}
						
			getpoolflag.fetch_add(-1);
			//either all pools are locked or they're full, add a new batch of pools
			if(ab.exchange(1)==FALSE)
			{
				ExtendPool();
				ab.store(0);
			}

			while(ab) {}
			goto fetchloop;
		}

		void UpdateOrder(unsigned int in)
		{
			if(orderlvl.fetch_add(1)==poolstep*2)
			{
				in++;
				while(ab) {} //extendpool lock
				ordering.store(1);
				
				unsigned int *ordtmp;
				memcpy(order2, order+in, sizeof(int)*(npools-in));
				memcpy(order2 +npools -in, order, sizeof(int)*in);
				
				ordtmp = order;
				order = order2;
				order2 = ordtmp;
				
				orderlvl.store(0);
				ordering.store(0);
			}
		}

		MemPool *GetPool(unsigned int size, byte* &out)
		{
			unsigned int i;
			MemPool *mp;
			
			fetchloop:
			getpoolflag.fetch_add(1);

			while((i=bufferfetch.fetch_add(1))<npools)
			{
				mp = MP[order[i]];
				if(mp->freemem>size || !mp->total) //look for available size
				{
					out = mp->GetPrivatePool(size);
					if(out)
					{
						getpoolflag.fetch_add(-1);
						bufferfetch.store(0);
						UpdateOrder(i);
						return mp;
					}
					else UpdateOrder(i);
				}
			}
						
			getpoolflag.fetch_add(-1);

			//getting here means there were no virgin pools available, extend the pools
			if(ab.exchange(1)==FALSE)
			{
				bufferfetch.store(npools);
				ExtendPool();
				bufferfetch.store(0);
				ab.store(0);
			}

			while(ab) {}
			goto fetchloop;
		}

		MemPool *GetPoolU(unsigned int size)
		{
			unsigned int i;
			MemPool *mp;
			byte *out;
			
			fetchloop:
			getpoolflagu.fetch_add(1);

			while((i=bufferfetchu.fetch_add(1))<npoolsu)
			{
				mp = MPu[i];
				if(mp->freemem>size || !mp->total) //look for available size
				{
					out = mp->GetPrivatePool(size);
					if(out)
					{
						getpoolflagu.fetch_add(-1);
						bufferfetchu.store(0);
						//UpdateOrder(i);
						return mp;
					}
					//else UpdateOrder(i);
				}
			}
						
			getpoolflagu.fetch_add(-1);

			//getting here means there were no virgin pools available, extend the pools
			if(abu.exchange(1)==FALSE)
			{
				bufferfetchu.store(npoolsu);
				ExtendPoolU();
				bufferfetchu.store(0);
				abu.store(0);
			}

			while(abu) {}
			goto fetchloop;
		}

		void ExtendPool()
		{
			while(ordering) {} //updateorder lock

			unsigned int F;
			S = npools +poolstep;
			MemPool **mptmp = (MemPool**)malloc(sizeof(MemPool*)*S);
			memcpy(mptmp, MP, sizeof(MemPool*)*npools);
			
			unsigned int *ordtmp = order2;
			order2 = (unsigned int*)malloc(sizeof(int)*S);
			memcpy(order2 +poolstep, order, sizeof(int)*npools);
			
			MemPool **mptmp2 = MP;
			MemPool *mptmp3 = new MemPool[poolstep];

			for(I=npools; I<S; I++)
			{
				F = I-npools;
				mptmp[I] = &mptmp3[F];
				order2[F] = I;
			}

			orderlvl.store(0);

			MP = mptmp;

			free(ordtmp);
			ordtmp = order;
			order = order2;
			
			while(getpoolflag) {} //lock this thread until all other threads reach the main lock.

			order2 = (unsigned int*)malloc(sizeof(int)*S);
			free(ordtmp);
			free(mptmp2);

			npools = S;
		}
		void ExtendPoolU()
		{
			unsigned int F, S;
			S = npoolsu +poolstep;
			MemPool **mptmp = (MemPool**)malloc(sizeof(MemPool*)*S);
			memcpy(mptmp, MPu, sizeof(MemPool*)*npoolsu);
						
			MemPool **mptmp2 = MPu;
			MemPool *mptmp3 = new MemPool[poolstep];

			for(I=npoolsu; I<S; I++)
			{
				F = I-npoolsu;
				mptmp[I] = &mptmp3[F];
			}

			MPu = mptmp;
			
			while(getpoolflagu) {} //lock this thread until all other threads reach the main lock.
			free(mptmp2);

			npoolsu = S;
		}

		unsigned int GetUnusedMem()
		{
			unsigned int i, m=0;
			for(i=0; i<npools; i++)
				m+=MP[i]->freemem;

			return m;
		}
		
		unsigned int GetTotalMem()
		{
			unsigned int i, m=0;
			for(i=0; i<npools; i++)
			{
				m+=MP[i]->total;
			}

			return m;
		}

		void FillRate()
		{
			unsigned int i, hd=0, ld=0;
			float c;

			for(i=0; i<npools; i++)
			{
				c = (float)MP[i]->freemem/(float)MP[i]->total;
				if(c<=0.2f) hd++;
				else if(c>=0.8f) ld++;
			}

			float fhd = (float)hd/(float)npools;
			float fld = (float)ld/(float)npools;

			int abc=0;
		}
};

BufferHandler hBuffer;
