#include <libpq-fe.h>
#include <bdb/db.h>

const int max_tries = 20;
const int max_games_per_list = 10;


class CharExiter
{
	int l, n, a, r, s;
	public:
		int ExitString(char *in)
		{
			if(in)
			{
				l = strlen(in);
				if(!l) return -1;
				n=0; r=0; s=0;

				while(n<l)
				{
					a = (int)in[n];
					if(a=='=') r=1;
					else if(a=='*') r=1;
					else if(a<33) r=1;
					else if(a=='(') r=1;
					else if(a==')') r=1;
					else if(a=='~') r=1;

					if(r)
					{
						memcpy(in +n, in +n+1, l-n-1);
						l--; n--;
						in[l]=0;
						r=0;
						s++;
					}

					n++;
				}

				return s;
			}

			return -1;
		}
};

class Player
{
	public: 
		byte round;
		DWORD bets;
		BTCaddress *btca;

	Player::Player()
	{
		btca=0;
		round=0;
		bets=0;
	}

	int CashOut()
	{
		//cash the player out
		return 0;
	}
};

class Rounds
{
	public:
		unsigned int block;
		time_t BetPeriod;

		static const unsigned int PLstep = 100;

		unsigned int PLbuffer;
		Player **players;

		std::atomic_uint32_t *sema, expanding, npb, nplayers;
		unsigned int* semaholder, nsema;

		Rounds::Rounds()
		{
			players=0;
			semaholder=0;
			semaholder = (unsigned int*)malloc(sizeof(int));
			sema = (std::atomic_uint32_t*)semaholder;
			nsema=1;

			nplayers=0;
			BetPeriod=0;
			block=0;
			*sema=1;
			expanding=0;
			npb=0;
		}

		Rounds::~Rounds()
		{
			DumpPlayerBuffer();
		}

		void Reset()
		{
			DumpPlayerBuffer();

			nplayers=0;
			players=0;
			BetPeriod=0;
			block=0;
			expanding=0;
			npb=0;
			PLbuffer=0;
			
			semaholder = (unsigned int*)realloc(semaholder, sizeof(int)*(nsema +1)); //get new semaphore since games are meant to be recycled
			sema = (std::atomic_uint32_t*)&semaholder[nsema];
			*sema=1;
			nsema++;
		}

		void DumpPlayerBuffer()
		{
			//mark sema as null
			sema->store(0);

			//dump buffer
			free(players);
		}

		int AddPlayer(BTCaddress *btca)
		{
			expand:
				if(npb+1>PLbuffer)
				{
					if(!expanding.fetch_add(1))
					{
						while(npb!=nplayers); //wait out on assigns that already got through to complete before getting a new holder

						/***request memory from hBuffer movable mem. This data is live so don't allow it to be moved. However, the whole package is to expire quickly. 
						At that point, flag the semaphare to NULL to let the memory manager thread clean it up.
						***/
						BufferHeader *bh = hBuffer.GetBuffer(sizeof(Player)*PLstep, (unsigned int*)sema);
						memset(bh->offset, 0, sizeof(Player)*PLstep); //0 the new buffer

						//get a new holder
						Player **ptmp = (Player**)malloc(sizeof(Player*)*(PLbuffer +PLstep));

						//copy old buffer
						memcpy(ptmp, players, sizeof(Player*)*PLbuffer);

						//dump old buffer
						free(players);
						players = ptmp;

						//add new player
						for(int i=0; i<PLstep; i++)
							players[i +PLbuffer] = (Player*)bh->offset +i;

						//Update PLbuffer
						PLbuffer += PLstep;
					}

					while(expanding);

					goto expand; //go back to expand once it is to done to make sure the new expansion is enough to cover all the calls to AddPlayer
				}

			while(expanding); //wait on expanding routine before starting to allocate new indexes from the buffer
			int bufferid = npb.fetch_add(1); //grab an index
			players[bufferid]->btca = btca; //assign btca to player index

			nplayers.fetch_add(1); //update nplayers
			return bufferid;
		}

		int DumpPlayer(int i)
		{
			//move player from player list to finishedplayers list

			//update nplayers, use same locks as in AddPlayer

			return 1; //success
		}
};

class GameDB
{
	public:
		DB_ENV *env;
		DB *gamedb;
		char *path;
		unsigned int path_length;

		GameDB::GameDB()
		{
			env=0;
			gamedb=0;
	
			path_length = strlen(database_path);
			path = (char*)malloc(path_length +15);
			strcpy(path, database_path);
			path[path_length]='/';
			path[++path_length]=0;
		}

		GameDB::~GameDB()
		{
			if(gamedb) gamedb->close(gamedb, 0);
			if(env) env->close(env, 0);

			free(path);
		}

		int Setup(int dbid)
		{
			_itoa(dbid, path +path_length, 10);

			if(db_env_create(&env, 0)) return 0;

			if(env->open(env, path, DB_CREATE | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_RECOVER | DB_THREAD, 0)) return 0;

			if(db_create(&gamedb, env, 0)) return 0;

			DB_TXN *tx1;
			if(env->txn_begin(env, NULL, &tx1, 0)) return 0;
			
			if(gamedb->open(gamedb, tx1, "gamedb", 0, DB_BTREE, DB_CREATE | DB_THREAD, 0)) return 0;

			if(tx1->commit(tx1, 0)) return 0;
		
			return 1;
		}

