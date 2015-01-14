#ifndef ENGINE_HPP
#define ENGINE_HPP

#include <OgreWindowEventUtilities.h>
#include <OgreFrameListener.h>

struct SDL_Window;
struct SDL_WindowEvent;

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

class TerrainPageProvider;

class Engine : public Ogre::WindowEventListener, public Ogre::FrameListener
{
    SDL_Window *mSDLWindow;

    Ogre::Root *mRoot;
    Ogre::RenderWindow *mWindow;
    Ogre::SceneManager *mSceneMgr;
    Ogre::Camera *mCamera;
    Ogre::Viewport *mViewport; // Not used with Ogre 2.0!

    Ogre::RenderWindow *createRenderWindow(SDL_Window *win);

    void handleWindowEvent(const SDL_WindowEvent &evt);
    bool pumpEvents();

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
