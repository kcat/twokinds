
#include "gui.hpp"

#include <MyGUI.h>
#include <MyGUI_OgrePlatform.h>

#include <SDL_scancode.h>
#include <SDL_mouse.h>

namespace
{
    const std::map<int,MyGUI::KeyCode> SDLScancode2MyGUIKeycode{
        {SDL_SCANCODE_UNKNOWN, MyGUI::KeyCode::None},

        {SDL_SCANCODE_A, MyGUI::KeyCode::A},
        {SDL_SCANCODE_B, MyGUI::KeyCode::B},
        {SDL_SCANCODE_C, MyGUI::KeyCode::C},
        {SDL_SCANCODE_D, MyGUI::KeyCode::D},
        {SDL_SCANCODE_E, MyGUI::KeyCode::E},
        {SDL_SCANCODE_F, MyGUI::KeyCode::F},
        {SDL_SCANCODE_G, MyGUI::KeyCode::G},
        {SDL_SCANCODE_H, MyGUI::KeyCode::H},
        {SDL_SCANCODE_I, MyGUI::KeyCode::I},
        {SDL_SCANCODE_J, MyGUI::KeyCode::J},
        {SDL_SCANCODE_K, MyGUI::KeyCode::K},
        {SDL_SCANCODE_L, MyGUI::KeyCode::L},
        {SDL_SCANCODE_M, MyGUI::KeyCode::M},
        {SDL_SCANCODE_N, MyGUI::KeyCode::N},
        {SDL_SCANCODE_O, MyGUI::KeyCode::O},
        {SDL_SCANCODE_P, MyGUI::KeyCode::P},
        {SDL_SCANCODE_Q, MyGUI::KeyCode::Q},
        {SDL_SCANCODE_R, MyGUI::KeyCode::R},
        {SDL_SCANCODE_S, MyGUI::KeyCode::S},
        {SDL_SCANCODE_T, MyGUI::KeyCode::T},
        {SDL_SCANCODE_U, MyGUI::KeyCode::U},
        {SDL_SCANCODE_V, MyGUI::KeyCode::V},
        {SDL_SCANCODE_W, MyGUI::KeyCode::W},
        {SDL_SCANCODE_X, MyGUI::KeyCode::X},
        {SDL_SCANCODE_Y, MyGUI::KeyCode::Y},
        {SDL_SCANCODE_Z, MyGUI::KeyCode::Z},

        {SDL_SCANCODE_1, MyGUI::KeyCode::One},
        {SDL_SCANCODE_2, MyGUI::KeyCode::Two},
        {SDL_SCANCODE_3, MyGUI::KeyCode::Three},
        {SDL_SCANCODE_4, MyGUI::KeyCode::Four},
        {SDL_SCANCODE_5, MyGUI::KeyCode::Five},
        {SDL_SCANCODE_6, MyGUI::KeyCode::Six},
        {SDL_SCANCODE_7, MyGUI::KeyCode::Seven},
        {SDL_SCANCODE_8, MyGUI::KeyCode::Eight},
        {SDL_SCANCODE_9, MyGUI::KeyCode::Nine},
        {SDL_SCANCODE_0, MyGUI::KeyCode::Zero},

        {SDL_SCANCODE_RETURN,    MyGUI::KeyCode::Return},
        {SDL_SCANCODE_ESCAPE,    MyGUI::KeyCode::Escape},
        {SDL_SCANCODE_BACKSPACE, MyGUI::KeyCode::Backspace},
        {SDL_SCANCODE_TAB,   MyGUI::KeyCode::Tab},
        {SDL_SCANCODE_SPACE, MyGUI::KeyCode::Space},

        {SDL_SCANCODE_MINUS,  MyGUI::KeyCode::Minus},
        {SDL_SCANCODE_EQUALS, MyGUI::KeyCode::Equals},
        {SDL_SCANCODE_LEFTBRACKET,  MyGUI::KeyCode::LeftBracket},
        {SDL_SCANCODE_RIGHTBRACKET, MyGUI::KeyCode::RightBracket},
        {SDL_SCANCODE_BACKSLASH, MyGUI::KeyCode::Backslash},

        {SDL_SCANCODE_SEMICOLON, MyGUI::KeyCode::Semicolon},
        {SDL_SCANCODE_APOSTROPHE, MyGUI::KeyCode::Apostrophe},
        {SDL_SCANCODE_GRAVE, MyGUI::KeyCode::Grave},

        {SDL_SCANCODE_COMMA, MyGUI::KeyCode::Comma},
        {SDL_SCANCODE_PERIOD, MyGUI::KeyCode::Period},
        {SDL_SCANCODE_SLASH, MyGUI::KeyCode::Slash},

        {SDL_SCANCODE_CAPSLOCK, MyGUI::KeyCode::Capital},

        {SDL_SCANCODE_F1, MyGUI::KeyCode::F1},
        {SDL_SCANCODE_F2, MyGUI::KeyCode::F2},
        {SDL_SCANCODE_F3, MyGUI::KeyCode::F3},
        {SDL_SCANCODE_F4, MyGUI::KeyCode::F4},
        {SDL_SCANCODE_F5, MyGUI::KeyCode::F5},
        {SDL_SCANCODE_F6, MyGUI::KeyCode::F6},
        {SDL_SCANCODE_F7, MyGUI::KeyCode::F7},
        {SDL_SCANCODE_F8, MyGUI::KeyCode::F8},
        {SDL_SCANCODE_F9, MyGUI::KeyCode::F9},
        {SDL_SCANCODE_F10, MyGUI::KeyCode::F10},
        {SDL_SCANCODE_F11, MyGUI::KeyCode::F11},
        {SDL_SCANCODE_F12, MyGUI::KeyCode::F12},

        {SDL_SCANCODE_PRINTSCREEN, MyGUI::KeyCode::SysRq},
        {SDL_SCANCODE_SCROLLLOCK, MyGUI::KeyCode::ScrollLock},
        {SDL_SCANCODE_PAUSE, MyGUI::KeyCode::Pause},

        {SDL_SCANCODE_INSERT, MyGUI::KeyCode::Insert},
        {SDL_SCANCODE_HOME, MyGUI::KeyCode::Home},
        {SDL_SCANCODE_PAGEUP, MyGUI::KeyCode::PageUp},
        {SDL_SCANCODE_DELETE, MyGUI::KeyCode::Delete},
        {SDL_SCANCODE_END, MyGUI::KeyCode::End},
        {SDL_SCANCODE_PAGEDOWN, MyGUI::KeyCode::PageDown},

        {SDL_SCANCODE_RIGHT, MyGUI::KeyCode::ArrowRight},
        {SDL_SCANCODE_LEFT, MyGUI::KeyCode::ArrowLeft},
        {SDL_SCANCODE_DOWN, MyGUI::KeyCode::ArrowDown},
        {SDL_SCANCODE_UP, MyGUI::KeyCode::ArrowUp},

        {SDL_SCANCODE_NUMLOCKCLEAR, MyGUI::KeyCode::NumLock},
        {SDL_SCANCODE_KP_DIVIDE, MyGUI::KeyCode::Divide},
        {SDL_SCANCODE_KP_MULTIPLY, MyGUI::KeyCode::Multiply},
        {SDL_SCANCODE_KP_MINUS, MyGUI::KeyCode::Subtract},
        {SDL_SCANCODE_KP_PLUS, MyGUI::KeyCode::Add},
        {SDL_SCANCODE_KP_ENTER, MyGUI::KeyCode::NumpadEnter},
        {SDL_SCANCODE_KP_1, MyGUI::KeyCode::Numpad1},
        {SDL_SCANCODE_KP_2, MyGUI::KeyCode::Numpad2},
        {SDL_SCANCODE_KP_3, MyGUI::KeyCode::Numpad3},
        {SDL_SCANCODE_KP_4, MyGUI::KeyCode::Numpad4},
        {SDL_SCANCODE_KP_5, MyGUI::KeyCode::Numpad5},
        {SDL_SCANCODE_KP_6, MyGUI::KeyCode::Numpad6},
        {SDL_SCANCODE_KP_7, MyGUI::KeyCode::Numpad7},
        {SDL_SCANCODE_KP_8, MyGUI::KeyCode::Numpad8},
        {SDL_SCANCODE_KP_9, MyGUI::KeyCode::Numpad9},
        {SDL_SCANCODE_KP_0, MyGUI::KeyCode::Numpad0},
        {SDL_SCANCODE_KP_PERIOD, MyGUI::KeyCode::Decimal},

        {SDL_SCANCODE_LCTRL, MyGUI::KeyCode::LeftControl},
        {SDL_SCANCODE_LSHIFT, MyGUI::KeyCode::RightControl},
        {SDL_SCANCODE_LALT, MyGUI::KeyCode::LeftAlt},
        {SDL_SCANCODE_LGUI, MyGUI::KeyCode::LeftWindows},
        {SDL_SCANCODE_RCTRL, MyGUI::KeyCode::RightControl},
        {SDL_SCANCODE_RSHIFT, MyGUI::KeyCode::RightShift},
        {SDL_SCANCODE_RALT, MyGUI::KeyCode::RightAlt},
        {SDL_SCANCODE_RGUI, MyGUI::KeyCode::RightWindows},
    };

