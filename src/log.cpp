
#include "log.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <ctime>

#include <OgreLog.h>

#include "gui/iface.hpp"

namespace TK
{

template<>
Log *Singleton<Log>::sInstance = nullptr;


class Log::OgreListener : public Ogre::LogListener {
public:
    virtual void messageLogged(const Ogre::String &message, Ogre::LogMessageLevel lml, bool maskDebug, const Ogre::String &logName, bool &skipThisMessage)
    {
        Log::Level lvl = Log::Level_Error;
        switch(lml)
        {
            case Ogre::LML_TRIVIAL:  lvl = Log::Level_Debug;  break;
            case Ogre::LML_NORMAL:   lvl = Log::Level_Normal; break;
            case Ogre::LML_CRITICAL: lvl = Log::Level_Error;  break;
        }
        Log::get().message(message, lvl);
    }
};


Log::Log(Level level, Ogre::Log *log)
  : mLevel(level), mGui(nullptr), mLog(nullptr), mOgreListener(nullptr)
{
    mOgreListener = new OgreListener();
    setLog(log);
}

Log::~Log()
{
    delete mOgreListener;
    mOgreListener = nullptr;

    setLog(nullptr);
}


std::string Log::getTimestamp()
{
    std::time_t cur_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm *tm = std::localtime(&cur_time);

    std::stringstream sstr;
    sstr<< std::setw(2)<<std::setfill('0')<<tm->tm_hour<<
           ":"<<std::setw(2)<<std::setfill('0')<<tm->tm_min<<
           ":"<<std::setw(2)<<std::setfill('0')<<tm->tm_sec<<
           ": ";
    return sstr.str();
}

void Log::setLog(Ogre::Log *log)
{
    if(mOutfile.is_open())
        mOutfile.close();

    if(mLog)
        mLog->removeListener(mOgreListener);
    mLog = log;
    if(mLog)
    {
        mLog->addListener(mOgreListener);
        mOutfile.open(mLog->getName());
        mOutfile<< getTimestamp()<<"--- Starting log ---" <<std::endl;
    }
}

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
    if(level < mLevel)
        return;

    std::ostream &out = (level==Level_Error) ? std::cerr : std::cout;
    if(mOutfile.is_open())
    {
        mOutfile<< getTimestamp()<<msg <<std::endl;
        mOutfile.flush();
    }
    out<< msg <<std::endl;

    if(mGui)
        mGui->printToConsole(msg);
    else
        mBuffer.push_back(msg);
}

LogStream Log::stream(Level level)
{
    return LogStream(this, level);
}

} // namespace TK
