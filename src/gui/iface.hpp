#ifndef GUI_IFACE_HPP
#define GUI_IFACE_HPP

#include <string>

namespace TK
{

template<typename ...Args> class IDelegate;
typedef IDelegate<const std::string&,const std::string&> CommandDelegateT;

class GuiIface {
    static GuiIface *sInstance;

public:
    GuiIface();
    virtual ~GuiIface();

    virtual void printToConsole(const std::string &str) = 0;

    virtual void addConsoleCallback(const char *command, CommandDelegateT *delegate) = 0;

    static GuiIface& get() { return *sInstance; }
};

} // namespace TK

#endif /* GUI_IFACE_HPP */
