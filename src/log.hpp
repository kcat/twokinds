#ifndef LOG_HPP
#define LOG_HPP

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "singleton.hpp"


namespace TK
{

class GuiIface;

class LogStream;

class Log : public Singleton<Log> {
public:
    enum Level {
        Level_Debug,
        Level_Normal,
        Level_Error,
    };

private:
    Level mLevel;
    GuiIface *mGui;
    std::vector<std::string> mBuffer;
    std::ofstream mOutfile;

    static std::string getTimestamp();

public:
    Log(Level level=Level_Normal, const std::string &name=std::string());
    virtual ~Log();

    void setLog(const std::string &name);
    void setLevel(Level level) { mLevel = level; }
    Level getLevel() const { return mLevel; }

    void setGuiIface(GuiIface *iface);

    LogStream stream(Level level=Level_Normal);
    void message(const std::string &msg, Level level=Level_Normal);
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
