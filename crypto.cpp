#include <math.h>
#include <time.h>
#include <gmp-dynamic/gmp.h>

#define Endian(x) ((x & 0xFF000000) >> 24 | (x & 0x00FF0000) >> 8 | (x & 0x0000FF00) << 8 | (x & 0x000000FF) << 24)

void memcpy_atomic(std::atomic_int32_t *in, void *cpy, unsigned int size)
{
	//cpy must be 32bit aligned

	int *icpy = (int*)cpy, *istart = (int*)cpy +size;
	while(icpy<istart)
	{
		std::atomic_store_explicit(in, *icpy, std::memory_order_relaxed);
		in++;
		icpy++;
	}
}

void memset_atomic(std::atomic_int32_t *in, int cpy, unsigned int size)
{
	unsigned int i=0;
	while(i<size)
	{
		std::atomic_store_explicit(&in[i], cpy, std::memory_order_relaxed);
		i++;
	}
}

void clear_char_volatile(char *cin)
{
	//takes null terminated chars
	int i=0, length = strlen(cin);
	volatile char* vin = (volatile char*)cin;
	while(i<length)
	{
		vin[i] = 0;
		i++;
	}
}

byte *GetByteStream(char *in)
{
	int s = strlen(in), i=0;
	byte *bout = (byte*)malloc(s/2);
	byte r; char c;

	for(i; i<s; i+=2)
	{
		c = in[i+1];
		if(c>='a') r = c -87;
		else if(c>='A') r = c -55;
		else if(c>='0') r = c -48;

		c = in[i];
		if(c>='a') r += (c -87)*16;
		else if(c>='A') r += (c -55)*16;
		else if(c>='0') r += (c -48)*16;

		bout[i/2] = r;
	}

	return bout;
}

void GetByteStream(char *in, byte *bout, int s)
{
	int i=0;
	byte r; char c;

	for(i; i<s; i+=2)
	{
		c = in[i+1];
		if(c>='a') r = c -87;
		else if(c>='A') r = c -55;
		else if(c>='0') r = c -48;

		c = in[i];
		if(c>='a') r += (c -87)*16;
		else if(c>='A') r += (c -55)*16;
		else if(c>='0') r += (c -48)*16;

		bout[i/2] = r;
	}
}

byte *GetByteStreamBE(char *in)
{
	int s = strlen(in), i=0;
	byte *bout = (byte*)malloc(s/2);
	byte r; char c;

	for(i; i<s; i+=2)
	{
		c = in[i+1];
		if(c>='a') r = c -87;
		else if(c>='A') r = c -55;
		else if(c>='0') r = c -48;

		c = in[i];
		if(c>='a') r += (c -87)*16;
		else if(c>='A') r += (c -55)*16;
		else if(c>='0') r += (c -48)*16;

		bout[i/2] = r;
	}

	byte *bout2 = (byte*)malloc(s/2);
	unsigned int *j, f;
	for(i=0; i<s/8; i++)
	{
		j = (unsigned int*)(bout +i*4);
		f = Endian(*j);
		memcpy(bout2 +i*4, &f, 4);
	}

	free(bout);
	return bout2;
}

void set_mpz(mpz_t &out, void *in, int n)
{
	byte *cin = (byte*)in;
	char *tmp = (char*)malloc(n*2+1);

	int i; byte c;

	for(i=0; i<n; i++)
	{
		c = cin[i] & 0x0F;
		if(c<10) c+=48;
		else c+=55;
		tmp[(n-1-i)*2 +1] = (char)c;

		c = (cin[i] & 0xF0) >> 4;
		if(c<10) c+=48;
		else c+=55;
		tmp[(n-1-i)*2] = (char)c;
	}
	tmp[n*2]=0;

	mpz_set_str(out, tmp, 16);
	free(tmp);
}

void DERdecode(mpz_t &out, void *in)
{
	byte *cin = (byte*)in;

	byte c = cin[0];
	cin++;
	if(!(*cin)) cin++, c--;
	
	unsigned int *f = (unsigned int*)malloc(c);
	unsigned int *g = f, *r;

	int i=0;
	while(i<c)
	{
		r = (unsigned int*)(cin +i);
		f[0] = Endian(*r);
		i+=4;
		f++;
	}

	set_mpz(out, g, c);
	free(g);
}

void DERdecode2(mpz_t &out, void *in)
{
	byte *cin = (byte*)in;

	byte c = cin[0];
	cin++;
	if(!(*cin)) cin++, c--;
	
	unsigned int *f = (unsigned int*)malloc(c);
	unsigned int *g = f, *r;

	int i=0;
	while(i<c)
	{
		r = (unsigned int*)(cin +i);
		f[(c-i-4)/4] = *r;//Endian(*r);
		i+=4;
	}

	set_mpz(out, g, c);
	free(g);
}

void DERdecode3(mpz_t &out, void *in)
{
	byte *cin = (byte*)in;

	byte c = cin[0];
	cin++;
	if(!(*cin)) cin++, c--;
	
	unsigned int *f = (unsigned int*)malloc(c);
	unsigned int *g = f, *r;

	int i=0;
	while(i<c)
	{
		r = (unsigned int*)(cin +i);
		f[(c-i-4)/4] = Endian(*r);
		i+=4;
	}

	set_mpz(out, g, c);
	free(g);
}

void set_mpzBE(mpz_t &out, void *in, int n)
{
	byte *cin = (byte*)in;
	unsigned int *f = (unsigned int*)malloc(n);
	unsigned int *g = f, *r;

	int i=0;
	while(i<n)
	{
		r = (unsigned int*)(cin +i);
		f[(n-i-4)/4] = Endian(*r);
		//f++;
		i+=4;
	}

	set_mpz(out, g, n);
	free(g);
}

char *OutputRaw(void *in, int size)
{
	char *abc = (char*)malloc(size*3);
	byte *tin = (byte*)in;
	int i, r=0, t=0;
	byte f, s;

	for(i=0; i<size; i++)
	{
		f = tin[i];

		s = f>>4;
		if(s>9) s+=55;
		else s+=48;

		abc[r] = (char)s;

		s = f & 0x0F;
		if(s>9) s+=55;
		else s+=48;

		abc[r+1] = (char)s;

		r+=2; t+=2;

		if(t==8)
		{
			abc[r] = ' ';
			r++;
			t=0;
		}
	}
	
	abc[r] = 0;
	return abc;
}

char *OutputRaw(void *in, int size, char *abc)
{
	byte *tin = (byte*)in;
	int i, r=0, t=0;
	byte f, s;

	for(i=0; i<size; i++)
	{
		f = tin[i];

		s = f>>4;
		if(s>9) s+=55;
		else s+=48;

		abc[r] = (char)s;

		s = f & 0x0F;
		if(s>9) s+=55;
		else s+=48;

		abc[r+1] = (char)s;

		r+=2; t+=2;
	}
	
	abc[r] = 0;
	return abc;
}

unsigned int ROTr(unsigned int a, unsigned int b)
{
	unsigned int l = a>>b;
	unsigned int k = a<<(32-b);

	return l | k;
}

class TimeData
{
	public:
		DWORD TimeMS;
		DWORD TimeS;
		byte sec, min, hr, day, month;
		short yr;
		time_t timer;
		clock_t clk;

		TimeData::TimeData()
		{
			Reset();
		}

		void Reset()
		{
			TimeMS = TimeS = 0;
			sec=min=hr=day=month=0;
			yr=0;
		}

		DWORD GetTimeMS()
		{
			#if defined WIN32
				TimeMS = GetTickCount();
			#endif

			return TimeMS;
		}

		DWORD GetTimeS()
		{
			time(&timer);
			TimeS = (DWORD)timer;

			return TimeS;
		}

		DWORD GetClock()
		{
			clk = clock();
			return clk;
		}
};

class Sha256Block
{
	public:
		unsigned int *blockInt;
		unsigned int *blockIntEx;
		unsigned int a, b, c, d;

		Sha256Block::Sha256Block()
		{
			blockInt = (unsigned int*)malloc(sizeof(int)*16);
			blockIntEx = (unsigned int*)malloc(sizeof(int)*64);
		}

		Sha256Block::~Sha256Block()
		{
			free(blockInt);
			free(blockIntEx);
		}

		unsigned int sigma0(unsigned int x)
		{
			//rotr(x,7) XOR rotr(x,18) XOR shiftr(x,3)
			
			unsigned int y = ROTr(x, 7);
			unsigned int z = ROTr(x, 18);
			unsigned int w = x>>3;

			unsigned int l = y^z;
			y = l^w;

			return y;
		}

