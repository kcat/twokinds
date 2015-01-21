#ifndef GUI_GUI_HPP
#define GUI_GUI_HPP

#include <string>

#include <SDL_keycode.h>

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

class Gui {
    MyGUI::OgrePlatform *mPlatform;
    MyGUI::Gui *mGui;

    MyGUI::TextBox *mStatusMessages;

    Console *mConsole;

public:
    enum Mode {
        Mode_Game,
        Mode_Console
    };

    Gui(Ogre::RenderWindow *window, Ogre::SceneManager *sceneMgr);
    virtual ~Gui();

    Mode getMode() const;

    void updateStatus(const std::string &str);

    void mouseMoved(int x, int y, int z);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void injectKeyPress(SDL_Keycode code);
    void injectKeyRelease(SDL_Keycode code);
    void injectTextInput(const char *text);
};

} // namespace TK

#endif /* GUI_GUI_HPP */