		int Put(void *Hash160, DWORD *bet)
		{

			return 1;
		}

		void Destroy()
		{
			//delete files and dump folder
		}
};

class Game
{
	public:
		time_t StartTime, BetPeriod;
		int started, id, RoundAt;
		BTCaddress *btca;
		unsigned int startblock, nextblock;

		Rounds rnd;
		unsigned int i, y;

		GameDB gdb;

		void Reset()
		{
			started=0;
			id=-1;
			rnd.Reset();
			RoundAt=0;
		}

		void Set(DWORD tin, int iin)
		{
			Reset();
			StartTime = tin;
			SetID(iin);
			startblock=nextblock=0;
		}

		void SetID(DWORD iIn)
		{
			id = iIn;
			SetupDB();
		}

		void SetupDB()
		{
			gdb.Setup(id);
		}

		void Update(time_t *tin)
		{
			/***
				started value:
				0: pre game
				1: started
				-1: ended
			***/
			
			if(!started)
			{
				if(StartTime<*tin)
				{
					started=1;
					RoundAt=1;
				}
			}
			else if(started==1)
			{
				//compare tin to time if BetPeriod!=0
				if(BetPeriod)
				{
					if(*tin>BetPeriod)
					{
						//Betting period is over, wait on next block to get toss result
						BetPeriod=0;
						nextblock = LAST_BLOCK+1;

						//dump players that didn't bet, take a cut
						for(i=0; i<rnd.nplayers; i++)
						{
							if(rnd.players[i]->round<RoundAt)
							{
								//player didn't bet, send his profit away, put him in the dump list
								rnd.players[i]->CashOut();
								rnd.DumpPlayer(i);
							}
						}
					}
				}
				else if(nextblock>=LAST_BLOCK) //if not, compare nextblock to LAST_BLOCK
				{
					//coin has been tossed

					//sum up amount of losers

					//dump losers

					//compute reward per winners

					//set new round if any is left
				}
			}
			else if(started==-1) //game is over
			{
				/*** build mechanic to maintain and retrieve addresses eligible for rewards (needs to work with games loaded after server restart)
				check game left over balance and standing tx. Once balance is 0, dump the player list.***/
			}

			//update game balance
		}

		Game::Game()
		{
			btca = 0;
			started=0;
			id=-1;
			startblock = nextblock = 0;
			RoundAt=0;
		}

		BTCaddress *Release()
		{
			return btca;
		}

		void AddPlayer(BTCaddress *bin)
		{
			UpdatePlayerInDB(rnd.AddPlayer(bin));
		}
		
		void UpdatePlayerInDB(int pid)
		{
			gdb.Put(rnd.players[pid]->btca->hash160, &rnd.players[pid]->bets);
		}
};

class brkdb
{
	public:
		DB_ENV *env;
		DB *users, *names, *sid, *games;

		int Setup(char *envname, FILE *errfile)
		{
			if(db_env_create(&env, 0)) return 0;

			env->set_errfile(env, errfile); //set error file
			env->set_errpfx(env, "tabfcgi_db error");


			if(env->open(env, envname, DB_CREATE | DB_INIT_LOG | DB_INIT_LOCK | DB_INIT_MPOOL | DB_INIT_TXN | DB_RECOVER | DB_THREAD, 0)) return 0;

			if(db_create(&users, env, 0)) return 0;
			if(db_create(&names, env, 0)) return 0;
			if(db_create(&sid, env, 0)) return 0;
			if(db_create(&games, env, 0)) return 0;

			DB_TXN *tx1;
			if(env->txn_begin(env, NULL, &tx1, 0)) return 0;
			
			if(users->open(users, tx1, "users", 0, DB_BTREE, DB_CREATE | DB_THREAD, 0)) return 0;
			if(names->open(names, tx1, "names", 0, DB_BTREE, DB_CREATE | DB_THREAD, 0)) return 0;
			if(sid->open(sid, tx1, "sid", 0, DB_BTREE, DB_CREATE | DB_THREAD, 0)) return 0;
			if(games->open(games, tx1, "games", 0, DB_BTREE, DB_CREATE | DB_THREAD, 0)) return 0;

			if(tx1->commit(tx1, 0)) return 0;
			return 1;
		}

		void Close()
		{
			users->close(users, 0);
			names->close(names, 0);
			sid->close(sid, 0);
			games->close(games, 0);

			env->close(env, 0);
		}
};

brkdb *BRKDB = NULL;
class dbTxn
{
	public:
		static brkdb *db;
		int i, rt;

		DB_TXN *txnid;
		DBT key, data;
		DB *dbtmp;
		byte *dump;

		dbTxn::dbTxn()
		{
			dump = (byte*)malloc(100);
			txnid=0;
		}

		dbTxn::~dbTxn()
		{
			free(dump);
		}

		int PutInSIDpartial(void *dkey, unsigned int dkeyS, void *ddata, unsigned int ddataS, unsigned int offset)
		{
			dbtmp = db->sid;
			return Put(dkey, dkeyS, ddata, ddataS, offset);
		}