		unsigned int sigma1(unsigned int x)
		{
			//rotr(x,17) XOR rotr(x,19) XOR shiftr(x,10)

			unsigned int y = ROTr(x, 17);
			unsigned int z = ROTr(x, 19);
			unsigned int w = x>>10;

			unsigned int l = y^z;
			y = l^w;

			return y;
		}

		void PrepBlock()
		{
			memcpy(blockIntEx, blockInt, sizeof(int)*16);

			for(a=16; a<64; a++)
			{
				b = sigma1(blockIntEx[a-2]);
				c = sigma0(blockIntEx[a-15]);

				d = blockIntEx[a-7] + blockIntEx[a-16];
				b+=c;

				blockIntEx[a] = b+d;
			}
		}
};

class Sha256Msg
{
	public:
		unsigned int *Msg;
		unsigned char *bin;
		Sha256Block *blocks;
		int nb, a, b, c, d, i, r, max_blocks;
		unsigned int s, t, z;

		Sha256Msg::Sha256Msg()
		{
			max_blocks=3;
			SetBuffers(max_blocks);
		}

		Sha256Msg::~Sha256Msg()
		{
			if(Msg) free(Msg);
			delete[] blocks;
		}

		void LargeBuffers(int bs)
		{
			max_blocks = bs;
			free(Msg);
			delete[] blocks;
			SetBuffers(max_blocks);
		}

		void SetBuffers(int mb)
		{
			while(!(Msg = (unsigned int*)malloc(sizeof(int)*mb*16))) {}
			blocks = new Sha256Block[mb]; //max n blocks for web app purpose is 3, max for txn hashing is 40
			nb=0;
		}

		void SetMessage(void *in, int l)
		{
			bin = (unsigned char*)in;
			a = l*8;
			if(!l) a = strlen((char*)in)*8;
			c = a-448;

			if(c<0)
			{
				b = 448;
			}
			else
			{
				b = c/512 +1;
				b*=512;
				b+=448;
			}

			d = (b+64)/32;
			if(d>max_blocks*16) d=max_blocks*16, a = d*32 -64;
			memset(Msg, 0, sizeof(int)*d);

			r=32, z=0; a/=8;
			for(i=0; i<a; i++)
			{
				r-=8; if(r<0) r=24, z++;
				s = unsigned int(bin[i]) << r;
				Msg[z]|=s;
			}
			
			r-=8; if(r<0) r=24, z++;
			s = 0x80 << r;
			Msg[z]|=s;

			Msg[d-1] = unsigned int(a)*8;

			nb = (b +64)/512;
			if(nb>max_blocks) nb=max_blocks;
			for(a=0; a<nb; a++)
			{
				memcpy(blocks[a].blockInt, Msg +a*16, sizeof(int)*16);
				blocks[a].PrepBlock();
			}
		}
};

class Sha256Round
{
	/*** FIXES TO IMPLEMENT:
		1) the XOR SHIFT and ROT functions are shit, fix them
		2) Max block has been statically set at 3. Should return error for messages longer than 3 blocks 
		   instead of partial hash
	***/
	public:
		static unsigned int *RoundCst;
		static unsigned int *HashCst;
		static char **hexits;
		unsigned int *HashOP;
		unsigned char *cbuffer;
		
		int y;
		unsigned int a, b, c, d, e, f, g, h,
					 i, j, k, l, m, n, o, p,
					 q, r, s, t, u;

		Sha256Msg Msg;
		char *HashOutput;

		Sha256Round::Sha256Round()
		{
			HashOutput = (char*)malloc(65);
			HashOP = (unsigned int*)malloc(sizeof(int)*8);
		}

		Sha256Round::~Sha256Round()
		{
			free(HashOutput);
			free(HashOP);
		}

		void LargeBuffers(int bs)
		{
			Msg.LargeBuffers(bs);
		}

		void Compute(int blocki)
		{
			a = HashOP[0]; b = HashOP[1];
			c = HashOP[2]; d = HashOP[3];
			e = HashOP[4]; f = HashOP[5];
			g = HashOP[6]; h = HashOP[7];

			for(i=0; i<64; i++)
			{
				m = h + RoundCst[i]; k = Sigma1(e); l = Ch(e, f, g); 
				m+=Msg.blocks[blocki].blockIntEx[i];
				n = Sigma0(a); o = Maj(a, b, c); k+=l;
				h = g;
				g = f;
				k+=m; n+=o;
				f = e;
				e = d+k;
				d = c;
				c = b;
				b = a;
				a = k+n;
			}
		}

		void HexitToChar(unsigned int in, char *out, int lilendian)
		{
			s = in & 0xFF000000;
			s >>= 24;
			memcpy(out +6, hexits[s], 2);

			s = in & 0x00FF0000;
			s>>=16;
			memcpy(out +4, hexits[s], 2);

			s = in & 0x0000FF00;
			s>>=8;
			memcpy(out +2, hexits[s], 2);

			s = in & 0x000000FF;			
			memcpy(out, hexits[s], 2);
		}

		void HexitToChar(unsigned int in, char *out)
		{
			s = in & 0x000000FF;
			memcpy(out +6, hexits[s], 2);

			s = in / 0x00000100;
			s&= 0x000000FF;
			memcpy(out +4, hexits[s], 2);

			s = in / 0x00010000;
			s&= 0x000000FF;
			memcpy(out +2, hexits[s], 2);

			s = in / 0x01000000;
			s&= 0x000000FF;
			memcpy(out, hexits[s], 2);
		}

		void HexitToAscii(unsigned int in, char *out)
		{
			memcpy(out, &in, 4);
		}

		
		void Hash(void *in, int rt)
		{
			Hash(in, rt, 0);
		}

		void Hash(void *in, int rt, int length)
		{
			Msg.SetMessage(in, length);
			memcpy(HashOP, HashCst, sizeof(int)*8);

			for(y=0; y<Msg.nb; y++)
			{
				Compute(y);

				HashOP[0] += a; HashOP[1] += b;
				HashOP[2] += c; HashOP[3] += d;
				HashOP[4] += e; HashOP[5] += f;
				HashOP[6] += g; HashOP[7] += h;
			}

			if(!rt)//hexit to char
			{
				HexitToChar(HashOP[0], HashOutput);
				HexitToChar(HashOP[1], HashOutput +8);
				HexitToChar(HashOP[2], HashOutput +16);
				HexitToChar(HashOP[3], HashOutput +24);
				HexitToChar(HashOP[4], HashOutput +32);
				HexitToChar(HashOP[5], HashOutput +40);
				HexitToChar(HashOP[6], HashOutput +48);
				HexitToChar(HashOP[7], HashOutput +56);
				HashOutput[64] = 0;
			}
			else if(rt==1)//hexit to ascii
			{
				HexitToAscii(HashOP[0], HashOutput);
				HexitToAscii(HashOP[1], HashOutput +4);
				HexitToAscii(HashOP[2], HashOutput +8);
				HexitToAscii(HashOP[3], HashOutput +12);
				HexitToAscii(HashOP[4], HashOutput +16);
				HexitToAscii(HashOP[5], HashOutput +20);
				HexitToAscii(HashOP[6], HashOutput +24);
				HexitToAscii(HashOP[7], HashOutput +28);
				HashOutput[32] = 0;
			}
			else if(rt==2) //hexit to char, little endian output
			{
				HexitToChar(HashOP[0], HashOutput +56, 1);
				HexitToChar(HashOP[1], HashOutput +48, 1);
				HexitToChar(HashOP[2], HashOutput +40, 1);
				HexitToChar(HashOP[3], HashOutput +32, 1);
				HexitToChar(HashOP[4], HashOutput +24, 1);
				HexitToChar(HashOP[5], HashOutput +16, 1);
				HexitToChar(HashOP[6], HashOutput +8, 1);
				HexitToChar(HashOP[7], HashOutput, 1);
				HashOutput[64] = 0;
			}
		}

		static void SetupHexits()
		{
			int a, b, c, d;
			for(a=0; a<256; a++)
			{
				hexits[a] = (char*)malloc(3);
				hexits[a][2] = 0;

				b = a/16;
				if(b<10) c=b+48;
				else c=b+55;
				hexits[a][0] = c;

				d = a - b*16;
				if(d<10) c=d+48;
				else c=d+55;
				hexits[a][1] = c;
			}
		}

