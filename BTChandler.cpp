#include <fcntl.h> 
#include <io.h>
#include <sys/types.h>

const int max_socket_buffer = 5000;
const int testnet = 1;

unsigned int LAST_BLOCK;

const char Base58str[] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

char *ChainFileG=NULL;
unsigned int MWG;

std::atomic_uint totalchain, n_txn, n_vouts, n_vins, at_block;

int nrounds=0;

int CompareBuffers(void *b1, void *b2, int size)
{
	int CBa, CBb;
	int *b3, *b4, *b5;
	CBa = size/4;
	b5 = b3 = (int*)b1;
	b4 = (int*)b2;
	b5+=4;

	CBb=0;

	while(!CBb & (b3<b5))
	{
		CBb = *b3 ^ *b4;
		b3++; b4++;
	}

	return CBb;
}

unsigned int B58toI(char in)
{
	for(unsigned int i=0; i<58; i++)
		if(Base58str[i]==in) return i;
	return -1;
}

#if defined(_MSC_VER) || defined(__BORLANDC__)
 typedef unsigned __int64 ulong64;
 typedef signed __int64 long64;
 #else
 typedef unsigned long long ulong64;
 typedef signed long long long64;
 #endif

static ulong64 txfee = 50000;

class MultiFile
{
	private:
		int na, m, fnameL;

	public:
		int handle;
		unsigned int findex, fcount;
		int status;

		char *fname;
		int ndigits;
		char *fext, *dbuffer;
		static char *chainfile;

		MultiFile::MultiFile()
		{
			handle = -1;
			findex = fcount = 0;
			status = 0;
			fnameL = 0;

			fname = (char*)malloc(MAX_PATH);
			ndigits = 0;
			dbuffer = (char*)malloc(20);
			fext = (char*)malloc(10);
		}

		MultiFile::~MultiFile()
		{		
			if(handle>-1) _close(handle);
			free(fname);
			free(dbuffer);
			free(fext);
		}

		static void Init()
		{
			if(!testnet) 
			{
				strcpy(chainfile, "C:\\Users\\goat\\AppData\\Roaming\\Bitcoin\\blocks\\blk");
			}
			else 
			{
				strcpy(chainfile, "C:\\Users\\goat\\AppData\\Roaming\\Bitcoin\\testnet3\\blocks\\blk");
			}

			ChainFileG = chainfile;

		}
		void SetFile(char *name, char *ext, int ndgt)
		{
			strcpy(fname, name);
			fnameL=strlen(name);
			strcpy(fext, ext);
			ndigits = ndgt;
		}

		void Reset()
		{
			if(handle>-1) _close(handle);
			handle = -1;
			findex = 0;
			status = 0;
		}

		int OpenFile(int ind)
		{
			if(!fnameL)
			{
				strcpy(fname, chainfile);
				fnameL = strlen(chainfile);
				strcpy(fext, ".dat");
				ndigits = 5;
			}

			if(handle>-1) _close(handle);
			else Reset();

			if(ind==-1)
			{
				if(!status) findex=0;
				else findex++;
			}
			else findex=ind;

			na = findex; m=1;
			while(na/10) m++;
			if(m>ndigits) m=ndigits;
			na = ndigits - m;

			memset(dbuffer, 0x30, ndigits-m);
			_itoa(findex, dbuffer +ndigits-m, 10);

			fname[fnameL] = 0;
			strcat(fname, dbuffer);
			strcat(fname, fext);

			handle = _open(fname, _O_BINARY | _O_RDONLY);

			if(handle==-1 && ind==-1) fcount = findex;
			status=1;

			return handle;
		}

		void Close()
		{
			Reset();
		}

		MultiFile &operator=(MultiFile &rhs)
		{
			if(&rhs!=this)
			{
				this->SetFile(rhs.fname, rhs.fext, rhs.ndigits);
				this->fnameL = rhs.fnameL;
				this->Reset();
				this->fcount = rhs.fcount;
			}
			return *this;
		}
};
char *MultiFile::chainfile = (char*)malloc(MAX_PATH);

#ifdef WIN32
//#include <winsock2.h>

class PlatformSocket
{
	private:
		SOCKET sock;
		sockaddr_in addr;
		int a, b, c;

		int SetupSocket()
		{
			sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
			if(sock==INVALID_SOCKET) return 0;

			addr.sin_family = AF_INET;
			addr.sin_port = htons(8340);

			hostent *host;
			host = gethostbyname("localhost");
			unsigned int zob;
			memcpy(&zob, host[0].h_addr_list[0], 4);
			addr.sin_addr.s_addr = zob;

			return 1;
		}

	public:
		static WSADATA wsaDATA;	
		static const int ReqVersion = 2;
		static int Init()
		{
			if(WSAStartup(MAKEWORD(ReqVersion, 0), &wsaDATA)!=0) return 0;
			if(LOBYTE(wsaDATA.wVersion)<ReqVersion) return 0;
			
			return 1;
		}

		PlatformSocket::PlatformSocket()
		{
			SetupSocket();
		}

		PlatformSocket::~PlatformSocket()
		{
			closesocket(sock);
		}

		int Post(char *in)
		{
			connect(sock, (sockaddr*)&addr, sizeof(sockaddr_in));
			if(send(sock, in, strlen(in), 0)<0) return -1;

			//what if c<a?
			c = 500;
			b = a = recv(sock, in, c, 0);
			while(a==c && b<max_socket_buffer)
			{
				if(b+c>max_socket_buffer) c = max_socket_buffer -b;

				a = recv(sock, in +b, c, 0);
				b+=a;
			}

			if(b<0) return -1;
			if(b>max_socket_buffer) b = max_socket_buffer;
			in[b] = 0;

			return 0;
		}
};

WSADATA PlatformSocket::wsaDATA;
#endif
class BTCsocket
{
	private:
		PlatformSocket S;
		int a, b;
		char *clength;

	public:
		static char *jsonheader;
		static int headerlength;

		static void Init()
		{
			strcpy(jsonheader, "POST / HTTP/1.1\r\n");
			strcat(jsonheader, "host: localhost:8340\r\n");
			//strcat(senddata, "user-agent: ...\r\n");
			//strcat(senddata, "accept: application/json-rpc\r\n");
			strcat(jsonheader, "content-type: application/json-rpc\r\n");
			strcat(jsonheader, "authorization: Basic ZGljazpidXR0\r\n");
			strcat(jsonheader, "content-length: ");

			headerlength = strlen(jsonheader);
		}

		BTCsocket::BTCsocket()
		{
			clength = (char*)malloc(10);
		}

		BTCsocket::~BTCsocket()
		{
			free(clength);
		}

		void Post(char *in)
		{
			//add id at the end end of the command
			strcat(in, ", \"id\":1}\r\n\r\n");
			a = strlen(in);
			_itoa(a, clength, 10);
			b = strlen(clength);

			memcpy(in +headerlength +b +4, in, a);
			memcpy(in, jsonheader, headerlength);
			memcpy(in +headerlength, clength, b);
			memcpy(in +headerlength +b, "\r\n\r\n", 4);

			in[headerlength +b +4 +a] = 0;
			

			//send it & receive answer in char buffer
			S.Post(in);

			int m=0;
		}
};

char *BTCsocket::jsonheader = (char*)malloc(400);
int BTCsocket::headerlength;

int charcmp(char *i1, char *i2, int lng)
{
	int s=0, cc=0;
	while(!cc && s<lng) cc |= i1[s] ^ i2[s], s++;

	return cc;
}

class HtmlHeader
{
	public:
		static char **cflags;
		static const int ncflags = 2;
		int content_length, l, n, i, m, status, t;
		float version;
		char *body;

		char *tmp;
		int *flags, nflags;
		unsigned int *clrf;

		HtmlHeader::HtmlHeader()
		{
			flags = (int*)malloc(sizeof(int)*20);
		}

		HtmlHeader::~HtmlHeader()
		{
			free(flags);
		}

		static void Init()
		{
			for(int i=0; i<ncflags; i++)
				cflags[i] = (char*)malloc(20);

			strcpy(cflags[0], "HTTP/");
			strcpy(cflags[1], "Content-Length: ");
		}


		char *Parse(char *in)
		{
			l = strlen(in);
			if(l>max_socket_buffer) return 0;
			
			//parse header, get end
			n = 0;

			nflags = 0;
			content_length = 0;
			while(n<l)
			{
				clrf = (unsigned int*)(in +n);
				if((*clrf & 0x0000FFFF)==0x00000A0D)
				{
					flags[nflags*2] = content_length;
					flags[nflags*2 +1] = n -1 -content_length;
					nflags++;

					content_length = n+2;

					if((*clrf & 0xFFFF0000)==0x0A0D0000) break;
					n++;
				}

				n++;
			}

			content_length = status = -1; version = -1;
			t=0; 
			for(i=0; i<nflags; i++)
			{
				tmp = in +flags[i*2];
				l = flags[i*2 +1];
				
				if(version==-1)
				{
					if(!charcmp(tmp, cflags[0], 5))
					{
						version = (float)atof(tmp+5);

						m=5;
						while(tmp[m]!=' ' && m<l) m++;
						m++;

						if(m<l)
						{
							status = atoi(tmp+m);
						}

						if(i==nflags-1) break;
						
						i++; t++;
						tmp = in +flags[i*2];
						l = flags[i*2 +1];
					}
				}

				if(content_length==-1)
				{
					if(!charcmp(tmp, cflags[1], 16))
					{
						content_length = atoi(tmp + 16);
						
						if(i==nflags-1) break;
						
						i++; t++;
						tmp = in +flags[i*2];
						l = flags[i*2 +1];					
					}
				}

				if(t==ncflags) break;
			}

			tmp = in +n +3;
			if(*(tmp +content_length +1)) return 0;
			return tmp;
		}
};
char **HtmlHeader::cflags = (char**)malloc(sizeof(char*)*ncflags);

class JSONhandler
{
	public:
		static char **cflags;
		float version;
		char *result, *error, *id, *tmp, C;
		byte *intmp;
		int l, m, n, g;

		static void Init()
		{
			for(int i=0; i<3; i++)
				cflags[i] = (char*)malloc(15);

			strcpy(cflags[0], "\"result\":");
			strcpy(cflags[1], "\"error\":");
			strcpy(cflags[2], "\"id\":");
		}

		int ParseResult(char *in)
		{
			l = strlen(in); m=0;

			result=error=id=0;

			//result
			while(m<l)
			{
				if(in[m]=='"')
				{
					if(!charcmp(in +m, cflags[0], 9))
					{
						m+=9;
						result = in +m;
						if(*result=='[') C = ']', result++;
						else C = ',';


						while(m<l)
						{
							if(in[m]==C)
							{
								in[m] = 0;
								break;
							}
							m++;
						}

						m++;
						break;
					}
				}

				m++;
			}

			//error
			while(m<l)
			{
				if(in[m]=='"')
				{
					if(!charcmp(in +m, cflags[1], 8))
					{
						m+=8;
						error = in +m;

						while(m<l)
						{
							if(in[m]==',')
							{
								in[m] = 0;
								break;
							}
							m++;
						}

						m++;
						break;
					}
				}

				m++;
			}			
			
			//id
			while(m<l)
			{
				if(in[m]=='"')
				{
					if(!charcmp(in +m, cflags[2], 5))
					{
						m+=5;
						id = in +m;

						while(m<l)
						{
							if(in[m]=='}')
							{
								in[m] = 0;
								break;
							}
							m++;
						}

						m++;
						break;
					}
				}

				m++;
			}			

			m = (int)result & (int)error & (int)id;
			return m;
		}

		void GetRawResult(byte *in)
		{
			//count of data packets is added as first 4 bytes. size of each data packet is written in front of it. as int_32
			l = strlen(result);
			m=g=0;
			C = 34;
			
			intmp = in;
			in+=4;

			while(m<l)
			{
				if(result[m]==C)
				{
					m++;
					tmp = result +m;

					while(m<l)
					{
						if(result[m]==C)
						{
							g++;
							n = m -int(tmp -result);
							GetByteStream(tmp, in +4, n);
							n/=2;
							memcpy(in, &n, 4);
							in+=n+4;
							break;
						}

						m++;
					}
				}

				m++;
			}

			memcpy(intmp, &g, 4);
		}
};

char **JSONhandler::cflags = (char**)malloc(sizeof(char*)*3);

inline unsigned int varintCompress(ulong64 in, byte *out)
{
	if(in<0xFD) 
	{
		out[0] = byte(in);
		return 1;
	}
	else if(in<=0xFFFF) 
	{
		out[0] = 0xFD;
		memcpy(out +1, &in, 2);
		return 3;
	}
	else if(in<=0xFFFFFFFF)
	{
		out[0] = 0xFE;
		memcpy(out +1, &in, 4);
		return 5;
	}
	else
	{
		out[0] = 0xFF;
		memcpy(out +1, &in, 8);
		return 9;
	}
}

struct VINv2
{
	int *refOutput;
	byte *script;
	unsigned int scriptLength;
	unsigned int *txnHash;
};

struct VOUTv2
{
	ulong64 *value;
	byte *script;
	unsigned int scriptLength;
	byte type;
};

class TXNv2
{
	public:
		byte *raw;
		BufferHeader *bh;
		void *holder;

		unsigned int nVin, nVout, version, *rawsize;
		byte coinbase;
		unsigned int *txHash;
		unsigned int *locktime;

		VINv2 *vin;
		VOUTv2 *vout;

		int QS; //counts how many branches refer to this txn. If it's 0 or less, txn can be dumped

		std::atomic_uint32_t sema;

		TXNv2::TXNv2()
		{
			txHash = 0;
			vin=0; vout=0;
			sema=1;
			holder=0;
			QS=0;
		}

		TXNv2::~TXNv2()
		{
		}
};



