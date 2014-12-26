
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

    // construct Ogre::Root
    mRoot = new Ogre::Root("plugins.cfg");

    // configure
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

    SDL_Event evt;
    while(SDL_WaitEvent(&evt))
    {
        switch(evt.type)
        {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            return true;

        case SDL_QUIT:
            return false;
        }
    }

    return true;
}

} // namespace TK