		unsigned int Sigma0(unsigned int e)
		{
			//rotr(e, 2) XOR rotr(e, 13) XOR rotr(e, 22)
			
			s = ROTr(e, 2);
			t = ROTr(e, 13);
			u = ROTr(e, 22);

			r=s^t; s=r^u;
			
			return s;
		}

		unsigned int Sigma1(unsigned int e)
		{
			//rotr(e, 6) XOR rotr(e, 11) XOR rotr(e, 25)
			
			s = ROTr(e, 6);
			t = ROTr(e, 11);
			u = ROTr(e, 25);

			r=s^t; s=r^u;
			
			return s;		
		}

		unsigned int Ch(unsigned int x, unsigned int y, unsigned int z)
		{
			//(x and y) XOR (~x and z)
			
			s = ~x;
			t = x&y;
			u = s&z;

			return t^u;
		}

		unsigned int Maj(unsigned int x, unsigned int y, unsigned int z)
		{
			//(x and y) XOR (x and z) XOR (y and z)
			
			s = x&y;
			t = x&z;
			u = y&z;
			
			r = s^t;
			
			return r^u;
		}

		void cap(char *in)
		{
			n = strlen(in);
			for(i=0; i<n; i++)
			{
				if(in[i]>='a') in[i]-=32;
			}
		}

		static void Init()
		{
			memset(RoundCst, 0, sizeof(int)*64);
			Sha256SetupCst(RoundCst, "428a2f98 71374491 b5c0fbcf e9b5dba5 3956c25b 59f111f1 923f82a4 ab1c5ed5"
									 "d807aa98 12835b01 243185be 550c7dc3 72be5d74 80deb1fe 9bdc06a7 c19bf174" 
									 "e49b69c1 efbe4786 0fc19dc6 240ca1cc 2de92c6f 4a7484aa 5cb0a9dc 76f988da" 
									 "983e5152 a831c66d b00327c8 bf597fc7 c6e00bf3 d5a79147 06ca6351 14292967" 
									 "27b70a85 2e1b2138 4d2c6dfc 53380d13 650a7354 766a0abb 81c2c92e 92722c85" 
									 "a2bfe8a1 a81a664b c24b8b70 c76c51a3 d192e819 d6990624 f40e3585 106aa070" 
									 "19a4c116 1e376c08 2748774c 34b0bcb5 391c0cb3 4ed8aa4a 5b9cca4f 682e6ff3" 
									 "748f82ee 78a5636f 84c87814 8cc70208 90befffa a4506ceb bef9a3f7 c67178f2");

			memset(HashCst, 0, sizeof(int)*8);
			Sha256SetupCst(HashCst, "6a09e667 bb67ae85 3c6ef372 a54ff53a 510e527f 9b05688c 1f83d9ab 5be0cd19");
		}

		static void Sha256SetupCst(unsigned int *out, char *in)
		{
			unsigned int l = strlen(in);
			unsigned int n=0, s=0, t, a, f;
			while(n<l)
			{
				a = in[n];
				if(a>='0')
				{
					if(a>='a') a-=87;
					else a-=48;

					t = s/32;
					f = s - t*32;
					f = 28-f;
					a<<=f;
					out[t] |= a;
					s+=4;
				}

				n++;
			}
		}

		void toBuff(void* out)
		{
			cbuffer = (unsigned char*)out;
			unsigned int *ho = HashOP;
			for(i=0; i<8; i++)
			{
				*cbuffer++ = (*ho & 0xFF000000) >> 24;
				*cbuffer++ = (*ho & 0x00FF0000) >> 16;
				*cbuffer++ = (*ho & 0x0000FF00) >> 8;
				*cbuffer++ =  *ho & 0x000000FF;
				ho++;
			}
		}

		void Wipe()
		{
			/*** CLEAR ALL BUFFERS ***/
		}

		void HashHex(char *in)
		{
			int n=0; int l=strlen(in), r=-1, s=0, m;
			unsigned char *abc = (unsigned char*)malloc(l/2 +1);
			memset(abc, 0, l/2 +1);

			while(n<l)
			{
				if(in[n]>='0')
				{
					if(in[n]>='a') m = in[n]-87;
					else if(in[n]>='A') m = in[n]-55;
					else m = in[n]-48;

					if(!(s&0x00000001)) m*=16, r++;
					s++;

					abc[r]+=m;
				}
				
				n++;
			}
			abc[r+1] = 0;

			Hash(abc, 0);
			free(abc);
		}

		void OutputBE(void *in)
		{
			cbuffer = (byte*)in;

			for(i=0; i<8; i++)
			{
				cbuffer[0] = (HashOP[i] & 0xFF000000) >> 24;
				cbuffer[1] = (HashOP[i] & 0x00FF0000) >> 16;
				cbuffer[2] = (HashOP[i] & 0x0000FF00) >> 8;
				cbuffer[3] = (HashOP[i] & 0x000000FF);
				
				cbuffer+=4;
			}
		}
};

unsigned int *Sha256Round::RoundCst = (unsigned int*)malloc(sizeof(int)*64);
unsigned int *Sha256Round::HashCst  = (unsigned int*)malloc(sizeof(int)*8);

char **Sha256Round::hexits = (char**)malloc(sizeof(char*)*256);

class sID
{
	protected:
		int i;
		double a, b, c, d;
		int e, f, g, h, l, m, r, s;
		Sha256Round SHA;

	public:
		char *token;
		
		sID::sID()
		{
			token = (char*)malloc(34);
			token[33] = 0;
		}

		sID::~sID()
		{
			free(token);
		}

		void Build(unsigned int *in)
		{
			/***This class is used to build the session id fed as a cookie to the client browser.
			Session IDs are 128 bits in hex.
			***/

			//step 1: build a random seed
					memcpy(token, in, 16);
					memcpy(token +16, in, sizeof(int)*4);

			//step 2: hash it
				//need to fix the sha256 class
				SHA.Hash((unsigned char*)token, 0, 32);
				memcpy(token, SHA.HashOutput +16, 32); //take half the hash from the middle
				memset(token +32, 0, 1);
		}

		void CharToHexit(int iin, int rounds, char *cin)
		{
			for(r=0; r<rounds; r++)
			{
				s = iin & 0x0000000f;
				iin /= 0x00000010;

				if(s<10) s+=48;
				else s+=55;

				cin[r] = (char)s;
			}
		}

		void HexitToChar(char *in, char *out, int rounds)
		{
			for(r=0; r<rounds; r++)
			{
				for(s=0; s<256; s++)
				{
					if(in[r*2]==Sha256Round::hexits[s][0])
						if(in[r*2+1]==Sha256Round::hexits[s][1])
						{
							out[r] = char(s);
							break;
						}
				}
			}
			out[rounds] = 0;
		}

		void Wipe()
		{
			/*** WIPE DATA ***/
		}
};

class secp256k1mpz
{
	/*** SPEED THIS SHIT UP 
	use mpn instead of mpz.
	mpz enforces memory tight schemes on its structures.
	***/
	public:
		static mpz_t Gx;
		static mpz_t Gy;
		static mpz_t p;
		static mpz_t N;

		static mpz_t *multx;
		static mpz_t *multy;

		mpz_t lambda, tmp1, tmp2, tmp3, tmpx;
		mpz_t X, Y;
		mpz_t W, R;

		int i, y, a, b, n, l, m, s, r;
		unsigned long t;

		unsigned int *bin;

		secp256k1mpz::secp256k1mpz()
		{
			mpz_init(tmp1); 
			mpz_init(tmp2); 
			mpz_init(tmp3);
			mpz_init(tmpx);
			mpz_init(lambda);
			mpz_init(X);
			mpz_init(Y);
			mpz_init(R);
			mpz_init(W);
		}

		secp256k1mpz::~secp256k1mpz()
		{
			mpz_clear(tmp1);
			mpz_clear(tmp2);
			mpz_clear(tmp3);
			mpz_clear(tmpx);
			mpz_clear(lambda);
			mpz_clear(X);
			mpz_clear(Y);
			mpz_clear(R);
			mpz_clear(W);
		}

