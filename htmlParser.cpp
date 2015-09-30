#define FILE_BUFFER_READ_SIZE 100

class TextNToken
{
	public:
		char *text;
		char *token;
		int length;

		TextNToken::TextNToken()
		{
			text=0;
			token=0;
			length=0;
		}

		TextNToken::~TextNToken()
		{
			if(text) free(text);
			if(token) free(token);
		}

		void Reset()
		{
			if(text) free(text);
			text=0;
			if(token) free(token);
			token=0;
			length=0;
		}

		void Set(char *in, int previous, int start, int end)
		{
			Reset();
			
			if(previous<start)
			{
				length = start -previous;
				text = (char*)malloc(length +1);
				memcpy(text, in +previous, start -previous);
				text[length] = 0;
			}

			if(start<end)
			{
				int r = end -start +1;
				token = (char*)malloc(r+1);
				memcpy(token, in +start, end-start +1);
				token[r] = 0;
			}
		}

		int IsToken(char* in)
		{
			if(token) return strcmp(token, in);
			return -1;
		}
};
class TokenizedString
{
	public:
		TextNToken *TTK;
		int nTTK, i, a;

		TokenizedString::TokenizedString()
		{
			TTK=0;
			nTTK=0;
		}

		TokenizedString::~TokenizedString()
		{
			Reset();
		}

		void Reset()
		{
			if(TTK) delete[] TTK;
			TTK=0;
			nTTK=0;
		}

		void Tokenize(char *in)
		{
			int r=0, m=0, start, end, previous=0;
			int l=strlen(in);
			char c;
			int *tokenIndex=0;
			int ntI=0;

			while(r<l)
			{
				c = in[r];

				if(!m)//looking for '$'
				{
					if(c=='$') 	start=r, m=1;
				}
				else if(m)//'$' found, looking for token's end
				{
					if(c==' ') end=r-1, m=2;
					else if(c=='<') end=r-1, m=2;
					else if(c==13) end=r-1, m=2;
					else if(c==0) end=r-1, m=2;
				}
				
				if(m==2)//token has been found, save and reset m
				{
					tokenIndex = (int*)realloc(tokenIndex, sizeof(int)*(ntI+1)*3);
					tokenIndex[ntI*3] = previous;
					tokenIndex[ntI*3 +1] = start;
					tokenIndex[ntI*3 +2] = end;

					previous = end+1;
					ntI++;
					m=0;
				}

				r++;
			}

			tokenIndex = (int*)realloc(tokenIndex, sizeof(int)*(ntI+1)*3);
			tokenIndex[ntI*3] = previous;
			tokenIndex[ntI*3 +1] = l;
			tokenIndex[ntI*3 +2] = l;

			ntI++;

			Reset();
			TTK = new TextNToken[ntI];
			for(r=0; r<ntI; r++)
				TTK[r].Set(in, tokenIndex[r*3], tokenIndex[r*3 +1], tokenIndex[r*3 +2]);

			nTTK = ntI;
			free(tokenIndex);
		}

		int IsToken(char *in)
		{
			for(i=0; i<nTTK; i++)
				if(!TTK[i].IsToken(in)) return i;
			
			return -1;
		}
};

class Module
{
	/*** MODULE SYSTEM IS SINGLE THREADED, CHANGE IT TO MULTI THREAD ***/
	protected:	
		char *data;
		char **names;
		int nnames;
		int i, y, a, b;

	public:
		static const int module_types = 5;
		static char  **controls_name;
		static const int nNames = 9;
		
		Module::Module()
		{
			data=NULL;
			names=NULL;
			nnames=0;
		}

		Module::~Module()
		{
 			if(data) free(data);
			if(names)
			{
				for(i=0; i<nnames; i++)
					free(names[i]);
				free(names);
			}
		}

		int IsModule(char *in)
		{
			for(i=0; i<nnames; i++)
			{
				if(!strcmp(in, names[i])) 
					return i;
			}

			return -1;
		}

		virtual int GetModule(void **param, char *out)
		{
			return -1;
		}

		static void InitModules();
		static void InitMainModule(Module **in);

