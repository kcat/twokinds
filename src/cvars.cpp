
#include "cvars.hpp"

#include <cassert>
#include <cstring>

//  FIXME: Shouldn't really use Ogre here...
#include <OgreString.h>
#include <OgreStringConverter.h>

#include "gui/iface.hpp"
#include "delegates.hpp"


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

    void setCVarValue(const std::string &name, const std::string &value)
    {
        auto cvar = mCVarRegistry.find(name);
        if(cvar != mCVarRegistry.end())
        {
            if(!value.empty())
                cvar->second->set(value);

            std::stringstream sstr;
            sstr<< name<<" = "<<cvar->second->get();
            TK::GuiIface::get().printToConsole(sstr.str());
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

void CVar::registerAll()
{
    CVarRegistry::getSingleton().initialize();
}


CVarString::CVarString(std::string&& name, std::string value)
  : CVar(std::move(name)), mValue(value)
{
}

void CVarString::set(const std::string &value)
{
    if(!value.empty() && value.front() == '"' && value.back() == '"')
        mValue = value.substr(1, value.length()-2);
    else
        mValue = value;
}

std::string CVarString::get() const
{
    return mValue;
}


CVarBool::CVarBool(std::string&& name, bool value)
  : CVar(std::move(name)), mValue(value)
{
}

void CVarBool::set(const std::string &value)
{
    if(strcasecmp(value.c_str(), "true") == 0 || strcasecmp(value.c_str(), "yes") == 0 || strcasecmp(value.c_str(), "on") == 0)
        mValue = true;
    else if(strcasecmp(value.c_str(), "false") == 0 || strcasecmp(value.c_str(), "no") == 0 || strcasecmp(value.c_str(), "off") == 0)
        mValue = false;
    else
    {
        std::stringstream sstr;
        sstr<< "Invalid boolean value: "<<value<<" (expected true or false)";
        GuiIface::get().printToConsole(sstr.str());
    }
}

std::string CVarBool::get() const
{
    return mValue ? "true" : "false";
}


CVarInt::CVarInt(std::string&& name, int value)
  : CVar(std::move(name)), mValue(value)
{
}

void CVarInt::set(const std::string &value)
{
    if(Ogre::StringConverter::isNumber(value))
        mValue = Ogre::StringConverter::parseInt(value);
    else
    {
        std::stringstream sstr;
        sstr<< "Invalid integer value: "<<value;
        GuiIface::get().printToConsole(sstr.str());
    }
}

std::string CVarInt::get() const
{
    std::stringstream sstr;
    sstr<< mValue;
    return sstr.str();
}

} // namespace TK