		int PutInSID(void *dkey, unsigned int dkeyS, void *ddata, unsigned int ddataS)
		{
			dbtmp = db->sid;
			return Put(dkey, dkeyS, ddata, ddataS, 0);
		}

		int PutInUsers(void *dkey, unsigned int dkeyS, void *ddata, unsigned int ddataS)
		{
			dbtmp = db->users;
			return Put(dkey, dkeyS, ddata, ddataS, 0);
		}

		int PutInNames(void *dkey, unsigned int dkeyS, void *ddata, unsigned int ddataS)
		{
			dbtmp = db->names;
			return Put(dkey, dkeyS, ddata, ddataS, 0);
		}

		int PutInGames(void *dkey, unsigned int dkeyS, void *ddata, unsigned int ddataS)
		{
			dbtmp = db->games;
			return Put(dkey, dkeyS, ddata, ddataS, 0);
		}

		int GetFromNames(void *dkey, unsigned int dkeyS, void *ddata, unsigned int ddataS, unsigned int offset)
		{
			dbtmp = db->names;
			if(!ddata) return Get(dkey, dkeyS, 0, 0, 0);
			else return Get(dkey, dkeyS, ddata, ddataS, offset);
		}

		int GetFromUsers(void *dkey, unsigned int dkeyS, void *ddata, unsigned int ddataS, unsigned int offset)
		{
			dbtmp = db->users;
			if(!ddata) return Get(dkey, dkeyS, 0, 0, 0);
			else return Get(dkey, dkeyS, ddata, ddataS, offset);
		}

		int GetFromSID(void *dkey, unsigned int dkeyS, void *ddata, unsigned int ddataS, unsigned int offset)
		{
			dbtmp = db->sid;
			return Get(dkey, dkeyS, ddata, ddataS, offset);
		}

		int GetFromGames(void *dkey, unsigned int dkeyS, void *ddata, unsigned int ddataS, unsigned int offset)
		{
			dbtmp = db->games;
			return Get(dkey, dkeyS, ddata, ddataS, offset);
		}

		int Put(void *dkey, unsigned int dkeyS, void *ddata, unsigned int ddataS, unsigned int offset)
		{
			if(!dbtmp) return -1; //wrong db index

			memset(&key, 0, sizeof(key));
			key.data = dkey;
			key.size = dkeyS;

			memset(&data, 0, sizeof(data));
			data.data = ddata;
			data.size = ddataS;

			if(offset)
			{
				data.doff = offset;
				data.dlen = ddataS;
				data.flags = DB_DBT_PARTIAL;
			}

			i=0;
			while(i<max_tries)
			{
				if(db->env->txn_begin(db->env, NULL, &txnid, 0)) return -2; //coudln't make tx
				rt = dbtmp->put(dbtmp, txnid, &key, &data, 0);

				switch(rt)
				{
					case 0: //success
					{
						if(txnid->commit(txnid, 0)) return -3; //couldn't commit
						return 0;
					}

					default: //everything else
					{
						if(txnid->abort(txnid)) return -4; //coudln't abort
					}
				}
				
				i++;
			}

			return -10; //exceeded max tries
		}

		int Get(void *dkey, unsigned int dkeyS, void *ddata, unsigned int ddataS, unsigned int offset)
		{
			//return data size. all errors have negative returns

			if(!dbtmp) return -1; //wrong db index

			memset(&key, 0, sizeof(key));
			key.data = dkey;
			key.size = dkeyS;

			memset(&data, 0, sizeof(data));
			data.flags = DB_DBT_USERMEM | DB_DBT_PARTIAL;
			
			if(ddata)
			{
				data.data = ddata;
				data.ulen = ddataS;
				data.doff = offset;
				data.dlen = ddataS;
			}
			else 
			{
				data.data = dump;
				data.ulen = 10;
				data.doff = 0;
				data.dlen = 0;
			}

			i=0;
			while(i<max_tries)
			{
				//if(db->env->txn_begin(db->env, NULL, &txnid, 0)) return -2; //coudln't make tx
				rt = dbtmp->get(dbtmp, 0, &key, &data, 0);

				switch(rt)
				{
					case 0:
					{
						//txnid->commit(txnid, 0);
						return data.size;
					}

					case DB_NOTFOUND:
					{
						//txnid->abort(txnid);
						return -20; //not found
					}

					case ENOMEM:
					{
						//txnid->abort(txnid);
						return -30;
					}
				}

				i++;
			}

			return -10; //max tries
		}

		int Del(void *dkey, unsigned int dkeyS)
		{
			memset(&key, 0, sizeof(key));
			key.data = dkey;
			key.size = dkeyS;

			i=0;
			while(i<max_tries)
			{
				if(db->env->txn_begin(db->env, NULL, &txnid, 0)) return -2; //coudln't make tx
				rt = dbtmp->del(dbtmp, txnid, &key, 0);

				switch(rt)
				{
					case 0: //success
					{
						if(txnid->commit(txnid, 0)) return -3; //couldn't commit
						return 0;
					}

					default: //everything else
					{
						if(txnid->abort(txnid)) return -4; //coudln't abort
					}
				}
					
				i++;
			}

			return -10; //exceeded max tries
		}
};