		static void InitControlsName()
		{
			int i=0;
			for(i=0; i<nNames; i++)
				controls_name[i] = (char*)malloc(20);

			strcpy(controls_name[0], "l_Login"); //login text input control
			strcpy(controls_name[1], "l_Password"); //password text input control
			strcpy(controls_name[2], "l_Register"); //calling for registration form
			strcpy(controls_name[3], "r_Login"); //registration form login
			strcpy(controls_name[4], "r_Pass");
			strcpy(controls_name[5], "r_RPass");
			strcpy(controls_name[6], "r_CPass");
			strcpy(controls_name[7], "r_RCPass");
			strcpy(controls_name[8], "JS");
			//-250 means cookie
		}
};

class LoginModule : protected Module
{
	protected:
		static const int max_data_size = 350;

		int GetModule(void **param, char *out)
		{
			strcpy(out, "LOG=");
			a = (int)param[1];
			if(!a) 
			{
				strcat(out, st_data);
				return 0;
			}
			else if(a==1) //good login
			{
				strcat(out, "LOG|");
				BTCaddress *btca = (BTCaddress*)param[0];
				double VV; int sr;
				//add user name, account balance, in change, in transit, and deposit address, in that order
				btca->ComputeBalance();
				out+=8;
				
				//user name
				sr = strlen(btca->user);
				memcpy(out, btca->user, sr);
				out[sr] = '|';
				out+=sr+1;

				//balance
				VV = (double)btca->value / 100000000;
				_gcvt(VV, 6, out);
				sr = strlen(out);
				out[sr] = '|';
				out+=sr+1;

				//in change
				VV = (double)btca->inChange / 100000000;
				_gcvt(VV, 6, out);
				sr = strlen(out);
				out[sr] = '|';
				out+=sr+1;

				//in transit
				VV = (double)btca->inTransit / 100000000;
				_gcvt(VV, 6, out);
				sr = strlen(out);
				out[sr] = '|';
				out+=sr+1;

				//deposit address
				sr = strlen(btca->Base58);
				memcpy(out, btca->Base58, sr);
				out[sr] = '|';
				out+=sr+1;

				//update counter
				memcpy(out, "upc+", 4);
				_itoa(btca->updatecounter.load(), out +4, 10); //note: itoa puts a 0 at the end of the buffer

				return 1;
			}
			else if(a==2) // failed login
			{
				strcat(out, "ERR");

				return 1;
			}
			else if(a==3) //register title
			{
				strcat(out, "REG");

				return 1;
			}

			return -1;
		}

	public:
		static char *st_data;
		static void InitModule()
		{
			strcpy(st_data, "NULL");
		}
		
		LoginModule::LoginModule()
		{
			nnames=4;
			names = (char**)malloc(sizeof(char*)*nnames);
			for(i=0; i<nnames; i++)
				names[i] = (char*)malloc(30);

			strcpy(names[0], "Login_Log");
			strcpy(names[1], "Login_Success");
			strcpy(names[2], "Login_Fail");
			strcpy(names[3], "Login_Reg");

			//data = (char*)malloc(max_data_size);
		}
};

class WelcomeModule : protected Module
{
	protected:
		static const int max_data_size = 100;
	
	public:
		static char *st_data;
		static void InitModule()
		{
			strcpy(st_data, "MAIN=WELC");
		}

		int GetModule(void **param, char *out)
		{
			strcat(out, st_data);
			return 0;
		}

		WelcomeModule::WelcomeModule()
		{
			nnames = 1;
			names = (char**)malloc(sizeof(char*)*nnames);
			names[0] = (char*)malloc(30);
			
			strcpy(names[0], "Welcome");
		}
};

class RegisterModule : protected Module
{
	protected:
		static const int max_data_size = 600;

	/***unused module***/

