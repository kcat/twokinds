
#include "cvars.hpp"

#include <cassert>
#include <cstring>
#include <cstdlib>

#include "gui/iface.hpp"
#include "delegates.hpp"
#include "log.hpp"


namespace
{

class CVarRegistry {
    std::map<std::string,TK::CVar*> mCVarRegistry;
    std::map<std::string,TK::CCmd*> mCCmdRegistry;

    CVarRegistry(const CVarRegistry&) = delete;
    CVarRegistry& operator=(const CVarRegistry&) = delete;

    CVarRegistry() { }

public:
    void add(std::string&& name, TK::CVar *cvar)
    {
        mCVarRegistry.insert(std::make_pair(std::move(name), cvar));
    }
    void add(std::string&& name, TK::CCmd *ccmd)
    {
        mCCmdRegistry.insert(std::make_pair(std::move(name), ccmd));
    }

    const std::map<std::string,TK::CVar*>& getAll() const
    {
        return mCVarRegistry;
    }

    void callCCmd(const std::string &name, const std::string &value)
    {
        auto ccmd = mCCmdRegistry.find(name);
        if(ccmd != mCCmdRegistry.end())
            (*ccmd->second)(value);
    }

    void setCVarValue(const std::string &name, const std::string &value)
    {
        auto cvar = mCVarRegistry.find(name);
        if(cvar != mCVarRegistry.end())
        {
            if(!value.empty())
            {
                if(!cvar->second->set(value))
                {
                    TK::Log::get().stream(TK::Log::Level_Error)<< "Invalid "<<name<<" value: "<<value;
                    return;
                }
            }

            TK::Log::get().stream()<< name<<" = \""<<cvar->second->get()<<"\"";
        }
    }

    void loadCVarValue(const std::string &name, const std::string &value)
    {
        auto cvar = mCVarRegistry.find(name);
        if(cvar == mCVarRegistry.end())
            TK::Log::get().stream(TK::Log::Level_Error)<< "CVar "<<name<<" does not exist.";
        else
        {
            if(!cvar->second->set(value))
                TK::Log::get().stream(TK::Log::Level_Error)<< "Invalid "<<name<<" value: "<<value;
        }
    }

    void initialize()
    {
        auto deleg = TK::makeDelegate(this, &CVarRegistry::setCVarValue);
        auto &gui = TK::GuiIface::get();
        for(auto &cvar : mCVarRegistry)
            gui.addConsoleCallback(cvar.first.c_str(), deleg);
        deleg = TK::makeDelegate(this, &CVarRegistry::callCCmd);
        for(auto &ccmd : mCCmdRegistry)
            gui.addConsoleCallback(ccmd.first.c_str(), deleg);
    }

    static CVarRegistry& getSingleton()
    {
        static CVarRegistry instance;
        return instance;
    }
};

} // namespace


namespace TK
{

CVar::CVar(std::string&& name)
{
    CVarRegistry::getSingleton().add(std::move(name), this);
}

void CVar::setByName(const std::string &name, const std::string &value)
{
    CVarRegistry::getSingleton().loadCVarValue(name, value);
}

std::map<std::string,std::string> CVar::getAll()
{
    std::map<std::string,std::string> ret;
    const auto &cvars = CVarRegistry::getSingleton().getAll();
    for(const auto &cvar : cvars)
        ret.insert(std::make_pair(cvar.first, cvar.second->get()));
    return ret;
}

void CVar::registerAll()
{
    CVarRegistry::getSingleton().initialize();
}


CCmd::CCmd(std::string&& name, std::initializer_list<const char*>&& aliases)
{
    CVarRegistry::getSingleton().add(std::move(name), this);
    for(auto alias : aliases)
        CVarRegistry::getSingleton().add(alias, this);
}


CVarString::CVarString(std::string&& name, std::string&& value)
  : CVar(std::move(name)), mValue(value)
{
}

bool CVarString::set(const std::string &value)
{
    if(value.length() >= 2 && value.front() == '"' && value.back() == '"')
        mValue = value.substr(1, value.length()-2);
    else
        mValue = value;
    return true;
}

std::string CVarString::get() const
{
    return mValue;
}


CVarBool::CVarBool(std::string&& name, bool value)
  : CVar(std::move(name)), mValue(value)
{
}

bool CVarBool::set(const std::string &value)
{
    if(strcasecmp(value.c_str(), "true") == 0 || strcasecmp(value.c_str(), "yes") == 0 || strcasecmp(value.c_str(), "on") == 0 || value == "1")
    {
        mValue = true;
        return true;
    }
    if(strcasecmp(value.c_str(), "false") == 0 || strcasecmp(value.c_str(), "no") == 0 || strcasecmp(value.c_str(), "off") == 0 || value == "0")
    {
        mValue = false;
        return true;
    }
    return false;
}

std::string CVarBool::get() const
{
    return mValue ? "true" : "false";
}


CVarInt::CVarInt(std::string&& name, int value, int minval, int maxval)
  : CVar(std::move(name)), mMinValue(minval), mMaxValue(maxval), mValue(value)
{
}

bool CVarInt::set(const std::string &value)
{
    char *end;
    long val = strtol(value.c_str(), &end, 0);
    if(!end || *end != '\0') return false;
    mValue = std::min<long>(std::max<long>(val, mMinValue), mMaxValue);
    return true;
}

std::string CVarInt::get() const
{
    std::stringstream sstr;
    sstr<< mValue;
    return sstr.str();
}

} // namespace TK
