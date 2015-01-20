#ifndef GUI_GUI_HPP
#define GUI_GUI_HPP

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

class Gui {
    MyGUI::OgrePlatform *mPlatform;
    MyGUI::Gui *mGui;

    MyGUI::TextBox *mStatusMessages;

public:
    Gui(Ogre::RenderWindow *window, Ogre::SceneManager *sceneMgr);
    virtual ~Gui();

    void updateStatus(const std::string &str);

    void mouseMoved(int x, int y, int z);
    void mousePressed(int x, int y, int button);
    void mouseReleased(int x, int y, int button);
    void injectKeyPress(int scancode, const char *text);
    void injectKeyRelease(int scancode);
};

} // namespace TK

#endif /* GUI_GUI_HPP */
