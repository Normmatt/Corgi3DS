#include <fstream>
#include "emuwindow.hpp"

EmuWindow::EmuWindow()
{
	SDL_Init(SDL_INIT_EVERYTHING);

	int posX = 100, posY = 100, width = 400, height = 240 * 2;

	win = SDL_CreateWindow("Corgi3DS - SDL", posX, posY, width, height, 0);
	if (win == NULL) {
		printf("Error creating SDL window\n");
		exit(1);
	}

	renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);

    running = true;
    pad_state = 0;
}

EmuWindow::~EmuWindow()
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(win);
	win = NULL;

	// Quit SDL subsystems
	SDL_Quit();
}

void EmuWindow::draw(uint8_t *top_screen, uint8_t *bottom_screen)
{
	int width = 240;
	int height = 400;
	int depth = 32;
	int pitch = 4 * width;
	Uint32 pixel_format = SDL_PIXELFORMAT_RGBA32;

	SDL_Surface* top = SDL_CreateRGBSurfaceWithFormatFrom((void*)top_screen, width, height, depth, pitch, pixel_format);
	SDL_Texture* texture_top = SDL_CreateTextureFromSurface(renderer, top);

	height = 320;
	SDL_Surface* bottom = SDL_CreateRGBSurfaceWithFormatFrom((void*)bottom_screen, width, height, depth, pitch, pixel_format);
	SDL_Texture* texture_bottom = SDL_CreateTextureFromSurface(renderer, bottom);

	SDL_Point center;
	center.x = 0;
	center.y = 0;

	SDL_Rect soos;
	soos.x = 0;
	soos.y = 0;
	soos.w = 240;
	soos.h = 400;

	SDL_Rect dest;
	dest.x = 0;
	dest.y = 240;
	dest.w = 240;
	dest.h = 400;
	SDL_RenderCopyEx(renderer, texture_top, &soos, &dest, 270.0F, &center, SDL_FLIP_NONE);

	soos.x = 0;
	soos.y = 0;
	soos.w = 240;
	soos.h = 320;

	dest.x = 40;
	dest.y = 480;
	dest.w = 240;
	dest.h = 320;
	SDL_RenderCopyEx(renderer, texture_bottom, &soos, &dest, 270.0F, &center, SDL_FLIP_NONE);

	SDL_RenderPresent(renderer);

	SDL_DestroyTexture(texture_top);
	SDL_FreeSurface(top);

	SDL_DestroyTexture(texture_bottom);
	SDL_FreeSurface(bottom);
}

static HID_PAD_STATE translate_key(const SDL_KeyboardEvent* key)
{
    switch (SDL_GetScancodeFromKey(key->keysym.sym))
    {
        case SDL_SCANCODE_UP:
            return PAD_UP;
        case SDL_SCANCODE_DOWN:
			return PAD_DOWN;
        case SDL_SCANCODE_LEFT:
			return PAD_LEFT;
        case SDL_SCANCODE_RIGHT:
			return PAD_RIGHT;
        case SDL_SCANCODE_Z:
			return PAD_B;
        case SDL_SCANCODE_X:
			return PAD_A;
        case SDL_SCANCODE_A:
			return PAD_Y;
        case SDL_SCANCODE_S:
			return PAD_X;
        case SDL_SCANCODE_Q:
			return PAD_L;
        case SDL_SCANCODE_W:
			return PAD_R;
        case SDL_SCANCODE_RETURN:
			return PAD_START;
		default:
			return PAD_A;

    }
}

void EmuWindow::press_key(HID_PAD_STATE state)
{
    pad_state |= 1 << state;
}

void EmuWindow::release_key(HID_PAD_STATE state)
{
    pad_state &= ~(1 << state);
}

void EmuWindow::handle_events()
{
	SDL_Event e;

	while (SDL_PollEvent(&e) != 0) {
		switch (e.type) {
		case SDL_KEYUP:
			release_key(translate_key(&e.key));
			break;
		case SDL_KEYDOWN:
			press_key(translate_key(&e.key));
			break;
		case SDL_QUIT:
			exit(0);
			break;
		default:
			break;
		}
	}
}
