#include "dosbox.h"

void GUI_ShutDown(Section*);
void SetPriority(PRIORITY_LEVELS level);
void KillSwitch(bool pressed);
void CaptureMouse(bool pressed);
void Restart(bool pressed);

#include "dosbox_splash.h"

// Forward Declare

void GUI_StartUp(Section * sec)
{
	sec->AddDestroyFunction(&GUI_ShutDown);
	
	Section_prop * section = static_cast<Section_prop *>(sec);
	s_Sdl.active = false;
	s_Sdl.updating = false;

	GFX_SetIcon();

	//s_Sdl.desktop.lazy_fullscreen = false;
	//s_Sdl.desktop.lazy_fullscreen_req = false;

	//s_Sdl.desktop.fullscreen = section->Get_bool("fullscreen");
	s_Sdl.wait_on_error = section->Get_bool("waitonerror");

	Prop_multival* p = section->Get_multival("priority");
	std::string focus = p->GetSection()->Get_string("active");
	std::string notfocus = p->GetSection()->Get_string("inactive");

	if (focus == "lowest") { s_Sdl.Priority.focus = PRIORITY_LEVEL_LOWEST; }
	else if (focus == "lower") { s_Sdl.Priority.focus = PRIORITY_LEVEL_LOWER; }
	else if (focus == "normal") { s_Sdl.Priority.focus = PRIORITY_LEVEL_NORMAL; }
	else if (focus == "higher") { s_Sdl.Priority.focus = PRIORITY_LEVEL_HIGHER; }
	else if (focus == "highest") { s_Sdl.Priority.focus = PRIORITY_LEVEL_HIGHEST; }

	if (notfocus == "lowest") { s_Sdl.Priority.nofocus = PRIORITY_LEVEL_LOWEST; }
	else if (notfocus == "lower") { s_Sdl.Priority.nofocus = PRIORITY_LEVEL_LOWER; }
	else if (notfocus == "normal") { s_Sdl.Priority.nofocus = PRIORITY_LEVEL_NORMAL; }
	else if (notfocus == "higher") { s_Sdl.Priority.nofocus = PRIORITY_LEVEL_HIGHER; }
	else if (notfocus == "highest") { s_Sdl.Priority.nofocus = PRIORITY_LEVEL_HIGHEST; }
	else if (notfocus == "pause") {
		/* we only check for pause here, because it makes no sense
		* for DOSBox to be paused while it has focus
		*/
		s_Sdl.Priority.nofocus = PRIORITY_LEVEL_PAUSE;
	}

	SetPriority(s_Sdl.Priority.focus); //Assume focus on startup

	s_Sdl.Mouse.locked = false;
	mouselocked = false; //Global for mapper
	s_Sdl.Mouse.requestlock = false;
	s_Sdl.Mouse.autoenable = section->Get_bool("autolock");
	if (!s_Sdl.Mouse.autoenable) SDL_ShowCursor(SDL_DISABLE);
	s_Sdl.Mouse.autolock = false;
	s_Sdl.Mouse.sensitivity = section->Get_int("sensitivity");

	// Starting window is 640x400 for displaying splash screen
	s_Sdl.Draw.width = 640;
	s_Sdl.Draw.height = 400;

	glGetIntegerv (GL_MAX_TEXTURE_SIZE, &s_Sdl.Opengl.max_texsize);
	s_Sdl.Opengl.packed_pixel = s_Sdl.Opengl.paletted_texture = false;

	GFX_Stop();

	// Draw Splash screen
	Bit8u* tmpbufp = new Bit8u[640 * 400 * 3];
	Bit32u* data = ((Bit32u*)&my_image_data[0]);
	GIMP_IMAGE_RUN_LENGTH_DECODE(tmpbufp, gimp_image.rle_pixel_data, 640 * 400, 3);
	for (Bitu y = 0; y<400; y++) 
	{
		Bit8u* tmpbuf = tmpbufp + y * 640 * 3;
		//Bit32u * draw = (Bit32u*)(((Bit8u *)splash_surf->pixels) + ((y)*splash_surf->pitch));
		for (Bitu x = 0; x<640; x++) 
		{
			//#if SDL_BYTEORDER == SDL_BIG_ENDIAN
			//				*draw++ = tmpbuf[x*3+2]+tmpbuf[x*3+1]*0x100+tmpbuf[x*3+0]*0x10000+0x00000000;
			//#else
			*data++ = tmpbuf[x * 3 + 0] + tmpbuf[x * 3 + 1] * 0x100 + tmpbuf[x * 3 + 2] * 0x10000 + 0xFF000000;
			//#endif
		}
	}

	static Bitu max_splash_loop = 600;
	static Bitu splash_fade = 100;
	for (Bit32u ct = 0, startticks = GetTicks();ct < max_splash_loop;ct = GetTicks() - startticks)
	{
		Debug_UpdateAndDraw();
	}

	delete[] tmpbufp;

	/* Get some Event handlers */
	MAPPER_AddHandler(KillSwitch, MK_f9, MMOD1, "shutdown", "ShutDown");
	MAPPER_AddHandler(CaptureMouse, MK_f10, MMOD1, "capmouse", "Cap Mouse");
	//MAPPER_AddHandler(SwitchFullScreen, MK_return, MMOD2, "fullscr", "Fullscreen");
	MAPPER_AddHandler(Restart, MK_home, MMOD1 | MMOD2, "restart", "Restart");
#if C_DEBUG
	/* Pause binds with activate-debugger */
#else
	MAPPER_AddHandler(&PauseDOSBox, MK_pause, MMOD2, "pause", "Pause DBox");
#endif
	/* Get Keyboard state of numlock and capslock */
	SDL_Keymod keystate = SDL_GetModState();
	if (keystate&KMOD_NUM) startup_state_numlock = true;
	if (keystate&KMOD_CAPS) startup_state_capslock = true;
}

static void GUI_ShutDown(Section * /*sec*/) {
	GFX_Stop();
	//if (sdl.draw.callback) (sdl.draw.callback)(GFX_CallBackStop);
	//if (sdl.mouse.locked) GFX_CaptureMouse();
	//if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
}

static void SetPriority(PRIORITY_LEVELS level) 
{

#if C_SET_PRIORITY
	// Do nothing if priorties are not the same and not root, else the highest
	// priority can not be set as users can only lower priority (not restore it)

	if ((sdl.priority.focus != sdl.priority.nofocus) &&
		(getuid() != 0)) return;

#endif
	switch (level) {
#ifdef WIN32
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_LOWER:
		SetPriorityClass(GetCurrentProcess(), BELOW_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_NORMAL:
		SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHER:
		SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHEST:
		SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
		break;
#elif C_SET_PRIORITY
		/* Linux use group as dosbox has mulitple threads under linux */
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		setpriority(PRIO_PGRP, 0, PRIO_MAX);
		break;
	case PRIORITY_LEVEL_LOWER:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - (PRIO_TOTAL / 3));
		break;
	case PRIORITY_LEVEL_NORMAL:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - (PRIO_TOTAL / 2));
		break;
	case PRIORITY_LEVEL_HIGHER:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - ((3 * PRIO_TOTAL) / 5));
		break;
	case PRIORITY_LEVEL_HIGHEST:
		setpriority(PRIO_PGRP, 0, PRIO_MAX - ((3 * PRIO_TOTAL) / 4));
		break;
#endif
	default:
		break;
	}
}