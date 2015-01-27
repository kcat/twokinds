
#include "input.hpp"

#include <SDL.h>

#include "gui/iface.hpp"

namespace TK
{

template<>
InputIface *Singleton<InputIface>::sInstance = nullptr;


Input::Input()
{
}

Input::~Input()
{
}


void Input::handleMouseMotionEvent(const SDL_MouseMotionEvent &evt)
{
    GuiIface::get().mouseMoved(evt.x, evt.y);
}

void Input::handleMouseWheelEvent(const SDL_MouseWheelEvent &evt)
{
    GuiIface::get().mouseWheel(evt.y);
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
