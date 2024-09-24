#pragma once

#include <functional>
#include <string>

typedef std::function<bool(int iSlot, const char* szKeyType, std::vector<std::string> szParams, const char*& szError)> KeyUseCallback;
typedef std::function<void()> OnCoreIsReady;

#define KEYS_INTERFACE "IKeysApi"
class IKeysApi
{
public:
    virtual bool IsCoreStarted() = 0;
    virtual void HookOnCoreIsReady(OnCoreIsReady fn) = 0;
    virtual void RegKey(const char* szKeyType, KeyUseCallback fn) = 0;
    virtual void UnregKey(const char* szKeyType) = 0;
    virtual const char* GetTranslation(const char* szKey) = 0;
};