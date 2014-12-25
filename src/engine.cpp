
#include "engine.hpp"

#include <stdexcept>
#include <iostream>
#include <sstream>

#include <SDL.h>
#include <SDL_syswm.h>


namespace TK
{

Engine::Engine(void)
  : mSDLWindow(nullptr)
{
}

Engine::~Engine(void)
{
    // If we don't do this, the desktop resolution is not restored on exit
    if(mSDLWindow)
    {
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

    // configure
    {
        int width = 1024;
        int height = 768;
        int xpos = SDL_WINDOWPOS_CENTERED;
        int ypos = SDL_WINDOWPOS_CENTERED;
        Uint32 flags = SDL_WINDOW_SHOWN;

        mSDLWindow = SDL_CreateWindow("Twokinds", xpos, ypos, width, height, flags);
        if(mSDLWindow == nullptr)
        {
            std::cerr<< "SDL_CreateWindow Error: "<<SDL_GetError() <<std::endl;
            return false;
        }
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
