/**
 * @file unix_ref_sdl.c
 * @brief This file contains SDL specific stuff having to do with the OpenGL refresh
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "r_local.h"
#include "../client/cl_keys.h"
#include "../client/cl_input.h"

/*#include <SDL_opengl.h>*/

cvar_t *sdl_debug;
static SDL_Surface *surface;

/* power of two please */
#define MAX_KEYQ 64

struct {
	unsigned char key;
	int down;
} keyq[MAX_KEYQ];

static int keyq_head = 0;
static int keyq_tail = 0;

/* Console variables that we need to access from this module */

/*****************************************************************************/
/* MOUSE                                                                     */
/*****************************************************************************/

static qboolean mouse_active;
int mx, my;

/* this is inside the renderer shared lib, so these are called from vid_so */

/**
 * @brief
 */
void IN_Activate (qboolean active)
{
	mouse_active = qtrue;
}

/*****************************************************************************/

/**
  * @brief Translate the keys to ufo keys
  */
static int SDLateKey (SDL_keysym *keysym, int *key)
{
	int buf = 0;
	*key = 0;

	switch (keysym->sym) {
	case SDLK_KP9:
		*key = K_KP_PGUP;
		break;
	case SDLK_PAGEUP:
		*key = K_PGUP;
		break;
	case SDLK_KP3:
		*key = K_KP_PGDN;
		break;
	case SDLK_PAGEDOWN:
		*key = K_PGDN;
		break;
	case SDLK_KP7:
		*key = K_KP_HOME;
		break;
	case SDLK_HOME:
		*key = K_HOME;
		break;
	case SDLK_KP1:
		*key = K_KP_END;
		break;
	case SDLK_END:
		*key = K_END;
		break;
	case SDLK_KP4:
		*key = K_KP_LEFTARROW;
		break;
	case SDLK_LEFT:
		*key = K_LEFTARROW;
		break;
	case SDLK_KP6:
		*key = K_KP_RIGHTARROW;
		break;
	case SDLK_RIGHT:
		*key = K_RIGHTARROW;
		break;
	case SDLK_KP2:
		*key = K_KP_DOWNARROW;
		break;
	case SDLK_DOWN:
		*key = K_DOWNARROW;
		break;
	case SDLK_KP8:
		*key = K_KP_UPARROW;
		break;
	case SDLK_UP:
		*key = K_UPARROW;
		break;
	case SDLK_ESCAPE:
		*key = K_ESCAPE;
		break;
	case SDLK_KP_ENTER:
		*key = K_KP_ENTER;
		break;
	case SDLK_RETURN:
		*key = K_ENTER;
		break;
	case SDLK_TAB:
		*key = K_TAB;
		break;
	case SDLK_F1:
		*key = K_F1;
		break;
	case SDLK_F2:
		*key = K_F2;
		break;
	case SDLK_F3:
		*key = K_F3;
		break;
	case SDLK_F4:
		*key = K_F4;
		break;
	case SDLK_F5:
		*key = K_F5;
		break;
	case SDLK_F6:
		*key = K_F6;
		break;
	case SDLK_F7:
		*key = K_F7;
		break;
	case SDLK_F8:
		*key = K_F8;
		break;
	case SDLK_F9:
		*key = K_F9;
		break;
	case SDLK_F10:
		*key = K_F10;
		break;
	case SDLK_F11:
		*key = K_F11;
		break;
	case SDLK_F12:
		*key = K_F12;
		break;
	case SDLK_F13:
		*key = K_F13;
		break;
	case SDLK_F14:
		*key = K_F14;
		break;
	case SDLK_F15:
		*key = K_F15;
		break;
	case SDLK_BACKSPACE:
		*key = K_BACKSPACE;
		break;
	case SDLK_KP_PERIOD:
		*key = K_KP_DEL;
		break;
	case SDLK_DELETE:
		*key = K_DEL;
		break;
	case SDLK_PAUSE:
		*key = K_PAUSE;
		break;
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		*key = K_SHIFT;
		break;
	case SDLK_LCTRL:
	case SDLK_RCTRL:
		*key = K_CTRL;
		break;
	case SDLK_LMETA:
	case SDLK_RMETA:
	case SDLK_LALT:
	case SDLK_RALT:
		*key = K_ALT;
		break;
	case SDLK_LSUPER:
	case SDLK_RSUPER:
		*key = K_SUPER;
		break;
	case SDLK_KP5:
		*key = K_KP_5;
		break;
	case SDLK_INSERT:
		*key = K_INS;
		break;
	case SDLK_KP0:
		*key = K_KP_INS;
		break;
	case SDLK_KP_MULTIPLY:
		*key = '*';
		break;
	case SDLK_KP_PLUS:
		*key = K_KP_PLUS;
		break;
	case SDLK_KP_MINUS:
		*key = K_KP_MINUS;
		break;
	case SDLK_KP_DIVIDE:
		*key = K_KP_SLASH;
		break;
	/* suggestions on how to handle this better would be appreciated */
	case SDLK_WORLD_7:
		*key = '`';
		break;
	case SDLK_MODE:
		*key = K_MODE;
		break;
	case SDLK_COMPOSE:
		*key = K_COMPOSE;
		break;
	case SDLK_HELP:
		*key = K_HELP;
		break;
	case SDLK_PRINT:
		*key = K_PRINT;
		break;
	case SDLK_SYSREQ:
		*key = K_SYSREQ;
		break;
	case SDLK_BREAK:
		*key = K_BREAK;
		break;
	case SDLK_MENU:
		*key = K_MENU;
		break;
	case SDLK_POWER:
		*key = K_POWER;
		break;
	case SDLK_EURO:
		*key = K_EURO;
		break;
	case SDLK_UNDO:
		*key = K_UNDO;
		break;
	case SDLK_SCROLLOCK:
		*key = K_SCROLLOCK;
		break;
	case SDLK_NUMLOCK:
		*key = K_KP_NUMLOCK;
		break;
	case SDLK_CAPSLOCK:
		*key = K_CAPSLOCK;
		break;
	case SDLK_SPACE:
		*key = K_SPACE;
		break;
	default:
		if (!keysym->unicode && (keysym->sym >= ' ') && (keysym->sym <= '~'))
			*key = (int) keysym->sym;
		break;
	}
	if ((keysym->unicode & 0xFF80) == 0) {  /* maps to ASCII? */
		buf = (int) keysym->unicode & 0x7F;
		if (buf == '~')
			*key = '~'; /* console HACK */
	}
	if (sdl_debug->integer)
		Com_Printf("unicode: %hx keycode: %i key: %hx\n", keysym->unicode, *key, *key);

	return buf;
}

