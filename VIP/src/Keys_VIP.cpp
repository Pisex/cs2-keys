#include <stdio.h>
#include "Keys_VIP.h"
#include "metamod_oslink.h"
#include "schemasystem/schemasystem.h"

Keys_VIP g_Keys_VIP;
PLUGIN_EXPOSE(Keys_VIP, g_Keys_VIP);
IVEngineServer2* engine = nullptr;
CGameEntitySystem* g_pGameEntitySystem = nullptr;
CEntitySystem* g_pEntitySystem = nullptr;
CGlobalVars *gpGlobals = nullptr;

IUtilsApi* g_pUtils;
IVIPApi* g_pVIPApi;
IKeysApi* g_pKeysApi;

#define VIP_ADD "vip_add"
#define VIP_EXT "vip_ext"
#define VIP_GC "vip_gc"

int GC_STATUS;
int CMP_VGRP;

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

bool Keys_VIP::Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late)
{
	PLUGIN_SAVEVARS();

	GET_V_IFACE_CURRENT(GetEngineFactory, g_pCVar, ICvar, CVAR_INTERFACE_VERSION);
	GET_V_IFACE_ANY(GetEngineFactory, g_pSchemaSystem, ISchemaSystem, SCHEMASYSTEM_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetEngineFactory, engine, IVEngineServer2, SOURCE2ENGINETOSERVER_INTERFACE_VERSION);
	GET_V_IFACE_CURRENT(GetFileSystemFactory, g_pFullFileSystem, IFileSystem, FILESYSTEM_INTERFACE_VERSION);

	g_SMAPI->AddListener( this, this );

	return true;
}

bool Keys_VIP::Unload(char *error, size_t maxlen)
{
	ConVar_Unregister();
	
	return true;
}

void CoreIsReady()
{
	g_pKeysApi->RegKey("vip_add", [](int iSlot, const char* szKeyType, std::vector<std::string> szParams, const char*& szError) -> bool
	{
		if(g_pVIPApi->VIP_IsClientVIP(iSlot))
		{
			const char* szGroup = szParams[0].c_str();
			int iTime = atoi(szParams[1].c_str());
			int iVIPTime = g_pVIPApi->VIP_GetClientAccessTime(iSlot);
			const char* szClientGroup = g_pVIPApi->VIP_GetClientVIPGroup(iSlot);
			if(GC_STATUS)
			{
				if(g_pVIPApi->VIP_IsValidVIPGroup(szGroup))
				{
					if(strcmp(szClientGroup, szGroup))
					{
						g_pVIPApi->VIP_SetClientVIPGroup(iSlot, szGroup, true);
						char szBuffer[128];
						g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pKeysApi->GetTranslation("USE_KEY_GRP_CNG"), szGroup);
						g_pUtils->PrintToChat(iSlot, "%s%s", g_pKeysApi->GetTranslation("Prefix"), szBuffer);
					}
				}
				else
				{
					szError = g_pKeysApi->GetTranslation("ERROR_INVALID_GROUP");
					return false;
				}
			}
			if(CMP_VGRP && iVIPTime)
			{
				if(!strcmp(szClientGroup, szGroup))
				{
					if(iTime)
					{
						g_pVIPApi->VIP_SetClientAccessTime(iSlot, iVIPTime + iTime, true);
						g_pUtils->PrintToChat(iSlot, "%s%s", g_pKeysApi->GetTranslation("Prefix"), g_pKeysApi->GetTranslation("USE_KEY_EXT"));
					}
					else
					{
						g_pVIPApi->VIP_SetClientAccessTime(iSlot, 0, true);
						g_pUtils->PrintToChat(iSlot, "%s%s", g_pKeysApi->GetTranslation("Prefix"), g_pKeysApi->GetTranslation("USE_KEY_EXT_FOREVER"));
					}
				}
			}
			
			if(!GC_STATUS && !CMP_VGRP)
			{
				szError = g_pKeysApi->GetTranslation("ERROR_VIP_ALREADY");
				return false;
			}
		}
		else
		{
			const char* szGroup = szParams[0].c_str();
			if(!g_pVIPApi->VIP_IsValidVIPGroup(szGroup))
			{
				szError = g_pKeysApi->GetTranslation("ERROR_INVALID_GROUP");
				return false;
			}

			int iTime = atoi(szParams[1].c_str());
			if(iTime)
			{
				g_pVIPApi->VIP_GiveClientVIP(iSlot, iTime, szGroup, true);
				g_pUtils->PrintToChat(iSlot, "%s%s", g_pKeysApi->GetTranslation("Prefix"), g_pKeysApi->GetTranslation("USE_KEY_EXT"));
			}
			else
			{
				g_pVIPApi->VIP_GiveClientVIP(iSlot, 0, szGroup, true);
				g_pUtils->PrintToChat(iSlot, "%s%s", g_pKeysApi->GetTranslation("Prefix"), g_pKeysApi->GetTranslation("USE_KEY_EXT_FOREVER"));
			}
		}
		return true;
	});

	g_pKeysApi->RegKey("vip_ext", [](int iSlot, const char* szKeyType, std::vector<std::string> szParams, const char*& szError) -> bool
	{
		if(g_pVIPApi->VIP_IsClientVIP(iSlot))
		{
			int iTime = atoi(szParams[1].c_str());
			g_pVIPApi->VIP_SetClientAccessTime(iSlot, iTime ? g_pVIPApi->VIP_GetClientAccessTime(iSlot) + iTime : 0, true);
			if(iTime)
			{
				g_pUtils->PrintToChat(iSlot, "%s%s", g_pKeysApi->GetTranslation("Prefix"), g_pKeysApi->GetTranslation("USE_KEY_EXT"));
			}
			else
			{
				g_pUtils->PrintToChat(iSlot, "%s%s", g_pKeysApi->GetTranslation("Prefix"), g_pKeysApi->GetTranslation("USE_KEY_EXT_FOREVER"));
			}
			return true;
		}
		else
		{
			szError = g_pKeysApi->GetTranslation("ERROR_CAN_NOT_USE");
			return false;
		}
	});

	g_pKeysApi->RegKey("vip_gc", [](int iSlot, const char* szKeyType, std::vector<std::string> szParams, const char*& szError) -> bool
	{
		if(g_pVIPApi->VIP_IsClientVIP(iSlot))
		{
			const char* szGroup = szParams[0].c_str();
			if(!g_pVIPApi->VIP_IsValidVIPGroup(szGroup))
			{
				szError = g_pKeysApi->GetTranslation("ERROR_INVALID_GROUP");
				return false;
			}

			const char* szClientGroup = g_pVIPApi->VIP_GetClientVIPGroup(iSlot);
			
			if(!strcmp(szClientGroup, szGroup))
			{
				szError = g_pKeysApi->GetTranslation("ERROR_ALREADY_VIP_GROUP");
				return false;
			}

			g_pVIPApi->VIP_SetClientVIPGroup(iSlot, szGroup, true);
			char szBuffer[128];
			g_SMAPI->Format(szBuffer, sizeof(szBuffer), g_pKeysApi->GetTranslation("USE_KEY_GRP_CNG"), szGroup);
			g_pUtils->PrintToChat(iSlot, "%s%s", g_pKeysApi->GetTranslation("Prefix"), szBuffer);
			return true;
		}
		else
		{
			szError = g_pKeysApi->GetTranslation("ERROR_CAN_NOT_USE");
			return false;
		}
	});
}

