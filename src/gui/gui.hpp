#ifndef GUI_GUI_HPP
#define GUI_GUI_HPP


namespace Ogre
{
    class RenderWindow;
    class SceneManager;
}

namespace MyGUI
{
    class OgrePlatform;
    class Gui;
}


namespace TK
{

class Gui {
    MyGUI::OgrePlatform *mPlatform;
    MyGUI::Gui *mGui;

public:
    Gui(Ogre::RenderWindow *window, Ogre::SceneManager *sceneMgr);
    virtual ~Gui();
};

} // namespace TK

#endif /* GUI_GUI_HPP */
