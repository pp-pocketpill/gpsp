/* gameplaySP - NTC CHIP SDL backend
 *
 * Copyright (C) 2016 Fox Cunning <naprossen@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include "../common.h"

u32 gamepad_config_map[PLAT_BUTTON_COUNT] =
{
  BUTTON_ID_UP,                 // Up
  BUTTON_ID_LEFT,               // Left
  BUTTON_ID_DOWN,               // Down
  BUTTON_ID_RIGHT,              // Right
  BUTTON_ID_START,              // Start
  BUTTON_ID_SELECT,             // Select
  BUTTON_ID_L,                  // Ltrigger
  BUTTON_ID_R,                  // Rtrigger
  BUTTON_ID_FPS,                // A
  BUTTON_ID_A,                  // B
  BUTTON_ID_B,                  // X
  BUTTON_ID_MENU,               // Y
  BUTTON_ID_SAVESTATE,          // 1
  BUTTON_ID_LOADSTATE,          // 2
  BUTTON_ID_FASTFORWARD,        // 3
  BUTTON_ID_NONE,               // 4
  BUTTON_ID_MENU                // Space
};


void gpsp_plat_init(void)
{
	int r;
	SDL_Surface *screen;
	
	r = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_NOPARACHUTE);
	if (r)
	{
		fprintf(stderr, "chip/gpsp_plat_init: SDL_Init failed -- %s\n",
			SDL_GetError());
		exit(1);
	}
	
	screen = SDL_SetVideoMode(0, 0, 16, SDL_SWSURFACE);
	if (!screen)
	{
		fprintf(stderr, "chip/gpsp_plat_init: SDL_SetVideoMode failed -- %s\n",
			SDL_GetError());
		exit(2);
	}
	
	SDL_ShowCursor(0);
	screen_scale = fullscreen;
}



void gpsp_plat_quit(void)
{
	SDL_Quit();
}

