#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <OgreWindowEventUtilities.h>
#include <OgreFrameListener.h>

struct SDL_Window;
struct SDL_WindowEvent;
struct SDL_MouseMotionEvent;
struct SDL_MouseWheelEvent;
struct SDL_MouseButtonEvent;
struct SDL_KeyboardEvent;
struct SDL_TextInputEvent;

namespace Ogre
{
    class Root;
    class RenderWindow;
    class SceneManager;
    class PageManager;
    class Light;
    class Camera;
    class Viewport;
}

namespace TK
{

class Input;
class Gui;

class Engine : public Ogre::WindowEventListener, public Ogre::FrameListener
{
    typedef void (Engine::*CmdFuncT)(const std::string&);
    typedef std::map<std::string,CmdFuncT> CommandFuncMap;

    SDL_Window *mSDLWindow;

    Ogre::Root *mRoot;
    Ogre::RenderWindow *mWindow;
    Ogre::SceneManager *mSceneMgr;
    Ogre::Camera *mCamera;
    Ogre::Viewport *mViewport; // Not used with Ogre 2.0!

    Input *mInput;
    Gui *mGui;

    bool mDisplayDebugStats;

    const CommandFuncMap mCommandFuncs;

    Ogre::RenderWindow *createRenderWindow(SDL_Window *win);

    void handleWindowEvent(const SDL_WindowEvent &evt);
    bool pumpEvents();

    void quitCmd(const std::string &value);
    void toggleBoundingBoxCmd(const std::string &value);
    void toggleDebugDisplayCmd(const std::string &value);
    void saveCfgCmd(const std::string &value);
    void internalCommand(const std::string &key, const std::string &value);

    virtual bool frameStarted(const Ogre::FrameEvent &evt);
    virtual bool frameRenderingQueued(const Ogre::FrameEvent &evt);

public:
    Engine(void);
    virtual ~Engine(void);

    bool parseOptions(int argc, char *argv[]);

    bool go(void);
};

} // namespace TK

#endif /* ENGINE_HPP */