	public:
		static char *st_data;
		static void InitModule()
		{
			strcpy(st_data, "<h1>Register</h1><br><br>");
			strcat(st_data, "<form action=\"index.php\" method=\"post\">");
			strcat(st_data, "Login:<br><input type=\"text\" name=\"");
			strcat(st_data, controls_name[3]);
			strcat(st_data, "\"><br>");
			strcat(st_data, "Password:<br><input type=\"password\" name=\"");
			strcat(st_data, controls_name[4]);
			strcat(st_data, "\"><br>");
			strcat(st_data, "Repeat Password:<br><input type=\"password\" name=\"");
			strcat(st_data, controls_name[5]);
			strcat(st_data, "\"><br>");
			strcat(st_data, "Coins Password:<br><input type=\"password\" name=\"");
			strcat(st_data, controls_name[6]);
			strcat(st_data, "\"><br>");
			strcat(st_data, "Repeat Coins Password:<br><inpu type=\"password\" name=\"");
			strcat(st_data, controls_name[7]);
			strcat(st_data, "\"><br>");
			strcat(st_data, "<input type=\"submit\" value=\"Submit\">");
			strcat(st_data, "</form>");
		}

		int GetModule(void **param, char *out)
		{
			strcpy(out, st_data);
			return 0;
		}

		RegisterModule::RegisterModule()
		{
			nnames = 1;
			names = (char**)malloc(sizeof(char*)*nnames);
			names[0] = (char*)malloc(30);
			
			strcpy(names[0], "Register");
		}
};

class GameMainModule : protected Module
{
	protected:
		void **vtmp;
		Game *gtmp;

	public:
		void Init()
		{
			nnames=1; 
			names = (char**)malloc(sizeof(char*)*nnames);
			for(i=0; i<nnames; i++)
				names[i] = (char*)malloc(30);

			strcpy(names[0], "GameMain");	
		}

		int GetModule(void **param, char *out)
		{
			vtmp = (void**)param[0];
			gtmp = (Game*)vtmp[0];

			strcpy(out, "MAIN=");

			if(gtmp)
			{
				strcat(out, "<h1>Game #");
				a = strlen(out);
				_itoa(gtmp->id, out +a, 10);
				strcat(out, "</h1><br><br>"
					"<h2><div style=\"display:none\" id=\"GameClock\">");
				a = strlen(out);
				_itoa((int)gtmp->StartTime, out +a, 10);
				strcat(out, "</div><div style=\"float:left; white-space:pre;\" id=\"CountDown\"></div></h2>");

				return 1;
			}
			return 0;
		}
};

class GameListModule : protected Module
{
	protected:
		char *moddata;
		char *ctmp, *abc, *ctmp2;
		dbHandle db;
		DWORD timea, timeb;
		TimeData TD;
		tm *timer;

		Game **gameslist;

		static const int max_gl_size = max_games_per_list*200;

	public:
		GameListModule::GameListModule()
		{
			moddata = (char*)malloc(max_gl_size);
			ctmp    = (char*)malloc(max_gl_size);
			abc     = (char*)malloc(50);

			nnames=1;
			names = (char**)malloc(sizeof(char*)*nnames);
			for(i=0; i<nnames; i++)
				names[i] = (char*)malloc(30);

			strcpy(names[0], "Games List");
			 gameslist = (Game**)malloc(sizeof(Game*)*max_games_per_list);
		}

		GameListModule::~GameListModule()
		{
			free(moddata);
			free(ctmp);
			free(abc);
		}

		void UpdateGameList()
		{
			/*** process all this within the game handler class 
			***/
			timea = TD.GetTimeS();

			//GH.getGames(gameslist, timea);

			
			//4) build html

			
			i = strlen(ctmp);

			ctmp2 = moddata;
			moddata = ctmp;
			ctmp = ctmp2;
		}

		int GetModule(void **param, char *out)
		{
			//strcpy(out, moddata);
			out=0;
			return 1;
		}
};

char* LoginModule::st_data = (char*)malloc(LoginModule::max_data_size);
char* WelcomeModule::st_data = (char*)malloc(WelcomeModule::max_data_size);
char* RegisterModule::st_data = (char*)malloc(RegisterModule::max_data_size);
char **Module::controls_name = (char**)malloc(sizeof(char*)*Module::nNames);
GameListModule GLM;

void Module::InitModules()
{
	LoginModule::InitModule();
	RegisterModule::InitModule();
	WelcomeModule::InitModule();
}