class GameHandler
{
	/*** Single threaded class. All operation are handled by a single thread performing the following task:
			1) update games list
			2) update games list output
			3) process dropable games
			4) tighten memory

		CONSIDER SPLITTING TESTNET AND REAL NET GAMES
	***/

	public:
	Game **games;
	Game **DropList;
	Game **Pending;
	unsigned int ngames, ndropped, dbuffersize, buffersize, i, s, m, rt, z, gcl;
	Game *tmpg;
	byte *dbCommand;
	dbTxn GAMESDB;
	time_t tme;
	char *gloutput, *ctmp, *abc, *GamesPass, *gamesCounter;

	BTCaddress **RecycleList;
	int nrecycled;
	BTCaddress *tbtca;
	
	static const int bufferstep = 100;
	static std::atomic_int gameIDcounter;

	GameHandler::GameHandler()
	{
		games=DropList=0;
		ngames=ndropped=0;
		buffersize=dbuffersize=0;

		Pending = (Game**)malloc(sizeof(Game)*max_games_per_list);

		dbCommand = (byte*)malloc(110);

		ctmp = (char*)malloc(50 +max_games_per_list*100);
		gloutput = (char*)malloc(50 +max_games_per_list*100);
		abc = (char*)malloc(20);
		GamesPass = (char*)malloc(20);
		strcpy(GamesPass, "dickbutt");
		
		gamesCounter = (char*)malloc(50);
		strcpy(gamesCounter, "gamesCounter");
		gcl = strlen(gamesCounter);

		/*if(GAMESDB.GetFromGames(gamesCounter, gcl, &gameIDcounter, 4, 0)==-20) //not found
		{
			gameIDcounter=0;
			GAMESDB.PutInGames(gamesCounter, gcl, &gameIDcounter, 4);
		}*/
	}

	GameHandler::~GameHandler()
	{
		for(i=0; i<buffersize; i++)
		{
			if(games[i]) delete games[i];
			if(RecycleList[i]) delete RecycleList[i];
		}
		free(games); free(RecycleList);

		for(i=0; i<dbuffersize; i++)
			if(DropList[i]) delete DropList[i];
		free(DropList);

		free(dbCommand);
		free(ctmp);
		free(abc);
		free(gloutput);
		free(GamesPass);
	}

	BTCaddress *GetBTCA()
	{
		/*** maintain a list of btca objects released by dead games, recycle them for new games.
			 fetch new btca from AH when recycle list is empty

			 Consider not recycling? (private key redudancy, DB removal)
		***/
		if(nrecycled)
		{
			tbtca = RecycleList[--nrecycled];
			RecycleList[--nrecycled] = 0;
		}
		else 
		{
			//get new btca
			tbtca = AH.New();

			//build it
			tbtca->CreateAccount(GamesPass);
		}

		return tbtca;
	}

	void Recycle(BTCaddress *bin)
	{
		RecycleList[nrecycled++] = bin;
	}

	void UpdateGames()
	{
		/*** all games need to be in chronological order in the games buffer ***/

		//build list of games based on tme and max_games_per_list. populate buffers as needed

		m = ngames;
		if(m<max_games_per_list)
		{
			tme /= 600;
			tme++; tme*=600;
			
			//add new games to buffer
			s = max_games_per_list - m;

			for(m=0; m<s; m++)
			{
				tmpg = New(-1);
				tmpg->StartTime = tme;
				tmpg->btca = GetBTCA();
				
				//commit new game to db
				CommitGame(tmpg);

				tme+=600;
			}
		}

		//update pending list
		memcpy(Pending, games +ngames -max_games_per_list, sizeof(Game*)*max_games_per_list);
	}

	void UpdateGamesList()
	{
		strcpy(ctmp, "<div id=\"GameList\" style=\"display:none\">");
		_itoa(max_games_per_list, abc, 10);
		strcat(ctmp, abc);
		strcat(ctmp, "</div>");

		strcat(ctmp, "<div id=\"GamesData\" style=\"display:none;\">");
			
		for(i=0; i<max_games_per_list; i++)
		{
			_itoa(Pending[i]->id, abc, 10);
			strcat(ctmp, abc);
			strcat(ctmp, "|");
				
			_itoa((unsigned int)Pending[i]->StartTime, abc, 10);
			strcat(ctmp, abc);
			strcat(ctmp, "|");
		}
			
		i = strlen(ctmp);
		ctmp[i-1]=0; //drop last |
		strcat(ctmp, "</div>");
		strcat(ctmp, "<script type=\"text/javascript\">"
					 "UpdateGameList();</script>");

		char *ptmp = gloutput;
		gloutput = ctmp;
		ctmp = ptmp;
	}

