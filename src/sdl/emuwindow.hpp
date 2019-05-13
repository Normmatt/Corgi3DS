#ifndef EMUWINDOW_HPP
#define EMUWINDOW_HPP

#define SDL_MAIN_HANDLED
#ifdef _MSC_VER
#include <SDL.h>
#else
#include <SDL2/SDL.h>
#endif
#include <cstdint>

enum HID_PAD_STATE
{
    PAD_A,
    PAD_B,
    PAD_SELECT,
    PAD_START,
    PAD_RIGHT,
    PAD_LEFT,
    PAD_UP,
    PAD_DOWN,
    PAD_R,
    PAD_L,
    PAD_X,
    PAD_Y
};

class EmuWindow
{
    private:
        bool running;
		SDL_Window* win;
		SDL_Renderer* renderer;
        uint16_t pad_state;

        void press_key(HID_PAD_STATE state);
        void release_key(HID_PAD_STATE state);
    public:
        EmuWindow();
		~EmuWindow();

        uint16_t get_pad_state();
        bool is_running();
		void handle_events();

        void draw(uint8_t* top_screen, uint8_t* bottom_screen);
};

inline bool EmuWindow::is_running()
{
    return running;
}

inline uint16_t EmuWindow::get_pad_state()
{
    return pad_state;
}

#endif // EMUWINDOW_HPP