    std::vector<unsigned int> utf8ToUnicode(const char *utf8)
    {
        std::vector<unsigned int> unicode;
        for(size_t i = 0;utf8[i];)
        {
            unsigned long uni;
            size_t todo;

            unsigned char ch = utf8[i++];
            if(ch <= 0x7F)
            {
                uni = ch;
                todo = 0;
            }
            else if(ch <= 0xBF)
                throw std::logic_error("not a UTF-8 string");
            else if(ch <= 0xDF)
            {
                uni = ch&0x1F;
                todo = 1;
            }
            else if(ch <= 0xEF)
            {
                uni = ch&0x0F;
                todo = 2;
            }
            else if(ch <= 0xF7)
            {
                uni = ch&0x07;
                todo = 3;
            }
            else
                throw std::logic_error("not a UTF-8 string");

            for(size_t j = 0;j < todo;++j)
            {
                if(utf8[i])
                    throw std::logic_error("not a UTF-8 string");
                unsigned char ch = utf8[i++];
                if (ch < 0x80 || ch > 0xBF)
                    throw std::logic_error("not a UTF-8 string");
                uni <<= 6;
                uni += ch & 0x3F;
            }
            if(uni >= 0xD800 && uni <= 0xDFFF)
                throw std::logic_error("not a UTF-8 string");
            if(uni > 0x10FFFF)
                throw std::logic_error("not a UTF-8 string");
            unicode.push_back(uni);
        }
        return unicode;
    }

} // namespace