/*void Module::InitMainModule(Module **in)
{
	in[0] = (Module*)new LoginModule;
	in[1] = (Module*)new RegisterModule;
	in[2] = (Module*)new WelcomeModule;
	in[3] = (Module*)&GLM;
	GameMainModule* gmm = new GameMainModule;
	gmm->Init();
	in[4] = (Module*)gmm;
	//in[5] = (Module*)new CookieModule;
}*/

class htmlLoader
{
	public:
		TokenizedString TKS;
		int a, b;

		htmlLoader::htmlLoader()
		{
		}

		htmlLoader::~htmlLoader()
		{
		}

		int LoadHTML(char *in)
		{
			char *fileContent=0;
			int nfC=0;

			//load html page in fileContent
			FILE *f = fopen(in, "rb");
			while(1)
			{
				fileContent = (char*)realloc(fileContent, FILE_BUFFER_READ_SIZE +nfC);
				memset(fileContent +nfC, 0, FILE_BUFFER_READ_SIZE);
				a = fread(fileContent +nfC, FILE_BUFFER_READ_SIZE, 1, f);

				if(a!=1)
				{
					b=feof(f);
					if(b) 
						break;
				}

				nfC+=FILE_BUFFER_READ_SIZE;
			}
			fclose(f);

			//parse for tokens (anything starting with $)
			
			TKS.Tokenize(fileContent);
			free(fileContent);

			return 0;
		}
};

class htmlRenderer
{
	private:
		static const int max_modules = 20;
		static const int max_headers = 10;
		static const int max_module_size = 1000;
		static const int max_header_size = 200;
		int i, a, n, r, JS, noJS;
		
		htmlLoader *hL;
		FCGX_Request *Request;
		
		char** modules_output;
		char** headers_output;
		int nmodules, nheaders;

		char** tmp_modules;
		int ntmp;

		int* modules_index;
		void **module_params;
		GameMainModule GMM;
		Module **localmodules;

		char *cookie;
		int set_cookie;
	
	public:
		//static Module **modules;
		static char *CGIHeader;

		static void InitModules()
		{
			//Module::InitMainModule(modules);
			strcpy(CGIHeader, "Content-type: text/html; charset=UTF-8\r\n");
		}

		htmlRenderer::htmlRenderer()
		{
			modules_output = (char**)malloc(sizeof(char*)*max_modules);
			headers_output = (char**)malloc(sizeof(char*)*max_headers);

			tmp_modules = (char**)malloc(sizeof(char*)*max_modules);
			for(i=0; i<max_modules; i++)
				tmp_modules[i] = (char*)malloc(max_module_size+1);

			for(i=0; i<max_headers; i++)
				headers_output[i] = (char*)malloc(max_header_size);

			modules_index = (int*)malloc(sizeof(int)*max_modules);
			module_params = (void**)malloc(sizeof(void*)*max_modules);
			hL=0;

			/*** thread access on static members behavior? ***/
			localmodules = (Module**)malloc(sizeof(Module*)*Module::module_types);
			//memcpy(localmodules, modules, sizeof(Module*)*Module::module_types);
			localmodules[0] = (Module*)new LoginModule;
			localmodules[1] = (Module*)new RegisterModule;
			localmodules[2] = (Module*)new WelcomeModule;
			localmodules[3] = (Module*)&GLM;
			localmodules[4] = (Module*)&GMM; //game main module are local for each renderer class instance

			Request=0;
			cookie = (char*)malloc(100);
			ResetPage();
		}

		htmlRenderer::~htmlRenderer()
		{
			CleanUpModules();
			free(modules_output);
			for(i=0; i<max_modules; i++)
				free(tmp_modules[i]);
			free(tmp_modules);
			free(modules_index);
			free(module_params);
			for(i=0; i<max_headers; i++)
				free(headers_output[i]);
			free(headers_output);
			hL=0;
		}

		void CleanUpModules()
		{
			for(i=0; i<max_modules; i++)
			{
				modules_output[i] = 0;
				tmp_modules[i][0] = 0;
			}
			for(i=0; i<max_headers; i++)
				headers_output[i][0] = 0;
			nmodules=0;
			nheaders=0;
			ntmp=0;
		}

