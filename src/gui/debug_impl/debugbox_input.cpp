#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define STDOUT_FILE	TEXT("stdout.txt")
#define STDERR_FILE	TEXT("stderr.txt")
#define DEFAULT_CONFIG_FILE "/dosbox.conf"
#elif defined(MACOSX)
#define DEFAULT_CONFIG_FILE "/Library/Preferences/DOSBox Preferences"
#else /*linux freebsd*/
#define DEFAULT_CONFIG_FILE "/.dosboxrc"
#endif

#if C_SET_PRIORITY
#include <sys/resource.h>
#define PRIO_TOTAL (PRIO_MAX-PRIO_MIN)
#endif

#ifdef OS2
#define INCL_DOS
#define INCL_WIN
#include <os2.h>
#endif

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) 
{
	if (s_Sdl.Mouse.locked || !s_Sdl.Mouse.autoenable)
	{
		Mouse_CursorMoved((float)motion->xrel*s_Sdl.Mouse.sensitivity / 100.0f,
		(float)motion->yrel*s_Sdl.Mouse.sensitivity / 100.0f,
			(float)(motion->x - s_Sdl.clip.x) / (s_Sdl.clip.w - 1)*s_Sdl.Mouse.sensitivity / 100.0f,
			(float)(motion->y - s_Sdl.clip.y) / (s_Sdl.clip.h - 1)*s_Sdl.Mouse.sensitivity / 100.0f,
			s_Sdl.Mouse.locked);
	}
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) 
{
	switch (button->state) {
	case SDL_PRESSED:
		if (s_Sdl.Mouse.requestlock && !s_Sdl.Mouse.locked) 
		{
			//Input_CaptureMouse();
			// Dont pass klick to mouse handler
			break;
		}
		if (!s_Sdl.Mouse.autoenable && s_Sdl.Mouse.autolock && button->button == SDL_BUTTON_MIDDLE)
		{
			//Input_CaptureMouse();
			break;
		}
		switch (button->button) 
		{
		case SDL_BUTTON_LEFT:
			Mouse_ButtonPressed(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonPressed(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonPressed(2);
			break;
		}
		break;
	case SDL_RELEASED:
		switch (button->button) 
		{
		case SDL_BUTTON_LEFT:
			Mouse_ButtonReleased(0);
			break;
		case SDL_BUTTON_RIGHT:
			Mouse_ButtonReleased(1);
			break;
		case SDL_BUTTON_MIDDLE:
			Mouse_ButtonReleased(2);
			break;
		}
		break;
	}
}

bool UpdateDebugBoxKeyInput(SDL_Event* event)
{
	bool keyConsumed = false;
	ImGuiIO& io = ImGui::GetIO();

	if (event->key.keysym.scancode == SDL_SCANCODE_INSERT)
	{
		static bool wasPressed = false;
		if (!wasPressed && event->type == SDL_KEYDOWN)
		{
			wasPressed = true;
		}
		else if (wasPressed && event->type == SDL_KEYUP)
		{
			DosboxWindowIsFocused = false;
			wasPressed = false;
		}
		keyConsumed = true;
	}

	if (DosboxWindowIsFocused)
		return keyConsumed;


	if (event->key.keysym.scancode == SDL_SCANCODE_F6)
	{
		static bool wasPressed = false;
		if (!wasPressed && event->type == SDL_KEYDOWN)
		{
			wasPressed = true;
		}
		else if (wasPressed && event->type == SDL_KEYUP)
		{
			DEBUG_EnableDebugger();
			wasPressed = false;
		}
		keyConsumed = true;
	}
	else if (event->key.keysym.scancode == SDL_SCANCODE_F5)
	{
		static bool wasPressed = false;
		if (!wasPressed && event->type == SDL_KEYDOWN)
		{
			wasPressed = true;
		}
		else if (wasPressed && event->type == SDL_KEYUP)
		{
			DEBUG_Run();
			wasPressed = false;
		}
		keyConsumed = true;
	}
	else if (event->key.keysym.scancode == SDL_SCANCODE_F10)
	{
		static bool wasPressed = false;
		if (!wasPressed && event->type == SDL_KEYDOWN)
		{
			wasPressed = true;
		}
		else if (wasPressed && event->type == SDL_KEYUP)
		{
			DEBUG_StepOver(); 
			wasPressed = false;
		}
		keyConsumed = true;
	}
	else if (event->key.keysym.scancode == SDL_SCANCODE_F11)
	{
		static bool modWasPressed = false;
		if(!modWasPressed && (event->key.keysym.mod & KMOD_SHIFT) != 0)
		{
			modWasPressed = true;
		}

		static bool wasPressed = false;
		if (!wasPressed && event->type == SDL_KEYDOWN)
		{
			wasPressed = true;
			if (modWasPressed)
			{
				if ((event->key.keysym.mod & KMOD_SHIFT) == 0)
				{
					modWasPressed = false;
				}
				else
				{
					DEBUG_StepOut();
				}
			}
		}
		else if (wasPressed && event->type == SDL_KEYUP)
		{
			if (modWasPressed && (event->key.keysym.mod & KMOD_SHIFT) == 0)
			{
				modWasPressed = false;
			}
			else if (!modWasPressed)
			{
				DEBUG_Step();
			}
			wasPressed = false;
		}
		keyConsumed = true;


	}

	return keyConsumed;
}

void ProcessEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) 
	{
		ImGui_ImplSDL2_ProcessEvent(&event);
		switch (event.type)
		{
		case SDL_WINDOWEVENT:
			if(event.window.windowID == SDL_GetWindowID(s_Sdl.Window))
			{
				if (event.window.event == SDL_WINDOWEVENT_CLOSE)
				{
					throw(0);
				}
				else if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
				{
					s_Sdl.focus_ticks = GetTicks();

					SetPriority(s_Sdl.Priority.focus);
					CPU_Disable_SkipAutoAdjust();
				}
				else if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST)
				{
					if (s_Sdl.Mouse.locked)
					{
						//Input_CaptureMouse();
					}
					SetPriority(s_Sdl.Priority.nofocus);
					GFX_LosingFocus();
					CPU_Enable_SkipAutoAdjust();
				}
			}
			break;
		case SDL_QUIT:
			throw(0);
			break;
		case SDL_MOUSEMOTION:
			//if(!DEBUG_IsDebugging())
				HandleMouseMotion(&event.motion);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			//if (!DEBUG_IsDebugging())
				HandleMouseButton(&event.button);
			break;
#ifdef WIN32
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			//if (!DEBUG_IsDebugging())
			{
				// ignore event alt+tab
				if (event.key.keysym.sym == SDLK_LALT) s_Sdl.laltstate = event.key.type;
				if (event.key.keysym.sym == SDLK_RALT) s_Sdl.raltstate = event.key.type;
				if (((event.key.keysym.sym == SDLK_TAB)) &&
					((s_Sdl.laltstate == SDL_KEYDOWN) || (s_Sdl.raltstate == SDL_KEYDOWN))) break;
				// This can happen as well.
				if (((event.key.keysym.sym == SDLK_TAB)) && (event.key.keysym.mod & KMOD_ALT)) break;
				// ignore tab events that arrive just after regaining focus. (likely the result of alt-tab)
				if ((event.key.keysym.sym == SDLK_TAB) && (GetTicks() - s_Sdl.focus_ticks < 2)) break;
			}
#endif
		default:
			void MAPPER_CheckEvent(SDL_Event * event);

			bool keyConsumed = UpdateDebugBoxKeyInput(&event);

			if (!keyConsumed && DosboxWindowIsFocused)
			{
				MAPPER_CheckEvent(&event);
			}
		}
	}
}