/**
 * @brief Debug function to print sdl key events
 */
static void printkey (const SDL_Event* event, int down)
{
	if (sdl_debug->integer) {
		Com_Printf("key name: %s (down: %i)", SDL_GetKeyName(event->key.keysym.sym), down);
		if (event->key.keysym.unicode) {
			Com_Printf(" unicode: %hx", event->key.keysym.unicode);
			if (event->key.keysym.unicode >= '0' && event->key.keysym.unicode <= '~')  /* printable? */
				Com_Printf(" (%c)", (unsigned char)(event->key.keysym.unicode));
		}
		Com_Printf("\n");
	}
}

/**
 * @brief
 */
static void GetEvent (SDL_Event *event)
{
	int key, mouse_buttonstate, p = 0;

	switch (event->type) {
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		switch (event->button.button) {
		case 1:
			mouse_buttonstate = K_MOUSE1;
			break;
		case 2:
			mouse_buttonstate = K_MOUSE3;
			break;
		case 3:
			mouse_buttonstate = K_MOUSE2;
			break;
		case 4:
			mouse_buttonstate = K_MWHEELUP;
			break;
		case 5:
			mouse_buttonstate = K_MWHEELDOWN;
			break;
		case 6:
			mouse_buttonstate = K_MOUSE4;
			break;
		case 7:
			mouse_buttonstate = K_MOUSE5;
			break;
		default:
			mouse_buttonstate = K_AUX1 + (event->button.button - 8) % 16;
			break;
		}
		keyq[keyq_head].down = event->type == SDL_MOUSEBUTTONDOWN ? qtrue : qfalse;
		keyq[keyq_head].key = mouse_buttonstate;
		keyq_head = (keyq_head + 1) & (MAX_KEYQ - 1);
		break;
	case SDL_MOUSEMOTION:
		if (mouse_active) {
			mx = event->motion.x;
			my = event->motion.y;
		}
		break;
	case SDL_KEYDOWN:
		printkey(event, 1);
		if (event->key.keysym.mod & KMOD_ALT && event->key.keysym.sym == SDLK_RETURN) {
			SDL_WM_ToggleFullScreen(surface);

			if (surface->flags & SDL_FULLSCREEN) {
				Cvar_SetValue("vid_fullscreen", 1);
			} else {
				Cvar_SetValue("vid_fullscreen", 0);
			}
			vid_fullscreen->modified = qfalse; /* we just changed it with SDL. */
			break; /* ignore this key */
		}

		if (event->key.keysym.mod & KMOD_CTRL && event->key.keysym.sym == SDLK_g) {
			SDL_GrabMode gm = SDL_WM_GrabInput(SDL_GRAB_QUERY);
			Cvar_SetValue("vid_grabmouse", (gm == SDL_GRAB_ON) ? 0 : 1);
			break; /* ignore this key */
		}

		p = SDLateKey(&event->key.keysym, &key);
		if (key) {
			keyq[keyq_head].key = key;
			keyq[keyq_head].down = qtrue;
			keyq_head = (keyq_head + 1) & (MAX_KEYQ - 1);
		} else if (p) {
			keyq[keyq_head].key = p;
			keyq[keyq_head].down = qtrue;
			keyq_head = (keyq_head + 1) & (MAX_KEYQ - 1);
		}
		break;
	case SDL_VIDEOEXPOSE:
		break;
	case SDL_KEYUP:
		printkey(event, 0);
		p = SDLateKey(&event->key.keysym, &key);
		if (key) {
			keyq[keyq_head].key = key;
			keyq[keyq_head].down = qfalse;
			keyq_head = (keyq_head + 1) & (MAX_KEYQ - 1);
		} else if (p) {
			keyq[keyq_head].key = p;
			keyq[keyq_head].down = qfalse;
			keyq_head = (keyq_head + 1) & (MAX_KEYQ - 1);
		}
		break;
	case SDL_QUIT:
		Cbuf_ExecuteText(EXEC_NOW, "quit");
		break;
	}

}

