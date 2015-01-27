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
        Mode_Console = 1<<0,

        Mode_Highest = Mode_Console
    };

    virtual void printToConsole(const std::string &str) = 0;

    virtual void addConsoleCallback(const char *command, CommandDelegateT *delegate) = 0;

    /**
     * Enable a specific GUI mode. The mode is not necessarily top level, so
     * another mode may have and continue to take precedence.
     */
    virtual void pushMode(Mode mode) = 0;
    /** Disable a specific GUI mode, so that it no longer shows up. */
    virtual void popMode(Mode mode) = 0;
    /**
     * Tests if the specified GUI mode is enabled or not.
     *
     * \return false if the given \param mode is not enabled.
     */
    virtual bool testMode(Mode mode) = 0;
    /** \return The current top-level GUI mode. */
    virtual Mode getMode() const = 0;

    virtual void mouseMoved(int x, int y, int z) = 0;
    virtual void mousePressed(int x, int y, int button) = 0;
    virtual void mouseReleased(int x, int y, int button) = 0;
    virtual void injectKeyPress(SDL_Keycode code) = 0;
    virtual void injectKeyRelease(SDL_Keycode code) = 0;
    virtual void injectTextInput(const char *text) = 0;
};

} // namespace TK

#endif /* GUI_IFACE_HPP */