namespace TK
{

Gui::Gui(Ogre::RenderWindow *window, Ogre::SceneManager *sceneMgr)
{
    try {
        mPlatform = new MyGUI::OgrePlatform();
        mPlatform->initialise(window, sceneMgr, "GUI");
        try {
            mGui = new MyGUI::Gui();
            mGui->initialise();
        }
        catch(...) {
            delete mGui;
            mGui = nullptr;
            mPlatform->shutdown();
            throw;
        }
    }
    catch(...) {
        delete mPlatform;
        mPlatform = nullptr;
        throw;
    }

    MyGUI::PointerManager::getInstance().setVisible(false);
    mStatusMessages = mGui->createWidgetReal<MyGUI::TextBox>("TextBox",
        MyGUI::FloatCoord(0.f, 0.f, 1.f, 0.25f), MyGUI::Align::Default,
        "Overlapped"
    );
    mStatusMessages->setTextShadow(true);
    mStatusMessages->setTextColour(MyGUI::Colour::White);
    mStatusMessages->setCaption("Here's some text, yay!");
}

Gui::~Gui()
{
    mGui->destroyWidget(mStatusMessages);
    mStatusMessages = nullptr;

    mGui->shutdown();
    delete mGui;
    mGui = nullptr;

    mPlatform->shutdown();
    delete mPlatform;
    mPlatform = nullptr;
}


void Gui::updateStatus(const std::string& str)
{
    // NOTE: Avoid extra work (UTF-8 -> UTF-16/32 conversion) if the string is empty
    mStatusMessages->setCaption(str.empty() ? MyGUI::UString() : MyGUI::UString(str));
}


void Gui::mouseMoved(int x, int y, int z)
{
    MyGUI::InputManager::getInstance().injectMouseMove(x, y, z);
}

void Gui::mousePressed(int x, int y, int button)
{
    MyGUI::MouseButton btn(MyGUI::MouseButton::None);
    if(button == SDL_BUTTON_LEFT)
        btn = MyGUI::MouseButton::Button0;
    else if(button == SDL_BUTTON_RIGHT)
        btn = MyGUI::MouseButton::Button1;
    else if(button == SDL_BUTTON_MIDDLE)
        btn = MyGUI::MouseButton::Button2;
    else if(button == SDL_BUTTON_X1)
        btn = MyGUI::MouseButton::Button3;
    else if(button == SDL_BUTTON_X2)
        btn = MyGUI::MouseButton::Button4;
    else
    {
        Ogre::LogManager::getSingleton().stream()<< "Unexpected SDL mouse button: "<<button;
        return;
    }
    MyGUI::InputManager::getInstance().injectMousePress(x, y, btn);
}

void Gui::mouseReleased(int x, int y, int button)
{
    MyGUI::MouseButton btn(MyGUI::MouseButton::None);
    if(button == SDL_BUTTON_LEFT)
        btn = MyGUI::MouseButton::Button0;
    else if(button == SDL_BUTTON_RIGHT)
        btn = MyGUI::MouseButton::Button1;
    else if(button == SDL_BUTTON_MIDDLE)
        btn = MyGUI::MouseButton::Button2;
    else if(button == SDL_BUTTON_X1)
        btn = MyGUI::MouseButton::Button3;
    else if(button == SDL_BUTTON_X2)
        btn = MyGUI::MouseButton::Button4;
    else
        return;
    MyGUI::InputManager::getInstance().injectMouseRelease(x, y, btn);
}


void Gui::injectKeyPress(int scancode, const char *text)
{
    std::vector<unsigned int> unicode = utf8ToUnicode(text);
    auto key = SDLScancode2MyGUIKeycode.find(scancode);
    if(key != SDLScancode2MyGUIKeycode.end())
        MyGUI::InputManager::getInstance().injectKeyPress(
            key->second, unicode.empty() ? 0 : unicode[0]
        );
    else
        Ogre::LogManager::getSingleton().stream()<< "Unexpected SDL scancode: "<<scancode;
}

void Gui::injectKeyRelease(int scancode)
{
    auto key = SDLScancode2MyGUIKeycode.find(scancode);
    if(key != SDLScancode2MyGUIKeycode.end())
        MyGUI::InputManager::getInstance().injectKeyRelease(key->second);
}


} // namespace TK