/**
 * @brief
 */
qboolean Rimp_Init (void)
{
	if (*r_driver->string)
		SDL_GL_LoadLibrary(r_driver->string);

	if (SDL_WasInit(SDL_INIT_AUDIO|SDL_INIT_CDROM|SDL_INIT_VIDEO) == 0) {
		if (SDL_Init(SDL_INIT_VIDEO) < 0) {
			Sys_Error("Video SDL_Init failed: %s\n", SDL_GetError());
			return qfalse;
		}
	} else if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
		if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
			Sys_Error("Video SDL_InitSubsystem failed: %s\n", SDL_GetError());
			return qfalse;
		}
	}

	SDL_EnableUNICODE(SDL_ENABLE);
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

	Rimp_InitGraphics(vid_fullscreen->integer);

	return qtrue;
}

#ifndef _WIN32
/**
 * @brief
 */
static void SetSDLIcon (void)
{
#include "../ports/linux/ufoicon.xbm"
	SDL_Surface *icon;
	SDL_Color color;
	Uint8 *ptr;
	unsigned int i, mask;

	icon = SDL_CreateRGBSurface(SDL_SWSURFACE, ufoicon_width, ufoicon_height, 8, 0, 0, 0, 0);
	if (icon == NULL)
		return; /* oh well... */
	SDL_SetColorKey(icon, SDL_SRCCOLORKEY, 0);

	color.r = color.g = color.b = 255;
	SDL_SetColors(icon, &color, 0, 1); /* just in case */
	color.r = color.b = 0;
	color.g = 16;
	SDL_SetColors(icon, &color, 1, 1);

	ptr = (Uint8 *)icon->pixels;
	for (i = 0; i < sizeof(ufoicon_bits); i++) {
		for (mask = 1; mask != 0x100; mask <<= 1) {
			*ptr = (ufoicon_bits[i] & mask) ? 1 : 0;
			ptr++;
		}
	}

	SDL_WM_SetIcon(icon, NULL);
	SDL_FreeSurface(icon);
}
#endif

