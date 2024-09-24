#include <stdio.h>
#include "keys_core.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"
#include <thread>

keys_core g_keys_core;
PLUGIN_EXPOSE(keys_core, g_keys_core);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IPlayersApi* g_pPlayers;

IMySQLClient *g_pMysqlClient;
IMySQLConnection* g_pConnection;

KeysApi* g_pKeysApi = nullptr;
IKeysApi* g_pKeysCore = nullptr;

int g_iServerID = -1;
bool g_bIsStarted;
bool g_bIsBlocked[64];
int g_iAttempts[64];

int g_iKeyLength;
int g_iAttempt;
int g_iBlockTime;
const char* g_sKeyTemplate;

std::map<std::string, KeyUseCallback> g_vecKeyUseCallbacks;
std::vector<OnCoreIsReady> g_vecOnCoreIsReady;

std::map<std::string, std::string> g_vecPhrases;

SH_DECL_HOOK0_void(IServerGameDLL, GameServerSteamAPIActivated, SH_NOATTRIB, 0);

CGameEntitySystem* GameEntitySystem()
{
	return g_pUtils->GetCGameEntitySystem();
}

void StartupServer()
{
	g_pGameEntitySystem = GameEntitySystem();
	g_pEntitySystem = g_pUtils->GetCEntitySystem();
	gpGlobals = g_pUtils->GetCGlobalVars();
}

void PrintToChat(int iSlot, const char *msg, ...)
{
	va_list args;
	va_start(args, msg);

	char buf[512];
	V_vsnprintf(buf, sizeof(buf), msg, args);
	va_end(args);
	
	g_pUtils->PrintToChat(iSlot, "%s %s", g_vecPhrases[std::string("Prefix")].c_str(), buf);
}

// static const int g_iNumbers[] = {0x30, 0x39};
// static const int g_iLettersUpper[] = {0x41, 0x5A};
// static const int g_iLettersLower[] = {0x61, 0x7A};


// int GenerateRandomInt(int iMin, int iMax)
// {
// 	return iMin + (rand() % (iMax - iMin + 1));
// }

// int RoundToCeil(float fValue)
// {
// 	return (fValue - int(fValue) > 0) ? int(fValue) + 1 : int(fValue);
// }

// int GetRandomInt(int iMin, int iMax)
// {
// 	int iRandom = GenerateRandomInt(1, 2147483647);
// 	if (iRandom == 0)
// 	{
// 		++iRandom;
// 	}

// 	return RoundToCeil(float(iRandom) / (float(2147483647) / float(iMax - iMin + 1))) + iMin - 1;
// }

// int GetCharTemplate(int iChar)
// {
// 	switch (iChar)
// 	{
// 	case 0x41:
// 		return GenerateRandomInt(1, 20) > 10 ? GetRandomInt(g_iLettersUpper[0], g_iLettersUpper[1]) : GetRandomInt(g_iLettersLower[0], g_iLettersLower[1]);
// 		break;
// 	case 0x42:
// 		return GetRandomInt(g_iNumbers[0], g_iNumbers[1]);
// 		break;
// 	case 0x58:
// 		return GenerateRandomInt(0, 2) == 1 ? GetRandomInt(g_iNumbers[0], g_iNumbers[1]) : GenerateRandomInt(1, 20) > 10 ? GetRandomInt(g_iLettersUpper[0], g_iLettersUpper[1]) : GetRandomInt(g_iLettersLower[0], g_iLettersLower[1]);
// 		break;
// 	default:
// 		return iChar;
// 		break;
// 	}

// 	return iChar;
// }

// void GenerateKey(std::string& sKey)
// {
// 	sKey[0] = '\0';
// 	int i = 0;
// 	if(g_sKeyTemplate[0])
// 	{
// 		int iLength = strlen(g_sKeyTemplate);
// 		while(i < iLength)
// 		{
// 			sKey[i] = GetCharTemplate(g_sKeyTemplate[i]);
// 			++i;
// 		}
// 	}
// 	else
// 	{
// 		while(i < g_iKeyLength)
// 		{
// 			sKey[i] = GetCharTemplate(0x58);
// 			++i;
// 		}
// 	}

// 	sKey[i] = '\0';
// }