TXNv2 *CopyTXN(BufferHeader *bh, unsigned int size)
{
	unsigned int y, z, k, n, Z;
	byte usbyte;

	TXNv2 *txntmp = (TXNv2*)bh->offset +size;
	Z = size + sizeof(TXNv2);
	bh->pinuse = (unsigned int*)&txntmp->sema; //LINKING BH->INUSE TO HOLDER'S SEMAPHORE. Needed step for lockless memory defragmentation
	txntmp->sema = 1;
			
	byte *raw = bh->offset;
	txntmp->raw = raw;
			
	//decode raw tx
	//txn version
	txntmp->version = (unsigned int)*raw; raw+=4;
								
	//compact size nVin
	txntmp->nVin = 0;
	usbyte = (byte)raw[0];
	if(usbyte<0xFD) txntmp->nVin = usbyte, n=1;
	else if(usbyte<0xFE) memcpy(&txntmp->nVin, raw +1, 2), n=3;
	else if(usbyte<0xFF) memcpy(&txntmp->nVin, raw +1, 4), n=5;
	raw+=n;

	VINv2 *vintmp = (VINv2*)(bh->offset + Z);
	Z+=sizeof(VINv2)*txntmp->nVin;
	txntmp->vin = vintmp;

	for(y=0; y<txntmp->nVin; y++)
	{
		//refered txn hash
		vintmp[y].txnHash = (unsigned int*)raw; raw+=32;
		k=vintmp[y].txnHash[0];
		for(z=1; z<8; z++)
			k|=vintmp[y].txnHash[z];
		if(k) txntmp->coinbase=0;
		else txntmp->coinbase=1;

		vintmp[y].refOutput = (int*)raw; raw+=4;

		//compact size response script length
		vintmp[y].scriptLength = 0;
		usbyte = (byte)raw[0];
		if(usbyte<0xFD) vintmp[y].scriptLength = usbyte, n=1;
		else if(usbyte<0xFE) memcpy(&vintmp[y].scriptLength, raw +1, 2), n=3;
		else if(usbyte<0xFF) memcpy(&vintmp[y].scriptLength, raw +1, 4), n=5;
		raw+=n;

		//script
		vintmp[y].script = raw;
		raw += vintmp[y].scriptLength;

		//sequence
		raw+=4;
	}

	txntmp->nVout = 0;
	usbyte = (byte)raw[0];
	if(usbyte<0xFD) txntmp->nVout = usbyte, n=1;
	else if(usbyte<0xFE) memcpy(&txntmp->nVout, raw +1, 2), n=3;
	else if(usbyte<0xFF) memcpy(&txntmp->nVout, raw +1, 4), n=5;
	raw+=n;
									
	VOUTv2 *vouttmp = (VOUTv2*)(bh->offset + Z);
	Z+=sizeof(VOUTv2)*txntmp->nVout;
	txntmp->vout = vouttmp;

	for(y=0; y<txntmp->nVout; y++)
	{
		//value;
		vouttmp[y].value = (ulong64*)raw; raw+=8;
		
		//compact size challenge script length
		vouttmp[y].scriptLength = 0;
		usbyte = (byte)raw[0];
		if(usbyte<0xFD) vouttmp[y].scriptLength = usbyte, n=1;
		else if(usbyte<0xFE) memcpy(&vouttmp[y].scriptLength, raw +1, 2), n=3;
		else if(usbyte<0xFF) memcpy(&vouttmp[y].scriptLength, raw +1, 4), n=5;
		raw+=n;

		//script
		vouttmp[y].script = raw;
		raw+=vouttmp[y].scriptLength;
		
		if(vouttmp[y].script[0]==0x41) vouttmp[y].type=1;
		else
		{
			n=0;
			while(vouttmp[y].script[n]==0x76) n++; //OP_DUP
			
			if(vouttmp[y].script[n]==0xA9) //OP_HASH160
			vouttmp[y].type=0;
		}
	}
								
	//lock time
	txntmp->locktime = (unsigned int*)raw;
	raw+=4;
				
	byte* bmp = bh->offset + Z;
	txntmp->txHash = (unsigned int*)(bmp +1);
	bmp[0]=1;
	Z+=33;

	bh->Tighten(Z);
								
	return txntmp;
}

void* bh_copy_txn(void *bhin, void *mpin)
{
	BufferHeader *BH = (BufferHeader*)bhin, *BHnew;

	TXNv2 *txntmp = (TXNv2*)BH->offset;

	//build entire txn size
	unsigned int m, i, s;
	m = BH->size /*- (unsigned int)(txntmp->raw -BH->offset)*/ + *txntmp->rawsize;

	//request m from list of unfragged BH
	MemPool** MP = (MemPool**)mpin;
	s = (unsigned int)*MP;

	for(i=1; i<=s; i++)
	{
		BHnew = MP[i]->GetBuffer(m, 0);

		if(BHnew)
		{
			memcpy(BHnew->offset, txntmp->raw, *txntmp->rawsize);
			TXNv2 *TXNnew = CopyTXN(BHnew, *txntmp->rawsize);

			//copy hash
			memcpy(TXNnew->txHash, txntmp->txHash, 32);

			//assign rawsize
			TXNnew->rawsize = txntmp->rawsize;

			//assign bh
			TXNnew->bh = BHnew;
			BHnew->move = 0;

			//set new txn in holder
			TXNnew->holder = txntmp->holder;
			void **pass;
			pass = (void**)txntmp->holder;
			*pass = (void*)TXNnew;
					
			//decrement old sema
			txntmp->sema.fetch_add(-1);
	
			/***debug purpose***/
			memset(BH->offset, 0, BH->total);
			
			break;
		}
	}

	return 0;
}

void* defrag_thread(void *in)
{
	//defrag thread, receives a BufferHandler object as argument
	BufferHandler *BH = (BufferHandler*)in;
	MemPool **mplf = (MemPool**)malloc(sizeof(MemPool*)*(BH->npools+1));

	unsigned int total=0, freemem=0, slack=0, i=0, y=0, reserved=0, a, b, c, e;
	int m;

	unsigned int *mno=0, mnos=0;
	unsigned int *efg = (unsigned int*)malloc(sizeof(int)*(BH->npools +1));
	unsigned int *hij = (unsigned int*)malloc(sizeof(int)*(BH->npools +1));
	*efg = 0; *hij = 0;

	unsigned int *abc = (unsigned int*)malloc(sizeof(int)*BH->npools);
	memset(abc, 0, sizeof(int)*BH->npools);


	
	for(i; i<BH->npools; i++)
	{
		if(BH->MP[i]->total)
		{
			BH->MP[i]->Stats();

			total+=BH->MP[i]->total;
			freemem+=BH->MP[i]->fmabs;
			slack+=BH->MP[i]->slack;
			reserved+=BH->MP[i]->reserved;
		}
	}

	float prt = (float)(freemem+reserved)/float(total);
	if(prt>=1.0f)
	{
		//not enough freemem
		//make a list of highly fragmented and low density mempools

		for(i=0; i<BH->npools; i++)
		{
			BH->MP[i]->ComputeMem();
			abc[i]=2; //mark as low fragmentation
			m = BH->MP[i]->fmabs - BH->MP[i]->freemem;
			if(m>0) 
			{
				//mark as highly fragmented
				abc[i] = 1;
			}
		}

		for(i=0; i<BH->npools; i++) //build list of fragged and unfragged pools
		{
			if(abc[i]==1) //fragmented
			{
				efg[*efg +1] = i;
				efg[0]++;
			}
			else if(abc[i]==2) // clean
			{
				if((float)BH->MP[i]->freemem>(float)BH->MP[i]->total*0.2f)
				{
					hij[*hij +1] = i;
					hij[0]++;
				}
			}
		}

		m=1;
		while(m) //order low frag pools by most available space
		{
			m=0;
			for(i=0; i<*hij-1; i++)
			{
				a = hij[i+1];
				b = hij[i+2];

				if(BH->MP[a]->freemem<BH->MP[b]->freemem)
				{
					hij[i+1] = b;
					hij[i+2] = a;
					m=1;
				}
			}
		}

		for(i=1; i<=*hij; i++)
			mplf[i] = BH->MP[hij[i]];
		*mplf = (MemPool*)*hij;

		for(i=0; i<*efg; i++) //run through fragged pools, move end of pools to unfragged pools
		{
			a = efg[i+1];
			//order BHs by offset. there's no guarantee they're tightly packed nor aligned
			if(mnos<BH->MP[a]->nBH)
				mno = (unsigned int*)realloc(mno, sizeof(int)*BH->MP[a]->nBH), mnos = BH->MP[a]->nBH;
			for(y=0; y<BH->MP[a]->nBH; y++)
				mno[y] = y;

			m=1;
			while(m)
			{
				m=0;
				for(y=0; y<BH->MP[a]->nBH-1; y++)
				{
					b = mno[y]; c = mno[i+y];
					if(BH->MP[a]->BH[b]->offset>BH->MP[a]->BH[c]->offset)
					{
						mno[y] = c;
						mno[y+1] = b;
						m=1;
					}
				}
			}

			b=0;
			while(b<BH->MP[a]->nBH)
			{
				if(BH->MP[a]->BH[mno[b]]->offset) break;
				b++;
			}

			//look for earliest fragmentation
			c=-1;
			for(b; b<BH->MP[a]->nBH-1; b++)
			{
				if(!(*BH->MP[a]->BH[mno[b]]->pinuse)) //count is at 0
				{
					c=b;
					break;
				}
				else 
				{
					e = BH->MP[a]->BH[mno[b+1]]->offset - BH->MP[a]->BH[mno[b]]->offset;
					if(e>BH->MP[a]->BH[mno[b]]->total) //unaccounted free space between 2 consecutive BH
					{
						c=b;
						break;
					}
				}				
			}

			//move everything from c onward
			for(b=BH->MP[a]->nBH-1; b>=c; b--)
			{
				if(*BH->MP[a]->BH[mno[b]]->pinuse && !BH->MP[a]->BH[mno[b]]->move)
				{
					if(BH->MP[a]->BH[mno[b]]->copy) //if a move function pointer exists for this BH, call it
						BH->MP[a]->BH[mno[b]]->copy((void*)BH->MP[a]->BH[mno[b]], (void*)mplf);
				}
			}

			BH->MP[a]->ComputeMem();

			/*** go over the mempool again, if the earliest point of fragmentation has changed, there's some more shit to move. Else, go to next one***/
		}
	}

	/*** CLEAN UP ALLOCATED MEM ***/
	
	float slc = (float)slack/float(freemem);
	if(slc>0.2)
	{
		//too much slack
	}

	unsigned int total2=0, freemem2=0, slack2=0, reserved2=0;

	for(i=0; i<BH->npools; i++)
	{
		if(BH->MP[i]->total)
		{
			BH->MP[i]->Stats();

			total2+=BH->MP[i]->total;
			freemem2+=BH->MP[i]->fmabs;
			slack2+=BH->MP[i]->slack;
			reserved2+=BH->MP[i]->reserved;
		}
	}

	free(mno);
	free(efg);
	free(hij);
	free(abc);
	free(mplf);

	return 0;
}
class TXNtmp
{
	/*** all around txn support/buffer handler class
	has to be thread safe
	***/
	private:
		TXNv2 **pending;
		unsigned int npending, totalpending, I, N, I2, S, G, M,
				*b1, *b2;

	public:		
		
		TXNtmp::TXNtmp()
		{
			pending=0;
			npending=totalpending=0;
		}

		TXNv2 *New(byte *raw, Sha256Round *sha, unsigned int* rawsize, MemPool *mp)
		{
			/***raw is the buffer holding the txn, sha is passed to hash the txn, rawsize is the pointer to the buffer in the block object perma storing txn size for eventual reload***/
			//thread safe
			unsigned int y, z, k, n;
			byte usbyte;
			BufferHeader *bh;

			bh = mp->PushBH(0, 0, 0);
			TXNv2 *txntmp = (TXNv2*)bh->offset;
			bh->pinuse = (unsigned int*)&txntmp->sema; //LINKING BH->INUSE TO HOLDER'S SEMAPHORE. Needed step for lockless memory defragmentation
			txntmp->sema = 1;
			bh->copy = bh_copy_txn;
			mp->PushBH(bh, sizeof(TXNv2), 0);
			
			txntmp->rawsize = rawsize;
			txntmp->raw = raw;
			
			//decode raw tx
			//txn version
			txntmp->version = (unsigned int)*raw; raw+=4;
								
			//compact size nVin
			txntmp->nVin = 0;
			usbyte = (byte)raw[0];
			if(usbyte<0xFD) txntmp->nVin = usbyte, n=1;
			else if(usbyte<0xFE) memcpy(&txntmp->nVin, raw +1, 2), n=3;
			else if(usbyte<0xFF) memcpy(&txntmp->nVin, raw +1, 4), n=5;
			raw+=n;

			n_vins.fetch_add(txntmp->nVin);

			VINv2 *vintmp = (VINv2*)(bh->offset + bh->size);
			mp->PushBH(bh, sizeof(VINv2)*(size_t)txntmp->nVin, 0);
			txntmp->vin = vintmp;

			/***fix varint unpacker***/

			for(y=0; y<txntmp->nVin; y++)
			{
				//refered txn hash
				vintmp[y].txnHash = (unsigned int*)raw; raw+=32;
				k=vintmp[y].txnHash[0];
				for(z=1; z<8; z++)
					k|=vintmp[y].txnHash[z];
				if(k) txntmp->coinbase=0;
				else txntmp->coinbase=1;

				vintmp[y].refOutput = (int*)raw; raw+=4;

				//compact size response script length
				vintmp[y].scriptLength = 0;
				usbyte = (byte)raw[0];
				if(usbyte<0xFD) vintmp[y].scriptLength = usbyte, n=1;
				else if(usbyte<0xFE) memcpy(&vintmp[y].scriptLength, raw +1, 2), n=3;
				else if(usbyte<0xFF) memcpy(&vintmp[y].scriptLength, raw +1, 4), n=5;
				raw+=n;

				//script
				vintmp[y].script = raw;
				raw += vintmp[y].scriptLength;

				//sequence
				raw+=4;
			}

			txntmp->nVout = 0;
			usbyte = (byte)raw[0];
			if(usbyte<0xFD) txntmp->nVout = usbyte, n=1;
			else if(usbyte<0xFE) memcpy(&txntmp->nVout, raw +1, 2), n=3;
			else if(usbyte<0xFF) memcpy(&txntmp->nVout, raw +1, 4), n=5;
			raw+=n;
						
			n_vouts.fetch_add(txntmp->nVout);

			VOUTv2 *vouttmp = (VOUTv2*)(bh->offset + bh->size);
			mp->PushBH(bh, sizeof(VOUTv2)*(size_t)txntmp->nVout, 0);
			txntmp->vout = vouttmp;

			for(y=0; y<txntmp->nVout; y++)
			{
				//value;
				vouttmp[y].value = (ulong64*)raw; raw+=8;

				//compact size challenge script length
				vouttmp[y].scriptLength = 0;
				usbyte = (byte)raw[0];
				if(usbyte<0xFD) vouttmp[y].scriptLength = usbyte, n=1;
				else if(usbyte<0xFE) memcpy(&vouttmp[y].scriptLength, raw +1, 2), n=3;
				else if(usbyte<0xFF) memcpy(&vouttmp[y].scriptLength, raw +1, 4), n=5;
				raw+=n;

				//script
				vouttmp[y].script = raw;
				raw+=vouttmp[y].scriptLength;

				if(vouttmp[y].script[0]==0x41) vouttmp[y].type=1;
				else
				{
					n=0;
					while(vouttmp[y].script[n]==0x76) n++; //OP_DUP

					if(vouttmp[y].script[n]==0xA9) //OP_HASH160
					vouttmp[y].type=0;
				}
			}
								
			//lock time
			txntmp->locktime = (unsigned int*)raw;
			raw+=4;
			
			*(txntmp->rawsize) = unsigned int(raw - txntmp->raw);
			if(sha)
			{
				sha->Hash(txntmp->raw, -1, *(txntmp->rawsize));
				sha->toBuff((unsigned char*)sha->HashOutput);
				sha->Hash((unsigned char*)sha->HashOutput, -1, 32);
			
				byte* bmp = bh->offset + bh->size;
				mp->PushBH(bh, 33, 0);
				txntmp->txHash = (unsigned int*)(bmp +1);
				bmp[0]=1;
			
				sha->OutputBE(txntmp->txHash);
			}
					
			return txntmp;
		}

