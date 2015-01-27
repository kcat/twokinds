#ifndef INPUT_INPUT_HPP
#define INPUT_INPUT_HPP

#include "iface.hpp"


struct SDL_MouseMotionEvent;
struct SDL_MouseWheelEvent;
struct SDL_MouseButtonEvent;
struct SDL_KeyboardEvent;
struct SDL_TextInputEvent;

namespace TK
{

class Input : public InputIface {
public:
    Input();
    virtual ~Input();

    void handleMouseMotionEvent(const SDL_MouseMotionEvent &evt);
    void handleMouseWheelEvent(const SDL_MouseWheelEvent &evt);
    void handleMouseButtonEvent(const SDL_MouseButtonEvent &evt);
    void handleKeyboardEvent(const SDL_KeyboardEvent &evt);
    void handleTextInputEvent(const SDL_TextInputEvent &evt);
};

} // namespace TK

#endif /* INPUT_INPUT_HPP */