bool IsKeyValid(std::string sKey, std::string& sError)
{
	if(!sKey.length())
	{
		sError = g_vecPhrases["ERROR_KEY_EMPTY"];
		return false;
	}
	
	if(sKey.length() < 8)
	{
		sError = g_vecPhrases["ERROR_KEY_SHORT"];
		return false;
	}

	if(sKey.length() > 64)
	{
		sError = g_vecPhrases["ERROR_KEY_LONG"];
		return false;
	}

	int i = 0;

	while(i < sKey.length())
	{
		if((sKey[i] > 0x2F && sKey[i] < 0x3A) || 
			(sKey[i] > 0x40 && sKey[i] < 0x5B) || 
			(sKey[i] > 0x60 && sKey[i] < 0x7B) ||
			sKey[i] == 0x2D)
		{
			++i;
			continue;
		}

		sError = g_vecPhrases["ERROR_KEY_INVALID_CHARACTERS"];
		return false;
	}

	return true;
}

bool keys_core::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetServerFactory, g_pSource2Server, ISource2Server, SOURCE2SERVER_INTERFACE_VERSION);

	SH_ADD_HOOK_MEMFUNC(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, this, &keys_core::Hook_GameServerSteamAPIActivated, false);

	g_SMAPI->AddListener( this, this );

	g_pKeysApi = new KeysApi();
	g_pKeysCore = g_pKeysApi;

	return true;
}

bool keys_core::Unload(char *error, size_t maxlen)
{
	SH_REMOVE_HOOK_MEMFUNC(IServerGameDLL, GameServerSteamAPIActivated, g_pSource2Server, this, &keys_core::Hook_GameServerSteamAPIActivated, false);
	ConVar_Unregister();
	
	return true;
}

void* keys_core::OnMetamodQuery(const char* iface, int* ret)
{
	if (!strcmp(iface, KEYS_INTERFACE))
	{
		*ret = META_IFACE_OK;
		return g_pKeysCore;
	}

	*ret = META_IFACE_FAILED;
	return nullptr;
}

void keys_core::Hook_GameServerSteamAPIActivated()
{
	std::thread(&keys_core::Authorization, this).detach();
	RETURN_META(MRES_IGNORED);
}

CSteamGameServerAPIContext g_steamAPI;

void DeleteExpiredKeys()
{
	char sQuery[256];
	if(!g_iServerID)
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "DELETE FROM `table_keys` WHERE `expires` > 0 AND `expires` < %d;", std::time(0));
	}
	else
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "DELETE FROM `table_keys` WHERE `expires` > 0 AND `expires` < %d AND `sid` = %d;",std::time(0), g_iServerID);
	}
	g_pConnection->Query(sQuery, [](ISQLQuery *pResult) {});
}

void keys_core::Authorization()
{
	int uAttempts = 500;
	while (--uAttempts)
	{
		if(g_bIsStarted)
		{
			if (engine->GetGameServerSteamID().GetStaticAccountKey())
			{
				g_steamAPI.Init();
				ISteamGameServer* pServer = SteamGameServer();
				ISteamHTTP* g_http = g_steamAPI.SteamHTTP();

				if (pServer && g_http && pServer->GetPublicIP().IsSet())
				{
					char sAddress[64];
					uint32_t ip = pServer->GetPublicIP().m_unIPv4;
					ConVarHandle hCvarHandle = g_pCVar->FindConVar("hostport");
					if (!hCvarHandle.IsValid()) return;
					ConVar* pConVar = g_pCVar->GetConVar(hCvarHandle);
					int iPort = *(int *)&pConVar->values;
					g_SMAPI->Format(sAddress, sizeof(sAddress), "%d.%d.%d.%d:%d", (ip >> 24) & 255, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255, iPort);
					char szQuery[512];
					g_SMAPI->Format(szQuery, sizeof(szQuery), "SELECT `sid` FROM `keys_servers` WHERE `address` = '%s';", sAddress);
					g_pConnection->Query(szQuery, [sAddress](ISQLQuery *pResult) {
						ISQLResult *pResultSet = pResult->GetResultSet();
						if (pResultSet->GetRowCount() == 0)
						{
							char szQuery[512];
							g_SMAPI->Format(szQuery, sizeof(szQuery), "INSERT INTO `keys_servers` (`address`) VALUES ('%s');", sAddress);
							g_pConnection->Query(szQuery, [](ISQLQuery *pResult) {
								g_iServerID = pResult->GetInsertId();
								g_pKeysApi->SendCoreReady();
								DeleteExpiredKeys();
							});
						}
						else if(pResultSet->FetchRow())
						{
							g_iServerID = pResultSet->GetInt(0);
							g_pKeysApi->SendCoreReady();
							DeleteExpiredKeys();
						}
					});
					break;
				}
			}
		}

		usleep(1000000);
	}
}

