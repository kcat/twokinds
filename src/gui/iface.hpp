#ifndef GUI_IFACE_HPP
#define GUI_IFACE_HPP

#include <string>

#include <SDL_keycode.h>

#include "singleton.hpp"


namespace TK
{

template<typename ...Args> class IDelegate;
typedef IDelegate<const std::string&,const std::string&> CommandDelegateT;

class GuiIface : public Singleton<GuiIface> {
public:
    enum Mode {
        Mode_Game,
        Mode_Console
    };

    virtual void printToConsole(const std::string &str) = 0;

    virtual void addConsoleCallback(const char *command, CommandDelegateT *delegate) = 0;

    virtual Mode getMode() const = 0;

    virtual void mouseMoved(int x, int y) = 0;
    virtual void mouseWheel(int z) = 0; /* Value is relative */
    virtual void mousePressed(int x, int y, int button) = 0;
    virtual void mouseReleased(int x, int y, int button) = 0;
    virtual void injectKeyPress(SDL_Keycode code) = 0;
    virtual void injectKeyRelease(SDL_Keycode code) = 0;
    virtual void injectTextInput(const char *text) = 0;
};

} // namespace TK

#endif /* GUI_IFACE_HPP */
