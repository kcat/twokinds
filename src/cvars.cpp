
#include "cvars.hpp"

#include <cassert>
#include <cstring>

//  FIXME: Shouldn't really use Ogre here...
#include <OgreString.h>
#include <OgreStringConverter.h>

#include "gui/iface.hpp"
#include "delegates.hpp"
#include "log.hpp"


namespace
{

class CVarRegistry {
    static CVarRegistry *sSingleton;

    std::map<std::string,TK::CVar*> mCVarRegistry;
    CVarRegistry()
    {
        assert(!sSingleton);
        sSingleton = this;
    }
public:
    ~CVarRegistry()
    {
        sSingleton = nullptr;
    }

    void add(std::string&& name, TK::CVar *cvar)
    {
        mCVarRegistry.insert(std::make_pair(std::move(name), cvar));
    }

    const std::map<std::string,TK::CVar*>& getAll() const
    {
        return mCVarRegistry;
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
        auto &gui = TK::GuiIface::get();
        for(auto &cvar : mCVarRegistry)
            gui.addConsoleCallback(cvar.first.c_str(), TK::makeDelegate(this, &CVarRegistry::setCVarValue));
    }

    static CVarRegistry& getSingleton()
    {
        static CVarRegistry instance;
        return instance;
    }
};
CVarRegistry *CVarRegistry::sSingleton = nullptr;

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


CVarString::CVarString(std::string&& name, std::string value)
  : CVar(std::move(name)), mValue(value)
{
}

bool CVarString::set(const std::string &value)
{
    if(!value.empty() && value.front() == '"' && value.back() == '"')
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


CVarInt::CVarInt(std::string&& name, int value)
  : CVar(std::move(name)), mValue(value)
{
}

bool CVarInt::set(const std::string &value)
{
    if(!Ogre::StringConverter::isNumber(value))
        return false;
    mValue = Ogre::StringConverter::parseInt(value);
    return true;
}

std::string CVarInt::get() const
{
    std::stringstream sstr;
    sstr<< mValue;
    return sstr.str();
}

} // namespace TK