void BlockClient(int iSlot, uint64 iSteamID64)
{
	if(iSteamID64 == 0) return;
	g_bIsBlocked[iSlot] = true;
	g_iAttempts[iSlot] = std::time(0) + (g_iBlockTime*60);
	char sQuery[256];
	if(!g_iServerID)
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "INSERT INTO `keys_blocked_players` (`auth`, `block_end`) VALUES ('%lld', %d);", iSteamID64, std::time(0) + g_iAttempts[iSlot]);
	}
	else
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "INSERT INTO `keys_blocked_players` (`auth`, `block_end`, `sid`) VALUES ('%lld', %d, %d);", iSteamID64, g_iAttempts[iSlot], g_iServerID);
	}
	g_pConnection->Query(sQuery, [](ISQLQuery *pResult) {});
}

void UnBlockPlayer(int iSlot, uint64 iSteamID64)
{
	if(iSteamID64 == 0) return;
	g_bIsBlocked[iSlot] = false;
	g_iAttempts[iSlot] = 0;
	char sQuery[256];
	if(!g_iServerID)
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "DELETE FROM `keys_blocked_players` WHERE `auth` = '%lld';", iSteamID64);
	}
	else
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "DELETE FROM `keys_blocked_players` WHERE `auth` = '%lld' AND `sid` = %d;", iSteamID64, g_iServerID);
	}
	g_pConnection->Query(sQuery, [](ISQLQuery *pResult) {});
}

void DeleteKey(const char* szKey)
{
	char sQuery[256];
	if(!g_iServerID)
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "DELETE FROM `table_keys` WHERE `key_name` = '%s';", szKey);
	}
	else
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "DELETE FROM `table_keys` WHERE `key_name` = '%s' AND `sid` = %d;", szKey, g_iServerID);
	}
	g_pConnection->Query(sQuery, [](ISQLQuery *pResult) {});
}

void OnClientAuthorized(int iSlot, uint64 iSteamID64)
{
	g_iAttempts[iSlot] = 0;
	g_bIsBlocked[iSlot] = false;

	if(g_pPlayers->IsFakeClient(iSlot)) return;
	char sQuery[256];
	if(!g_iServerID)
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "SELECT `block_end` FROM `keys_blocked_players` WHERE `auth` = '%lld';", iSteamID64);
	}
	else
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "SELECT `block_end` FROM `keys_blocked_players` WHERE `auth` = '%lld' AND `sid` = %d;", iSteamID64, g_iServerID);
	}
	g_pConnection->Query(sQuery, [iSlot, iSteamID64](ISQLQuery *pResult) {
		ISQLResult *pResultSet = pResult->GetResultSet();
		if (pResultSet->FetchRow())
		{
			g_iAttempts[iSlot] = pResultSet->GetInt(0);
			if (g_iAttempts[iSlot] > std::time(0))
			{
				UnBlockPlayer(iSlot, iSteamID64);
				return;
			}

			g_bIsBlocked[iSlot] = true;
		}
	});
}

