#ifndef ENGINE_HPP
#define ENGINE_HPP

struct SDL_Window;

namespace Ogre
{
    class Root;
    class RenderWindow;
    class SceneManager;
    class Camera;
    class Viewport;
}

namespace TK
{

class Engine
{
    SDL_Window *mSDLWindow;

    Ogre::Root *mRoot;
    Ogre::RenderWindow *mWindow;
    Ogre::SceneManager *mSceneMgr;
    Ogre::Camera *mCamera;
    Ogre::Viewport *mViewport;

    Ogre::RenderWindow *createRenderWindow(SDL_Window *win);

public:
    Engine(void);
    virtual ~Engine(void);

    bool parseOptions(int argc, char *argv[]);

    bool go(void);
};

} // namespace TK

#endif /* ENGINE_HPP */