		void LinkHtml(htmlLoader *in, FCGX_Request *request)
		{
			hL=in;
			Request = request;
		}

		void Set(char *token, char *module, void* params)
		{
			if(strcmp(token, "$header"))
			{
				a = hL->TKS.IsToken(token);
			
				if(a>-1)
				{
					/*** shouldn't use static member for per-thread function call ***/
					modules_index[ntmp] = a;
				
					for(i=0; i<Module::module_types; i++)
					{
						r = localmodules[i]->IsModule(module);
						if(r>-1)
						{
							module_params[0] = params;
							module_params[1] = (void*)r;
							localmodules[i]->GetModule(module_params, tmp_modules[ntmp]);

							break;
						}
					}

					ntmp++;
				}
			}
			else
			{
				SetHeader(module, params);
			}

			/*if(!strcmp(token, "$cookie"))
			{
				r = localmodules[5]->IsModule(module);
				module_params[0] = params;
				module_params[1] = (void*)r;
				localmodules[5]->GetModule(module_params, cookie);
				set_cookie = 1;
			}*/
		}

		void ResetPage()
		{
			CleanUpModules();
			set_cookie=0;
			JS=noJS=0;
		}

		char *SetHeader(char *mod, void *params)
		{
			if(!strcmp(mod, "set_cookie"))
			{
				strcpy(headers_output[nheaders], "Set-Cookie: sID=");
				strcat(headers_output[nheaders], (char*)params);
				strcat(headers_output[nheaders], "\n");
			}
			else if(!strcmp(mod, "kill_cookie"))
				strcpy(headers_output[nheaders], "Set-Cookie: sID=0;\n");

			return headers_output[nheaders++];
		}

		void BuildUpModules()
		{
			/*run through all existing text'n'tokens in order, copy text from ttk, then module from tmp_modules 
			the specific token was flagged in the list. save it all in modules_output*/

			if(!noJS) a = hL->TKS.IsToken("$PanelsData");

			//load text and modules
			nmodules=0;
			for(i=0; i<hL->TKS.nTTK; i++)
			{
				//add text
				if(!JS)
				{
					if(hL->TKS.TTK[i].text)
					{
						modules_output[nmodules] = hL->TKS.TTK[i].text;
						nmodules++;
					}
				}
				
				if(!noJS && a==i)
				{
					for(r=0; r<ntmp; r++)
					{
						strcat(tmp_modules[r], "||");
						modules_output[nmodules] = tmp_modules[r];
						nmodules++;
						/*if(modules_index[r]==i)
						{
							//a module was loaded for this token, pass it to the output buffer
							if(tmp_modules[r])
							{
							}
							break;
						}*/
					}
				}
				else if(noJS)
				{
					//interpret modules
				}
			}
		}

		void Output()
		{
			BuildUpModules();
			
			FCGX_FPrintF(Request->out, CGIHeader); //cgi header
			for(i=0; i<nheaders; i++)
				FCGX_FPrintF(Request->out, headers_output[i]); 

			FCGX_FPrintF(Request->out, "\n"); //http header terminator


			for(i=0; i<nmodules; i++)
			{
				FCGX_FPrintF(Request->out, modules_output[i]);
			}

			FCGX_Finish_r(Request);
			ResetPage();
		}

		void SetJS()
		{
			JS=1;
		}
};

//Module **htmlRenderer::modules = (Module**)malloc(sizeof(Module)*Module::module_types);
char *htmlRenderer::CGIHeader = (char*)malloc(50);

class RequestForm
{
	private:
		int i, y, n, l, s;

	public:
		int *flags;
		void **flagdata;
		int nflags;

		//static const int MAX_INPUT_BYTES = 128; //max char per user input strings
		static const int N_REQUESTS = 20; //total amount of request types

		RequestForm::RequestForm()
		{
			flags = (int*)malloc(sizeof(int)*N_REQUESTS*2); //index n*2 is the request index, n*2 +1 is whether the value is valid (1) or not (0)
			flagdata = (void**)malloc(sizeof(void*)*N_REQUESTS);
			nflags=0;
		}

		RequestForm::~RequestForm()
		{
			if(flags) free(flags);
			free(flagdata);
		}

