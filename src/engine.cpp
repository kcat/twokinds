
#include "engine.hpp"

#include <stdexcept>
#include <iostream>
#include <sstream>

#include <OgreRoot.h>
#include <OgreSceneManager.h>
#include <OgreCamera.h>
#include <OgreRenderWindow.h>
#include <OgreViewport.h>

#include <SDL.h>
#include <SDL_syswm.h>


namespace TK
{

Engine::Engine(void)
  : mSDLWindow(nullptr)
  , mRoot(nullptr)
  , mWindow(nullptr)
  , mSceneMgr(nullptr)
  , mCamera(nullptr)
  , mViewport(nullptr)
{
}

Engine::~Engine(void)
{
    if(mRoot)
    {
        if(mSceneMgr)
        {
            if(mCamera)
                mSceneMgr->destroyCamera(mCamera);
            mCamera = nullptr;

            mRoot->destroySceneManager(mSceneMgr);
            mSceneMgr = nullptr;
        }

        if(mWindow)
        {
            if(mViewport)
                mWindow->removeViewport(mViewport);
            mViewport = nullptr;

            mWindow->destroy();
            mWindow = nullptr;
        }

        delete mRoot;
        mRoot = nullptr;
    }

    if(mSDLWindow)
    {
        // If we don't do this, the desktop resolution is not restored on exit
        SDL_SetWindowFullscreen(mSDLWindow, 0);
        SDL_DestroyWindow(mSDLWindow);
        mSDLWindow = nullptr;
    }
    SDL_Quit();
}


bool Engine::parseOptions(int argc, char *argv[])
{
    for(int i = 1;i < argc;i++)
    {
        {
            std::stringstream str;
            str<< "Unrecognized option: "<<argv[i];
            throw std::runtime_error(str.str());
        }
    }

    return true;
}


Ogre::RenderWindow *Engine::createRenderWindow(SDL_Window *win)
{
    // Get the native window handle
    struct SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    if(SDL_GetWindowWMInfo(win, &wmInfo) == SDL_FALSE)
        throw std::runtime_error("Couldn't get WM Info!");

    Ogre::NameValuePairList params;
    Ogre::String winHandle;
    switch(wmInfo.subsystem)
    {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
    case SDL_SYSWM_WINDOWS:
        winHandle = Ogre::StringConverter::toString((DWORD_PTR)wmInfo.info.win.window);
        break;
#endif
#if defined(SDL_VIDEO_DRIVER_COCOA)
    case SDL_SYSWM_COCOA:
        // Required to make OGRE play nice with our window
        params.insert(std::make_pair("macAPI", "cocoa"));
        params.insert(std::make_pair("macAPICocoaUseNSView", "true"));

        winHandle = Ogre::StringConverter::toString(WindowContentViewHandle(wmInfo));
        break;
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
    case SDL_SYSWM_X11:
        winHandle = Ogre::StringConverter::toString((unsigned long)wmInfo.info.x11.window);
        break;
#endif
    default:
        throw std::runtime_error("Unsupported windowing subsystem: " +
                                 Ogre::StringConverter::toString(wmInfo.subsystem));
    }
    /* TODO: externalWindowHandle is deprecated according to the source code.
     * Figure out a way to get parentWindowHandle to work properly. On Linux/
     * X11 it causes an occasional GLXBadDrawable error. */
    params.insert(std::make_pair("externalWindowHandle", std::move(winHandle)));

    int width, height;
    Ogre::String title = SDL_GetWindowTitle(win);
    SDL_GetWindowSize(win, &width, &height);
    bool fs = (SDL_GetWindowFlags(win)&SDL_WINDOW_FULLSCREEN);
    return mRoot->createRenderWindow(title, width, height, fs, &params);
}


void Engine::handleWindowEvent(const SDL_WindowEvent &evt)
{
    switch(evt.event)
    {
        case SDL_WINDOWEVENT_SHOWN:
            mWindow->setVisible(true);
            break;

        case SDL_WINDOWEVENT_HIDDEN:
            mWindow->setVisible(false);
            break;

        case SDL_WINDOWEVENT_EXPOSED:
            // Needs redraw
            break;

        case SDL_WINDOWEVENT_ENTER:
        case SDL_WINDOWEVENT_LEAVE:
        case SDL_WINDOWEVENT_FOCUS_GAINED:
        case SDL_WINDOWEVENT_FOCUS_LOST:
            break;

        case SDL_WINDOWEVENT_CLOSE:
            // FIXME: Inject an SDL_QUIT event? Seems to happen anyway...
            break;

        default:
            std::cerr<< "Unhandled window event: "<<(int)evt.event <<std::endl;
    }
}

bool Engine::pumpEvents()
{
    SDL_PumpEvents();

    SDL_Event evt;
    while(SDL_PollEvent(&evt))
    {
        switch(evt.type)
        {
        case SDL_WINDOWEVENT:
            handleWindowEvent(evt.window);
            break;

        case SDL_KEYDOWN:
        case SDL_KEYUP:
            break;

        case SDL_QUIT:
            return false;
        }
    }

    return true;
}


bool Engine::go(void)
{
    // Kindly ask SDL not to trash our OGL context
    SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");

    // Init everything except audio (we will use OpenAL for that)
    if(SDL_Init(SDL_INIT_EVERYTHING & ~SDL_INIT_AUDIO) != 0)
    {
        std::cerr<< "SDL_Init Error: "<<SDL_GetError() <<std::endl;
        return false;
    }

    // Construct Ogre::Root
    mRoot = new Ogre::Root("plugins.cfg");

    // Configure
    {
        // Show the configuration dialog and initialise the system
        if(!(mRoot->restoreConfig() || mRoot->showConfigDialog()))
            return false;

        int width = 1280;
        int height = 720;
        int xpos = SDL_WINDOWPOS_CENTERED;
        int ypos = SDL_WINDOWPOS_CENTERED;
        Uint32 flags = SDL_WINDOW_SHOWN;

        mSDLWindow = SDL_CreateWindow("Twokinds", xpos, ypos, width, height, flags);
        if(mSDLWindow == nullptr)
        {
            std::cerr<< "SDL_CreateWindow Error: "<<SDL_GetError() <<std::endl;
            return false;
        }

        mRoot->initialise(false);
        mWindow = createRenderWindow(mSDLWindow);
    }

    // Initialise all resource groups
    Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();

    // And away we go!
    mRoot->addFrameListener(this);
    mRoot->startRendering();

    return true;
}


bool Engine::frameRenderingQueued(const Ogre::FrameEvent &evt)
{
    if(!pumpEvents() || mWindow->isClosed())
        return false;

    const Uint8 *keystate = SDL_GetKeyboardState(NULL);
    if(keystate[SDL_SCANCODE_ESCAPE])
        return false;

    return true;
}

} // namespace TK
