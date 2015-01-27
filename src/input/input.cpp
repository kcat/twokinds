
#include "input.hpp"

#include <SDL.h>

#include "gui/iface.hpp"

namespace TK
{

template<>
InputIface *Singleton<InputIface>::sInstance = nullptr;


Input::Input()
  : mMouseX(0)
  , mMouseY(0)
  , mMouseZ(0)
{
}

Input::~Input()
{
}


void Input::handleMouseMotionEvent(const SDL_MouseMotionEvent &evt)
{
    mMouseX = evt.x;
    mMouseY = evt.y;
    GuiIface::get().mouseMoved(mMouseX, mMouseY, mMouseZ);
}

void Input::handleMouseWheelEvent(const SDL_MouseWheelEvent &evt)
{
    mMouseZ += evt.y;
    GuiIface::get().mouseMoved(mMouseX, mMouseY, mMouseZ);
}

void Input::handleMouseButtonEvent(const SDL_MouseButtonEvent &evt)
{
    if(evt.state == SDL_PRESSED)
        GuiIface::get().mousePressed(evt.x, evt.y, evt.button);
    else if(evt.state == SDL_RELEASED)
        GuiIface::get().mouseReleased(evt.x, evt.y, evt.button);
}

void Input::handleKeyboardEvent(const SDL_KeyboardEvent &evt)
{
    if(evt.state == SDL_PRESSED)
    {
        if(!evt.repeat)
            GuiIface::get().injectKeyPress(evt.keysym.sym);
    }
    else if(evt.state == SDL_RELEASED)
        GuiIface::get().injectKeyRelease(evt.keysym.sym);
}

void Input::handleTextInputEvent(const SDL_TextInputEvent &evt)
{
    GuiIface::get().injectTextInput(evt.text);
}

} // namespace TK