		void ParsePendingTXNID(byte *bin)
		{
			/***here we receive tnxids from bitocind's pending pool. these have to be checked with the txntmp's pending pool.
			all those that are already in the pool have been processed. those that aren't have to be fetched from bitcoind for processing
			***/

			memcpy(&N, bin, 4);

			for(I=0; I<N; I++)
			{
				M=0;
				b1 = (unsigned int*)bin +1 +I*9;
				for(I2=0; I2<npending; I2++)
				{
					b2 = pending[I2]->txHash; 
					if(*b1==*b2)
					{
						//first 4 bytes match, check whole thing
						G=0; S=0;
						while(++S<8)
							G += b1[S] ^ b2[S];

						if(!G) //hashes match
						{
							//this pending txn is already accounted for, skip this index
							M=1;
							break;
						}
					}
				}

				if(M) *(b1 -1) = 0; //match found, deflag txn
			
				//if no match is found, the raw txn has to be queried from bitcoind. tnxids whose size is null are considered matched and deflaged
			}
		}

		TXNv2* AddPendingTXN(byte *bin, unsigned int size, unsigned int *hash)
		{
			unsigned int y, z, k, n, Z;
			byte usbyte;
			BufferHeader *bh;

			//estimate size of the overhead: sizeof(TXNv2) + 32 for hash + estimate on nvins and nvouts
			Z = sizeof(TXNv2) + 32 + (size/110)*sizeof(VINv2) +size;

			bh = hBuffer.GetBuffer(Z, 0);
			memcpy(bh->offset, bin, size);
			TXNv2 *txntmp = (TXNv2*)bh->offset +size;
			Z = size + sizeof(TXNv2);
			bh->pinuse = (unsigned int*)&txntmp->sema; //LINKING BH->INUSE TO HOLDER'S SEMAPHORE. Needed step for lockless memory defragmentation
			txntmp->sema = 1;
			
			byte *raw = bh->offset;
			txntmp->rawsize = (unsigned int*)size;
			txntmp->raw = raw;
			
			//decode raw tx
			//txn version
			txntmp->version = (unsigned int)*raw; raw+=4;
								
			//compact size nVin
			txntmp->nVin = 0;
			usbyte = (byte)raw[0];
			if(usbyte<0xFD) txntmp->nVin = usbyte, n=1;
			else if(usbyte<0xFE) memcpy(&txntmp->nVin, raw +1, 2), n=3;
			else if(usbyte<0xFF) memcpy(&txntmp->nVin, raw +1, 4), n=5;
			raw+=n;

			if(sizeof(VINv2)*txntmp->nVin +Z > bh->size) //buffer is too small, return error
				return 0;

			VINv2 *vintmp = (VINv2*)(bh->offset + Z);
			Z+=sizeof(VINv2)*txntmp->nVin;
			txntmp->vin = vintmp;

			for(y=0; y<txntmp->nVin; y++)
			{
				//refered txn hash
				vintmp[y].txnHash = (unsigned int*)raw; raw+=32;
				k=vintmp[y].txnHash[0];
				for(z=1; z<8; z++)
					k|=vintmp[y].txnHash[z];
				if(k) txntmp->coinbase=0;
				else txntmp->coinbase=1;

				vintmp[y].refOutput = (int*)raw; raw+=4;

				//compact size response script length
				vintmp[y].scriptLength = 0;
				usbyte = (byte)raw[0];
				if(usbyte<0xFD) vintmp[y].scriptLength = usbyte, n=1;
				else if(usbyte<0xFE) memcpy(&vintmp[y].scriptLength, raw +1, 2), n=3;
				else if(usbyte<0xFF) memcpy(&vintmp[y].scriptLength, raw +1, 4), n=5;
				raw+=n;

				//script
				vintmp[y].script = raw;
				raw += vintmp[y].scriptLength;

				//sequence
				raw+=4;
			}

			txntmp->nVout = 0;
			usbyte = (byte)raw[0];
			if(usbyte<0xFD) txntmp->nVout = usbyte, n=1;
			else if(usbyte<0xFE) memcpy(&txntmp->nVout, raw +1, 2), n=3;
			else if(usbyte<0xFF) memcpy(&txntmp->nVout, raw +1, 4), n=5;
			raw+=n;
						
			
			if(sizeof(VOUTv2)*txntmp->nVout +Z > bh->size) //buffer is too small, return error
				return 0;

			VOUTv2 *vouttmp = (VOUTv2*)(bh->offset + Z);
			Z+=sizeof(VOUTv2)*txntmp->nVout;
			txntmp->vout = vouttmp;

			for(y=0; y<txntmp->nVout; y++)
			{
				//value;
				vouttmp[y].value = (ulong64*)raw; raw+=8;

				//compact size challenge script length
				vouttmp[y].scriptLength = 0;
				usbyte = (byte)raw[0];
				if(usbyte<0xFD) vouttmp[y].scriptLength = usbyte, n=1;
				else if(usbyte<0xFE) memcpy(&vouttmp[y].scriptLength, raw +1, 2), n=3;
				else if(usbyte<0xFF) memcpy(&vouttmp[y].scriptLength, raw +1, 4), n=5;
				raw+=n;

				//script
				vouttmp[y].script = raw;
				raw+=vouttmp[y].scriptLength;

				if(vouttmp[y].script[0]==0x41) vouttmp[y].type=1;
				else
				{
					n=0;
					while(vouttmp[y].script[n]==0x76) n++; //OP_DUP

					if(vouttmp[y].script[n]==0xA9) //OP_HASH160
					vouttmp[y].type=0;
				}
			}
								
			//lock time
			txntmp->locktime = (unsigned int*)raw;
			raw+=4;
						
			byte* bmp = bh->offset + Z;
			memcpy(bmp+1, hash, 32);
			txntmp->txHash = (unsigned int*)(bmp +1);
			bmp[0]=1;
			Z+=33;

			bh->Tighten(Z);
								
			return txntmp;
		}
};

TXNtmp TxnHandler;

class MToverlay
{
	public:
		std::atomic_flag af;
		
		MToverlay::MToverlay()
		{
			std::atomic_flag_clear(&af);
		}
};

class BTCHandle
{
	private:
		BTCsocket S;
		char *BTCcommand;
		HtmlHeader hh;
		JSONhandler json;
		char *tmp;
		BufferHeader *bh, *bh2;
		unsigned int i, j, s, *p;

	public:
		byte *raw;

		BTCHandle::BTCHandle()
		{
			BTCcommand = (char*)malloc(max_socket_buffer+1);
			raw = (byte*)malloc(1000);
		}

		BTCHandle::~BTCHandle()
		{
			free(BTCcommand);
		}

		char *SendCoins(int size)
		{
			strcpy(BTCcommand, "{\"json-rpc\": \"1.0\", \"method\": \"sendrawtransaction\", \"params\":[\"");
			OutputRaw(raw, size, BTCcommand + strlen(BTCcommand));
			strcat(BTCcommand, "\"]");

			S.Post(BTCcommand);

			return BTCcommand;
		}

		unsigned int GetChainLength()
		{
			strcpy(BTCcommand, "{\"json-rpc\": \"1.0\", \"method\": \"getblockcount\", \"params\":[]");
			S.Post(BTCcommand);

			tmp = hh.Parse(BTCcommand);
			if(tmp)
			{
				if(json.ParseResult(tmp))
				{
					return atoi(json.result);
				}
			}

			return 0;
		}

		unsigned int GetPendingTXN()
		{
			strcpy(BTCcommand, "{\"json-rpc\": \"1.0\", \"method\": \"getrawmempool\", \"params\":[]");
			S.Post(BTCcommand);

			tmp = hh.Parse(BTCcommand);
			if(tmp)
			{
				json.ParseResult(tmp);
				if(*json.result)
				{
					bh = hBuffer.GetBuffer(hh.content_length, 0);
					bh->linuse = 1; //flag this bh so that it won't be moved by defrag
					bh->move = -1;
					json.GetRawResult(bh->offset);

					//bitcoind doesn't push new txnid, there's no way only ask for these. instead, check all returned txnid with TXNtmp's pending txn buffer
					TxnHandler.ParsePendingTXNID(bh->offset);

					//txnids in the original buffer are returned flagged for further querry. build up the list and get the raw txn
					//json v1 code

					strcpy(BTCcommand, "{\"json-rpc\": \"1.0\", \"method\": \"getrawtransaction\", \"params\":[\"");
					s = strlen(BTCcommand);
					memcpy(&j, bh->offset, 4);
					
					for(i=0; i<j; i++)
					{
						p = (unsigned int*)bh->offset +1 +i*9;
						if(*p)
						{
							//flagged, grab the raw txn
							OutputRaw(p+1, 32, BTCcommand +s);
							strcat(BTCcommand, "\"]");
							S.Post(BTCcommand);
							tmp = hh.Parse(BTCcommand);
							json.ParseResult(tmp);

							bh2 = hBuffer.GetBuffer(hh.content_length, 0);
							*bh2->pinuse = 1;
							bh2->move = -1;
							json.GetRawResult(bh2->offset);
							TxnHandler.AddPendingTXN(bh->offset +8, (unsigned int)(bh->offset +4), p+1);
						}
					}
				}
			}

			return 0;
		}
};

class AddressConverter : public MToverlay
{
	public:
		static mp_limb_t limb58;
		Sha256Round sha;
		ripemd160 ripe;
		byte *abc;
		byte *bout, *vt, *efg;
		mp_limb_t *btcAddress, *Qt, *btctmp, btcRem;
		int n, r, a;

		AddressConverter::AddressConverter()
		{
			abc = (byte*)malloc(65);
			efg = (byte*)malloc(65);
			btcAddress = (mp_limb_t*)malloc(sizeof(mp_limb_t)*8);
			Qt = (mp_limb_t*)malloc(sizeof(mp_limb_t)*8);
			limb58 = 58;
			sha.LargeBuffers(10);
		}

		AddressConverter::~AddressConverter()
		{
			free(abc);
			free(btcAddress);
			free(Qt);
		}

		int GetHash160(void *pubkey, void *out)
		{
			if(pubkey)
			{
				if(!out) bout = efg;
				else bout = (byte*)out;

				sha.Hash(pubkey, -1, 65);
				sha.toBuff(abc);
				ripe.Hash(abc, 32);
				ripe.toBuff(bout);

				return 1;
			}

			return 0;
		}

		void GetBase58(void *hash160, char *out)
		{
			bout = abc;

			//sha256(sha256(hash));
			if(!testnet) bout[0] = 0;
			else bout[0] = 0x6f;
			memcpy(bout +1, hash160, 20);
			sha.Hash(bout, -1, 21);
			sha.toBuff(efg);
			sha.Hash(efg, -1, 32);

				
			//add checksum
			memcpy(bout, sha.HashOP, 4);
			
			vt = (byte*)hash160;
			bout+=4;
			for(a=0; a<20; a++)
				bout[a] = vt[19-a];
			bout-=4;

			if(!testnet) bout[24] = 0;
			else bout[24] = 0x06f;
			btcAddress[6]=0;
			memcpy(btcAddress, bout, 25);

			
			a=6; r=0;
			while(a>-1) 
			{
				mpn_tdiv_qr(Qt, &btcRem, 0, btcAddress, a+1, &limb58, 1);
				bout[r] = Base58str[btcRem];
				btctmp = btcAddress;
				btcAddress = Qt;
				Qt = btctmp;
				
				r++;
				
				if(a==0)
					int klnkl =0;

				if(a>-1) 
					if(!btcAddress[a]) a--;
			}

			if(!testnet)
			{
				bout[r++] = '1';
				bout[r] = 0;
			}
			else bout[r] = 0;
					
			//flip address backwards
			if(out)
			{
				for(n=0; n<r; n++)
					out[r-1-n] = bout[n];

				out[r] = 0;
			}
			else
			{
				for(n=0; n<r; n++)
					efg[r-1-n] = bout[n];

				efg[r] = 0;
				bout = efg;
			}
		}

		int Base58toHash160(char *in, void *out)
		{
			unsigned int *pdc, dc;

			memset(Qt, 0, sizeof(mp_limb_t)*8);
			//memset(btcAddress, 0, sizeof(mp_limb_t)*8);
			n = strlen(in);
			for(r=0; r<n; r++)
			{
				mpn_mul(btcAddress, Qt, 7, &limb58, 1);
				
				dc = B58toI(in[r]);
				memcpy(&btcRem, &dc, 4);

				mpn_add(Qt, btcAddress, 7, &btcRem, 1);
			}

			pdc = (unsigned int*)out;
			for(r=0; r<5; r++)
				pdc[4-r] = Endian(*(Qt +r+1));

			//verify checksum for return data
			byte *chk = (byte*)malloc(25);
			if(!testnet) chk[0]=0;
			else chk[0] = 0x6F;
			memcpy(chk +1, out, 20);

			sha.Hash(chk, -1, 21);
			sha.toBuff(sha.HashOutput);
			sha.Hash(sha.HashOutput, -1, 32);

			if(*sha.HashOP==*Qt) return 1;

			return 0;
		}
		
		void Wipe()
		{
			//CLEAR DATA
		}
};

mp_limb_t AddressConverter::limb58 = 58;

class txnBranch
{
	public:
		unsigned int *CurrentHash;
		ulong64 value;
		unsigned int blockref;
		int outputref;
		TXNv2 **txnref;

		byte intransit;
		byte *TransitHash;