bool UseKeyCmd(int iSlot, const char* szContent)
{
	if(g_bIsBlocked[iSlot])
	{
		if(g_iAttempts[iSlot] > std::time(0))
		{
			PrintToChat(iSlot, g_vecPhrases["ERROR_BLOCKED"].c_str());
			return true;
		}
		else
		{
			UnBlockPlayer(iSlot, g_pPlayers->GetSteamID64(iSlot));
			g_bIsBlocked[iSlot] = false;
			g_iAttempts[iSlot] = 0;
		}
	}
	
	CCommand arg;
	arg.Tokenize(szContent);
	if(arg.ArgC() != 1)
	{
		PrintToChat(iSlot, g_vecPhrases["USAGE_ERROR_USE_KEY"].c_str());
		return true;
	}
	const char* szKey = arg[0];
	char sQuery[512];
	std::string sError;
	if(!IsKeyValid(szKey, sError))
	{
		PrintToChat(iSlot, sError.c_str());
		
		if(g_iAttempt)
		{
			if(g_iAttempts[iSlot]++ >= g_iAttempt)
			{
				BlockClient(iSlot, g_pPlayers->GetSteamID64(iSlot));
				PrintToChat(iSlot, g_vecPhrases["ERROR_BLOCKED"].c_str());
				return true;
			}

			PrintToChat(iSlot, g_vecPhrases["ERROR_INCORRECT_KEY_LEFT"].c_str(), g_iAttempt - g_iAttempts[iSlot]);
		}
		else
		{
			PrintToChat(iSlot, g_vecPhrases["ERROR_INCORRECT_KEY"].c_str());
		}

		return true;
	}

	if(!g_iServerID)
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "SELECT `key_name`, `type`, `expires`, `uses`, IF((SELECT `key_name` FROM `keys_players_used` WHERE `auth` = '%lld' AND `key_name` = '%s') IS NULL, 0, 1) as `used`, `param1`, `param2`, `param3`, `param4`, `param5` FROM `table_keys` WHERE `key_name` = '%s' LIMIT 1;", g_pPlayers->GetSteamID64(iSlot), szKey, szKey);
	}
	else
	{
		g_SMAPI->Format(sQuery, sizeof(sQuery), "SELECT `key_name`, `type`, `expires`, `uses`, IF((SELECT `key_name` FROM `keys_players_used` WHERE `auth` = '%lld' AND `key_name` = '%s') IS NULL, 0, 1) as `used`, `param1`, `param2`, `param3`, `param4`, `param5` FROM `table_keys` WHERE `key_name` = '%s' AND `sid` = %d LIMIT 1;", g_pPlayers->GetSteamID64(iSlot), szKey, szKey, g_iServerID);
	}

	g_pConnection->Query(sQuery, [iSlot, szKey](ISQLQuery *pResult) {
		ISQLResult *pResultSet = pResult->GetResultSet();
		if (pResultSet->FetchRow())
		{
			const char* szType = pResultSet->GetString(1);
			if(g_vecKeyUseCallbacks[szType])
			{
				const char* szKey = pResultSet->GetString(0);
				int iExpires = pResultSet->GetInt(2);
				if(iExpires)
				{
					if(iExpires < std::time(0))
					{
						DeleteKey(szKey);
						PrintToChat(iSlot, g_vecPhrases["ERROR_KEY_NOT_EXIST"].c_str());
						return;
					}
				}
				int iUses = pResultSet->GetInt(3);
				if(!iUses)
				{
					DeleteKey(szKey);
					PrintToChat(iSlot, g_vecPhrases["ERROR_KEY_NOT_EXIST"].c_str());
					return;
				}
				int iUsed = pResultSet->GetInt(4);
				if(iUsed)
				{
					PrintToChat(iSlot, g_vecPhrases["ERROR_KEY_ALREADY_USED"].c_str());
					return;
				}

				std::vector<std::string> szParams;
				for(int i = 5; i < 10; ++i)
				{
					if(pResultSet->IsNull(i))
					{
						break;
					}
					szParams.push_back(pResultSet->GetString(i));
				}
				const char* szError;
				bool bResult = g_vecKeyUseCallbacks[szType](iSlot, szKey, szParams, szError);
				if(!bResult)
				{
					Msg("Error: %s\n", szError);
					PrintToChat(iSlot, szError);
					return;
				}

				char sQuery[256];
				if(--iUses)
				{
					if(!g_iServerID)
					{
						g_SMAPI->Format(sQuery, sizeof(sQuery), "INSERT INTO `keys_players_used` (`auth`, `key_name`) VALUES ('%lld', '%s');", g_pPlayers->GetSteamID64(iSlot), szKey);
					}
					else
					{
						g_SMAPI->Format(sQuery, sizeof(sQuery), "INSERT INTO `keys_players_used` (`auth`, `key_name`, `sid`) VALUES ('%lld', '%s', %d);", g_pPlayers->GetSteamID64(iSlot), szKey, g_iServerID);
					}
					g_pConnection->Query(sQuery, [](ISQLQuery *pResult) {});

					if(!g_iServerID)
					{
						g_SMAPI->Format(sQuery, sizeof(sQuery), "UPDATE `table_keys` SET `uses` = %d WHERE `key_name` = '%s';", iUses, szKey);
					}
					else
					{
						g_SMAPI->Format(sQuery, sizeof(sQuery), "UPDATE `table_keys` SET `uses` = %d WHERE `key_name` = '%s' AND `sid` = %d;", iUses, szKey, g_iServerID);
					}
					g_pConnection->Query(sQuery, [](ISQLQuery *pResult) {});
				}
				else
				{
					DeleteKey(szKey);
					if(!g_iServerID)
					{
						g_SMAPI->Format(sQuery, sizeof(sQuery), "DELETE FROM `keys_players_used` WHERE `key_name` = '%s';", szKey);
					}
					else
					{
						g_SMAPI->Format(sQuery, sizeof(sQuery), "DELETE FROM `keys_players_used` WHERE `key_name` = '%s' AND `sid` = %d;", szKey, g_iServerID);
					}
					g_pConnection->Query(sQuery, [](ISQLQuery *pResult) {});
				}

				PrintToChat(iSlot, g_vecPhrases["SUCCESS_USE_KEY"].c_str(), szKey);
				return;
			}
			return;
		}
		
		if(g_iAttempt)
		{
			if(g_iAttempts[iSlot]++ >= g_iAttempt)
			{
				BlockClient(iSlot, g_pPlayers->GetSteamID64(iSlot));
				PrintToChat(iSlot, g_vecPhrases["ERROR_BLOCKED"].c_str());
				return;
			}

			PrintToChat(iSlot, g_vecPhrases["ERROR_INCORRECT_KEY_LEFT"].c_str(), g_iAttempt - g_iAttempts[iSlot]);
		}
		else
		{
			PrintToChat(iSlot, g_vecPhrases["ERROR_INCORRECT_KEY"].c_str());
		}
	});
	return true;
}

