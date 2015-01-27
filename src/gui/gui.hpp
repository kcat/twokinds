#ifndef GUI_GUI_HPP
#define GUI_GUI_HPP

#include "iface.hpp"

#include <string>


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
    Gui(Ogre::RenderWindow *window, Ogre::SceneManager *sceneMgr);
    virtual ~Gui();

    virtual void printToConsole(const std::string &str);

    virtual void addConsoleCallback(const char *command, CommandDelegateT *delegate);

    virtual Mode getMode() const;

    virtual void mouseMoved(int x, int y);
    virtual void mouseWheel(int z); /* Value is relative */
    virtual void mousePressed(int x, int y, int button);
    virtual void mouseReleased(int x, int y, int button);
    virtual void injectKeyPress(SDL_Keycode code);
    virtual void injectKeyRelease(SDL_Keycode code);
    virtual void injectTextInput(const char *text);

    void updateStatus(const std::string &str);
};

} // namespace TK

#endif /* GUI_GUI_HPP */