		txnBranch::txnBranch()
		{
			value=0;
			outputref = -1;
			intransit = 0;

			TransitHash = (byte*)malloc(32);
		}

		txnBranch::~txnBranch()
		{
			free(TransitHash);
		}
};

class BitcoinKey : public MToverlay
{
	private:
		unsigned int *randomk;
		int n, b, c, i, y, d, s, r;
		unsigned int a;
		secp256k1mpz p256k1;
		unsigned char* Msg;
		AES aes256;
		Fortuna Frt1, Frt2;
		byte *btmp;

		std::atomic_int32_t *b1, *b2;

		byte *derin, *derout, derb, dc;
		mpz_t R, S;

	public:
		
		AddressConverter AC;
		char *EncryptedPrivateKey;

		BitcoinKey::BitcoinKey()
		{
			randomk = (unsigned int*)malloc(sizeof(int)*8);
			EncryptedPrivateKey = (char*)malloc(32);
			Msg = (unsigned char*)malloc(65);
			b1 = new std::atomic_int32_t[12];
			b2 = new std::atomic_int32_t[12];
		}

		BitcoinKey::~BitcoinKey()
		{
			free(randomk);
			free(EncryptedPrivateKey);
			free(Msg);
			delete[] b1;
			delete[] b2;
		}		

		void EncryptPrivateKey(void *pass, void *privatekey, void *out)
		{
			//takes a 256bit unsigned int passphrase
			aes256.Encrypt(pass, privatekey, 32, out, 0);
		}

		void GetPubKey(void *in, void *out)
		{
			//get public key
			p256k1.MultD(in);
		
			p256k1.GetPubKey(out);
		}

		inline int DERencode(void *in, void *out)
		{
			derin = (byte*) in;
			derout = (byte*) out;
			int ny=0;

			derb = 31;

			while(!derin[derb])
			{
				derb--;
				if(derb==0) break;
			}

			if(derin[derb] & 0x80) *derout = 0, derout++, ny=1;
			dc=0;
			while(dc<=derb)
			{
				derout[dc] = derin[derb -dc];
				dc++;
			}

			char *tt = OutputRaw(in, 32);
			char *cc = OutputRaw(out, 32);

			return derb+1+ny;
		}

		int GetRN()
		{
			if(!Frt1.GetRN(0)) return 0;
			memcpy_atomic(b1, Frt1.PRN, 4);
			if(!Frt2.GetRN(0)) return 0;
			memcpy_atomic(b1 +4, Frt2.PRN, 4);
			if(!Frt2.GetRN(0)) return 0;
			memcpy_atomic(b1 +8, Frt1.PRN, 4);

			n=1;
			char *f1 = OutputRaw(Frt1.PRN, 16);
			char *f2 = OutputRaw(Frt2.PRN, 16);

			char *oc = OutputRaw(b1, 48);
			mpn_tdiv_qr((mp_limb_t*)b2, (mp_limb_t*)b1, 0, (mp_limb_t*)b1, 12, secp256k1mpz::p->_mp_d, 8);
			mpn_add((mp_limb_t*)b2, (mp_limb_t*)b1, 8, (mp_limb_t*)&n, 1);

			return 1;
		}

		int SignTransaction(void *serializedtx, unsigned int txlength, void *DER, void *privatekey)
		{
			mpz_init(R); mpz_init(S);

			btmp = (byte*)DER;
			btmp[0] = 0x30;
			btmp[2] = 0x02;
			btmp+=3;

			//sha256(sha256(serializedtx))
			AC.sha.Hash(serializedtx, -1, txlength);
			AC.sha.toBuff(AC.sha.HashOutput);
			AC.sha.Hash(AC.sha.HashOutput, -1, 32);
			//AC.sha.toBuff(AC.sha.HashOutput);

			char *sm = OutputRaw(AC.sha.HashOutput, 32);
			/*** RNG ***/
			//get random k;
			//same as private key generation
			if(!Frt1.GetRN(randomk)) return 0;
			if(!Frt2.GetRN(randomk+4)) return 0;

			mpz_t x; mpz_init(x); 
			mpz_t z; mpz_init(z);
			set_mpz(x, randomk, 32);
			mpz_mod(z, x, secp256k1mpz::p);

			unsigned int *one = (unsigned int*)malloc(32);
			memset(one, 0, 32);
			one[0] = 1;

			//k*G
			p256k1.MultD((unsigned int*)z->_mp_d);
			mpz_mod(x, p256k1.X, secp256k1mpz::N);
			
			n = DERencode(x->_mp_d, btmp+1);
			//mpz_set(R, x);
			btmp[0] = (byte)n;
			btmp += n+1;
			
			btmp[0] = 0x02;
			btmp++;

			p256k1.getS(z->_mp_d, AC.sha.HashOP, x->_mp_d, privatekey);
			//p256k1.getS(z->_mp_d, serializedtx, p256k1.X->_mp_d, privatekey); 

			b = DERencode(p256k1.tmp1->_mp_d, btmp+1);
			btmp[0] =(byte)b;
			//mpz_set(S, p256k1.tmp1);

			btmp = (byte*)DER;

			btmp[1] = (byte)(n+b+4);
			
			mpz_clear(z);
			mpz_clear(x);

			return btmp[1]+2;
		}

		int VerifySignature(void *serializedtx, int txlength, void *DER, void *pubkeyx, void *pubkeyy)
		{
			//sha256(sha256(serializedtx))
			AC.sha.Hash(serializedtx, -1, txlength);
			AC.sha.toBuff(AC.sha.HashOutput);
			AC.sha.Hash(AC.sha.HashOutput, -1, 32);
			//AC.sha.toBuff(AC.sha.HashOutput);

			char *sr = OutputRaw(AC.sha.HashOutput, 32);

			return p256k1.VerifySig(AC.sha.HashOP, DER, pubkeyx, pubkeyy);
		}

		int BuildKeyPair(void *out)
		{
			/*** RNG ***/
			/*** get rn = 384 bits worth of rng then privatekey = rn mod (N-1) +1***/
			//build private key
			if(!GetRN()) return 0;

			memcpy(out, b2, 32);
			memset_atomic(b1, 0, 12);
			memset_atomic(b2, 0, 12);

			return 1;
		}
		
		void Wipe()
		{
			/*** Clean up everything used in the address creation ***/

			n = b = c = i = y = d = s = r = 0;
			a = 0;
			AC.Wipe();
			p256k1.Wipe();
			aes256.Wipe();

			memset(Msg, 0xCD, 66);
		}

		void SetPubKey(char *in)
		{
			p256k1.SetPubKey(in);
		}
};

class ConvertHandler
{
	private:
		static const unsigned int nAC = 100;
		static const unsigned int nBC = 20;
		std::atomic_int ac_last_flagged, bc_last_flagged;

		/***Consider managing SHA objects as well (small and big messages)***/

		AddressConverter *AC;
		BitcoinKey *BC;
	public:
		
		ConvertHandler::ConvertHandler()
		{
			AC = new AddressConverter[nAC];
			BC = new BitcoinKey[nBC];
			ac_last_flagged = bc_last_flagged = 0;
		}

		AddressConverter* AcquireAC()
		{
			int i = std::atomic_load(&ac_last_flagged);
			while(std::atomic_flag_test_and_set(&AC[i].af)==TRUE)
			{
				i++;
				if(i>=nAC) i=0;
			}

			std::atomic_store(&ac_last_flagged, i);
			return AC +i;
		}

		BitcoinKey* AcquireBC()
		{
			int i = std::atomic_load(&bc_last_flagged);
			while(std::atomic_flag_test_and_set(&BC[i].af)==TRUE)
			{
				i++;
				if(i>=nBC) i=0;
			}

			std::atomic_store(&bc_last_flagged, i);
			return BC +i;
		}

		void Release(MToverlay *min)
		{
			std::atomic_flag_clear(&min->af);
		}
};

ConvertHandler CV;

class BTCaddress
{
	private:
		std::atomic_int32_t *privatekey;

	public:
		char *encrypted_pkey;
		char *user; unsigned int Iindex;
		unsigned int *hash160;
		char *Base58;
		unsigned char *publickey;

		static BitcoinKey sBTCK;
		AddressConverter *pAC;
		BitcoinKey *pBTCK;

		txnBranch **Branches;
		unsigned int nBranches;
		txnBranch *tbr;
		unsigned int *vinp, nvinp;
		byte *bvins, *bvouts, *bo;

		ulong64 value, inChange, inTransit, tV, tV2;
		VOUTv2 *tmpvout;

		unsigned int i, nDER;
		byte *bin, **DER, a; 
		int *b3, *b4;
		unsigned int CBa, CBb, *bt, bos, bis, m, n, b, c;
		
		unsigned int refblock, earliestblock;
		std::atomic_uint32_t *psema;
		std::atomic_uint32_t updatecounter;

		BTCaddress::BTCaddress()
		{
			hash160=0; //(unsigned char*)malloc(20);
			Base58=0; //(char*)malloc(30);
			publickey=0;// (unsigned char*)malloc(65);
			refblock=0;
			earliestblock=0;
			value=0;
			bvouts = (byte*)malloc(500);
			bvins  = (byte*)malloc(500);
			DER=0; vinp=0;

			Branches=0;
			nBranches=0;

			nvinp=0;
			nDER=0;
			privatekey=0;

			encrypted_pkey = (char*)malloc(65);
			user = (char*)malloc(33);
			Iindex=-1;

			updatecounter.store(0);
		}

		BTCaddress::~BTCaddress()
		{
			if(hash160) free(hash160);
			if(Base58) free(Base58);
			if(publickey) free(publickey);

			if(Branches)
			{
				for(CBa=0; CBa<nBranches; CBa++)
					delete[] Branches[CBa];
				free(Branches);
			}

			delete[] privatekey;
			free(encrypted_pkey);
			free(user);
		}

		int CreateAccount(void *pass)
		{
			pBTCK = CV.AcquireBC();
			pAC = CV.AcquireAC();
			
			//get key pair
			if(!BuildKeyPair()) return 0;

			//Encrypt private key
			pBTCK->EncryptPrivateKey(pass, privatekey, encrypted_pkey);
			
			earliestblock = LAST_BLOCK;
			SetupP();

			//Dump pass and unencrypted privatekey
			memset_atomic(privatekey, 0, 8);
			memset_atomic((std::atomic_int32_t*)pass, 0, 8);

			CV.Release(pBTCK);
			CV.Release(pAC);
			pBTCK=0; pAC=0;

			return 1;
		}

		int BuildKeyPair()
		{
			if(!privatekey) privatekey = new std::atomic_int32_t[8];
			
			/*** DEAL WITH FAILED RNG ***/
			if(!pBTCK->BuildKeyPair(privatekey)) return 0;

			//Setup();

			return 1;
		}

		unsigned int GetEarliestBLock()
		{
			unsigned int se = 0;
			
			if(nBranches)
			{
				unsigned int me = 1;
				se = Branches[0]->blockref;
				for(me; me<nBranches; me++)
					if(Branches[me]->blockref<se) se = Branches[me]->blockref;
			}
			else se = earliestblock;

			return se;
		}

		void Setup()
		{
			if(privatekey)
			{
				if(!publickey) publickey = (byte*)malloc(65);

				sBTCK.GetPubKey(privatekey, publickey);
			}

			if(publickey)
			{
				if(!hash160) hash160 = (unsigned int*)malloc(20);

				pAC = &sBTCK.AC;
				pAC->GetHash160(publickey, hash160);
			}

			if(hash160)
			{
				if(!Base58) Base58 = (char*)malloc(50);

				pAC = &sBTCK.AC;
				pAC->GetBase58(hash160, Base58);
			}

			refblock = earliestblock;
		}

		void SetupP()
		{
			if(privatekey)
			{
				if(!publickey) publickey = (byte*)malloc(65);

				pBTCK->GetPubKey(privatekey, publickey);
			}

			if(publickey)
			{
				if(!hash160) hash160 = (unsigned int*)malloc(20);

				pAC->GetHash160(publickey, hash160);
			}

			if(hash160)
			{
				if(!Base58) Base58 = (char*)malloc(50);

				pAC->GetBase58(hash160, Base58);
			}

			refblock = earliestblock;
		}

		unsigned int CompareBuffers(void *b1, void *b2, int size)
		{
			CBa = size/4;
			b3 = (int*)b1;
			b4 = (int*)b2;

			CBb=0; i=0;

			while(!CBb && i<CBa)
			{
				CBb = *b3 ^ *b4;
				b3++; b4++;

				i++;
			}

			return CBb;
		}

		int Compare(void *in, int n)
		{
			//single threaded

			pAC = &sBTCK.AC;
			/*return values:
			-1: can't process
			 0: equal
			 1: not equal
			*/

			bin = (byte*)in;

			if(n==20) //hash160
			{
				if(hash160) 
				{
					return CompareBuffers(in, hash160, n);
				}
				else
				{
					if(Base58)
					{
						pAC->GetBase58(in, 0);
						return strcmp((char*)pAC->bout, Base58);
					}
				}
			}
			else if(n==65) //pubkey
			{
				if(publickey)
					return CompareBuffers(in, publickey, 65);
				else 
				{
					pAC->GetHash160(in, 0);
					pAC->GetBase58(pAC->bout, 0);
					return strcmp((char*)pAC->bout, Base58);
				}
			}
			else if(!n) //base58
			{
				return !strcmp((char*)in, Base58);
			}

			return -1; //can't process
		}

		void ClearPrivateKey()
		{
			memset(privatekey, 0xCD, 32);
		}

		void SetPrivateKeyStr(char *in)
		{
			//ignore white spaces, stop at 256bits
			if(!privatekey) privatekey = new std::atomic_int32_t[8];

			n=0, i=strlen(in); c=32; b=0;
			memset(privatekey, 0, 32);

			unsigned int *bp = (unsigned int*)privatekey;
			bp+=7;

			while(n<i)
			{
				a = in[n];

				if(a>='0')
				{
					if(a>='a') a-=87;
					else if(a>='A') a-=55;
					else a-=48;

					c-=4;
					b |= a << c;
					if(!c) 
					{
						c=32;
						bp[0] = b;
						bp--;
						b=0;
					}
				}

				n++;
			}
		}
		void SetEncryptedPrivateKey(void *in, int l)
		{
			/***why is encrypted_pkey 65 bytes long??***/
			memcpy(encrypted_pkey, (byte*)in, l);
		}

