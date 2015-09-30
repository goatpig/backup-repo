#define _CRT_SECURE_NO_WARNINGS

int run = 1;
unsigned int CURRENT_BLOCK = 0;
char database_path[] = "db";

//#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#include <windows.h>
byte *txnhashout = 0;
#include <atomic>
#include <fcgi_config_x86.h>
#include <fcgiapp.h>
#include "MemHandler.cpp"
#include "crypto.cpp"
#include "BTChandler.cpp"
#include "dbHandler.cpp"
#include "htmlParser.cpp"


FILE *ferr = NULL;

DWORD bcbm=0;

#define THREAD_COUNT 3
pthread_mutex_t mutex_accept = PTHREAD_MUTEX_INITIALIZER;

int sockfd;
int count=0;


htmlLoader    htmlPage1;
htmlRequest  *htmlReq=NULL;

void *game_updater(void *in)
{
	while(1)
	{
		//GH.Update();
		Sleep(60000);
	}
	return 0;
}

void *thread_start(void* in)
{
	int thread_id = int(in);
	int rc;
	FCGX_Request request;

	int re = FCGX_InitRequest(&request, sockfd, 0);
	htmlReq[thread_id].Setup(&request, &htmlPage1);

	while(run)
	{
		//serializing accept (apparently some platforms require this).
		pthread_mutex_lock(&mutex_accept);
		rc = FCGX_Accept_r(&request);
        pthread_mutex_unlock(&mutex_accept);

		if(rc>=0)
		{
			/***to do: 
				1) call request interpreter with header data (to detect js and shit) + body
					1.a) call db for relevant requests
					1.b) check login status/user id token
					1.c) update blockchain relevant info
					1.d) update games info
				2) call html output code with parsed request + parameters
			***/
				//1.a) & 1.b) done by htmlReq
			htmlReq[thread_id].ProcessRequest();

				//1.c) done by chain update dedicated thread

				//1.d) done by game update dedicated thread
		}
	}

	return 0;
}

void Init()
{
	//init fcgi library
	FCGX_Init();
	sockfd = FCGX_OpenSocket(":9050", 10);

	PlatformSocket::Init();
	BTCsocket::Init();
	HtmlHeader::Init();
	JSONhandler::Init();


	//init all static members
	Module::InitControlsName();
	Module::InitModules();
	htmlRenderer::InitModules();
	Base64::Init();
	Sha256Round::Init();
	Sha256Round::SetupHexits();
	secp256k1mpz::Init();
	ripemd160::Init();
	AES::Init();
	MultiFile::Init();
	RawChain::Init();

	//BC.Init();

	//Init htmlLoader object
		//one loader per html template
		htmlPage1.LoadHTML("Toss a Bitcoin.html");

	//Init Database objects

		//ferr = fopen("db\\errdb.txt", "w"); /***ODD BEHAVIOR: have to open the db without the err file linked first for it to work with it later***/
		/*BRKDB = new brkdb;
		BRKDB->Setup(database_path, ferr);
		dbTxn::db = BRKDB;
		GameHandler::gameIDcounter.store(1);
		dbInit();*/
		
	//init htmlRenderer objects
		//an output per socket to process requests in parallel
		htmlReq = new htmlRequest[THREAD_COUNT];
		/*each htmlRenderer object needs to be linked to the html template it's based upon
		  this is done at thread creation*/
}

void CleanUp()
{
	//clean up objects
	delete[] htmlReq;

	//kill mutex
	pthread_mutex_destroy(&mutex_accept);

	//shut down db
	BRKDB->Close();

	//shut down libs
}

void* Maintenance_Thread(void *in)
{
	/***Clean up old games and used SID***/

	while(run)
	{

	}

	return 0;
}

int main()
{
	Init(); //class initialization

	_stat abc;
	_wstat(L"mempool.bin", &abc);
	byte *fop = (byte*)malloc(abc.st_size);
	byte *fend = fop + abc.st_size;
	byte *duncare;

	FILE *fh = fopen("mempool.bin", "rb");
	fread(fop, abc.st_size, 1, fh);
	fclose(fh);

	MemPool *mp = hBuffer.GetPool(abc.st_size*3, duncare);
	mp->reserved-=abc.st_size*3; //decrement reserved so that PushBH can assign memory from the extra estimate
	mp->freemem+=abc.st_size*3;

	TXNv2 *rt;
	unsigned int raws;

	while(fop<fend)
	{
		fop+=8;
		rt = TxnHandler.New(fop, NULL, &raws, mp);
		fop+=raws;

		nrounds++;
		if(rt->nVin>10 || rt->nVout>10)
		{
			int efr=0;
		}
	}

			
	/*BitcoinKey btckey; 
	//btckey.SetPrivateKey("15614A7B 6A307F42 FBC0F811 4701E7C8 E124E7F9 A47E2C20 35DB29A2 F6321721");
	BTCaddress addy;
	addy.SetPrivateKeyStr("15614A7B 6A307F42 FBC0F811 4701E7C8 E124E7F9 A47E2C20 35DB29A2 F6321721");
	//addy.SetPrivateKeyStr("18E14A7B6A307F426A94F8114701E7C8E774E7F9A47E2C2035DB29A206321725");
	addy.refblock = 54000;
	addy.Setup();
	byte *h160 = (byte*)malloc(20);
	int rr = btckey.AC.Base58toHash160("mrVXJ5kTmspn9ENf42EjbFgLhzB8kBBxVD", h160);

	BTCHandle btchandle;*/
	//BTCHandle bch;
	//bch.GetPendingTXN();
	//while(run){Sleep(60000);}
	//int rm = BC.GetTxnByHash(BC.blocksv2[170]->txn[1].vin[0].txnHash, 170);
	//long benchfetch1 = BC.FetchAll();
	//long benchdelete = BC.freeblocks();
//	char *yy = OutputRaw(addy.hash160, 20); 
	//BC.ScanAddresses(&addy, 1);
	//addy.Spend(500000000, h160, &btchandle);
	/*char *sp = OutputRaw(addy.Branches[0]->txnref->raw, addy.Branches[0]->txnref->rawsize);
	FILE *fr = fopen("txn2.txt", "wb");
	fwrite(sp, strlen(sp), 1, fr);
	fclose(fr);*/
	//BTCHandle btc;
	//btc.SendCoins(0, "mrVXJ5kTmspn9ENf42EjbFgLhzB8kBBxVD", "5");
	/*unsigned int *aeskey = (unsigned int*)malloc(sizeof(int)*8);
	aeskey[0] = 0x00010203;
	aeskey[1] = 0x04050607;
	aeskey[2] = 0x08090a0b;
	aeskey[3] = 0x0c0d0e0f;
	aeskey[4] = 0x10111213;
	aeskey[5] = 0x14151617;
	aeskey[6] = 0x18191a1b;
	aeskey[7] = 0x1c1d1e1f;

	unsigned int *data = (unsigned int*)malloc(sizeof(int)*4);
	data[0] = 0x00112233; 
	data[1] = 0x44556677;
	data[2] = 0x8899aabb;
	data[3] = 0xccddeeff;

	AES aes;
	aes.Encrypt(aeskey, data, 0);*/
	pthread_t id[THREAD_COUNT +1];
	int thread_index[THREAD_COUNT];
	int i;

	pthread_create(&id[THREAD_COUNT], NULL, game_updater, NULL);

	for(i=0; i<THREAD_COUNT; i++)
	{
		thread_index[i] = i;
		pthread_create(&id[i], NULL, thread_start, (void*)thread_index[i]);
	}

	CheckChain(0);

	CleanUp();

	exit(0);
}


