#ifndef GUI_IFACE_HPP
#define GUI_IFACE_HPP

#include <string>

#include "singleton.hpp"


namespace TK
{

template<typename ...Args> class IDelegate;
typedef IDelegate<const std::string&,const std::string&> CommandDelegateT;

class GuiIface : public Singleton<GuiIface> {
public:
    virtual void printToConsole(const std::string &str) = 0;

    virtual void addConsoleCallback(const char *command, CommandDelegateT *delegate) = 0;
};

} // namespace TK

#endif /* GUI_IFACE_HPP */
