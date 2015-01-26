#ifndef GUI_GUI_HPP
#define GUI_GUI_HPP

#include <string>

#include <SDL_keycode.h>

#include "iface.hpp"


namespace Ogre
{
    class RenderWindow;
    class SceneManager;
}

namespace MyGUI
{
    class OgrePlatform;
    class Gui;
    class TextBox;
}


namespace TK
{

class Console;

class Gui : public GuiIface {
    MyGUI::OgrePlatform *mPlatform;
    MyGUI::Gui *mGui;

    MyGUI::TextBox *mStatusMessages;

    Console *mConsole;

    int mMouseX;
    int mMouseY;
    int mMouseZ;

public:
    enum Mode {
        Mode_Game,
        Mode_Console
    };

    Gui(Ogre::RenderWindow *window, Ogre::SceneManager *sceneMgr);
    virtual ~Gui();

    virtual void printToConsole(const std::string &str);

    virtual void addConsoleCallback(const char *command, CommandDelegateT *delegate);

    Mode getMode() const;

    void updateStatus(const std::string &str);

    void mouseMoved(int x, int y);
    void mouseWheel(int z); /* Value is relative */
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void injectKeyPress(SDL_Keycode code);
    void injectKeyRelease(SDL_Keycode code);
    void injectTextInput(const char *text);
};

} // namespace TK

#endif /* GUI_GUI_HPP */