		void BuildAddress(AddressConverter *AC)
		{
			if(AC) pAC = AC;
			else pAC = &sBTCK.AC;

			
			if(!Base58)

			if(!hash160) hash160 = (unsigned int*)malloc(sizeof(int)*5);

			if(pAC->GetHash160(publickey, hash160))
			{
				if(!Base58) Base58 = (char*)malloc(60);
				pAC->GetBase58(hash160, Base58);
			}
		}

		void SetPubKey(void *in, int len)
		{
			if(!publickey)
				publickey = (unsigned char*)malloc(65);

			if(len==64)
			{
				publickey[0] = 0x04;
				memcpy(publickey+1, in, len);
			}
			else memcpy(publickey, in, 65);
		}

		void SetBase58(char *in)
		{
			if(!Base58) Base58 = (char*)malloc(60);
			strcpy(Base58, in);
		}

		void ComputeBalance()
		{
			value=inChange=inTransit=0;
	
			for(CBa=0; CBa<nBranches; CBa++)
			{
				if(!Branches[CBa]->intransit) value += Branches[CBa]->value;
				else inTransit += Branches[CBa]->value;
			}

			//order branches from highest to lowest value
			i=1;
			while(i)
			{
				i=0;

				for(CBa=1; CBa<nBranches; CBa++)
				{
					CBb = CBa-1;

					if(Branches[CBa]->value>Branches[CBb]->value)
					{
						tbr = Branches[CBa];
						Branches[CBa] = Branches[CBb];
						Branches[CBb] = tbr;

						i=1;
					}
				}
			}

			//dump branches with 0 value
			for(i=nBranches -1; i>-1; i--)
			{
				if(!Branches[i]->value && !Branches[i]->intransit)
				{
					delete[] Branches[i];
					nBranches--;
				}
			}

			updatecounter.fetch_add(1);
		}

		int Spend(ulong64 V, byte *h160, BTCHandle *out)
		{
			/*** SHIT TO CHECK
			1: that you have the privatekey for this publickey
			2: that hash160 has been computed
			***/

			//check V vs value
			if(V<=value)
			{
				if(nvinp<nBranches)
				{
					free(vinp);
					vinp = (unsigned int*)malloc(sizeof(int)*nBranches+1);
					nvinp = nBranches;
				}

				if(nDER<nBranches)
				{
					DER = (byte**)realloc(DER, sizeof(byte*)*nBranches);
					for(i=nDER; i<nBranches; i++)
						DER[i] = (byte*)malloc(150);

					nDER = nBranches;
				}

				//build list of branches
				tV = V + txfee; tV2 = 0;
				for(i=0; i<nBranches; i++)
				{
					if(!Branches[i]->intransit) tV2+=Branches[i]->value;
					
					if(tV2>=tV) 
					{
						i++;
						break;
					}
				}
				CBa = i;
				
				if(tV2>=tV)
				{
					//build Vouts
					bin = bvouts;

					/* tx to h160 */
					//nVout
					if(tV2==tV) bvouts[0]=1;
					else bvouts[0]=2;
					bvouts++;
					
					//value
					memcpy(bvouts, &V, 8); bvouts+=8;

					bt = (unsigned int*)bvouts;
					bt[0] = 0x14A97619;
					memcpy(bvouts +4, h160, 20);
					bvouts[24] = 0x88; //OP_EQUALVERIFY
					bvouts[25] = 0xAC; //OP_CHECKSIG
					bvouts+=26;
					
					char *cf = OutputRaw(bt, 26);
					char *rf = OutputRaw(h160, 20);

					//change
					if(tV2>tV)
					{
						//value
						tV2 -= tV;
						memcpy(bvouts, &tV2, 8); bvouts+=8;
						
						bt = (unsigned int*)bvouts;
						bt[0] = 0x14A97619;
						memcpy(bvouts +4, hash160, 20);
						bvouts[24] = 0x88; //OP_EQUALVERIFY
						bvouts[25] = 0xAC; //OP_CHECKSIG
						bvouts+=26;
					}
					
					bt = (unsigned int*)bvouts;
					bt[0]=0; //lock time
					bt[1]=1; //hashtype

					//bvouts size
					bos = (unsigned int)(bvouts -bin) +8;
					bvouts = bin;

					//build naked Vins
					bin = bvins;
					bt = (unsigned int*)bvins;
					//version
					bt[0] = 1;
					bin+=4;


					//varint nVin
					CBb = varintCompress((ulong64)CBa, bin);
					bin+=CBb;

					for(i=0; i<CBa; i++)
					{
						if(Branches[i]->intransit)
						{
							//tx hash
							memcpy(bin, Branches[i]->CurrentHash, 32);
							bin+=32;

							//output ref
							memcpy(bin, &Branches[i]->outputref, 4);
							bin+=4;

							//mark start of script
							vinp[i] = (unsigned int)(bin -bvins);

							//varint scriptlength
							bin[0]=0; bin++;

							//sequence
							bt = (unsigned int*)bin;
							bt[0] = 0xFFFFFFFF;
							bin+=4;
						}
					}

					bis = (unsigned int)(bin -bvins);
					vinp[i] = bis;

					//build per Vin serialized tx
					for(i=0; i<CBa; i++)
					{
						if(!Branches[i]->intransit)
						{
							bo = (byte*)out->raw;
							psema = &(*Branches[i]->txnref)->sema;
							psema->fetch_add(1);
							tmpvout = &(*Branches[i]->txnref)->vout[Branches[i]->outputref];

							//copy header + empty vins
							memcpy(bo, bvins, vinp[i]);
							bo+=vinp[i];

							//append refered vout script
								//first find last op_codesperatator (0xab)
								CBb = -1; m=0;
								while(m<tmpvout->scriptLength)
								{
									/*** expand to multi byte op_code script length ***/
									if(tmpvout->script[m]<0x4C) m+=tmpvout->script[m] +1;
									else 
									{
										if(tmpvout->script[m]==0xAB) CBb=m;
										m++;
									}
								}

								m = tmpvout->scriptLength -CBb -1;
								CBb++;

								b = varintCompress(m, bo);
								bo+=b;
								memcpy(bo, tmpvout->script +CBb, m);
								bo+=m;
								
							//append end of the vins
							memcpy(bo, bvins +vinp[i]+1, bis -vinp[i]-1);
							bo+= (bis -vinp[i]-1);

							//append vouts
							memcpy(bo, bvouts, bos);
							CBb = (unsigned int)bo - (unsigned int)out->raw +bos;

							//sign
							m = sBTCK.SignTransaction(out->raw, CBb, DER[i] +1, privatekey);
							//sBTCK.VerifySignature(out->raw, CBb, DER[i] +1, publickey +1, publickey +33);

							//append public key
							DER[i][0] = m+1; //signature size
							DER[i][m+1] = 0x01; //hash type
							DER[i][m+2] = 0x41; //publickey size
							memcpy(DER[i] +m+3, publickey, 65); //publickey

							psema->fetch_add(-1);
						}
					}

					//pack it all with the signature
					bo = (byte*)out->raw;
					memcpy(bo, bvins, vinp[0]);
					bo+=vinp[0];

					for(i=0; i<CBa; i++)
					{
						if(!Branches[i]->intransit)
						{
							//scriptlength
							m = DER[i][0] + 67;
							CBb = varintCompress(m, bo);
							bo+=CBb;

							//script
							memcpy(bo, DER[i], m);
							bo+=m;

							//extra shit till next script
							m = vinp[i+1] - vinp[i] -1;
							memcpy(bo, bvins +vinp[i] +1, m);
							bo+=m;
						}
					}

					//append vouts without the hashtype (last 4 bytes)
					memcpy(bo, bvouts, bos-4);
					CBb = (unsigned int)(bo -out->raw) +bos-4;
					
					pAC->sha.Hash(out->raw, -1, CBb);
					pAC->sha.toBuff(bvouts);
					pAC->sha.Hash(bvouts, -1, 32);

					//send to network
					char *rt = out->SendCoins(CBb);

					//check return with tx hash
					char *tt = OutputRaw(pAC->sha.HashOP, 32);
					if(!strcmp(rt, tt))
					{
						//tx was accepted by network
						//update branches

						//if several branches were used, collapse all but index 0
						m=0;
						for(i=0; i<CBa; i++)
						{
							if(!Branches[i]->intransit) 
							{
								if(!m)
								{
									Branches[i]->intransit = 1;
									memcpy(Branches[i]->TransitHash, pAC->sha.HashOP, 32);
									m=1;
								}
								else Branches[i]->value = 0;
							}
						}

						ComputeBalance();

						return 1; //succesful send
					}

					return -3; //network turned down the tx
				}
				
				return -2; //enough for spend, not enough for tx fee
			}

			return -1; //balance too low
		}
};

BitcoinKey BTCaddress::sBTCK;

class AddressHandler
{
	private:
		static const unsigned int bufferincrement = 100;

		unsigned int i;

	public:
		BTCaddress **address;
		BufferHeader *BHcurrent;
		std::atomic_uint naddress_pre, buffersize, naddress, flag;

		AddressHandler::AddressHandler()
		{
			address=0;
			naddress=naddress_pre=0;
			buffersize=0;
			flag.store(0);
			BHcurrent=0;
		}

		AddressHandler::~AddressHandler()
		{
		}

		void Drop(BTCaddress *bin)
		{
			if(bin)
			{
				address[bin->Iindex] = 0;
				delete bin;
			}
		}

		BTCaddress *New()
		{
			while(flag) {};
			unsigned int r;
			r = naddress_pre.fetch_add(1);

			fetchloop:
				if(naddress_pre>buffersize)
				{
					if(flag.exchange(1)==FALSE)
					{
						Extend();
						flag.store(0);
					}

					while(flag) {};
					goto fetchloop;
				}

			std::atomic_uint32_t *p = (std::atomic_uint32_t*)address -1;
			p->fetch_add(1);
			BTCaddress *ba = new BTCaddress;
			address[r] = ba;
			ba->Iindex = r;
			p->fetch_add(-1);
			naddress.fetch_add(1);	

			return ba;
		}

		void Extend()
		{
			std::atomic_uint32_t *au;
			
			if(address)
			{
				au = (std::atomic_uint32_t*)address -1;
				while(au) {} //wait for addresssw to be unused
			}

			unsigned int n = buffersize + bufferincrement;

			BufferHeader *bh = hBuffer.GetBuffer(n*sizeof(BTCaddress*) +4, 0);
			bh->move=-1; //possible timing issue if move is set after pool is unlocked
			bh->pinuse = (unsigned int*)bh->offset;
			*bh->pinuse = 1; //same with inuse

			BTCaddress **newAd = (BTCaddress**)(bh->offset +4);
			memcpy(newAd, address, sizeof(BTCaddress*)*buffersize);
			memset(newAd +buffersize, 0, sizeof(BTCaddress*)*bufferincrement);

			if(address) au->fetch_add(-1); //decrement sema

			address = newAd;
			
			if(BHcurrent) BHcurrent->move=0;
			BHcurrent = bh;

			std::atomic_fetch_add(&buffersize, bufferincrement);
		}

		BTCaddress *GetBTCA(unsigned int ind)
		{
			//consider semaphore for these
			if(ind<buffersize) return address[ind];
			
			return 0;
		}

		char *GetName(unsigned int ind)
		{
			//consider semaphore for these
			if(ind<buffersize) return address[ind]->user;

			return 0;
		}

		BTCaddress *GetAddress(unsigned int ind)
		{
			//consider semaphore for these
			if(ind<buffersize) return address[ind];

			return 0;
		}

		BTCaddress *Find(byte *bin)
		{
			//compares bin to pubkeys in address
			std::atomic_uint32_t *au = (std::atomic_uint32_t*)address -1;
			au->fetch_add(1);

			unsigned int i=0, *a = (unsigned int*)(bin +1), *b, c, m;
			for(i; i<naddress; i++)
			{
				b = (unsigned int*)(address[i]->publickey +1);

				if(*a==*b)
				{
					c=0; m=1;

					while(m<8)
					{
						c += a[m] ^ b[m];
						m++;
					}

					if(!c) goto found;
				}
			}

			au->fetch_add(-1);
			return 0;

			found:
			au->fetch_add(-1);
			return address[i];
		}
};

AddressHandler AH;

class BlockHeader
{
	public:
		/* 80 bytes block header:
			 4 bytes: block version
			32 bytes: hash of previous block
			32 bytes: merkle root
			 4 bytes: timestamp
			 4 bytes: compressed target difficulty
			 4 bytes: nonce

		   68 extra bytes:
			32 bytes: block hash
			 4 bytes: block height, -1 for forked blocks
			 4 bytes: size
			 4 bytes: loaded
			 8 bytes: offset
		*/
		unsigned int version;
		unsigned int HashPre [8];
		unsigned int Merkle [8];
		unsigned int Timestamp;
		unsigned int difficulty;
		unsigned int nonce;
		unsigned int Hash [8];
		int height, mfi, size;
		std::atomic_int8_t loaded; //0: virgin, -1: loading, 1: loaded, 2: loaded but dumped some or all txn
		
		ulong64 offset;
			

		BlockHeader::BlockHeader()
		{
		}

		BlockHeader::~BlockHeader()
		{
		}
};

class Forks
{
	private:
		unsigned int *ob, nob;

	public:
		unsigned int *blocks; //index reference to blocks ordered as found in the raw chain file
		unsigned int nblocks, bufferat;
		unsigned int parentblockid; //index in parent fork block list

		unsigned int *children; //2 ints per step: 1) pointer address to child class 2) blockid the child spawns at
		ulong64 *lengthbeyondchild; //strength of the fork beyond the child spawning point, calculated by adding the target of all blocks after the spawn point, used to compare child to main
		unsigned int nchildren;

		ulong64 length; //total fork strength, calculated by adding the target of all blocks within the fork
		int deadfork; //flag marking fork is dead and rdy for wiping
		unsigned int copyat; //marks starting offset for partial copies
		unsigned int lengthid;

		static const unsigned int bstep = 100;
		BlockHeader **lbh;

		Forks::Forks()
		{
			blocks=children=0; 
			lengthbeyondchild=0;
			nblocks=nchildren=0;
			length=0;
			deadfork=0;
			bufferat=0;
			copyat=0;
			parentblockid=0;
			lbh=0;
			ob=0;
			nob=0;
			lengthid=0;
		}

		Forks::~Forks()
		{
			free(blocks);
			free(children);
			free(lengthbeyondchild);
			free(ob);
		}