		static void Init()
		{
			mpz_init(Gx);
			mpz_init(Gy);
			mpz_init(p);

			mpz_set_str(Gx, "79BE667E F9DCBBAC 55A06295 CE870B07 029BFCDB 2DCE28D9"
										"59F2815B 16F81798", 16);
			mpz_set_str(Gy, "483ADA77 26A3C465 5DA4FBFC 0E1108A8 FD17B448 A6855419" 
										"9C47D08F FB10D4B8", 16);

			mpz_set_str(p, "FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF"
								"FFFFFFFE FFFFFC2F", 16);

			mpz_set_str(N, "FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFE BAAEDCE6 AF48A03B"
								"BFD25E8C D0364141", 16);

			mpz_t lambda, tmp1, tmp2, tmp3, tmpx;
			mpz_init(tmp1); mpz_init(tmp2); mpz_init(tmp3); mpz_init(tmpx); mpz_init(lambda);

			mpz_init(multx[0]);
			mpz_init(multy[0]);
			mpz_set(multx[0], Gx);
			mpz_set(multy[0], Gy);

			for(int i=1; i<256; i++)
			{
				mpz_init(multx[i]); mpz_init(multy[i]);

				//lambda = 3x²/2y (a=0 for secp256k1)
				mpz_mul(tmp1, multx[i-1], multx[i-1]);
				mpz_mul_ui(tmp2, tmp1, 3);
				
				mpz_mul_ui(tmp1, multy[i-1], 2);
				mpz_invert(tmpx, tmp1, p);
				mpz_mul(tmp3, tmpx, tmp2);

				mpz_mod(lambda, tmp3, p);

				//X = lambda² - 2x
				mpz_mul(tmp1, lambda, lambda);
				mpz_mul_ui(tmp2, multx[i-1], 2);
				mpz_sub(tmp3, tmp1, tmp2);
				mpz_mod(multx[i], tmp3, p);

				//Y = lambda(x - X) -y
				mpz_sub(tmp1, multx[i-1], multx[i]);
				mpz_mul(tmp2, tmp1, lambda);
				mpz_sub(tmp3, tmp2, multy[i-1]);
				mpz_mod(multy[i], tmp3, p);
			}

			mpz_clear(tmp1);
			mpz_clear(tmp2);
			mpz_clear(tmp3);
			mpz_clear(tmpx);
			mpz_clear(lambda);
		}

		void SetPubKey(char *in)
		{
			mpz_set_str(Y, in +64, 16);
			char *tmp = (char*)malloc(65);
			memcpy(tmp, in, 64);
			tmp[64] = 0;
			mpz_set_str(X, tmp, 16);
			free(tmp);
		}

		void Mult(void *w, mpz_t &x, mpz_t &y)
		{
			mpz_set(X, x); mpz_set(Y, y);
			unsigned int *bt = (unsigned int*)w;
			unsigned int *bz = (unsigned int*)w;
			bt+=7;

			r=1;
			t = 0x80000000;
			while(r)
			{
				n = *bt&t;

				t>>=1;
				if(!t) t=0x80000000, bt--;
				if(n) break;
			}

			r = bt-bz;
			while(r>=0)
			{
				double_dot(X, Y);
				if(*bt&t) add_dots(X, Y, x, y);

				t>>=1;
				if(!t) t=0x80000000, bt--, r = bt-bz;
			}
		}

		void GetPubKey(void *out)
		{
			memset(out, 0x04, 1);
			unsigned int *bo = (unsigned int*)((byte*)out +1);
			for(i=0; i<8; i++)
			{
				bo[7-i] = Endian(X->_mp_d[i]);
				bo[15-i] = Endian(Y->_mp_d[i]);
			}
		}

		void GetPubKeyBE(unsigned char *in)
		{
			for(i=7; i>-1; i--)
			{
				a  = (X[0]._mp_d[i] & 0xFF000000) >> 24;
				a |= (X[0]._mp_d[i] & 0x00FF0000) >> 8;
				a |= (X[0]._mp_d[i] & 0x0000FF00) << 8;
				a |= (X[0]._mp_d[i] & 0x000000FF) << 24;

				memcpy(in, &a, 4);
				in+=4;
			}

			for(i=7; i>-1; i--)
			{
				a  = (Y[0]._mp_d[i] & 0xFF000000) >> 24;
				a |= (Y[0]._mp_d[i] & 0x00FF0000) >> 8;
				a |= (Y[0]._mp_d[i] & 0x0000FF00) << 8;
				a |= (Y[0]._mp_d[i] & 0x000000FF) << 24;

				memcpy(in, &a, 4);
				in+=4;
			}
		}


		void double_dot(mpz_t &x, mpz_t &y)
		{
			//lambda = 3x²/2y (a=0 for secp256k1)
			mpz_mul(tmp1, x, x);
			mpz_mul_ui(tmp2, tmp1, 3);
			
			mpz_mul_ui(tmp1, y, 2);
			mpz_invert(tmpx, tmp1, p);
			mpz_mul(tmp3, tmpx, tmp2);

			mpz_mod(lambda, tmp3, p);

			//X = lambda² - 2x
			mpz_mul(tmp1, lambda, lambda);
			mpz_mul_ui(tmp2, x, 2);
			mpz_sub(tmp3, tmp1, tmp2);
			mpz_mod(tmpx, tmp3, p);

			//Y = lambda(x - X) -y
			mpz_sub(tmp1, x, tmpx);
			mpz_mul(tmp2, tmp1, lambda);
			mpz_sub(tmp3, tmp2, y);
			mpz_mod(y, tmp3, p);
			mpz_set(x, tmpx);
		}

		void add_dots(mpz_t &x1, mpz_t &y1, mpz_t &x2, mpz_t &y2)
		{
			//lambda = (y2-y1)/(x2-x1)
			mpz_sub(tmp1, y2, y1);
			mpz_sub(tmp2, x2, x1);
			
			mpz_invert(tmpx, tmp2, p);
			mpz_mul(tmp3, tmpx, tmp1);
			mpz_mod(lambda, tmp3, p);

			//X = lambda² -x1 -x2
			mpz_mul(tmp1, lambda, lambda);
			mpz_sub(tmp2, tmp1, x1);
			mpz_sub(tmp3, tmp2, x2);
			mpz_mod(tmpx, tmp3, p);

			//Y = lambda*(x1 - X) -y1
			mpz_sub(tmp1, x1, tmpx);
			mpz_mul(tmp2, lambda, tmp1);
			mpz_sub(tmp3, tmp2, y1);
			mpz_mod(y1, tmp3, p);

			//assign X to x
			mpz_set(x1, tmpx);
		}

		long MultD(void *in)
		{
			//clock_t ct = clock();
			//first init
			n=0; l=1;
			bin = (unsigned int*)in;

			while(l)
			{
				t = n/32;
				m = n -t*32;
				s = 1<<m;

				r = bin[t] & s;

				if(r)
				{
					mpz_set(X, multx[n]);
					mpz_set(Y, multy[n]);
					l=0;
				}

				n++;

				if(n>255) l=0;
			}

			//next add
			for(i=n; i<256; i++)
			{
				t = i/32;
				m = i -t*32;
				s = 1<<m;

				r = bin[t] & s;

				if(r) 
					add_dots(X, Y, multx[i], multy[i]);
			}

			//ct = clock() - ct;
			return 0;
		}

		void Wipe()
		{
			//wipe data
			memset(&lambda[0]._mp_d, 0xCD, sizeof(mp_limb_t)*lambda[0]._mp_alloc);
			memset(&tmp1[0]._mp_d, 0xCD, sizeof(mp_limb_t)*tmp1[0]._mp_alloc);
			memset(&tmp2[0]._mp_d, 0xCD, sizeof(mp_limb_t)*tmp2[0]._mp_alloc);
			memset(&tmp3[0]._mp_d, 0xCD, sizeof(mp_limb_t)*tmp3[0]._mp_alloc);
			memset(&tmpx[0]._mp_d, 0xCD, sizeof(mp_limb_t)*tmpx[0]._mp_alloc);
			
			//not wiping pubkey, makes no sense oO
			i = y = a = b = n = l = m = s = r = 0;
			t = 0;
		}
		
		void getS(void *k, void *hash, void *x1, void *dA)
		{
			//mod inv k
			set_mpz(lambda, k, 32);
			mpz_invert(tmpx, lambda, N);

			//x1*dA
			set_mpz(tmp1, x1, 32);
			set_mpz(tmp2, dA, 32);

			mpz_mul(tmp3, tmp2, tmp1);

			//+z
			set_mpz(tmp1, hash, 32);
			mpz_add(lambda, tmp1, tmp3);

			//*modinv(k)
			mpz_mul(tmp2, lambda, tmpx);

			//mod(p);
			mpz_mod(tmp1, tmp2, N);
		}

