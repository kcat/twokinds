#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <OgreWindowEventUtilities.h>
#include <OgreFrameListener.h>

struct SDL_Window;
struct SDL_WindowEvent;
struct SDL_MouseMotionEvent;
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

class Gui;

class Engine : public Ogre::WindowEventListener, public Ogre::FrameListener
{
    SDL_Window *mSDLWindow;

    Ogre::Root *mRoot;
    Ogre::RenderWindow *mWindow;
    Ogre::SceneManager *mSceneMgr;
    Ogre::Camera *mCamera;
    Ogre::Viewport *mViewport; // Not used with Ogre 2.0!

    Gui *mGui;

    bool mDisplayDebugStats;

    Ogre::RenderWindow *createRenderWindow(SDL_Window *win);

    void handleWindowEvent(const SDL_WindowEvent &evt);
    void handleMouseMotionEvent(const SDL_MouseMotionEvent &evt);
    void handleMouseButtonEvent(const SDL_MouseButtonEvent &evt);
    void handleKeyboardEvent(const SDL_KeyboardEvent &evt);
    void handleTextInputEvent(const SDL_TextInputEvent &evt);
    bool pumpEvents();

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
