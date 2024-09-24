#ifndef _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
#define _INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_

#include <ISmmPlugin.h>
#include <sh_vector.h>
#include "utlvector.h"
#include "ehandle.h"
#include <iserver.h>
#include <entity2/entitysystem.h>
#include <steam/steam_gameserver.h>
#include "igameevents.h"
#include "vector.h"
#include <deque>
#include <functional>
#include <utlstring.h>
#include <KeyValues.h>
#include "CCSPlayerController.h"
#include "include/menus.h"
#include "include/mysql_mm.h"
#include "include/keys_core.h"

class keys_core final : public ISmmPlugin, public IMetamodListener
{
public:
	bool Load(PluginId id, ISmmAPI* ismm, char* error, size_t maxlen, bool late);
	bool Unload(char* error, size_t maxlen);
	void AllPluginsLoaded();
	void* OnMetamodQuery(const char* iface, int* ret);
private:
	const char* GetAuthor();
	const char* GetName();
	const char* GetDescription();
	const char* GetURL();
	const char* GetLicense();
	const char* GetVersion();
	const char* GetDate();
	const char* GetLogTag();
private:
	void Hook_GameServerSteamAPIActivated();
	void Authorization();
};

class KeysApi: public IKeysApi
{
public:
    bool IsCoreStarted();
    void HookOnCoreIsReady(OnCoreIsReady fn);
    void RegKey(const char* szKeyType, KeyUseCallback fn);
    void UnregKey(const char* szKeyType);
	void SendCoreReady();
	const char* GetTranslation(const char* szKey);
};

#endif //_INCLUDE_METAMOD_SOURCE_STUB_PLUGIN_H_