	int CommitGame(Game *gin)
	{
		/***
		GAME DATA STRUCTURE:
		key: gameID, 4 bytes uint
		data: private key, 32 bytes
			  public key,  65 bytes			  
			  startblock, 4 bytes uint
			  nextblock, 4 bytes uint
			  
			  starttime, 8 bytes time_t
			  earliestblock, 4 bytes uint
		***/

		memcpy(dbCommand, gin->btca->encrypted_pkey, 32);
		memcpy(dbCommand +32, gin->btca->publickey, 65);
		memcpy(dbCommand +97, &gin->startblock, 4);
		memcpy(dbCommand +101, &gin->nextblock, 4);
		memcpy(dbCommand +105, &gin->StartTime, 8);
		memcpy(dbCommand +113, &gin->btca->earliestblock, 4);

		return GAMESDB.PutInGames(&gin->id, 4, &dbCommand, 117);
	}

	Game *New(void *key, void *dbIn, unsigned int inS)
	{
		//request a new game pointer from New(0)
		Game *g = New((*(int*)key));
		g->SetupDB();
		g->btca = AH.New();
		
		//parse raw db data to fill the class members
		g->btca->SetEncryptedPrivateKey((byte*)dbIn, 32);
		g->btca->SetPubKey((byte*)dbIn +32, 65);
		
		memcpy(&g->btca->earliestblock, (byte*)dbIn +113, 4);
		memcpy(&g->StartTime, (byte*)dbIn +105, 8);
		memcpy(&g->startblock, (byte*)dbIn +97, 4);
		memcpy(&g->nextblock, (byte*)dbIn +101, 4);

		g->btca->Setup();

		g->started=-1;

		return g;
	}

	Game *New(int ID)
	{
		//return new game pointer
		if(ngames+1>buffersize) Extend();
		games[ngames] = new Game;
		if(ID<0) ID = gameIDcounter.fetch_add(1);
		games[ngames]->id = ID;
		
		return games[ngames++];
	}

	void Extend()
	{
		buffersize+=bufferstep;
		Game** Tmpg = (Game**)malloc(sizeof(Game*)*buffersize);
		BTCaddress **tmpbtca = (BTCaddress**)malloc(sizeof(BTCaddress*)*buffersize);

		memcpy(Tmpg, games, sizeof(Game*)*ngames);
		memcpy(tmpbtca, RecycleList, sizeof(BTCaddress*)*nrecycled);

		memset(Tmpg +ngames, 0, sizeof(Game*)*(buffersize -ngames));
		memset(tmpbtca +nrecycled, 0, sizeof(BTCaddress*)*(buffersize -nrecycled));
		
		free(games); free(RecycleList);
		games = Tmpg;
		RecycleList = tmpbtca;
	}

	void ExtendDropped()
	{
		dbuffersize+=bufferstep;
		Game** Tmpg = (Game**)malloc(sizeof(Game*)*dbuffersize);

		memcpy(Tmpg, DropList, sizeof(Game*)*ndropped);
		memset(Tmpg +ndropped, 0, sizeof(Game*)*(dbuffersize -ndropped));

		free(DropList);
		DropList = Tmpg;
	}

	void Update()
	{
		//routine called by the game_update thread

		
		//Updates games and check for expired ones
		time(&tme);

		for(i=0; i<ngames; i++)
		{
			games[i]->Update(&tme);

			if(games[i]->started==-1)
				AddToDropList(i), i--;
		}
		
		//update games list
		UpdateGames();

		//update games list output
		UpdateGamesList();

		//process droplist
		UpdateDropList();

		//tighten memory
		Compact();

		//Update gameIDcounter value in DB
		GAMESDB.PutInGames(gamesCounter, gcl, &gameIDcounter, 4);
	}

	void UpdateDropList()
	{
		//update all running games
		for(i=0; i<ndropped; i++)
		{
			DropList[i]->Update(&tme);

			//dump expired games with 0 balance
			if(!DropList[i]->btca->value)
			{
				//Drop game from the list entirely
			}
		}
	}

	void Compact()
	{
		for(i=0; i<ngames; i++)
		{
			if(!games[i])
			{
				memcpy(games +i, games +i+1, (ngames -i -2));
				ngames--;
			}
		}
	}

	void Order()
	{
		s=1;
		while(s)
		{	
			s=0;
			for(i=1; i<ngames; i++)
			{
				m = i-1;
				if(games[i]->StartTime<games[m]->StartTime)
				{
					tmpg = games[i];
					games[i] = games[m];
					games[m] = tmpg;

					s=1;
				}
			}
		}
	}

	int Drop(unsigned int index)
	{
		if(!games[i]) return -2; //null buffer
		
		int rt = GAMESDB.Del(&games[index]->id, 4);

		if(rt<0) return rt;

		/*memset(&key, 0, sizeof(key));
		key.data = &games[index]->StartTime;
		key.size = 4;

		m=0;
		while(m++<max_tries)
		{
			if(env->txn_begin(env, 0, &tID, 0)) return -1;

			rt = gdb->del(gdb, tID, &key, 0);

			switch(rt)
			{
				case 0:
				{
					tID->commit(tID, 0);
					goto dropfrombuffer;
				}

				case DB_NOTFOUND:
				{
					tID->abort(tID);
					goto dropfrombuffer;
				}

				default:
				{
					if(tID->abort(tID)) return -1;
				}
			}
		}

		return -1;*/

		Recycle(games[index]->Release());
		
		delete games[index];
		games[index] = 0;

		return 0;
	}