		void ResetFlags()
		{
			nflags=0;
		}

		void SetCookieFlag()
		{
			flags[nflags*2] = -250;
			nflags++;
		}

		void ParseRequest(char **in, int nin)
		{
			for(i=0; i<nin; i++)
			{
				//separate key and value
				l = strlen(in[i]);
				n=0;
				while(n<l)
				{
					if(in[i][n]=='=') break;
					n++;
				}

				in[i][n]=0;


				//test key
				for(y=0; y<Module::nNames; y++)
				{
					if(!strcmp(in[i], Module::controls_name[y]))
					{
						flags[nflags*2] = y;
						if(strlen(in[i] +n+1))
						{
							flags[nflags*2 +1] = 1;
							flagdata[nflags] = in[i] +n+1;
						}
						else flags[nflags*2 +1] = 0, flagdata[nflags] = 0;

						nflags++;
						break;
					}
				}
			}
		}
};

class htmlRequest
{
	protected:
		char *content;
		char *content_length;
		char *cookie;
		char *tmpc;
		char **client_requests;
		htmlRenderer hO;
		int a, b, c, d, e, i, y, st, ed, n, r, l;
		FCGX_Request *request;
		dbHandle db;
		RequestForm RF;
		void **tmpv;
		char *name;
		
		static const int MAX_CONTENT_LENGTH = 1000;
		static const int MAX_REQUESTS = 20;
		static const int MAX_REQUEST_LENGTH = 100;

		/*All status_ flags but Status_Header share the same last bit mechanics: a high bit means the flag should be parsed and processed. A low bit means the flag should be ignored, 
		default output included. By default all last bits are high, effectively producing data for all panels. The bits main use is to be toggled to allow for JS only responses
		to update only the data required by the client background pull. This also allows for JS flagged to be processed in any given order while still maintaining its rendering 
		priority over non client side scripted pulls.
		Status_Header has no default output. Its potential output is also unrelated to the nature of the client pull. As such it does not require this mechanic*/

		int Status_Login;
		/* Bit0: Hi=logged in
		   Bit1: Hi=log in attempt
		   Bit2: Hi=Incorrect log/pass 
		   Bit3: Hi=Error
		   Bit4: Hi=Kill Cookie
		   Bit30: Hi="Register"
		*/
		int Status_MainPanel; //main panel state
		/*  Bit0: Hi=conflict/render welcome panel
			Bit1: Hi=Register Panel
				Bit16: Hi=invalid/unavailable user name
				Bit17: Hi=Bad Password
				Bit18: Hi=Password and Repeat don't match
				Bit19: Hi=Bad coin password or no coin password
				Bit20: Hi=Coin password and repeat don't match
				Bit21: Hi=All good, save the new user
				Bit22: Hi=Just Render Empty Panel
				Bit23: Hi=Error

			Bit2: Hi=Account Management Panel
			Bit3: Hi=Current Bet
		*/

		int Status_Header;
		/*Bit0: Set Cookie
		  Bit1: Kill Cookie
		*/

		int Status_Games;

	public:
		htmlRequest::htmlRequest()
		{
			content = (char*)malloc(MAX_CONTENT_LENGTH+1);
			content_length=0;
			request=0;
			client_requests = (char**)malloc(sizeof(char*)*MAX_REQUESTS);
			for(i=0; i<MAX_REQUESTS; i++)
				client_requests[i] = (char*)malloc(MAX_REQUEST_LENGTH);

			tmpv = (void**)malloc(sizeof(void*)*10);
			name = 0;
		}

		htmlRequest::~htmlRequest()
		{
			if(content) free(content);
			if(client_requests)
			{
				for(i=0; i<MAX_REQUESTS; i++)
				{
					if(client_requests[i]) free(client_requests[i]);
				}
				free(client_requests);
			}
			free(tmpv);
			free(name);
		}

		void Setup(FCGX_Request *in, htmlLoader *in2)
		{
			request = in;
			hO.LinkHtml(in2, in);
		}