void keys_core::AllPluginsLoaded()
{
	char error[64];
	int ret;
	g_pUtils = (IUtilsApi *)g_SMAPI->MetaFactory(Utils_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_SMAPI->Format(error, sizeof(error), "Missing Utils system plugin");
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pPlayers = (IPlayersApi *)g_SMAPI->MetaFactory(PLAYERS_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_SMAPI->Format(error, sizeof(error), "Missing Players system plugin");
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	
	ISQLInterface* g_SqlInterface = (ISQLInterface *)g_SMAPI->MetaFactory(SQLMM_INTERFACE, &ret, nullptr);
	if (ret == META_IFACE_FAILED) {
		g_pUtils->ErrorLog("[%s] Missing MYSQL plugin", g_PLAPI->GetLogTag());
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pMysqlClient = g_SqlInterface->GetMySQLClient();

	// Load config
	{
		KeyValues* pKVConfig = new KeyValues("Config");
		if (!pKVConfig->LoadFromFile(g_pFullFileSystem, "addons/configs/keys/core.ini")) {
			g_pUtils->ErrorLog("[%s] Failed to load config addons/configs/keys/core.ini", g_PLAPI->GetLogTag());
			return;
		}

		// g_iKeyLength = pKVConfig->GetInt("key_length", 32);
		// g_sKeyTemplate = pKVConfig->GetString("key_template", "");
		g_iAttempt = pKVConfig->GetInt("key_attempts", 3);
		g_iBlockTime = pKVConfig->GetInt("key_block_time", 60);
	}

	// Load databases
	{
		KeyValues* pKVConfig = new KeyValues("Databases");
		if (!pKVConfig->LoadFromFile(g_pFullFileSystem, "addons/configs/databases.cfg")) {
			g_pUtils->ErrorLog("[%s] Failed to load databases config addons/configs/databases.cfg", g_PLAPI->GetLogTag());
			return;
		}

		pKVConfig = pKVConfig->FindKey("keys_core", false);
		if (!pKVConfig) {
			g_pUtils->ErrorLog("[%s] No databases.cfg 'keys_core'", g_PLAPI->GetLogTag());
			return;
		}

		MySQLConnectionInfo info;
		info.host = pKVConfig->GetString("host", nullptr);
		info.user = pKVConfig->GetString("user", nullptr);
		info.pass = pKVConfig->GetString("pass", nullptr);
		info.database = pKVConfig->GetString("database", nullptr);
		info.port = pKVConfig->GetInt("port");
		g_pConnection = g_pMysqlClient->CreateMySQLConnection(info);

		g_pConnection->Connect([](bool connect) {
			if (!connect) {
				g_pUtils->ErrorLog("[%s] Failed to connect to the database, verify the data and try again", g_PLAPI->GetLogTag());
			} else {
				char szQuery[1024];
				Transaction txn;
				g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS `table_keys` (\
									`key_name` VARCHAR(64) NOT NULL, \
									`type` VARCHAR(64) NOT NULL, \
									`expires` INTEGER UNSIGNED NOT NULL default 0, \
									`uses` INTEGER UNSIGNED NOT NULL default 1, \
									`sid` INTEGER NOT NULL, \
									`param1` VARCHAR(64) NULL default NULL, \
									`param2` VARCHAR(64) NULL default NULL, \
									`param3` VARCHAR(64) NULL default NULL, \
									`param4` VARCHAR(64) NULL default NULL, \
									`param5` VARCHAR(64) NULL default NULL, \
									PRIMARY KEY(`key_name`)) DEFAULT CHARSET=utf8;");
				txn.queries.push_back(szQuery);

				g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS `keys_blocked_players` (\
									`auth` VARCHAR(24) NOT NULL, \
									`block_end` INTEGER UNSIGNED NOT NULL, \
									`sid` INTEGER NOT NULL, \
									PRIMARY KEY(`auth`)) DEFAULT CHARSET=utf8;");
				txn.queries.push_back(szQuery);

				g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS `keys_players_used` (\
									`auth` VARCHAR(24) NOT NULL, \
									`key_name` VARCHAR(64) NOT NULL, \
									`sid` INTEGER NOT NULL) DEFAULT CHARSET=utf8;");
				txn.queries.push_back(szQuery);

				g_SMAPI->Format(szQuery, sizeof(szQuery), "CREATE TABLE IF NOT EXISTS `keys_servers` (\
									`sid` INTEGER NOT NULL AUTO_INCREMENT,\
									`address` VARCHAR(24) NOT NULL, \
									PRIMARY KEY(`sid`), \
									UNIQUE KEY `address` (`address`)) DEFAULT CHARSET=utf8;");
				txn.queries.push_back(szQuery);
				
				g_iServerID = -1;
				g_bIsStarted = true;

				g_pConnection->ExecuteTransaction(txn, [](std::vector<ISQLQuery *>) {}, [](std::string error, int code) {
					g_pUtils->ErrorLog("[%s] Failed to create tables: %s", g_PLAPI->GetLogTag(), error.c_str());
				});
			}
		});
	}	
	
	// Load translations
	{

		KeyValues::AutoDelete g_kvPhrases("Phrases");
		const char *pszPath = "addons/translations/keys.phrases.txt";
		if (!g_kvPhrases->LoadFromFile(g_pFullFileSystem, pszPath))
		{
			Warning("Failed to load %s\n", pszPath);
			return;
		}

		std::string szLanguage = std::string(g_pUtils->GetLanguage());
		const char* g_pszLanguage = szLanguage.c_str();
		for (KeyValues *pKey = g_kvPhrases->GetFirstTrueSubKey(); pKey; pKey = pKey->GetNextTrueSubKey())
			g_vecPhrases[std::string(pKey->GetName())] = std::string(pKey->GetString(g_pszLanguage));
	}


	g_pUtils->RegCommand(g_PLID, {"key", "usekey"}, {"!key", "!usekey"}, UseKeyCmd);
	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pPlayers->HookOnClientAuthorized(g_PLID, OnClientAuthorized);
}

bool KeysApi::IsCoreStarted()
{
	return g_bIsStarted;
}

void KeysApi::HookOnCoreIsReady(OnCoreIsReady fn)
{
	g_vecOnCoreIsReady.push_back(fn);
}

void KeysApi::SendCoreReady()
{
	for (auto& fn : g_vecOnCoreIsReady)
		fn();
}

void KeysApi::RegKey(const char* szKey, KeyUseCallback fn)
{
	g_vecKeyUseCallbacks[std::string(szKey)] = fn;
}

void KeysApi::UnregKey(const char* szKey)
{
	g_vecKeyUseCallbacks.erase(std::string(szKey));
}

const char* KeysApi::GetTranslation(const char* szKey)
{
	return g_vecPhrases[std::string(szKey)].c_str();
}

///////////////////////////////////////
const char* keys_core::GetLicense()
{
	return "GPL";
}

const char* keys_core::GetVersion()
{
	return "1.0";
}

const char* keys_core::GetDate()
{
	return __DATE__;
}

const char *keys_core::GetLogTag()
{
	return "keys_core";
}

const char* keys_core::GetAuthor()
{
	return "Pisex";
}

const char* keys_core::GetDescription()
{
	return "Keys Core";
}

const char* keys_core::GetName()
{
	return "[Keys] Core";
}

const char* keys_core::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