		int AddChild(Forks *fin)
		{
			//look for fin origin in current fork
			int l;
			for(l=nblocks-1; l>-1; l--)
			{
				if(blocks[l]==fin->blocks[0]) break; //found fork origin within current block list
			}

			if(l==-1) return 0;

			//slow code for quick dev purpose, speed it up later
			children = (unsigned int*)realloc(children, sizeof(int)*(nchildren+1)*2);
			children[nchildren*2] = (unsigned int)fin;
			children[nchildren*2 +1] = l;

			lengthbeyondchild = (ulong64*)realloc(lengthbeyondchild, sizeof(ulong64)*(nchildren+1));

			nchildren++;
			return 1;
		}

		void GetLength(BlockHeader **bh)
		{
			if(nob<nchildren) 
			{
				free(ob);
				ob = (unsigned int*)malloc(sizeof(int)*nchildren);
				nob = nchildren;
			}

			if(!bh) bh=lbh;
			else lbh=bh;

			unsigned int i, m, s;
			int y=-1, p;

			int chcut=-1;

			if(nchildren) 
			{
				for(i=0; i<nchildren; i++)
					ob[i] = children[i*2 +1];

				p=1;
				while(p)
				{
					p=0;
					for(i=0; i<nchildren-1; i++)
					{
						s=i+1;
						if(ob[i]>ob[s])
						{
							m=ob[i];
							ob[i] = ob[s];
							ob[s] = m;

							p=1;
						}
					}
				}

				y = ob[0];

				for(i=0; i<nchildren; i++)
					if(ob[i]<=lengthid) chcut = i;

			}

			ulong64 extlength = 0;
			m=chcut+1;
			for(i=lengthid; i<nblocks; i++)
			{
				extlength+=bh[blocks[i]]->difficulty;

				if(i==y)
				{
					if(m<nchildren)
					{
						lengthbeyondchild[m] = extlength;
						m++;
						y = ob[m];
					
						i--; //this guaranties forks sharing the same start index get size calculation
						extlength-=bh[blocks[i]]->difficulty;
					}
				}
			}

			/*** check this code ***/

			length+=extlength;
			lengthid = nblocks;

			for(i=chcut+1; i<nchildren; i++)
				lengthbeyondchild[i] = length -lengthbeyondchild[i];

			for(p=0; p<=chcut; p++)
				lengthbeyondchild[p]+=extlength;

			SortChildren();
		}

		unsigned int GetLast()
		{
			return blocks[nblocks-1];
		}

		void AddBlock(unsigned int id)
		{
			if(nblocks==bufferat)
			{
				blocks = (unsigned int*)realloc(blocks, sizeof(int)*(nblocks+1)*bstep);
				bufferat+=bstep;
			}

			blocks[nblocks] = id;
			nblocks++;
		}

		void SortChildren()
		{
			unsigned int m=1, i, s, p, v;
			ulong64 r;
			while(m)
			{
				m=0;
				for(i=1; i<nchildren; i++)
				{
					s=i-1;
					if(children[s*2 +1]>children[i*2 +1])
					{
						/*** error swapping children ***/
						r = lengthbeyondchild[s];
						p = children[s*2]; v = children[s*2 +1];

						lengthbeyondchild[s] = lengthbeyondchild[i];
						children[s*2]    = children[i*2];
						children[s*2 +1] = children[i*2 +1];

						lengthbeyondchild[i] = r;
						children[i*2] = p;
						children[i*2 +1] = v;

						m=1;
					}
				}
			}
		}

		Forks& Forks::operator= (Forks &rhs) //copies a fork from block index in copyat
		{
			if(&rhs!=this)
			{
				unsigned int bfat = rhs.bufferat - rhs.copyat;
				unsigned int nbl  = rhs.nblocks  - rhs.copyat;

				if(this->bufferat<bfat)
				{
					this->blocks = (unsigned int*)realloc(this->blocks, sizeof(int)*bfat);
					this->bufferat = bfat;
				}

				memcpy(this->blocks, rhs.blocks + rhs.copyat, sizeof(int)*nbl);
				this->nblocks = nbl;

				unsigned int nch=0, chst=0;
				unsigned int i;

				for(i=0; i<rhs.nchildren; i++)
				{
					if(rhs.children[i*2 +1]>rhs.copyat)
					{
						nch = rhs.nchildren - i;
						chst = i;
						break;
					}
				}

				if(this->nchildren<nch)
				{
					this->children = (unsigned int*)realloc(this->children, sizeof(int)*nch*2);
					this->lengthbeyondchild = (ulong64*)realloc(this->lengthbeyondchild, sizeof(ulong64)*nch);
				}

				memcpy(this->children, rhs.children +chst*2, sizeof(int)*nch*2);
				for(i=0; i<nch; i++)
					this->children[i*2]-=copyat;

				this->nchildren = nch;

				this->deadfork = rhs.deadfork;

				this->lengthid=0;
				this->GetLength(rhs.lbh);
			}

			return *this;
		}

		Forks& Forks::operator+= (Forks& rhs) //used to merge a fork with its child
		{
			if(&rhs!=this)
			{
				//step 1: make sure rhs is a fork of this
				unsigned int i=0, n, p;
				for(i=0; i<this->nchildren; i++)
				{
					if(&rhs==(Forks*)this->children[i*2])
						goto step2;
				}

				return *this;

				//step 2: copy blocks over
				step2:
				
					unsigned int nbl = this->nblocks - this->children[i*2 +1];
					unsigned int bfat = this->bufferat - this->children[i*2 +1];
					
					this->copyat = nbl;
					
					//Forks *ftmp = (Forks*)this->children[i*2];
					
					if(bfat<rhs.nblocks)
					{
						this->blocks = (unsigned int*)realloc(this->blocks, sizeof(int)*(this->children[i*2 +1] +rhs.nblocks));
						this->bufferat = this->children[i*2 +1] +rhs.nblocks;
					}

					memcpy(this->blocks +this->children[i*2 +1], rhs.blocks, sizeof(int)*rhs.nblocks);

					this->nblocks = this->children[i*2 +1] +rhs.nblocks;

				//step 3: copy children over
					p = this->children[i*2 +1];
					if(rhs.nchildren +i>this->nchildren)
					{
						this->children = (unsigned int*)realloc(this->children, sizeof(int)*(i +rhs.nchildren +1)*2);
						this->lengthbeyondchild = (ulong64*)realloc(this->lengthbeyondchild, sizeof(ulong64)*(i +rhs.nchildren +1));
					}
				
					this->nchildren = i+rhs.nchildren +1;
					for(n=0; n<rhs.nchildren; n++)
					{
						this->children[(i+1+n)*2] = rhs.children[n*2];
						this->children[(i+1+n)*2 +1] = rhs.children[n*2 +1] +p;
					}

				//step 4: fix length past copy point
					/*ulong64 l = this->length - this->lengthbeyondchild[i];
					for(n=0; n<rhs.nchildren; n++)
						this->lengthbeyondchild[i+1] = rhs.lengthbeyondchild[n] + l;

					this->length = l + rhs.length;*/

					this->lengthid=0;
					this->GetLength(0);
			}

			return *this;
		}
};

class BLOCKSv2
{
	public:
		ulong64 ntx;
		BlockHeader *BH;

		unsigned int length;

		TXNv2 **txn;
		unsigned int *rawsize; //holds raw size for each txn, pointed to by txn, saved when txn are dumped to smooth up eventual reload
		BufferHeader *bh;

		std::atomic_uint32_t sema;

		BLOCKSv2::BLOCKSv2()
		{
			txn=0;
			rawsize=0;
			BH=0;
			ntx=0;
			sema=1;
		}

		BLOCKSv2::~BLOCKSv2()
		{
		}
};

class RawChain
{
	private:
		static const unsigned int nBlocksBuffer = 1000;
		unsigned int bhbs, blength;

		void ExtendRawBH()
		{
			BlockHeader **bhtmp = (BlockHeader**)malloc(sizeof(BlockHeader*)*(bhbs+nBlocksBuffer));
			memcpy(bhtmp, rawBH, sizeof(BlockHeader*)*bhbs);
			MemPool *mp = hBuffer.GetPoolU(nBlocksBuffer*sizeof(BlockHeader));

			mp->reserved-=nBlocksBuffer*sizeof(BlockHeader);
			mp->freemem+=nBlocksBuffer*sizeof(BlockHeader);
			BufferHeader *pbh = mp->PushBH(0, 0, 0);
			mp->PushBH(pbh, nBlocksBuffer*sizeof(BlockHeader), 0);

			for(int i=0; i<nBlocksBuffer; i++)
				bhtmp[i +bhbs] = (BlockHeader*)pbh->offset +i;

			bhbs+=nBlocksBuffer;
			free(rawBH); /***thread unsafe***/
			rawBH = bhtmp;
		}

		void ReallocBlocks(unsigned int size)
		{
			/***needs semaphore for aggressive timing issues***/
			if(size<blength) return;
			int a = size + blength;
		
			unsigned int b = a/nBlocksBuffer +1;
			b*=nBlocksBuffer;

			BLOCKSv2 **bvtmp = (BLOCKSv2**)malloc(sizeof(BLOCKSv2*)*b);
			memcpy(bvtmp, blocksv2, sizeof(BLOCKSv2*)*blength);
				
			unsigned int ss = b-blength, k;
			ss*=sizeof(BLOCKSv2);

			MemPool *mp = hBuffer.GetPoolU(ss);
			mp->reserved-=ss;
			mp->freemem+=ss;
			BufferHeader *pBH = mp->PushBH(0, 0, 0);
			memset(pBH->offset, 0, ss);
			for(k=blength; k<b; k++)
				bvtmp[k] = (BLOCKSv2*)pBH->offset + k;
			mp->PushBH(pBH, ss, 0);
				
			BLOCKSv2 **bvtmp2 = blocksv2;
			if(blockstmp) free(blockstmp); //semaphore here to make sure noone is using the buffer before freeing it
			blockstmp = blocksv2; 
			blocksv2 = bvtmp;
			
			blength = b;
		}

	public:
		Forks **forks;
		unsigned int nforks;

		BlockHeader **rawBH;
		BLOCKSv2 **blocksv2, **blockstmp;
		unsigned int NB;

		MultiFile MF;
		unsigned int n;

		static const unsigned int MOFUCKINMAGICWORD_BITCH = 0xD9B4BEF9;
		static const unsigned int magicword_testnet = 0x0709110B;
		static int MW;
		Sha256Round sha;

		RawChain::RawChain()
		{
			rawBH=0;
			NB=0;
			forks=0;
			nforks=0;
			blocksv2=0;
			blockstmp=0;
			blength=0;
		}

		RawChain::~RawChain()
		{
			free(rawBH);

			for(unsigned int i=0; i<nforks; i++)
				delete[] forks[i];
			free(forks);
		}

		BlockHeader *NewRawBH()
		{
			if(NB>=bhbs)
				ExtendRawBH();

			return rawBH[NB++];
		}

		static void Init()
		{
			if(!testnet) 
				MW = MOFUCKINMAGICWORD_BITCH;
			else 
				MW = magicword_testnet;

			MWG = MW;
		}

		void ParseRawBlocks(unsigned int rawid)
		{
			unsigned int n, j, i, c;
			int l;

			//find all forks
			/*forks = (Forks**)malloc(sizeof(Forks*));
			forks[0] = new Forks;
			forks[0]->AddBlock(0);
			nforks=1;*/

			for(n=rawid; n<NB; n++)
			{
				for(j=0; j<nforks; j++)
				{
					i=forks[j]->GetLast();
					if(!CompareBuffers(rawBH[n]->HashPre, rawBH[i]->Hash, 32))
					{
						forks[j]->AddBlock(n);
						break;
					}
				}

				if(j==nforks) //didn't find a corresponding fork, add a fork
				{
					//look for fork start
					for(l=n-1; l>-1; l--)
					{
						if(!CompareBuffers(rawBH[n]->HashPre, rawBH[l]->Hash, 32))
						{
							forks = (Forks**)realloc(forks, sizeof(Forks*)*(nforks+1));
							forks[nforks] = new Forks;
							forks[nforks]->AddBlock(l);
							forks[nforks]->AddBlock(n);
							nforks++;
							break;
						}
					}

					if(l>-1) //fork start was found
					{
						for(c=0; c<nforks-1; c++)
						{
							if(forks[c]->AddChild(forks[nforks-1])) break; //fork is child to another fork
						}

						if(c==nforks) l=-1;
					}

					if(l==-1) //orphan fork
					{
						forks = (Forks**)realloc(forks, sizeof(Forks*)*(nforks+1));
						forks[nforks] = new Forks;
						forks[nforks]->AddBlock(n);
						nforks++;					
					}
				}
			}

			//find longest chain
			SortForks();
		}

		int ParseRawChainFile(int mfi, ulong64 offset)
		{
			int state=0;
			unsigned int mw;
			unsigned int oNB=NB;
			BlockHeader *tmp;

			/***The raw blocks as saved in the chain files come with some minor forks. It is safe to assume while these single block forks occured and got written in the file, considering the binary nature of the file
			the reallignement cost of single block removal was too heavy compared to chain length assesment code, which existed in the bitcoin source to begin with. For the sake of consistency, a full chain length 
			assessment code will be implement, instead of just pruning short forks off of the block buffer.
			***/

			if(MF.OpenFile(mfi)==-1) 
				return -1;
			if(_lseeki64(MF.handle, offset, SEEK_SET)==-1) 
				return -2;


			//read block headers and hash them
			do
			{
				n=1;
				while(n)
				{
					if(_eof(MF.handle))
					{
						state=1;
						break;
					}
					
					_read(MF.handle, &mw, 4);
					if(mw==MW)
					{
						tmp = NewRawBH();
 						tmp->mfi = MF.findex;
						tmp->offset = _telli64(MF.handle) -4;
						if(_read(MF.handle, &tmp->size, 4))
						{
							_read(MF.handle, tmp, 80);
							_lseeki64(MF.handle, tmp->size -80, SEEK_CUR);
					
							//hash the block
							sha.Hash(tmp, 0, 80);
							sha.toBuff(sha.HashOutput);
							sha.Hash(sha.HashOutput, 0, 32);
							sha.toBuff(tmp->Hash);
							tmp->loaded = 0;
						}
						else break;
					}
					else break;
				}

				mfi++;
			}
			while(MF.OpenFile(mfi)!=-1);

			ReallocBlocks(NB);
			for(n=oNB; n<NB; n++)
				blocksv2[n]->BH = rawBH[n];

			ParseRawBlocks(oNB);

			//MainFork = GetMainFork();
			//nBlocks = &MainFork->nblocks;
			//LAST_BLOCK = *nBlocks;

			return NB-oNB;
		}

