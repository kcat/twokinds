
#include "log.hpp"

#include "gui/iface.hpp"

namespace TK
{

template<>
Log *Singleton<Log>::sInstance = nullptr;


void Log::setGuiIface(GuiIface *iface)
{
    mGui = iface;
    if(mGui)
    {
        for(const auto &str : mBuffer)
            mGui->printToConsole(str);
        std::vector<std::string>().swap(mBuffer);
    }
}

void Log::message(const std::string &msg, Level level)
{
    if(mLog)
    {
        if(level == Level_Debug)
            mLog->logMessage(msg, Ogre::LML_TRIVIAL);
        else if(level == Level_Normal)
            mLog->logMessage(msg, Ogre::LML_NORMAL);
        else if(level == Level_Error)
            mLog->logMessage(msg, Ogre::LML_CRITICAL);
    }
    if(level >= mLevel)
    {
        if(mGui)
            mGui->printToConsole(msg);
        else
            mBuffer.push_back(msg);
    }
}

LogStream Log::stream(Level level)
{
    return LogStream(this, level);
}

} // namespace TK
