#ifndef LOG_HPP
#define LOG_HPP

#include <sstream>
#include <string>
#include <vector>

#include <OgreLog.h>

namespace TK
{

class GuiIface;

class LogStream;

class Log {
    static Log sInstance;

public:
    enum Level {
        Level_Debug,
        Level_Normal,
        Level_Error,
    };

private:
    Level mLevel;
    Ogre::Log *mLog;
    GuiIface *mGui;
    std::vector<std::string> mBuffer;

    Log() : mLevel(Level_Normal), mLog(nullptr), mGui(nullptr)
    { }

public:
    void setLog(Ogre::Log *log) { mLog = log; }
    void setLevel(Level level) { mLevel = level; }

    void setGuiIface(GuiIface *iface);

    LogStream stream(Level level=Level_Normal);
    void message(const std::string &msg, Level level=Level_Normal);

    static Log& get() { return sInstance; }
    static Log* getPtr() { return &sInstance; }
};

class LogStream {
    Log *mLog;
    Log::Level mLevel;
    std::stringstream mStream;

    LogStream(const LogStream&) = delete;
    LogStream& operator=(const LogStream&) = delete;

public:
    LogStream(Log *log, Log::Level level) : mLog(log), mLevel(level) { }
    ~LogStream() { if(mLog) mLog->message(mStream.str(), mLevel); }

    LogStream(LogStream&& rhs) : mLog(nullptr), mLevel(Log::Level_Normal)
    {
        std::swap(mLog, rhs.mLog);
        std::swap(mLevel, rhs.mLevel);
        mStream.str(rhs.mStream.str());
        mStream.seekp(0, std::ios::end);
    }

    template<typename T>
    LogStream& operator<<(const T& val)
    {
        mStream << val;
        return *this;
    }
};

} // namespace TK

#endif /* LOG_HPP */