	void AddToDropList(int i)
	{
		//add to drop list
		if(ndropped+1>dbuffersize) ExtendDropped();
		DropList[ndropped++] = games[i];

		//drop from game list
		memcpy(games +i, games +i+1, sizeof(Game*)*(ngames -i -1));
		ngames--;
	}

	void DropEnded()
	{
		//Update(0) should have been called before calling this

		for(i=0; i<ngames; i++)
		{
			if(games[i])
			{
				if(games[i]->started==-1)
					AddToDropList(i), i--;
			}
		}

		Compact();
	}
};

std::atomic_int GameHandler::gameIDcounter;

GameHandler GH;

int dbInit()
{
	int rt; void *p;
	
	DBC *curs;
	
	DBT key, data;
	memset(&key, 0, sizeof(key));
	memset(&data, 0, sizeof(data));
	
	data.data = (unsigned int*)malloc(sizeof(int)*10*1024);
	data.ulen = sizeof(int)*10*1024;
	data.flags = DB_DBT_USERMEM;
	BTCaddress *btca=0;
	
	unsigned int dkeyS, ddataS;
	byte *dkey, *ddata;
	
	//load all public keys from names table
	DB *db = BRKDB->names;
	if(db->cursor(db, 0, &curs, 0)) goto dropinit;

	while(!(rt = curs->c_get(curs, &key, &data, DB_MULTIPLE_KEY | DB_NEXT)))
	{
		p=0;
		DB_MULTIPLE_INIT(p, &data);

		while(p)
		{
			DB_MULTIPLE_KEY_NEXT(p, &data, dkey, dkeyS, ddata, ddataS);

			if(p && ddataS==101)
			{
				btca = AH.New(); /***fix New so that it is thread safe***/
				btca->SetPubKey(ddata +32, 65);
				memcpy(&btca->earliestblock, ddata +97, 4);
				btca->Setup();
			}
		}
	}
	curs->close(curs);

	//Load up the list of games saved in the db for recovery purpose only. Drop all processed or otherwise virgin games from buffer and db.
	db = BRKDB->games;
	memset(&key, 0, sizeof(key));

	if(db->cursor(db, 0, &curs, 0)) goto dropinit;

	while(!(rt = curs->c_get(curs, &key, &data, DB_MULTIPLE_KEY | DB_NEXT)))
	{
		p=0;
		DB_MULTIPLE_INIT(p, &data);

		while(p)
		{
			DB_MULTIPLE_KEY_NEXT(p, &data, dkey, dkeyS, ddata, ddataS);

			//add data to games list
			if(p) GH.New(dkey, ddata, ddataS);
		}
	}
	curs->close(curs);

	GH.Order();
	GH.Update();
	//GH.DropEnded();

	/***Scan all addresses***/
	BC.ScanAddresses(AH.address, AH.naddress);
	
	//run the defrag thread right after
	defrag_thread((void*)&hBuffer);


	/***clean up***/
	free(data.data);

	return 1;

	dropinit:
		free(data.data);
		return 0;
}



brkdb *dbTxn::db = 0;

class dbHandle
{
	private:
		int status;
		CharExiter CEx;
		Sha256Round SHA;
		sID SIDtoken;
		int r, a, i, b, c, y, namesi;
		unsigned int swapa, me, lsalt;
		int tns;
		float f;
		char *Hash, *Hash2;
		char *dbCommand;
		unsigned int Tm, Tm2;
		TimeData TD;
		BitcoinKey BTCkey;
		Fortuna Frt;
		std::atomic_flag af;

		dbTxn db;

		int HashCmp(unsigned int *h1, unsigned int *h2)
		{
			int b=-1;
			unsigned int c=0;
			while(!c && ++b<8)
			{
				c = h1[b] ^ h2[b];
			}

			return c;
		}

	public:
		int NameIndex;
		
		dbHandle::dbHandle()
		{
			Hash = (char*)malloc(65);
			Hash2 = (char*)malloc(65);
			
			dbCommand = (char*)malloc(1024);
			NameIndex = -1;
			std::atomic_flag_clear(&af);
		}

		dbHandle::~dbHandle()
		{
			free(Hash); free(Hash2);
			free(dbCommand);
		}