		int VerifySig(void *hash, void *DER, void *pkeyx, void *pkeyy)
		{
			byte *der = (byte*)DER;
			der+=3;
			
			//extract r;
			DERdecode3(R, der);

			//extract s;
			der += *der +2;
			DERdecode3(tmp2, der);

			//w = mod inv s
			mpz_invert(W, tmp2, N);

			//u1 = hash*w
			mpz_t Z; mpz_init(Z);
			set_mpz(tmpx, hash, 32);
			mpz_mul(tmp3, W, tmpx);
			mpz_mod(Z, tmp3, N);

			//u2 = r*w
			mpz_t U2; mpz_init(U2);
			mpz_mul(tmp3, W, R);
			mpz_mod(U2, tmp3, N);

			//u1*G
			MultD((unsigned int*)Z->_mp_d);
			mpz_t tmpX, tmpY;
			mpz_init(tmpX); mpz_init(tmpY);
			mpz_set(tmpX, X); mpz_set(tmpY, Y);

			//load pkey
			mpz_t pkx, pky;
			mpz_init(pkx); mpz_init(pky);
			set_mpzBE(pkx, pkeyx, 32);
			set_mpzBE(pky, pkeyy, 32);
			Mult(U2->_mp_d, pkx, pky);
			
			add_dots(X, Y, tmpX, tmpY);
			mpz_mod(W, X, N);
			
			mpz_clear(tmpX);
			mpz_clear(tmpY);

			return mpz_cmp(W, R);
		}
};

mpz_t secp256k1mpz::Gx;
mpz_t secp256k1mpz::Gy;
mpz_t secp256k1mpz::p;
mpz_t secp256k1mpz::N;
mpz_t *secp256k1mpz::multx = (mpz_t*)malloc(sizeof(mpz_t)*256);
mpz_t *secp256k1mpz::multy = (mpz_t*)malloc(sizeof(mpz_t)*256);

class ripemd160
{
	private:
		static const int max_blocks = 3;
		static unsigned int *HashCst, *RoundCstm, *RoundCstp;
		static byte *sm, *sp, *rm, *rp;


		unsigned int **blocks;
		int nblocks;
		int i, z, r, l, s, y;
		unsigned int a, b, c, d, e, A, B, C, D, E, m, n, o, p, M, N, O, P;
		unsigned int (ripemd160::**F)(unsigned int, unsigned int, unsigned int);

	public:
		unsigned int *h;

		static void Init()
		{
			memset(HashCst, 0, sizeof(int)*5);
			SetupCst(HashCst, "67452301 EFCDAB89 98BADCFE 10325476 C3D2E1F0");

			SetupCstSht(sm, "11 14 15 12 5 8 7 9 11 13 14 15 6 7 9 8 "
							"7 6 8 13 11 9 7 15 7 12 15 9 11 7 13 12 "
							"11 13 6 7 14 9 13 15 14 8 13 6 5 12 7 5 "
							"11 12 14 15 14 15 9 8 9 14 5 6 8 6 5 12 "
							"9 15 5 11 6 8 13 12 5 12 13 14 11 8 5 6 ");

			SetupCstSht(sp, "8 9 9 11 13 15 15 5 7 7 8 11 14 14 12 6 "
							"9 13 15 7 12 8 9 11 7 7 12 7 6 15 13 11 "
							"9 7 15 11 8 6 6 14 12 13 5 14 13 13 7 5 "
							"15 5 8 11 14 14 6 14 6 9 12 9 12 5 15 8 "
							"8 5 12 9 12 5 14 6 8 13 6 5 15 13 11 11 ");

			SetupCstSht(rm, "0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 "
							"7 4 13 1 10 6 15 3 12 0 9 5 2 14 11 8 "
							"3 10 14 4 9 15 8 1 2 7 0 6 13 11 5 12 "
							"1 9 11 10 0 8 12 4 13 3 7 15 14 5 6 2 "
							"4 0 5 9 7 12 2 10 14 1 3 8 11 6 15 13 ");

			SetupCstSht(rp, "5 14 7 0 9 2 11 4 13 6 15 8 1 10 3 12 "
							"6 11 3 7 0 13 5 10 14 15 8 12 4 9 1 2 "
							"15 5 1 3 7 14 6 9 11 8 12 2 10 0 4 13 "
							"8 6 4 1 3 11 15 0 5 12 2 13 9 7 10 14 "
							"12 15 10 4 1 5 8 7 6 2 13 14 0 3 9 11 ");

			memset(RoundCstm, 0, sizeof(int)*5);
			memset(RoundCstp, 0, sizeof(int)*5);
			SetupCst(RoundCstm, "00000000 5A827999 6ED9EBA1 8F1BBCDC A953FD4E");
			SetupCst(RoundCstp, "50A28BE6 5C4DD124 6D703EF3 7A6D76E9 00000000");
		}

		ripemd160::ripemd160()
		{
			blocks = (unsigned int**)malloc(sizeof(int*)*max_blocks);
			for(i=0; i<max_blocks; i++)
				blocks[i] = (unsigned int*)malloc(sizeof(int)*16);

			nblocks=0;

			F = (unsigned int (__thiscall ripemd160::* *)(unsigned int, unsigned int, unsigned int))malloc(sizeof(void*)*5);
			F[0] = &ripemd160::f1;
			F[1] = &ripemd160::f2;
			F[2] = &ripemd160::f3;
			F[3] = &ripemd160::f4;
			F[4] = &ripemd160::f5;

			h = (unsigned int*)malloc(sizeof(int)*5);
		}

		ripemd160::~ripemd160()
		{
			for(i=0; i<max_blocks; i++)
				free(blocks[i]);
			free(blocks);

			free(F);
			free(h);
		}

		int SetMessage(unsigned char *in, int len)
		{
			a = len*8;
			if(!len) a = strlen((char*)in)*8;

			//nblocks = 448 mod[512]
			b = a/512;
			c = a - b*512;
			if(c<448) nblocks = b +1;
			else nblocks = b +2;

			if(nblocks>max_blocks) return 0;

			for(i=0; i<nblocks; i++)
				memset(blocks[i], 0, sizeof(int)*16);

			l = a/8; z=24;
			for(i=0; i<l; i++)
			{
				r = i/64;
				s = i-r*64;
				s/=4; y=24-z;
				b = unsigned int(in[i]);
				b<<=y;
				z-=8; if(z<0) z=24;

				blocks[r][s] |= b;
			}

			r = i/64;
			s = i-r*64;
			d = s/4; y = 24-z;
			b = 0x00000080<<y;
			blocks[r][d] |= b;

			blocks[nblocks-1][14] = a;

			return 1;
		}

		int Hash(unsigned char *in, int len)
		{
			if(SetMessage(in, len))
			{
				h[0] = a = A = HashCst[0];
				h[1] = b = B = HashCst[1];
				h[2] = c = C = HashCst[2];
				h[3] = d = D = HashCst[3];
				h[4] = e = E = HashCst[4];

				for(z=0; z<nblocks; z++)
				{
					Compute(z);

					a = A = h[0];
					b = B = h[1];
					c = C = h[2];
					d = D = h[3];
					e = E = h[4];
				}

				//result is in h0-4
				return 1;
			}

			return 0;
		}

		void Compute(int blocki)
		{
			for(i=0; i<5; i++)
			{
				for(y=0; y<16; y++)
				{
					r = i*16 +y;
					m = (*this.*F[i])(b, c, d);
					s = 4-i;
					M = (*this.*F[s])(B, C, D);
					
					n = RoundCstm[i] + blocks[blocki][rm[r]];
					N = RoundCstp[i] + blocks[blocki][rp[r]];
					o = c<<10 | c>>22;
					O = C<<10 | C>>22;
					
					m+=n;
					M+=N;
					m+=a;
					M+=A;
					a = e;
					A = E;
					
					p = m<<sm[r] | m>>(32-sm[r]);
					P = M<<sp[r] | M>>(32-sp[r]);
					p+=e;
					P+=E;
					
					e = d;
					E = D;
					
					d = o;
					D = O;
					
					c = b;
					C = B;
					
					b = p;
					B = P;
				}
			}

			p = h[1] + c + D; h[1] = h[2] + d + E; h[2] = h[3] + e + A;
			h[3] = h[4] + a + B; h[4] = h[0] + b + C; h[0] = p;
		}

		unsigned int f1(unsigned int ina, unsigned int inb, unsigned int inc)
		{
			ina ^= inb;
			return ina ^ inc;
		}