		Forks* GetMainFork()
		{
			unsigned int u=0, n=0;

			for(u; u<nforks; u++)
			{
				if(forks[u]->length>forks[n]->length) n=u;
			}

			if(n)
			{
				Forks *ftmp = forks[n];
				forks[n] = forks[0];
				forks[0] = ftmp;
			}

			return forks[0];
		}

		void SortForks()
		{
			unsigned int i, m=1;
			int l, s, g;
			
			for(i=0; i<nforks; i++)
				forks[i]->GetLength(rawBH);

			Forks ftmp, *pftmp, *pftmp2;

			int count=0;
			while(m)
			{
				count++;
				m=0;
				for(l=nforks-1; l>-1; l--)
				{
					if(forks[l]->nchildren)
					{
						for(s=forks[l]->nchildren-1; s>-1; s--)
						{
							pftmp = (Forks*)forks[l]->children[s*2];
							if(!pftmp->deadfork)
							{
								g=0;
								if(pftmp->nchildren) 
								{
									for(i=0; i<pftmp->nchildren; i++)
									{
										pftmp2 = (Forks*)pftmp->children[i*2];
										if(!pftmp2->deadfork) g++;
									}
								}
									
								if(!g)//childless fork or dead children, check to see if it's long enough to merge with mother
								{
									//find which of the mother or child is longer, merge longest fork, save the other one as a dead fork
									if(forks[l]->lengthbeyondchild[s]>=pftmp->length)
									{
										//main is longer than child, mark child as dead fork
										pftmp->deadfork=1;
									}
									else if(forks[l]->lengthbeyondchild[s]<pftmp->length)
									{
										//main is shorter than child, swap child with main
										//from forking point; copy main's children over to child, then copy child  and its children over to main

										//save child
										forks[l]->copyat = forks[l]->children[s*2+1];
										ftmp = *forks[l];
										*forks[l] += *pftmp;
										*pftmp = ftmp;
										pftmp->deadfork = 1;
									}							
								}
								else //child has live children, move to that fork and merge it first
								{
									m=1;
									break;
								}
							}
						}
					}
				}
			}
		}
};
int RawChain::MW=0;


struct QueueData
{
	std::atomic_uint Qstart;
	unsigned int Qend;
	unsigned int nthreads;
	pthread_t *tids;

	void *(*callatend)(void *);
};

void *MT_cleanup(void *in)
{
	TimeData td;
	void **res=0;
	QueueData *qd = (QueueData*)in;

	for(unsigned int i=0; i<qd->nthreads; i++)
		pthread_join(qd->tids[i], res);

	free(qd->tids); //drop pthread_t pointer
	if(qd->callatend) 
	{
		void (*endcall)();
		endcall = (void(*)())qd->callatend;
		endcall();
	}

	free(qd);

	return 0;
}

class MTQueue
{
	public:
		QueueData *StartQueue(unsigned int st, unsigned int ed, void* (*start)(void*), void **varg, void* (*endcall)(void*))
		{
			QueueData *qd = (QueueData*)malloc(sizeof(QueueData));
			qd->Qstart = st;
			qd->Qend = ed;

			qd->nthreads = (st-ed)/10;
			if(!qd->nthreads) qd->nthreads++;
			else if(qd->nthreads>200) qd->nthreads = 200;

			qd->nthreads = 1;

			qd->tids = (pthread_t*)malloc(sizeof(pthread_t)*qd->nthreads);
			qd->callatend = endcall;

			varg[0] = (void*)qd;
			
			for(unsigned int i=0; i<qd->nthreads; i++)
				pthread_create(&qd->tids[i], 0, start, varg);

			pthread_t cleanupid;
			pthread_create(&cleanupid, 0, MT_cleanup, (void*)qd);

			return qd;
		}

		unsigned int PopQueue(QueueData *qin)
		{
			unsigned int rt;
			rt = qin->Qstart.fetch_add(1);

			if(rt<=qin->Qend) return rt;
			return -1;
		}
};

void* ChainBenchmark(void *in)
{
	DWORD ec = GetTickCount();
	unsigned int freemem = hBuffer.GetUnusedMem();
	unsigned int totalmem = hBuffer.GetTotalMem();
	hBuffer.FillRate();
	return 0;
}


void* FetchBlockMT(void *vin)
{
	void **vvin = (void**)vin;
	
	unsigned int *nBlocks = (unsigned int*)vvin[2];
	QueueData *qd = (QueueData*)vvin[0];
	BLOCKSv2 **blocksv2 = (BLOCKSv2**)vvin[1];
	RawChain *rc = (RawChain*)vvin[4];
	Forks** Frk = (Forks**)vvin[5]; 

	MultiFile mf;
	if(mf.OpenFile(-1)==-1) return 0;

	unsigned int blockn, blockm, mw, l, m;
	MTQueue mtq;
	MemPool *mp, *mpU;
	byte *txnb=0, *ubuffer=0, usbyte; char n;
	Sha256Round sha; sha.LargeBuffers(100);
	BufferHeader *BH, *pBH;

	while((blockm = mtq.PopQueue(qd))!=-1)
	{
		blockn = (*Frk)->blocks[blockm];
		if(blockm<*nBlocks)
		{
			//blocksv2[blockn]->BH = rc->rawBH[blockm];
			if(blocksv2[blockn]->BH->mfi!=mf.findex) mf.OpenFile(blocksv2[blockn]->BH->mfi);
			//if(blocksv2[blockn]->BH->offset)
			{
				n = blocksv2[blockn]->BH->loaded.exchange(-1);
				switch(n)
				{
					case 0: //virgin block
					{
						_lseeki64(mf.handle, blocksv2[blockn]->BH->offset, SEEK_SET);
						//load header

						_read(mf.handle, &mw, 4);
						if(mw==MWG)
						{
							_read(mf.handle, &l, 4);
							if(l==blocksv2[blockn]->BH->size)
							{
								
								l = (unsigned int)blocksv2[blockn]->BH->size -80;
								blocksv2[blockn]->length = l;
								
								unsigned int s = (l/150 +10)*8;
								mpU = hBuffer.GetPoolU(s);
								mpU->reserved-=s;
								mpU->freemem+=s;
								pBH = mpU->PushBH(0, 0, 0);
						
								_lseeki64(mf.handle, 80, SEEK_CUR);

								//TXN								
								m = l/150 +1;
								m*=149;

								mp = hBuffer.GetPool(l +m, txnb);
								mp->reserved-=m; //decrement reserved so that PushBH can assign memory from the extra estimate
								mp->freemem+=m;

								/***a pool is requested of the size of the block data (provided by the block itself) and an estimated 100% more to hold the other buffers.
								First there's (**TXNv2 and (unsigned int*))*ntxn: DWORD pointers held by the block. This memory won't be dumped or recycled. ntxn is anywhere between 1 and 10k
								right now. Average Txn is 1.5 vins, 1.8 vouts, rounded to 2 for both.
								
								Average vin is:
									32 bytes of tx hash
									2~5 byte for nvins and scriptlength
									70 bytes of script (20ish bytes for coinbase)
									4 bytes of sequence
									4 bytes refoutput
								for a total of 110~120 bytes, or 115 average

								Average vout is:
									8 bytes for the value
									2~5 bytes for nvouts and scriptlength
									25 bytes of script
								for a total of 30~40 bytes, or 35 average
								
								putting entire txn at 150 bytes on the smallest (1 vin, 1 vout) and a 300 bytes average

								The block only data is 8 bytes per txn
								33 bytes for tx hash + 1 byte for flag
								16 bytes per vin for the header class
								16 bytes per vout for header class
								44 bytes for TXN header class

								so an average of 33 + 16*2 +16*2 +44 +8 per txn, putting it at 149 bytes per txn, or an estimated average of 50%.

								as such, each 150 bytes, 149 bytes of extra space should be prepared

								Protocol to deal with bursting out of the estimated space allocation has to be devised still.

								This model, of dynamic and estimated space allocation for header classes isn't memory tight, however this is irrelevant as the mempool class will come with a
								recycling maintenance thread that will tighten remaining data once the exact sizes are known. Keep in mind that the vast majority of data read from the blockchain can
								and will be dumped, when it has been parsed to find relevant txn branches. Ideally only the last txn in live branches will be kept, so a single txn per branch. Assuming
								we're running on 10 branches per address, 1 address (maybe up to 5 in the future) per account and 100,000 accounts, we're looking at 1 to 5 mil txn to hold in the ram at 
								high capacity. At an average 300 bytes per txn, that's 1.5 gig of data + overhead at most
								***/

								_read(mf.handle, txnb, l);
								totalchain.fetch_add(l);
								/*remark: the compact size ntxn value preceeding the transactions is read in the same pool. 
								The value can be overwritten however as it used only once and never accounted for in the mempool class*/

								//compact size ntxn
								blocksv2[blockn]->ntx = 0;
								usbyte = (byte)txnb[0];
								if(usbyte<0xFD) blocksv2[blockn]->ntx = usbyte, n=1;
								else if(usbyte<0xFE) memcpy(&blocksv2[blockn]->ntx, txnb +1, 2), n=3;
								else if(usbyte<0xFF) memcpy(&blocksv2[blockn]->ntx, txnb +1, 4), n=5;
								txnb += n;

								blocksv2[blockn]->txn = (TXNv2**)(pBH->offset +pBH->size);
								mpU->PushBH(pBH, sizeof(TXNv2*)*(unsigned int)blocksv2[blockn]->ntx, 0);
								memset(blocksv2[blockn]->txn, 0, sizeof(TXNv2*)*(size_t)blocksv2[blockn]->ntx);
								
								blocksv2[blockn]->rawsize = (unsigned int*)(pBH->offset +pBH->size);
								mpU->PushBH(pBH, sizeof(int)*(unsigned int)blocksv2[blockn]->ntx, 0);
								memset(blocksv2[blockn]->rawsize, 0, sizeof(int)*(unsigned int)blocksv2[blockn]->ntx);
								blocksv2[blockn]->bh = pBH;
								pBH->pinuse = (unsigned int*)&blocksv2[blockn]->sema;
								blocksv2[blockn]->sema = 1;


								at_block.fetch_add(1);
								mpU->ReleasePool();

								n_txn.fetch_add((unsigned int)blocksv2[blockn]->ntx);
								for(l=0; l<blocksv2[blockn]->ntx; l++)
								{
									blocksv2[blockn]->txn[l] = TxnHandler.New(txnb, &sha, &blocksv2[blockn]->rawsize[l], mp);
									BH = mp->PopBH(txnb, blocksv2[blockn]->rawsize[l], 0);
									BH->pinuse = (unsigned int*)&blocksv2[blockn]->txn[l]->sema; //linking data bh to semaphore
									blocksv2[blockn]->txn[l]->holder = (void*)&blocksv2[blockn]->txn[l];

									txnb+=blocksv2[blockn]->rawsize[l];
									blocksv2[blockn]->txn[l]->bh = BH; //not sure this matters anymore with the "pointer to semaphore from bh" approach
								}

								mp->ReleasePool();
								blocksv2[blockn]->BH->loaded.store(1);
							}
						}
					}
					break;

					case 1: //block was already loaded
						blocksv2[blockn]->BH->loaded.store(1);
					break;
					
					case 2: //block has been loaded but some or all of the txn have been dumped
					{
						/*** redo this one ***/

						//set read offset at transactions
						/*if(blocksv2[blockn]->ntx<0xFD) mw=1;
						else if(blocksv2[blockn]->ntx<=0xFFFF) mw=3;
						else if(blocksv2[blockn]->ntx<=0xFFFFFFFF) mw=5;
						else mw=9;

						_lseeki64(s, blocksv2[blockn]->offset +80 +mw, SEEK_SET);

						for(l=0; l<blocksv2[blockn]->ntx; l++)
						{
							if(!blocksv2[blockn]->txn[l])//only fetch null txn
							{
								BH = hBuffer.GetBuffer(blocksv2[blockn]->rawsize[l]);
								_read(s, BH->offset, blocksv2[blockn]->rawsize[l]);

								//blocksv2[blockn]->txn[l] = TxnHandler.New(BH->offset, &sha, blocksv2[blockn]->rawsize[l]);
								blocksv2[blockn]->txn[l]->bh = BH;
							}
							else //seek to next block
								_lseek(s, blocksv2[blockn]->rawsize[l], SEEK_CUR);
						}

						blocksv2[blockn]->loaded.store(1);*/
					}
				}
			}
		}
	}
	
	mf.Close();
	return 0;
}

class BlockChain
{
	private:
		unsigned int mw, ef;
		byte *txnb, *txn2;
		unsigned char *databuffer;
		ulong64 l, cnt;
		ulong64 i, y;
		unsigned int BR, QS, m, mm, n, r, t, z, s, k, hashtype;
		int CBa, CBb, *b3, *b4, *b5;
		int *vinID; unsigned int nvinID;
		unsigned int rawI, GI, lGI;
		byte usbyte;
		long benchmark;
		byte *txnV; unsigned int txnVB;
		TXNv2 *tmpv2;
		void **varg;

		byte *pkeyx, *pkeyy;
		byte fileon;

		MTQueue mtq;
		QueueData *qd;

		txnBranch **ptB;
		unsigned int *nptB;
	
	public:
		unsigned int *nBlocks;
		static const int nBlocksBuffer = 200;
		unsigned int txnbS;
				
		std::atomic_uint32_t *block_sema, *txn_sema;

		Sha256Round sha;
		secp256k1mpz p256k1;

		RawChain RC;

		Forks *MainFork;

		BlockChain::BlockChain()
		{
			nBlocks=0;
			databuffer = 0;
			sha.LargeBuffers(100);
			vinID = (int*)malloc(sizeof(int)*10000); /***possible buffer overrun, figure out max vin per txn.***/

			txnbS=0;
			txnV=0; txnVB=0;
			fileon=0;
			MainFork=0;

			varg = (void**)malloc(sizeof(void*)*6);
			varg[1] = 0;
			varg[2] = (void*)&nBlocks;
			varg[4] = (void*)&RC;
			varg[5] = (void*)&MainFork;

			MainFork=0;
		}

		BlockChain::~BlockChain()
		{
			free(databuffer);
			free(vinID);
			free(varg);

			/***new shit to free in here ***/
		}

		void Init()
		{
			GetBlocks(0, 0);
		}

		int GetBlocks(int mfi, ulong64 offset)
		{
			int nbr = RC.ParseRawChainFile(mfi, offset);
			MainFork = RC.GetMainFork();
			nBlocks = &MainFork->nblocks;
			LAST_BLOCK = MainFork->nblocks;

			return nbr;
		}