		int ProcessLoginAttempt(char *log, char *pass, FCGX_Request *req)
		{
			a=r=0;
			if(log && pass)
			{
				me = strlen(log);
				if(CEx.ExitString(log))  r++, a |= 0x00000004;
				if(me>32)  r++, a |= 0x00000004;
				if(CEx.ExitString(pass)) r++, a |= 0x00000004;

				/*** consider forcing signing in to a single instance per account ***/
				if(!r)
				{
					//hash pass and log
					HashLognPass(log, pass);

					//Dump pass
					clear_char_volatile(pass);
					
					//check with db hash
					if(db.GetFromUsers(SHA.HashOP, 32, &lsalt, 4, 0)==4)
					{
						strcpy(Hash, log);
						memcpy(Hash +me, &lsalt, 4);
						SHA.Hash(Hash, -1, me+4);
					
						if(db.GetFromNames(SHA.HashOP, 32, dbCommand, 101, 0)==101)
						{
							BTCaddress *btca = AH.Find((byte*)dbCommand +32);
							if(!btca) //account isn't present in the list, build new one
							{
								btca = AH.New();
								btca->SetPubKey(dbCommand +32, 65);
								memcpy(&btca->refblock, dbCommand +97, 4);
								btca->Setup();
							}

							swapa = btca->Iindex;
							strcpy(btca->user, log);

							if(!Frt.GetRN(0)) return 0x00000008;
						
							//build new sid cookie
							BuildSID(req);

							//add times to it
							memcpy(Hash2 +32, &Tm, 4);
							Tm -=14400; Tm += 600;
							memcpy(Hash2 +36, &Tm, 4);
							memcpy(Hash2 +40, &swapa, 4);

							//save it all in a whole new sid table entry
							db.PutInSID(Hash, 32, Hash2, 44);

							//Get NameIndex
							NameIndex = swapa;

							//mark a as logged in
							a=1;
						}
						else a |= 0x00000004;
					}
					else a |= 0x00000004;

					/*** WIPE ALL DATA ***/
					//Wipe(0);

					clear_char_volatile(log);
				}
			}
			else a = 4;

			return a;
		}

		int KillAttempt(int rt, BTCaddress *bin)
		{
			/*** KILL POSSIBLY SAVED RECORDS IN DB DURING FAILED REG PROCESS ***/
			AH.Drop(bin);
			return rt;
		}

		int ProcessRegisterAttempt(char *log, char *pass, char *rpass, char *cpass, char *rcpass, FCGX_Request *req)
		{
			//exit the user input strings
			a=0; r=0;
			if(log && pass)
			{
				me = strlen(log);
				if(CEx.ExitString(log)) r++, a |= 0x00010000;
				if(me>32) r++, a |= 0x00010000;
				if(CEx.ExitString(pass))   r++, a |= 0x00020000;
				if(CEx.ExitString(rpass))  r++, a |= 0x00020000;
				if(CEx.ExitString(cpass))  r++, a |= 0x00080000;
				if(CEx.ExitString(rcpass)) r++, a |= 0x00080000;

				if(!r) //check pass and repeat are identical
				{
					if(strcmp(pass, rpass)) r++, a |= 0x00040000;
					if(strcmp(cpass, rcpass)) r++, a |= 0x0010000;
				}

				if(!r)
				{
					if(!strcmp(pass, cpass)) r++, a |= 0x00080000;
				}

				if(!r)
				{
					//names table
						if(!Frt.GetRN(0)) return 0x00800000;
						strcpy(Hash, log);
						lsalt = Frt.PRN[0];
						memcpy(Hash +me, &lsalt, 4);
					

						//check name doesn't exist
						SHA.Hash(Hash, -1, me+4);
						memcpy(Hash2, SHA.HashOP, 32);
						if(db.GetFromNames(Hash, 32, 0, 0, 0)!=-20) return 0x00010000;

						//check name+pass doesn't exist (in case name appears several times in db)
						HashLognPass(log, pass);
						clear_char_volatile(pass);
						clear_char_volatile(rpass);
						memcpy(Hash, SHA.HashOP, 32);
						if(db.GetFromUsers(SHA.HashOP, 32, 0, 0, 0)>=0) return 0x00010000;

					//get private key + build pubkey
						//encrpyt private key with AES(hash(hash(coin pass)))
						SHA.Hash(cpass, -1);
						SHA.Hash(SHA.HashOP, -1, 32);
						//Dump Password
						clear_char_volatile(cpass);		
						clear_char_volatile(rcpass);		
					
						BTCaddress *btca = AH.New();
						swapa = btca->Iindex;
						strcpy(btca->user, log);

						//creates private key and encrypts it with hash
						if(!btca->CreateAccount(SHA.HashOP)) return KillAttempt(0x00800000, btca);

					/*** RNG ***/
						if(!Frt.GetRN(0)) return 0x00800000;

					//create new user in db with login salt for id in name db
						//hashed log n pass
						if(db.PutInUsers(Hash, 32, &lsalt, 4)) return KillAttempt(0x00800000, btca);
				
					//save salted and hashed name with private and pub key
						memcpy(dbCommand, btca->encrypted_pkey, 32);
						memcpy(dbCommand +32, btca->publickey, 65);
						me = btca->GetEarliestBLock();
						memcpy(dbCommand +97, &me, 4);

						if(db.PutInNames(Hash2, 32, dbCommand, 101)) return KillAttempt(0x00800000, btca);
					
						//build SID
						BuildSID(req);

						//add time to command
						memcpy(Hash2 +32, &Tm, 4); //exp time
						Tm -=14400 +600;
						memcpy(Hash2 +36, &Tm, 4); //exp tick
						memcpy(Hash2 +40, &swapa, 4); //name list index

						if(db.PutInSID(Hash, 32, Hash2, 44)) return KillAttempt(0x00800000, btca);

						NameIndex = swapa;
					
						/*** wipe all sensitive data: btc class, all hashes, function arguments, sql request strings ***/
									
					//mark 'a' as registration completed
					a = 0x00200000;
				}
			}
			else a = 0x00020000;

			return a;
		}