		unsigned int f2(unsigned int ina, unsigned int inb, unsigned int inc)
		{
			inb &= ina;
			inc &= ~ina;
			
			return inb | inc;
		}

		unsigned int f3(unsigned int ina, unsigned int inb, unsigned int inc)
		{
			ina |= ~inb;

			return ina ^ inc;
		}

		unsigned int f4(unsigned int ina, unsigned int inb, unsigned int inc)
		{
			ina &= inc;
			inb &= ~inc;

			return ina | inb;
		}

		unsigned int f5(unsigned int ina, unsigned int inb, unsigned int inc)
		{
			inb |= ~inc;

			return ina ^ inb;
		}

		static void SetupCst(unsigned int *out, char *in)
		{
			unsigned int l = strlen(in);
			unsigned int n=0, s=0, t, a, f;
			while(n<l)
			{
				a = in[n];
				if(a>='0')
				{
					if(a>='a') a-=87;
					else if(a>='A') a-=55;
					else a-=48;

					t = s/32;
					f = s - t*32;
					f = 28-f;
					a<<=f;
					out[t] |= a;
					s+=4;
				}

				n++;
			}
		}

		static void SetupCstSht(byte *out, char *in)
		{
			int l = strlen(in);
			char *cin;
			int n=0, s=0, c, i, k, r=0;
			byte b;
			while(n<l)
			{
				if(in[n]==' ')
				{
					b=0;
					cin = in +s;
					c=n-s; k=1;

					for(i=c-1; i>=0; i--)
					{
						b+=(cin[i]-48)*k;
						k*=10;
					}
						
					out[r] = b;
					s = ++n;
					r++;
				}
				
				n++;
			}
		}

		void toString(char *out)
		{
			n=0;
			for(i=0; i<5; i++)
			{
				a = h[i];
				for(y=0; y<4; y++)
				{
					b = a & 0x0000000F;
					a>>=4;

					if(b<10) b+=48;
					else b+=55;

					out[n+1] = b;
					
					b = a & 0x0000000F;
					a>>=4;

					if(b<10) b+=48;
					else b+=55;

					out[n] = b;
					
					n+=2;
				}
			}
			out[n] = 0;
		}

		void toBuff(unsigned char* out)
		{
			memcpy(out, h, sizeof(int)*5);
		}

		void toBuffBE(unsigned char* out)
		{
			for(i=0; i<5; i++)
			{
				out[i] = Endian(h[i]);
			}
		}

		void Wipe()
		{
			// wipe data		
			for(i=0; i<max_blocks; i++)
				memset(blocks[i], 0xCD, sizeof(int)*16);

			i = z = r = l = s = y = 0;
			a = b = c = d = e = A = B = C = D = E = m = n = o = p = M = N = O = P = 0;
			nblocks = 0;
			memset(h, 0xCD, sizeof(int)*5);
		}
};

unsigned int *ripemd160::HashCst = (unsigned int*)malloc(sizeof(int)*5);
unsigned int *ripemd160::RoundCstm = (unsigned int*)malloc(sizeof(int)*5);
unsigned int *ripemd160::RoundCstp = (unsigned int*)malloc(sizeof(int)*5);
byte *ripemd160::sm = (byte*)malloc(80);
byte *ripemd160::sp = (byte*)malloc(80);
byte *ripemd160::rm = (byte*)malloc(80);
byte *ripemd160::rp = (byte*)malloc(80);

class AES
{
	/*** state matrix representation for S[0-15]
	S0 S4 S8  S12
	S1 S5 S9  S13
	S2 S6 S10 S14
	S3 S7 S11 S15
	***/
	private:
		
		static unsigned char *SBox, *SBoxInv;
		int i;
		unsigned char *tmps, *tmps2, *tmps3;
		unsigned char a, b, c, d, e, f, g, h;
		int m, n, o, p, q, r, y;
		unsigned int rk, r1, A, B, C, D;
		unsigned int *ExpKey, *EncD;
		unsigned int *key, *output, *data;

		void KeyExpansion(void *K, void *OP)
		{
			//(14 rounds +1 for original key)*8 unsigned ints
			//only doing 256bit keys

			key = (unsigned int*)K;
			output = (unsigned int*)OP;

			memcpy(output, key, sizeof(int)*8);

			n=8;
			while(n<60)
			{
				rk = output[n-1];
				m = n/8;
				o = n-m*8;
				if(!o)
				{
					r1 = RotWord(rk);
					rk = SubWord(r1);
					A = 0x01000000 << (m-1); 
					rk ^= A;
				}
				else if(o==4)
				{
					rk = SubWord(rk);
				}

				output[n] = output[n-8] ^ rk;

				n++;
			}

			key=output=0;
		}

		unsigned int RotWord(unsigned int in)
		{
			A = in << 8;
			B = in >> 24;

			return A | B;
		}

		unsigned int SubWord(unsigned int in)
		{
			a = in;
			b = in >> 8;
			c = in >> 16;
			d = in >> 24;

			A = SBox[a];
			B = SBox[b] << 8;
			C = SBox[c] << 16;
			D = SBox[d] << 24;

			A |= B;
			C |= D;

			return A | C;
		}

		void SubBytes(unsigned char *state)
		{
			for(i=0; i<16; i++)
			{
				state[i] = SBox[state[i]];
			}
		}
		void AddRoundKey(unsigned char *state, unsigned int *roundkey)
		{
			for(i=0; i<4; i++)
			{
				o=i*4; p = o+1; q = o+2; r = o+3;

				state[o] ^= roundkey[i] >> 24;
				state[p] ^= roundkey[i] >> 16;
				state[q] ^= roundkey[i] >> 8;
				state[r] ^= roundkey[i];
			}
		}

		void MixColumns(unsigned char *state)
		{
			/***Mixing Matrix:
				2 3 1 1
				1 2 3 1
				1 1 2 3
				3 1 1 2
			***/

			for(i=0; i<4; i++)
			{
				m = i*4; n = m+1; o = m+2; p = m+3;
				
				a = state[m] & 0x80;
				b = state[m]<<1;
				
				c = state[n] & 0x80;
				d = state[n]<<1;
				
				if(a) b^=0x1b;
				if(c) d^=0x1b;
				
				//2*S0
				tmps[0] = b;
				//2*S1
				tmps[2] = d;

				//3*S0
				tmps[1] = b^state[m];
				//3*S1
				tmps[3] = d^state[n];

				//
				a = state[o] & 0x80;
				b = state[o]<<1;
				
				c = state[p] & 0x80;
				d = state[p]<<1;
				
				if(a) b^=0x1b;
				if(c) d^=0x1b;
				
				//2*S0
				tmps[4] = b;
				//2*S1
				tmps[6] = d;

				//3*S0
				tmps[5] = b^state[o];
				//3*S1
				tmps[7] = d^state[p];

				//
				tmps2[m] = tmps[0] ^ tmps[3];
				tmps2[n] = state[m] ^ tmps[2];
				tmps2[o] = state[m] ^ state[n];
				tmps2[p] = tmps[1] ^ state[n];

				//
				tmps2[m] ^= state[o];
				tmps2[n] ^= tmps[5];
				tmps2[o] ^= tmps[4];
				tmps2[p] ^= state[o];

				//
				tmps2[m] ^= state[p];
				tmps2[n] ^= state[p];
				tmps2[o] ^= tmps[7];
				tmps2[p] ^= tmps[6];
			}

			memcpy(state, tmps2, 16);
		}

		void ShiftRows(unsigned char *state)
		{
			/***Row Shifting:
			S0 S4 S8  S12    S0  S4  S8  S12
			S1 S5 S9  S13 -> S5  S9  S13 S1
			S2 S6 S10 S14    S10 S14 S2  S6
			S3 S7 S11 S15    S15 S3  S7  S11
			***/
			tmps[0] = state[0];  tmps[4] = state[4];  tmps[8]  = state[8];  tmps[12] = state[12];
			tmps[1] = state[5];  tmps[5] = state[9];  tmps[9]  = state[13]; tmps[13] = state[1];
			tmps[2] = state[10]; tmps[6] = state[14]; tmps[10] = state[2];  tmps[14] = state[6];
			tmps[3] = state[15]; tmps[7] = state[3];  tmps[11] = state[7];  tmps[15] = state[11];

			memcpy(state, tmps, 16);
		}

