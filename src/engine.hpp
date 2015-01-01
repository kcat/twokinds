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
    class Light;
    class Camera;
    class Viewport;
    class Terrain;
    class TerrainGroup;
    class TerrainGlobalOptions;
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
    Ogre::Viewport *mViewport; // Not used with Ogre 2.0!

    Ogre::TerrainGroup  *mTerrainGroup;
    Ogre::TerrainGlobalOptions *mTerrainGlobals;
    bool mTerrainsImported;

    Ogre::RenderWindow *createRenderWindow(SDL_Window *win);

    void handleWindowEvent(const SDL_WindowEvent &evt);
    bool pumpEvents();

    void getTerrainImage(bool flipX, bool flipY, Ogre::Image &img);
    void defineTerrain(long x, long y);
    void initBlendMaps(Ogre::Terrain *terrain);
    void configureTerrainDefaults(Ogre::Light *light);

    virtual bool frameRenderingQueued(const Ogre::FrameEvent &evt);

public:
    Engine(void);
    virtual ~Engine(void);

    bool parseOptions(int argc, char *argv[]);

    bool go(void);
};

} // namespace TK

#endif /* ENGINE_HPP */