		QueueData *FetchBlocks(unsigned int st, unsigned int ed)
		{
			/***Make sure blocksv2 is large enough before starting the queue***/
			if(ed>*nBlocks) return 0;

			varg[1] = (void*)RC.blocksv2;
			
			/*** caller should free the qd struct***/
			return mtq.StartQueue(st, ed, FetchBlockMT, varg, ChainBenchmark);
		}

		void ScanAddresses(BTCaddress **addies, unsigned int nadds)
		{
			if(nadds)
			{
				std::atomic_uint32_t *au = (std::atomic_uint32_t*)addies -1; /*** check for possible deadlocks ***/
				au->fetch_add(1);

				unsigned int lowestblock=(long)addies[0]->refblock;
				unsigned int chid;

				for(m=1; m<nadds; m++)
				{
					if(addies[m]->refblock<lowestblock)
						lowestblock = (long)addies[m]->refblock;
				}

				FetchBlocks(lowestblock, MainFork->nblocks-1);

				for(BR=lowestblock; BR<*nBlocks; BR++)
				{
					chid = MainFork->blocks[BR];
					while(RC.blocksv2[chid]->BH->loaded!=1) {}
					ScanAddressesPerBlock(addies, nadds, chid);
				}
				
				for(m=0; m<nadds; m++)
				{
					addies[m]->ComputeBalance();
					addies[m]->refblock = *nBlocks-1;
				}

				au->fetch_add(-1);
			}
		}

		void ScanAddressesPerBlock(BTCaddress **addies, unsigned int nadds, unsigned int blockn)
		{
			block_sema = &RC.blocksv2[blockn]->sema;
			block_sema->fetch_add(1);


			for(t=0; t<RC.blocksv2[blockn]->ntx; t++)
			{
				tmpv2 = RC.blocksv2[blockn]->txn[t];
				txn_sema = &tmpv2->sema;
				txn_sema->fetch_add(1);

				for(r=0; r<nadds; r++)
				{
					if(blockn>=addies[r]->refblock)
					{
						ptB = addies[r]->Branches;
						nptB = &addies[r]->nBranches;

						//Vin: look for txn hash reference
						nvinID=0;
						for(z=0; z<tmpv2->nVin; z++)
						{
							if(!tmpv2->coinbase)
							{
								for(k=0; k<*nptB; k++)
								{
									if(*tmpv2->vin[z].refOutput==ptB[k]->outputref) //make sure the output reference matches
									if(!CompareBuffers(tmpv2->vin[z].txnHash, ptB[k]->CurrentHash, 32))
									{
										//vinID[nvinID++] = k;
										ptB[k]->outputref = -1; //mark branch for deletion
										ptB[k]->txnref[0]->QS--;

										if(ptB[k]->txnref[0]->QS<=0) ptB[k]->txnref[0]->sema.fetch_add(-1); //is QS is 0 or less, dump txn
									}
								}
							}
						}

						//Vout 
						/*** With the mechanics of bitcoin transaction, one would expect that each address receiving funds has a single output pointing to it in the Vout section of the txn.
						When you redeem a 50 BTC output from txn A, you build txn B. In txn B's Vin section, you point to the outputs from txn A which scripts you can satisfy and provide the 
						necessary data that goes with the verification (in stock transactions, that means holding the private keys that matches the public key pointed by in txn A's outputs, and
						signing the Vins you built in txn B with the private keys).

						That done, the coins are now available to spend in the Vout section. With each Vout you write in an amount of coins to attach to it and the script details that define
						how to redeem those coins. In stock txn, this is a public address with script requirement of providing a signature on it proving the redeemer holds the private key.

						In that sense, while scanning a transaction for redeemable coins, once you find the a Vout entry that points to your address, you stop looking any further, as you're not
						expecting more branches (each Vout entry alawys corresponds to a branch). However, there is no indication in the protocol that within a same tnx one can't spend coins to 
						the same address in the form of several Vout entries all pointing to that one address.

						While this practice is uncommon and counter intuitive, it does produce several branches and need to be scanned for, unless some branches are to be missed.
						***/
						l=0;

						for(z=0; z<tmpv2->nVout; z++)
						{
							s=0;
								
							if(tmpv2->vout[z].type==0)
							{
								//check add vs hash160
								if(!addies[r]->Compare(tmpv2->vout[z].script +3, 20)) s++;
							}
							else if(tmpv2->vout[z].type==1)
							{
								//check add vs pubkey
								if(!addies[r]->Compare(tmpv2->vout[z].script +1, 65)) s++;
							}
							else
							{
								/*** other scripts go here ***/
							}

							if(s) //address matches txn out, new branch
							{
								txnBranch *tbtmp=0;

								//look for redeemable branches
								for(k=0; k<*nptB; k++)
								{
									if(ptB[k]->outputref==-1)
									{
										tbtmp = ptB[k];
										break;
									}
								}
								
								if(k==*nptB)
								{
									/*** SEMAPHORE HERE to avoid timing issues with the spend code ***/
									tbtmp = new txnBranch;
									ptB = (txnBranch**)realloc(ptB, sizeof(txnBranch*)*(*nptB +1));
									ptB[nptB[0]++] = tbtmp;
									addies[r]->Branches = ptB;
								}

								tbtmp->value = *tmpv2->vout[z].value;
								tbtmp->CurrentHash = tmpv2->txHash;
								tbtmp->blockref = BR;
								tbtmp->outputref = z;
								tbtmp->txnref = RC.blocksv2[blockn]->txn +t;
								/***link txn inside branch to &tmpv2 for defrag purpose ***/
							
								tmpv2->QS++; //mark txn as useful, don't dump it

							}
						}

						/*if(nvinID)
						{
							//update exisiting branches
							s = vinID[0];
							ptB[0]->value = l;
							ptB[0]->CurrentHash = tmpv2->txHash;
							ptB[0]->blockref = BR;
							ptB[0]->outputref = z;
							ptB[0]->txnref = RC.blocksv2[blockn]->txn +t;
							for(z=1; z<nvinID; z++)
								ptB[0]->value = 0;

							//clean up any branch at 0
							for(z=0; z<*nptB; z++)
							{
								if(!ptB[z]->value)
								{
									delete[] ptB[z];
									if(*nptB) ptB[z] = ptB[--nptB[0]];
								}
							}
							
							QS=1;
						}
						else if(l)
						{
							//add new branch
							txnBranch *tbtmp = new txnBranch;
							tbtmp->value = l;
							tbtmp->CurrentHash = tmpv2->txHash;
							tbtmp->blockref = BR;
							tbtmp->outputref = z;
							tbtmp->txnref = RC.blocksv2[blockn]->txn +t;
							
							ptB = (txnBranch**)realloc(ptB, sizeof(txnBranch*)*(*nptB +1));
							ptB[nptB[0]++] = tbtmp;
							addies[r]->Branches = ptB;

							QS=1;
						}*/
					}
				}

				txn_sema->fetch_add(-1);
				if(tmpv2->QS<=0) txn_sema->fetch_add(-1); //flag txn for deleting as it holds no usefull data
				/***make sure to mark the block as partially loaded since the useless data is to be dumped***/
			}

			block_sema->fetch_add(-1);
		}

		void FetchAll()											
		{
			at_block.store(0);
			totalchain.store(0);
			n_txn.store(0);
			n_vouts.store(0);
			n_vins.store(0);
			FetchBlocks(0, *nBlocks-1);
		}

		int GetTxnByHash(void *txnhash, unsigned int blockn)
		{
			if(blockn)
			{
				for(BR=blockn-1; BR>0; BR--)
				{
					 /*if(!blocksv2[BR]->loaded)
						 FetchBlockv2(BR);*/
						
					for(t=0; t<RC.blocksv2[BR]->ntx; t++)
					{
						if(!CompareBuffers(txnhash, RC.blocksv2[BR]->txn[t]->txHash, 32))
						{
							//return block #, let caller grab txn# from t
							return BR;
						}
					}
				}
			}

			return -1;
		}

		int VerifyTX(int blockn, int txid)
		{
			TXNv2 *tmp; int rt = 0;
			if(blockn && txid>-1)
			{				
				/*if(!blocksv2[blockn]->loaded)
					FetchBlockv2(blockn);*/
				
				tmp = RC.blocksv2[blockn]->txn[txid];

				//build raw length
				//version +varint nVin + nVin*(txhash+ output index + varint scriptlength +script + sequence) + varint nVout + nvout*(value + varint scriptlength + script) +locktime
				m = 16 + tmp->nVin*(44) + tmp->nVout*(12);
				for(s=0; s<tmp->nVin; s++)
					m+=tmp->vin[s].scriptLength;
				
				for(s=0; s<tmp->nVout; s++)
					m+=tmp->vout[s].scriptLength;
				
				//update serialized tx buffer
				if(m>txnVB)
				{
					free(txnV);
					txnV = (byte*)malloc(m);
					txnVB = m;
				}

				//build tx start
				txnb = txnV;

				//verion
				memset(txnb, 0, 4);
				txnb[0] = 1; txnb+=4;

				//nVin
				m = varintCompress(tmp->nVin, txnb);
				txnb+=m;

				for(s=0; s<tmp->nVin; s++)
				{
					//txhash
					memcpy(txnb, tmp->vin[s].txnHash, 32); txnb+=32;
					memcpy(txnb, tmp->vin[s].refOutput, 4); txnb+=4;

					//mark as start of vin[s] script
					vinID[s] = unsigned int(txnb - txnV);
					
					//scriptlength=0 + empty script
					txnb[0] = 0; txnb++;

					//sequence
					memset(txnb, 0xFF, 4);
					txnb+=4;
				}

				//mark as start of Vouts
				nvinID = unsigned int(txnb - txnV);

				//nVout
				m = varintCompress(tmp->nVout, txnb);
				txnb+=m;

				for(s=0; s<tmp->nVout; s++)
				{
					//value
					memcpy(txnb, tmp->vout[s].value, 8);
					txnb+=8;

					//scriptlength
					m = varintCompress(tmp->vout[s].scriptLength, txnb);
					txnb+=m;

					//script
					memcpy(txnb, tmp->vout[s].script, tmp->vout[s].scriptLength);
					txnb+=tmp->vout[s].scriptLength;
				}

				//locktime
				memcpy(txnb, tmp->locktime, 4);
				txnb+=4;

				//clear next 4 bytes for hash type
				memset(txnb, 0, 4);
				txnb+=4;

				//compute cleaned script size
				r = unsigned int(txnb - txnV);
				
				//check buffer size
				if(txnbS<r+100)
				{
					free(databuffer);
					databuffer = (unsigned char*)malloc(r+100);
					txnbS = r+100;
				}

				//build tx hash
				for(s=0; s<tmp->nVin; s++)
				{
					//get hash type
					hashtype=0;
					k = tmp->vin[s].script[0];
					memcpy(&hashtype, tmp->vin[s].script +k, 1);

					//get public key
					pkeyx=pkeyy=0;

					//check vin first
					if(tmp->vin[s].script[k+1]==0x41)
					{
						pkeyx = tmp->vin[s].script +k+3;
						pkeyy = tmp->vin[s].script +k+35;
					}

					//fetch referenced tx
					z = GetTxnByHash(tmp->vin[s].txnHash, blockn);

					if(!pkeyx)
					{
						if(RC.blocksv2[z]->txn[t]->vout[*tmp->vin[s].refOutput].script[0]==0x41 &&
						   RC.blocksv2[z]->txn[t]->vout[*tmp->vin[s].refOutput].script[1]==0x04)
						{
							pkeyx = RC.blocksv2[z]->txn[t]->vout[*tmp->vin[s].refOutput].script +2;
							pkeyy = RC.blocksv2[z]->txn[t]->vout[*tmp->vin[s].refOutput].script +34;
						}
					}

					if(pkeyx)
					{
						//scan ref output script for op_cs (0xab)
						BR = RC.blocksv2[z]->txn[t]->vout[*tmp->vin[s].refOutput].scriptLength;
						txnb = RC.blocksv2[z]->txn[t]->vout[*tmp->vin[s].refOutput].script;
						n=0; m=-1;

						while(n<BR)
						{
							if(txnb[n]<0x4C) n+=txnb[n]+1;
							else 
							{
								if(txnb[n]==0xAB) m=n;
								n++;
							}
						}

						k = BR -m -1;
						n=m+1;

						if(txnbS<r+k+8)
						{
							free(databuffer);
							databuffer = (unsigned char*)malloc(r+k+8);
							txnbS = r+k+8;
						}

						//build serialized tx
						txn2 = databuffer;
						memcpy(txn2, txnV, vinID[s]);
						txn2 += vinID[s];

						t = varintCompress(k, txn2);
						txn2+=t;

						memcpy(txn2, txnb +n, k); txn2+=k;

						memcpy(txn2, txnV +vinID[s] +1, r -vinID[s] -1);
						txn2+= r -vinID[s] -1;
						
						//add hash type
						memcpy(txn2 -4, &hashtype, 4);
						t = unsigned int(txn2 - databuffer);

						//hash it
						if(hashtype==1)
						{
							sha.Hash(databuffer, -1, t);
							sha.toBuff(sha.HashOutput);
							sha.Hash(sha.HashOutput, -1, 32);
							//sha.toBuff(sha.HashOutput);
						}

						/*char *rm = OutputRaw(sha.HashOutput, 32);
						char *tt = OutputRaw(databuffer, t);
						FILE *fc = fopen("serialized.txt", "wb");
						fwrite(tt, strlen(tt), 1, fc);
						fclose(fc);*/

						//verify it
						rt |= p256k1.VerifySig(sha.HashOutput, tmp->vin[s].script+1, pkeyx, pkeyy);
					}
				}

				return rt;
			}
		
			return -1;
		}
};

BlockChain BC;

		
void *CheckChain(void *in)
{
	BTCHandle btcH; 
	
	unsigned int i, s;
	ulong64 offset;
	int mfi;

	while(run)
	{
		i = btcH.GetChainLength();
		if(i>=*BC.nBlocks)
		{
			//new block is out, update address branches
			offset = BC.RC.rawBH[BC.RC.NB-1]->offset +BC.RC.rawBH[BC.RC.NB-1]->size +8; //4 bytes for mw, 4 bytes for size, then size worth of bytes per block
			mfi = BC.RC.rawBH[BC.RC.NB-1]->mfi;

			s = BC.GetBlocks(mfi, offset);

			if(s==1) BC.ScanAddressesPerBlock(AH.address, AH.naddress, *BC.nBlocks -1);
			else if(s>1) BC.ScanAddresses(AH.address, AH.naddress);
			else 
			{
				int debughere = 0;
			}
		}
		else
		{
		}

		Sleep(10000);
	}

	return 0;
}