		void Cipher(void *D, void *OP)
		{
			output = (unsigned int*)OP;
			data = (unsigned int*)D;
			memset(output, 0, 16);
			
			for(y=0; y<4; y++)
			{
				p=y*4;
				tmps3[p]   = (data[y] & 0xFF000000) >> 24;
				tmps3[p+1] = (data[y] & 0x00FF0000) >> 16;
				tmps3[p+2] = (data[y] & 0x0000FF00) >> 8;
				tmps3[p+3] = data[y];
			}

			AddRoundKey(tmps3, ExpKey);

			for(y=1; y<14; y++)
			{
				SubBytes(tmps3);
				ShiftRows(tmps3);
				MixColumns(tmps3);
				AddRoundKey(tmps3, ExpKey +y*4);
			}

			SubBytes(tmps3);
			ShiftRows(tmps3);
			AddRoundKey(tmps3, ExpKey +y*4);

			for(y=0; y<4; y++)
			{
				p=y*4;
				output[y]  = tmps3[p]   << 24;
				output[y] |= tmps3[p+1] << 16;
				output[y] |= tmps3[p+2] << 8;
				output[y] |= tmps3[p+3];
			}

			data=0;
		}

		void InvShiftRows(unsigned char *state)
		{
			/***Row Shifting:
			S0 S4 S8  S12    S0  S4  S8  S12
			S1 S5 S9  S13 -> S13 S1  S5  S9
			S2 S6 S10 S14    S10 S14 S2  S6
			S3 S7 S11 S15    S7  S11 S15 S3
			***/
			tmps[0] = state[0];  tmps[4] = state[4];  tmps[8]  = state[8];  tmps[12] = state[12];
			tmps[1] = state[13]; tmps[5] = state[1];  tmps[9]  = state[5];  tmps[13] = state[9];
			tmps[2] = state[10]; tmps[6] = state[14]; tmps[10] = state[2];  tmps[14] = state[6];
			tmps[3] = state[7];  tmps[7] = state[11]; tmps[11] = state[15]; tmps[15] = state[3];

			memcpy(state, tmps, 16);
		}

		void InvSubBytes(unsigned char *state)
		{
			for(i=0; i<16; i++)
			{
				state[i] = SBoxInv[state[i]];
			}
		}

		void InvMixColumns(unsigned char *state)
		{
			//polynom mult by 09, 0b, 0d, 0e
			for(i=0; i<4; i++)
			{
				n = i*4;
				for(m=0; m<2; m++)
				{
					o = m*2+n;
					p = (m*2)*4;
					q = o+1;
					r = (m*2 +1)*4;
					
					//x2
					a = 0x80 & state[o];
					b = state[o]<<1;
					e = 0x80 & state[q];
					f = state[q]<<1;
					if(a) b ^= 0x1b;
					if(e) f ^= 0x1b;

					//x4
					a = 0x80 & b;
					c = b<<1;
					e = 0x80 & f;
					g = f<<1;
					if(a) c ^= 0x1b;
					if(e) g ^= 0x1b;

					//x8
					a = 0x80 & c;
					d = c<<1;
					e = 0x80 & g;
					h = g<<1;
					if(a) d ^= 0x1b;
					if(e) h ^= 0x1b;

					tmps[p]   = d ^ state[o]; // 8 +1
					tmps[p+3] = b ^ c ^ d;    // 8 +4 +2
					tmps[p+1] = tmps[n] ^ b;  // 9 +2
					tmps[p+2] = tmps[n] ^ c;  // 9 +4

					tmps[r]   = d ^ state[o]; // 8 +1
					tmps[r+3] = b ^ c ^ d;    // 8 +4 +2
					tmps[r+1] = tmps[n] ^ b;  // 9 +2
					tmps[r+2] = tmps[n] ^ c;  // 9 +4
				}
				
				/*0e  0b  0d  09 
				  09  0e  0b  0d 
				  0d  09  0e  0b 
				  0b  0d  09  0e*/

				p = n+1; q = n+2; r = n+3;
				
				tmps2[n] = tmps[3] ^ tmps[5];
				tmps2[p] = tmps[0] ^ tmps[7];
				tmps2[q] = tmps[2] ^ tmps[4];
				tmps2[r] = tmps[1] ^ tmps[6];

				tmps2[n] ^= tmps[10];
				tmps2[p] ^= tmps[9];
				tmps2[q] ^= tmps[11];
				tmps2[r] ^= tmps[8];

				tmps2[n] ^= tmps[12];
				tmps2[p] ^= tmps[14];
				tmps2[q] ^= tmps[13];
				tmps2[r] ^= tmps[15];
			}
		}

		void InvCipher(unsigned int *data, unsigned int *out)
		{
			memcpy(tmps3, data, 16);

			AddRoundKey(tmps3, ExpKey +56);

			for(y=13; y<0; y--)
			{
				InvShiftRows(tmps3);
				InvSubBytes(tmps3);
				AddRoundKey(tmps3, ExpKey +y*4);
				InvMixColumns(tmps3);
			}

			InvShiftRows(tmps3);
			InvSubBytes(tmps3);
			AddRoundKey(tmps3, ExpKey);

			memcpy(out, tmps3, 16);
		}

	public:

		AES::AES()
		{
			tmps = (unsigned char*)malloc(16);
			tmps2 = (unsigned char*)malloc(16);
			tmps3 = (unsigned char*)malloc(16);
			ExpKey = (unsigned int*)malloc(sizeof(int)*60);
			EncD = (unsigned int*)malloc(sizeof(int)*4);
		}

		AES::~AES()
		{
			free(tmps);
			free(tmps2);
			free(tmps3);
			free(EncD);
			free(ExpKey);
		}

		static void Init()
		{
			SetupSBox();
		}

		static void SetupSBox()
		{
			memset(SBox, 0, 256);
			char *sboxstr = (char*)malloc(1009);
			strcpy(sboxstr, "63 7c 77 7b f2 6b 6f c5 30 01 67 2b fe d7 ab 76 "
							"ca 82 c9 7d fa 59 47 f0 ad d4 a2 af 9c a4 72 c0 "
							"b7 fd 93 26 36 3f f7 cc 34 a5 e5 f1 71 d8 31 15 "
							"04 c7 23 c3 18 96 05 9a 07 12 80 e2 eb 27 b2 75 "
							"09 83 2c 1a 1b 6e 5a a0 52 3b d6 b3 29 e3 2f 84 "
							"53 d1 00 ed 20 fc b1 5b 6a cb be 39 4a 4c 58 cf "
							"d0 ef aa fb 43 4d 33 85 45 f9 02 7f 50 3c 9f a8 "
							"51 a3 40 8f 92 9d 38 f5 bc b6 da 21 10 ff f3 d2 "
							"cd 0c 13 ec 5f 97 44 17 c4 a7 7e 3d 64 5d 19 73 "
							"60 81 4f dc 22 2a 90 88 46 ee b8 14 de 5e 0b db "
							"e0 32 3a 0a 49 06 24 5c c2 d3 ac 62 91 95 e4 79 "
							"e7 c8 37 6d 8d d5 4e a9 6c 56 f4 ea 65 7a ae 08 "
							"ba 78 25 2e 1c a6 b4 c6 e8 dd 74 1f 4b bd 8b 8a "
							"70 3e b5 66 48 03 f6 0e 61 35 57 b9 86 c1 1d 9e "
							"e1 f8 98 11 69 d9 8e 94 9b 1e 87 e9 ce 55 28 df "
							"8c a1 89 0d bf e6 42 68 41 99 2d 0f b0 54 bb 16 ");

			int n=0, l=strlen(sboxstr), s=0, m=0;
			char *tmp; int i; unsigned char r;
			
			while(n<l)
			{
				if(sboxstr[n]==' ')
				{
					tmp = sboxstr +s;
					for(i=0; i<2; i++)
					{
						if(tmp[i]>='a') r = tmp[i] - 87;
						else r = tmp[i] - 48;
						if(!i) r*=16;

						SBox[m] += r;
					}

					m++;
					s = n+1;
				}
				n++;
			}

			strcpy(sboxstr, "52 09 6a d5 30 36 a5 38 bf 40 a3 9e 81 f3 d7 fb "
							"7c e3 39 82 9b 2f ff 87 34 8e 43 44 c4 de e9 cb "
							"54 7b 94 32 a6 c2 23 3d ee 4c 95 0b 42 fa c3 4e "
							"08 2e a1 66 28 d9 24 b2 76 5b a2 49 6d 8b d1 25 "
							"72 f8 f6 64 86 68 98 16 d4 a4 5c cc 5d 65 b6 92 "
							"6c 70 48 50 fd ed b9 da 5e 15 46 57 a7 8d 9d 84 "
							"90 d8 ab 00 8c bc d3 0a f7 e4 58 05 b8 b3 45 06 "
							"d0 2c 1e 8f ca 3f 0f 02 c1 af bd 03 01 13 8a 6b "
							"3a 91 11 41 4f 67 dc ea 97 f2 cf ce f0 b4 e6 73 "
							"96 ac 74 22 e7 ad 35 85 e2 f9 37 e8 1c 75 df 6e "
							"47 f1 1a 71 1d 29 c5 89 6f b7 62 0e aa 18 be 1b "
							"fc 56 3e 4b c6 d2 79 20 9a db c0 fe 78 cd 5a f4 "
							"1f dd a8 33 88 07 c7 31 b1 12 10 59 27 80 ec 5f "
							"60 51 7f a9 19 b5 4a 0d 2d e5 7a 9f 93 c9 9c ef "
							"a0 e0 3b 4d ae 2a f5 b0 c8 eb bb 3c 83 53 99 61 "
							"17 2b 04 7e ba 77 d6 26 e1 69 14 63 55 21 0c 7d ");			

			n=0, l=strlen(sboxstr), s=0, m=0;
			
			while(n<l)
			{
				if(sboxstr[n]==' ')
				{
					tmp = sboxstr +s;
					for(i=0; i<2; i++)
					{
						if(tmp[i]>='a') r = tmp[i] - 87;
						else r = tmp[i] - 48;
						if(!i) r*=16;

						SBoxInv[m] += r;
					}

					m++;
					s = n+1;
				}
				n++;
			}
			free(sboxstr);
		}