		int GetContent()
		{
			ed = 0;
			//check for cookie
			cookie=0;
			cookie = FCGX_GetParam("HTTP_COOKIE", request->envp);
			if(cookie)
			{
				//parse to sID
				n=0; a = strlen(cookie)-2;
				while(n<a)
				{
					if(cookie[n]=='s')
						if(cookie[n+1]=='I')
							if(cookie[n+2]=='D') break;

					n++;
					if(n>=a) break;
				}
				n+=4;

				if(n<a)
				{
					//parse to white space or end of string
					r=n; a+=2; tmpc = cookie +n;
					while(r<a)
					{
						if(cookie[r]==' ' || cookie[r]==0) break;
						
						r++;
					}

					tmpc[r-n] = 0;

					RF.SetCookieFlag();
				}
			}

			content_length = FCGX_GetParam("CONTENT_LENGTH", request->envp);
			a = atoi(content_length);
			if(a)
			{
				if(a>MAX_CONTENT_LENGTH) a = MAX_CONTENT_LENGTH;
				int rm = FCGX_GetStr(content, a, request->in);
				content[a]=0;
				/*frq = fopen("frq.txt", "wb");
				fwrite(content, a, 1, frq);
				fclose(frq);*/

				//parse content for key/values pairs
				st=n=0;
				while(n<a)
				{
					if(content[n]=='&')
					{
						if(n-st<MAX_REQUEST_LENGTH) 
						{
							memcpy(client_requests[ed], content +st, n-st);
							client_requests[ed][n-st] = 0;
							ed++;
						}
						st = n+1;
					}
					n++;
				}
					
				if(n-st<MAX_REQUEST_LENGTH) 
				{
					memcpy(client_requests[ed], content +st, n-st);
					client_requests[ed][n-st] = 0;
					ed++;
				}

				return ed;
			}
			return -1;
		}

		void ProcessRequest()
		{
			RF.ResetFlags();
			db.NameIndex = -1;
			a = GetContent();
			if(a>-1) RF.ParseRequest(client_requests, a);

			Status_Login=0x80000000;
			Status_MainPanel=0x80000000;
			Status_Header=0;
			Status_Games=0x80000000;
			
			//go through requests, process as it comes
			for(i=0; i<RF.nflags; i++)
			{
				a = RF.flags[i*2];

				switch(a)
				{
					case -250: //cookie found
					{
						//cookies should always be processed before content, as such cookies should always be the first flag when available.
						//pass to db for verification
						b = db.VerifyCookie(tmpc, request);
						if(b)
						{
							//Status_Login |= b;
							name = AH.GetName(db.NameIndex);
						}
						else Status_Header |= 0x00000002;
					}
					break;

					case 0: //login request found
					{
						Status_Login |= 0x00000002;
						tmpv[0] = RF.flagdata[i];
						RF.flags[a*2]=-1;
						tmpv[1] = 0;
						
						//find matching password
						for(y=0; y<RF.nflags; y++)
						{
							if(RF.flags[y*2]==1)
							{
								tmpv[1] = RF.flagdata[y];
								RF.flags[y*2]=-1;
								break;
							}
						}

						//send pass and log to dbhandler
						a = db.ProcessLoginAttempt((char*)tmpv[0], (char*)tmpv[1], request);
						Status_Login |= a;
						if(a==1) Status_Header |= 0x00000001;
					}
					break;

					case 2: //summon Register panel on Main panel
					{
						Status_MainPanel |= 0x00400002;
						
						//mark log panel as register title
						Status_Login |= 0x40000000;
					}
					break;

					case 3: //r_login. process everything at db lvl
					{
						//look for password and rpassword
						tmpv[0]=RF.flagdata[i];
						RF.flags[i*2]=-1;
						tmpv[1]=tmpv[2]=tmpv[3]=tmpv[4]=0;
						for(y=0; y<RF.nflags; y++)
						{
							if(RF.flags[y*2]==4) tmpv[1]=(void*)RF.flagdata[y], RF.flags[y*2]=-1;
							else if(RF.flags[y*2]==5) tmpv[2]=(void*)RF.flagdata[y], RF.flags[y*2]=-1;
							else if(RF.flags[y*2]==6) tmpv[3]=(void*)RF.flagdata[y], RF.flags[y*2]=-1;
							else if(RF.flags[y*2]==7) tmpv[4]=(void*)RF.flagdata[y], RF.flags[y*2]=-1;
						}

						Status_MainPanel |= db.ProcessRegisterAttempt((char*)tmpv[0], (char*)tmpv[1], (char*)tmpv[2], (char*)tmpv[3], (char*)tmpv[4], request);
						if(Status_MainPanel==0x00200000) 
							{
								Status_Login |= 0x00000001;
								Status_Header |= 1;
							}
					}
					break;

					case 8: //js request
					{
						ProcessJS(i);
					}
					break;
				}
			}

			//prune requests that are found with their dependencies. do a second run to find remaining dependencies

			//process the status
				//headers
				if(Status_Header & 0x00000001) hO.Set("$header", "set_cookie", (void*)db.GetCookie());
				else if(Status_Header & 0x00000002) hO.Set("$header", "kill_cookie", 0);

				//login status
				if(Status_Login & 0x80000000)
				{
					if(!(Status_Login & 0x00000001)) hO.Set("$login", "Login_Log", 0);
					else 
					{
						if(Status_Login & 0x40000000) hO.Set("$login", "Login_Reg", 0);
						else if(Status_Login & 0x00000004) hO.Set("$login", "Login_Fail", 0);
						else if(Status_Login & 0x00000001) 
						{
							/*** id through token ***/
							hO.Set("$login", "Login_Success", (void*)AH.GetAddress(db.NameIndex)); //change params to pass user name
						}
						else if(Status_MainPanel & 0x00200000)
						{
							/*** display loged in panel ***/
						}
					}
				}


				//main panel status
				if(Status_MainPanel & 0x80000000)
				{
					a = Status_MainPanel & 0x7FFFFFFE;
					b = Status_MainPanel & 0x00000001;
					if(b || !a) hO.Set("$space1", "Welcome", 0); //plain welcome page
					else
					{
						if(Status_MainPanel & 0x00200000) hO.Set("$space1", "Welcome", 0); //registration completed, show welcome page (for now)
						if(Status_MainPanel & 0x00000002) hO.Set("$space1", "Register", (void*)Status_MainPanel); //register panel
					}
				}

				//if(Status_Games & 0x80000000) hO.Set("$space2", "Games List", 0); //games list panel

				//Output
				hO.Output();

				//wipe client_requests
		}