bool mouselocked; //Global variable for mapper
static void CaptureMouse(bool pressed) 
{
	if (!pressed)
		return;
	Input_CaptureMouse();
}

void Input_CaptureMouse(void)
{
	s_Sdl.Mouse.locked = !s_Sdl.Mouse.locked;
	if (s_Sdl.Mouse.locked)
	{
		SDL_SetRelativeMouseMode(SDL_TRUE);
		SDL_SetWindowGrab(s_Sdl.Window, SDL_TRUE);
	}
	else 
	{
		SDL_SetRelativeMouseMode(SDL_FALSE);
		SDL_SetWindowGrab(s_Sdl.Window, SDL_FALSE);
		if (s_Sdl.Mouse.autoenable || !s_Sdl.Mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
	mouselocked = s_Sdl.Mouse.locked;
}

void Mouse_AutoLock(bool enable) 
{
	s_Sdl.Mouse.autolock = enable;
	if (s_Sdl.Mouse.autoenable) s_Sdl.Mouse.requestlock = enable;
	else {
		SDL_ShowCursor(enable ? SDL_DISABLE : SDL_ENABLE);
		s_Sdl.Mouse.requestlock = false;
	}
}

void INPUT_LosingFocus()
{
	MAPPER_LosingFocus();
}

#if defined (WIN32)
STICKYKEYS stick_keys = { sizeof(STICKYKEYS), 0 };
void sticky_keys(bool restore) 
{
	static bool inited = false;
	if (!inited) {
		inited = true;
		SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
	}
	if (restore) {
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
		return;
	}
	//Get current sticky keys layout:
	STICKYKEYS s = { sizeof(STICKYKEYS), 0 };
	SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	if (!(s.dwFlags & SKF_STICKYKEYSON)) { //Not on already
		s.dwFlags &= ~SKF_HOTKEYACTIVE;
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	}
}
#endif