		void Encrypt(void *key, void *data, int size, void *out, int mode)
		{
			/*** MUST BE 128 BITS ALIGNED ***/
			KeyExpansion(key, ExpKey);

			int rs = size/16, is = 0;
			if(rs*16!=size) rs++;

			while(is<rs)
			{
				if(!out || mode) Cipher(data, EncD);
				else Cipher(data, (byte*)out +is*16);

				if(mode)
				{
					/*** output encrypted data as hexits ***/
					char *cout = (char*)out +is*32;

					OutputRaw(EncD, 16, cout);
				}

				is++;
			}

			/*** CLEAR Expanded Key. Key should be cleared by caller ***/
			memset_atomic((std::atomic_int32_t*)ExpKey, 0, 60);
		}

		void CleanUp()
		{
			memset(tmps , 0, 16);
			memset(tmps2, 0, 16);
			memset(tmps3, 0, 16);
		}

		void Wipe()
		{
			memset(tmps, 0xCD, 16);
			memset(tmps2, 0xCD, 16);
			memset(tmps3, 0xCD, 16);

			memset(ExpKey, 0xCD, 60);
			memset(EncD, 0xCD, 16);

			a = b = c = d = e = f = g = h = 0;
			m = n = o = p = q = r = y = 0;
			rk = r1 = A = B = C = D = 0;
		}
};

unsigned char *AES::SBox    = (unsigned char*)malloc(256);
unsigned char *AES::SBoxInv = (unsigned char*)malloc(256);

void *seed_thread(void *in);
pthread_mutex_t seed_lock;

class Fortuna
{
	/*** consider time based seed reseting too ***/

	private:
		
		Sha256Round SHA;
		AES aes;

		int count, state, sw;
		unsigned int *seed;
		unsigned int *counter;
		unsigned int a, b;
		pthread_t id;

	public:

		TimeData TD;
		std::atomic_int seeded;
		unsigned int *PRN;

		Fortuna::Fortuna()
		{
			seeded = 0;
			count = state = 0;
			counter = (unsigned int*)malloc(sizeof(int)*4);
			counter[0]= 0x01020304 ^ (int)counter;
			counter[1]= 0x05060708 ^ (int)this;
			counter[2]= 0x090a0b0c ^ (int)PRN;
			counter[3]= 0x110f0e0d;
			seed = (unsigned int*)malloc(sizeof(int)*8);
			PRN = (unsigned int*)malloc(sizeof(int)*4);

			GetSeed();
		}

		Fortuna::~Fortuna()
		{
			Wipe();

			memset(seed, 0xCD, sizeof(int)*8);
			free(counter);
			free(seed);
			free(PRN);
		}

		void Wipe()
		{
			/*** CLEAR SENSIBLE DATA ***/
			memset(counter, 0xCD, sizeof(int)*4);
			memset(PRN, 0xCD, sizeof(int)*4);
		}

		unsigned int* GetRN(void *in)
		{
			sw=0;
			while(!seeded)
			{
				Sleep(10);
				sw++;

				if(sw>200) return 0;
			}

			//increment counter
			a = count/4;
			b = count - a*4;
			counter[b]++;

			//process it
			if(!in)aes.Encrypt(seed, counter, 16, (char*)PRN, 0);
			else aes.Encrypt(seed, counter, 16, (char*)in, 0);
			state+=16;

			//if data produced > 10KB, reseed
			if(state>=10000) GetSeed();

			//return
			return PRN;
		}

		void GetSeed()
		{
			//start thread
			pthread_create(&id, NULL, seed_thread, (void*)this);
		}

		void SetSeed(unsigned int *in)
		{
			SHA.Hash(in, -1, 32);
			SHA.Hash(SHA.HashOP, -1, 32);

			seeded=0;
			std::atomic_store(&seeded, 0);
			
			memcpy(seed, SHA.HashOP, 32);
			state=0;

			seeded=1;
			std::atomic_store(&seeded, 1);
			
			SHA.Wipe();
		}
};

void *seed_thread(void *in)
{
	Fortuna *FP = (Fortuna*)in;

	//building seed
	unsigned int *seedtmp = (unsigned int*)malloc(sizeof(int)*8);
	char *p = (char*)seedtmp;
	memset(seedtmp, 0, sizeof(int)*8);
	seedtmp[0] = (unsigned int)seedtmp;

	DWORD ms = FP->TD.GetTimeMS()/60000;
	memcpy(seedtmp +1, &ms, 2);
	int at = 6, a, b, c=0;
	ms = 0;
	
	while(at<32)
	{
		a = FP->TD.GetTimeMS() & 0x000000FF;
		if(a!=ms)
		{
			b = at & 0x00000001;
			if(b) a<<=8;
			ms |= a;
			c++;

			if(c==2)
			{
				memcpy(p +at-2, &ms, 2);
				Sleep(55);
				c=0;
				ms=0;
			}

			at++;
		}
	}

	FP->SetSeed(seedtmp);
	at=a=b=c=ms=0;
	free(seedtmp);
	seedtmp=0;
	p=0;
	FP=0;
	return 0;
}



class Base64
{
	public:
		static char *Base64str;

		static void Init()
		{
			strcpy(Base64str, "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");
		}

		void Encode(char *in, char *out)
		{
			int a = strlen(in);
			char *ctmp = (char*)malloc(a*4);
			int b = a/4 +1;
			mp_limb_t *bin = (mp_limb_t*)malloc(sizeof(mp_limb_t)*b);
			mp_limb_t *div = (mp_limb_t*)malloc(sizeof(mp_limb_t)*b);
			mp_limb_t *tmp;
			mp_limb_t d64 = 64, rem;
			int i, r=0, m=0;

			memset(bin, 0, sizeof(mp_limb_t)*b);
			memset(div, 0, sizeof(mp_limb_t)*b);

			for(i=0; i<a; i++)
			{
				m = i/4;
				bin[m] |= in[a-1-i] << r;
				r+=8;
				if(r==32) r=0;
			}

			a=1; r=0;
			while(a)
			{
				mpn_tdiv_qr(div, &rem, 0, bin, b, &d64, 1);
				ctmp[r] = Base64str[rem];
				r++;

				tmp=div;
				div=bin;
				bin=tmp;

				a=bin[0];
				for(i=1; i<b; i++)
				{
					a^=bin[i];
				}
			}

			for(i=0; i<r; i++)
				out[r-1-i] = ctmp[i];

			out[r]=0;

			free(ctmp);
			free(bin);
			free(div);
		}

		void HTTPAuth(char *log, char *pass, char *out)
		{
			char *abc = (char*)malloc(strlen(log) + strlen(pass) + 2);
			strcpy(abc, log);
			strcat(abc, ":");
			strcat(abc, pass);

			Encode(abc, out);
			free(abc);
		}
};

char *Base64::Base64str = (char*)malloc(65);