/**
 * @brief Init the SDL window
 * @param fullscreen Start in fullscreen or not (bool value)
 */
qboolean Rimp_InitGraphics (qboolean fullscreen)
{
	int flags;
	int stencil_bits;

#ifndef __APPLE__
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);
	Com_Printf("SDL version: %i.%i.%i\n", info.version.major, info.version.minor, info.version.patch);
#endif

	/* Just toggle fullscreen if that's all that has been changed */
	if (surface && (surface->w == viddef.width) && (surface->h == viddef.height)) {
		qboolean isfullscreen = (surface->flags & SDL_FULLSCREEN) ? qtrue : qfalse;
		if (fullscreen != isfullscreen)
			SDL_WM_ToggleFullScreen(surface);

		isfullscreen = (surface->flags & SDL_FULLSCREEN) ? qtrue : qfalse;
		if (fullscreen == isfullscreen)
			return qtrue;
	}

	/* free resources in use */
	if (surface)
		SDL_FreeSurface(surface);

	/* let the sound and input subsystems know about the new window */
	VID_NewWindow(viddef.width, viddef.height);

	SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 5);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 1);

	flags = SDL_OPENGL;
	if (fullscreen)
		flags |= SDL_FULLSCREEN;

#ifndef _WIN32
	SetSDLIcon(); /* currently uses ufoicon.xbm data */
#endif

	if ((surface = SDL_SetVideoMode(viddef.width, viddef.height, 0, flags)) == NULL) {
		Sys_Error("(SDLGL) SDL SetVideoMode failed: %s\n", SDL_GetError());
		return qfalse;
	}

	SDL_WM_SetCaption(GAME_TITLE, GAME_TITLE_LONG);

	SDL_ShowCursor(SDL_DISABLE);

	if (!SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencil_bits))
		Com_Printf("I: got %d bits of stencil\n", stencil_bits);

	return qtrue;
}

/**
 * @brief
 */
rserr_t Rimp_SetMode (unsigned int *pwidth, unsigned int *pheight, int mode, qboolean fullscreen)
{
	Com_Printf("setting mode %d:", mode );

	if (!VID_GetModeInfo((int*)pwidth, (int*)pheight, mode)) {
		Com_Printf(" invalid mode\n");
		return rserr_invalid_mode;
	}

	Com_Printf(" %d %d\n", *pwidth, *pheight);

	if (!Rimp_InitGraphics(fullscreen)) {
		/* failed to set a valid mode in windowed mode */
		return rserr_invalid_mode;
	}

	return rserr_ok;
}

/**
 * @brief
 */
void Rimp_Shutdown (void)
{
	if (surface)
		SDL_FreeSurface(surface);
	surface = NULL;

	SDL_ShowCursor(SDL_ENABLE);

	if (SDL_WasInit(SDL_INIT_EVERYTHING) == SDL_INIT_VIDEO)
		SDL_Quit();
	else
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
}


/**
 * @brief
 */
void KBD_Update (void)
{
	SDL_Event event;

	/* get events from x server */
	while (SDL_PollEvent(&event))
		GetEvent(&event);

	if (!mx && !my)
		SDL_GetRelativeMouseState(&mx, &my);

	if (vid_grabmouse->modified) {
		vid_grabmouse->modified = qfalse;

		if (!vid_grabmouse->integer) {
			/* ungrab the pointer */
			SDL_WM_GrabInput(SDL_GRAB_OFF);
		} else {
			/* grab the pointer */
			SDL_WM_GrabInput(SDL_GRAB_ON);
		}
	}
	while (keyq_head != keyq_tail) {
		Key_Event(keyq[keyq_tail].key, keyq[keyq_tail].down, Sys_Milliseconds());
		keyq_tail = (keyq_tail + 1) & (MAX_KEYQ - 1);
	}
}