		void ProcessJS(int iin)
		{
			//process JS requests here
			hO.SetJS();

			//turn down all flags' last bit
			Status_Login &= 0x7FFFFFFF;
			Status_MainPanel &= 0x7FFFFFFF;
			Status_Games &= 0x7FFFFFFF;

			tmpc=(char*)RF.flagdata[iin];
			
			/*JS request comes as a pack of numbers separated by |, with the following meaning, based on position
				#0: login panel update status
				#1: game panel update status
				#2: main panel update status

				When the update status returned by the client differs from the current status at server level, updated data should be returned. 
				Otherwise no data should be returned for the relevant panel.

				0 means no update required
			*/

			//cycle through |, indentify panel status and add relevant flags to RF, then return to regular request processing
			a=y=st=0;
			b = strlen(tmpc);
			while(y<b)
			{
				if(tmpc[y]=='|')
				{
					tmpv[a] = tmpc +ed;
					tmpc[y] = 0;
					a++;
					ed = y+1;
				}

				y++;
			}


			for(y=0; y<a; y++)
			{
				st = atoi((char*)tmpv[y]);
				if(st)
				{
					//mimic token in RF.flags
					if(!y) //login panel
					{
						if(db.NameIndex>-1)
						{
							//compare user data update counter to client value
							BTCaddress *btca = AH.GetBTCA(db.NameIndex);
							
							if(btca->updatecounter!=st) 
								Status_Login |= 0x80000001; //set last and first bit on Status_Login
						}
					}
					else if(y==1) //games list
					{

					}
					else if(y==2) //main panel
					{
						//compare game list update counter to client value

						//set last bit on Status_Games
						Status_Games |= 0x80000000;
					}
				}
			}

			if(Status_Header && 0x00000002) Status_Login = 0x8000000; //reset login on panel on dead cookie
		}
};