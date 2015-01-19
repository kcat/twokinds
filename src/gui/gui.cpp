
#include "gui.hpp"

#include <MyGUI.h>
#include <MyGUI_OgrePlatform.h>

namespace TK
{

Gui::Gui(Ogre::RenderWindow *window, Ogre::SceneManager *sceneMgr)
{
    try {
        mPlatform = new MyGUI::OgrePlatform();
        mPlatform->initialise(window, sceneMgr, "GUI");
        try {
            mGui = new MyGUI::Gui();
            mGui->initialise();
        }
        catch(...) {
            delete mGui;
            mGui = nullptr;
            mPlatform->shutdown();
            throw;
        }
    }
    catch(...) {
        delete mPlatform;
        mPlatform = nullptr;
        throw;
    }
}

Gui::~Gui()
{
    mGui->shutdown();
    delete mGui;
    mGui = nullptr;

    mPlatform->shutdown();
    delete mPlatform;
    mPlatform = nullptr;
}


} // namespace TK
