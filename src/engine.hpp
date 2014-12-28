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
    class Camera;
}

namespace TK
{

class Engine : public Ogre::WindowEventListener, public Ogre::FrameListener
{
    SDL_Window *mSDLWindow;

    Ogre::Root *mRoot;
    Ogre::RenderWindow *mWindow;
    Ogre::SceneManager *mSceneMgr;
    Ogre::Camera *mCamera;

    Ogre::RenderWindow *createRenderWindow(SDL_Window *win);

    void handleWindowEvent(const SDL_WindowEvent &evt);
    bool pumpEvents();

    virtual bool frameRenderingQueued(const Ogre::FrameEvent &evt);

public:
    Engine(void);
    virtual ~Engine(void);

    bool parseOptions(int argc, char *argv[]);

    bool go(void);
};

} // namespace TK

#endif /* ENGINE_HPP */