void Keys_VIP::AllPluginsLoaded()
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
	g_pVIPApi = (IVIPApi *)g_SMAPI->MetaFactory(VIP_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_SMAPI->Format(error, sizeof(error), "Missing VIP system plugin");
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}
	g_pKeysApi = (IKeysApi *)g_SMAPI->MetaFactory(KEYS_INTERFACE, &ret, NULL);
	if (ret == META_IFACE_FAILED)
	{
		g_SMAPI->Format(error, sizeof(error), "Missing Keys system plugin");
		ConColorMsg(Color(255, 0, 0, 255), "[%s] %s\n", GetLogTag(), error);
		std::string sBuffer = "meta unload "+std::to_string(g_PLID);
		engine->ServerCommand(sBuffer.c_str());
		return;
	}

	KeyValues* pKVConfig = new KeyValues("Config");
	if (!pKVConfig->LoadFromFile(g_pFullFileSystem, "addons/configs/keys/vip.ini")) {
		g_pUtils->ErrorLog("[%s] Failed to load config addons/configs/keys/vip.ini", g_PLAPI->GetLogTag());
		return;
	}

	GC_STATUS		 = pKVConfig->GetInt("gc_status", 0);
	CMP_VGRP		 = pKVConfig->GetInt("cmp_vgrp", 0);

	g_pUtils->StartupServer(g_PLID, StartupServer);
	g_pKeysApi->HookOnCoreIsReady(CoreIsReady);
}

///////////////////////////////////////
const char* Keys_VIP::GetLicense()
{
	return "GPL";
}

const char* Keys_VIP::GetVersion()
{
	return "1.0";
}

const char* Keys_VIP::GetDate()
{
	return __DATE__;
}

const char *Keys_VIP::GetLogTag()
{
	return "Keys_VIP";
}

const char* Keys_VIP::GetAuthor()
{
	return "Pisex";
}

const char* Keys_VIP::GetDescription()
{
	return "Keys_VIP";
}

const char* Keys_VIP::GetName()
{
	return "[Keys] VIP";
}

const char* Keys_VIP::GetURL()
{
	return "https://discord.gg/g798xERK5Y";
}