		void BuildSID(FCGX_Request *req)
		{
			/*** RUN A PRNG PASS BEFORE CALLING THIS FUNCTION ***/
			//build SID
			SIDtoken.Build(Frt.PRN);

			//build hash2: hash token + ip + browser data
			strcpy(dbCommand, SIDtoken.token);
			strcat(dbCommand, FCGX_GetParam("REMOTE_ADDR", req->envp));
			strcat(dbCommand, FCGX_GetParam("HTTP_USER_AGENT", req->envp));
			SHA.Hash(dbCommand, -1);
			memcpy(Hash, SHA.HashOP, 32);
			
			//build entire token hash: hash token + ip + browser data + expire time
			//get current time, add 4h, return the value
			Tm = TD.GetTimeS() +14400;
			c = strlen(dbCommand);
			memcpy(dbCommand +c, &Tm, 4);
			
			SHA.Hash(dbCommand, -1, c+4);
			memcpy(Hash2, SHA.HashOP, 32);
		}

		char *GetCookie()
		{
			return SIDtoken.token;
		}

		void HashLognPass(char *log, char *pass)
		{
			SHA.Hash((unsigned char*)pass, 1);
			memcpy(Hash, SHA.HashOutput, 32);
			SHA.Hash((unsigned char*)log, 1);
			memcpy(Hash +32, SHA.HashOutput, 32);

			SHA.Hash((unsigned char*)Hash, -1, 64);
		}

		int VerifyCookie(char *cookie, FCGX_Request *req)
		{
			
			if(strlen(cookie)==32)
			{
				strcpy(dbCommand, cookie);
				strcat(dbCommand, FCGX_GetParam("REMOTE_ADDR", req->envp));
				strcat(dbCommand, FCGX_GetParam("HTTP_USER_AGENT", req->envp)); //verify that request_uri = client name
				SHA.Hash((unsigned char*)dbCommand, -1);

				//wipe data
				//clear_char_volatile(cookie);

				if(db.GetFromSID(SHA.HashOP, 32, Hash, 44, 0)==44)
				{
					BTCaddress *btca=NULL;
					memcpy(Hash2, SHA.HashOP, 32);
					Tm2 = TD.GetTimeS();
					
					memcpy(&Tm, Hash +36, 4);
					if(Tm2>Tm) goto killentry; //tick expired, kill entry, return bad cookie

					memcpy(&Tm, Hash +32, 4);
					if(Tm2>Tm) goto killentry; //token expired, kill entry, return bad cookie

					//check exp time is consistant
					b = strlen(dbCommand);
					memcpy(dbCommand +b, &Tm, 4);
					SHA.Hash(dbCommand, -1, b+4);

					if(HashCmp(SHA.HashOP, (unsigned int*)Hash)) goto killentry;
						
					//token is valid
						//get name index
						memcpy(&NameIndex, Hash +40, 4);
					
					//if name index is invalid, kill SID
					btca = AH.GetBTCA(NameIndex);
					if(!btca) goto killentry;

					//update exp tick
					Tm2+=600;
					if(db.PutInSIDpartial(Hash2, 32, &Tm2, 4, 36)) goto killentry;

					return 1;
					
					killentry:
						//delete sid table entry and drop BTCaddress from AH
						/*** check db.Del, seems to return success without deleting entry ***/
						db.Del(Hash2, 32);
						AH.Drop(btca);
				}
			}

			return 0;
		}
		
		int GetGames(DWORD time, Game *out, int max_games)
		{
			/*_itoa(time, Hash, 10);
			strcpy(dbCommand, "select * from games where starttime > ");
			strcat(dbCommand, Hash);
			_itoa(max_games, Hash, 10);
			strcat(dbCommand, " order by starttime asc limit ");
			strcat(dbCommand, Hash);

			res = PQexec(conn, dbCommand);
			if(PQresultStatus(res)==PGRES_TUPLES_OK)
			{
				r = PQntuples(res);

				for(i=0; i<r; i++)
				{
					//save games into ram
					dbResult = PQgetvalue(res, i, 1);
					Tm = atoi(dbResult);
					dbResult = PQgetvalue(res, i, 0);
					a = atoi(dbResult);
					out[i].Set(Tm, a);
				}

				return r;
			}*/

			return 0;
		}

		void AddGames(DWORD time)
		{
			//get next 10 minutes
			/*time /= 600;
			time++;
			time*=600;

			strcpy(dbCommand, "insert into games (StartTime) values ");
			for(i=0; i<20; i++)
			{
				_itoa(time, Hash, 10);
				strcat(dbCommand, "(");
				strcat(dbCommand, Hash);
				strcat(dbCommand, "),");
				time+=600;
			}

			r = strlen(dbCommand);
			dbCommand[r-1] = 0;
			PQexec(conn, dbCommand);*/
		}

		void Wipe(int in)
		{
			//if(in) BTCkey.Wipe();
			SHA.Wipe();	
			SIDtoken.Wipe();

			memset(dbCommand, 0xCD, 1024);
			memset(Hash, 0xCD, 65);

			r = a = i = b = c = y = 0;
			tns = 0;
			f = 0;
			Tm=Tm2=0;
		}
};