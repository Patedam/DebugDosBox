#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif


#include <string>
#include <vector>
#ifdef WIN32
#include <signal.h>
#endif

#include "dosbox.h"
#include "control.h"
#include "debug.h"
#include "mapper.h"
#include "mouse.h"
#include "programs.h"
#include "render.h"
#include "timer.h"
#include "video.h"

#include "SDL.h"
#include <GL/gl3w.h>
#include "gui/debug_impl/imgui/imgui.h"
#include "gui/debug_impl/imgui/imgui_impl_sdl.h"
#include "gui/debug_impl/imgui/imgui_impl_opengl3.h"


#define MAPPERFILE "mapper-" VERSION ".map"

// debugbox data structures
enum PRIORITY_LEVELS
{
	PRIORITY_LEVEL_PAUSE,
	PRIORITY_LEVEL_LOWEST,
	PRIORITY_LEVEL_LOWER,
	PRIORITY_LEVEL_NORMAL,
	PRIORITY_LEVEL_HIGHER,
	PRIORITY_LEVEL_HIGHEST
};

struct SDL_Block 
{
	bool inited;
	bool active;							//If this isn't set don't draw
	bool updating;
	bool wait_on_error = true;

	// Gfx
	SDL_GLContext GL_Context = nullptr;
	GLuint DosBoxRenderTextureId = 0;
	float DosBoxRenderTextureAlpha = 1.0f;
	struct {
		Bit32u width;
		Bit32u height;
		Bit32u bpp;
		Bitu flags;
		double scalex, scaley;
		GFX_CallBack_t callback;
	} Draw;
	struct {
		Bitu pitch;
		void * framebuf;
		GLuint buffer;
		GLuint texture;
		GLuint displaylist;
		GLint max_texsize;
		bool bilinear;
		bool packed_pixel;
		bool paletted_texture;
		bool pixel_buffer_object;
	} Opengl;
	SDL_Rect clip;

	// GUI
	SDL_Window* Window = nullptr;
	std::string DosBoxWindowTitle = "DOSBOX";
	struct {
		PRIORITY_LEVELS focus;
		PRIORITY_LEVELS nofocus;
	} Priority;

	// Events
	struct {
		bool autolock;
		bool autoenable;
		bool requestlock;
		bool locked;
		Bitu sensitivity;
	} Mouse;
	// Time when sdl regains focus (alt-tab) in windowed mode
	Bit32u focus_ticks;
	// state of alt-keys for certain special handlings
	Bit16u laltstate;
	Bit16u raltstate;
};

static SDL_Block s_Sdl;

//Globals for keyboard initialisation
bool startup_state_numlock = false;
bool startup_state_capslock = false;

constexpr int my_image_width = 640, my_image_height = 400;
unsigned char my_image_data[my_image_height * my_image_width * 4];

// Forward Declare External
void InitDebug();
void UpdateDebugWindows();
void DrawDebugWindows();

// debugbox blobs
#include "gui/debug_impl/debugbox_gfx.cpp"
#include "gui/debug_impl/debugbox_gui.cpp"
#include "gui/debug_impl/debugbox_input.cpp"

// Forward Declare internal
void RunDosBox(int argc, char* argv[]);
static BOOL WINAPI ConsoleEventHandler(DWORD event);
void Console_ShowMsg(char const* format, ...);
void Config_Add_SDL();

int main(int argc, char* argv[]) 
{
	// Setup SDL
	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0)
	{
		printf("Error: %s\n", SDL_GetError());
		return -1;
	}

	// GL 3.2 Core + GLSL 150
	const char* glsl_version = "#version 150";
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

	// Create SDL Window and OpenGL context
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	SDL_DisplayMode current;
	SDL_GetCurrentDisplayMode(0, &current);
	s_Sdl.Window = SDL_CreateWindow("DebugBox 0.1 - Dosbox 0.74-2", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 1000, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
	s_Sdl.GL_Context = SDL_GL_CreateContext(s_Sdl.Window);
	SDL_GL_SetSwapInterval(0); // Disable vsync

	// Init gl3w
	bool err = gl3wInit() != 0;
	if (err)
	{
		fprintf(stderr, "Failed to initialize OpenGL loader!\n");
		return 1;
	}

	// Setup Dear ImGui context
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

	// Setup Platform/Renderer bindings
	ImGui_ImplSDL2_InitForOpenGL(s_Sdl.Window, s_Sdl.GL_Context);
	ImGui_ImplOpenGL3_Init(glsl_version);

	// Setup Style
	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	CreateDosBoxRenderTexture();

	InitDebug();

	// Run DosBox!
	RunDosBox(argc, argv);

	// Cleanup
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();

	SDL_GL_DeleteContext(s_Sdl.GL_Context);
	SDL_DestroyWindow(s_Sdl.Window);
	SDL_Quit();

	return 0;
}

static constexpr float RefreshTime = 33.3f / 1000; // 16.6ms = 60fps / 33.3ms = 30fps
static float TimeAccumulator = 0.0f;
static Uint64 LatestTime = 0;
void Debug_UpdateAndDraw(bool forceUpdate)
{
	if (!forceUpdate)
	{
		static Uint64 frequency = SDL_GetPerformanceFrequency();
		Uint64 current_time = SDL_GetPerformanceCounter();
		float deltaTime = LatestTime > 0 ? (float)((double)(current_time - LatestTime) / frequency) : (float)(1.0f / 60.0f);
		LatestTime = current_time;

		TimeAccumulator += deltaTime;
		if (TimeAccumulator > RefreshTime)
		{
			TimeAccumulator = 0.0f;
		}
		else
		{
			return;
		}
	}
	else
	{
		TimeAccumulator = 0.0f;
		LatestTime = SDL_GetPerformanceCounter();
	}

	// Start the Dear ImGui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplSDL2_NewFrame(s_Sdl.Window);
	ImGui::NewFrame();

	{
		char buf[128];
		sprintf(buf, "%s###DosBoxWindow", s_Sdl.DosBoxWindowTitle.c_str());
		ImGui::Begin(buf);
		if (s_Sdl.Opengl.framebuf == nullptr)
		{
			// Turn the RGBA pixel data into an OpenGL texture:
			glBindTexture(GL_TEXTURE_2D, s_Sdl.DosBoxRenderTextureId);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, s_Sdl.Draw.width, s_Sdl.Draw.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, my_image_data);
		}

		static const ImVec4 breakedColor(1.f, 0.f, 0.f, 1.f);
		static const ImVec4 focusedColor(1.f, 1.f, 1.f, 1.f);
		static const ImVec4 defaultColor(0.f, 0.f, 0.f, 1.f);
		// Now that we have an OpenGL texture, assuming our imgui rendering function (imgui_impl_xxx.cpp file) takes GLuint as ImTextureID, we can display it:
		ImGui::Image(
			(ImTextureID)s_Sdl.DosBoxRenderTextureId,
			ImVec2((float)s_Sdl.Draw.width,(float)s_Sdl.Draw.height), // Size
			ImVec2(0, 0), // UV0
			ImVec2(1, 1), // UV1
			ImVec4(1, 1, 1, s_Sdl.DosBoxRenderTextureAlpha), // Tint color
			DEBUG_IsDebugging() ? breakedColor : DosboxWindowIsFocused ? focusedColor : defaultColor);

		if(ImGui::IsItemClicked(0) && !DosboxWindowIsFocused)
		{
			DosboxWindowIsFocused = true;
		}

		ImGui::Text("Has Captured: %d", s_Sdl.Mouse.locked ? 1 : 0);
		ImGui::Text("Is Focused: %d", DosboxWindowIsFocused ? 1 : 0);

		ImGui::End();
	}

	ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
	if (s_Sdl.active)
	{
		bool show_demo_window = true;
		bool show_another_window = false;

		// 1. Show the big demo window (Most of the sample code is in ImGui::ShowDemoWindow()! You can browse its code to learn more about Dear ImGui!).
		if (show_demo_window)
			ImGui::ShowDemoWindow(&show_demo_window);

		// 2. Show a simple window that we create ourselves. We use a Begin/End pair to created a named window.
		{
			static float f = 0.0f;
			static int counter = 0;

			ImGui::Begin("Hello, world!");                          // Create a window called "Hello, world!" and append into it.

			ImGui::Text("This is some useful text.");               // Display some text (you can use a format strings too)
			ImGui::Checkbox("Demo Window", &show_demo_window);      // Edit bools storing our window open/close state
			ImGui::Checkbox("Another Window", &show_another_window);

			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);            // Edit 1 float using a slider from 0.0f to 1.0f    
			ImGui::ColorEdit3("clear color", (float*)&clear_color); // Edit 3 floats representing a color

			if (ImGui::Button("Button"))                            // Buttons return true when clicked (most widgets return true when edited/activated)
				counter++;
			ImGui::SameLine();
			ImGui::Text("counter = %d", counter);

			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
			ImGui::End();
		}

		UpdateDebugWindows();
		DrawDebugWindows();
	}

	// Rendering
	ImGui::Render();
	SDL_GL_MakeCurrent(s_Sdl.Window, s_Sdl.GL_Context);
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
	glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
	glClear(GL_COLOR_BUFFER_BIT);
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
	SDL_GL_SwapWindow(s_Sdl.Window);
}

void RunDosBox(int argc, char* argv[])
{
	try {
		CommandLine com_line(argc, argv);
		Config myconf(&com_line);
		control = &myconf;
		/* Init the configuration system and add default values */
		Config_Add_SDL();
		DOSBOX_Init();

		std::string editor;
		//if (control->cmdline->FindString("-editconf", editor, false)) launcheditor();
		//if (control->cmdline->FindString("-opencaptures", editor, true)) launchcaptures(editor);
		//if(control->cmdline->FindExist("-eraseconf")) eraseconfigfile();
		//if(control->cmdline->FindExist("-resetconf")) eraseconfigfile();
		//if(control->cmdline->FindExist("-erasemapper")) erasemapperfile();
		//if(control->cmdline->FindExist("-resetmapper")) erasemapperfile();
		
		if (control->cmdline->FindExist("-version") ||
			control->cmdline->FindExist("--version")) {
			printf("\nDOSBox version %s, copyright 2002-2018 DOSBox Team.\n\n", VERSION);
			printf("DOSBox is written by the DOSBox Team (See AUTHORS file))\n");
			printf("DOSBox comes with ABSOLUTELY NO WARRANTY.  This is free software,\n");
			printf("and you are welcome to redistribute it under certain conditions;\n");
			printf("please read the COPYING file thoroughly before doing so.\n\n");
			return;
		}
		//if (control->cmdline->FindExist("-printconf")) printconfiglocation();

#if C_DEBUG
		DEBUG_SetupConsole();
#endif

#if defined(WIN32)
		SetConsoleCtrlHandler((PHANDLER_ROUTINE)ConsoleEventHandler, TRUE);
#endif

		/* Display Welcometext in the console */
		LOG_MSG("DOSBox version %s", VERSION);
		LOG_MSG("Copyright 2002-2018 DOSBox Team, published under GNU GPL.");
		LOG_MSG("---");

#ifndef DISABLE_JOYSTICK
		//Initialise Joystick separately. This way we can warn when it fails instead
		//of exiting the application
		if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0) LOG_MSG("Failed to init joystick support");
#endif

		s_Sdl.laltstate = SDL_KEYUP;
		s_Sdl.raltstate = SDL_KEYUP;

		//sdl.num_joysticks = SDL_NumJoysticks();

		/* Parse configuration files */
		std::string config_file, config_path;
		Cross::GetPlatformConfigDir(config_path);

		//First parse -userconf
		if (control->cmdline->FindExist("-userconf", true)) {
			config_file.clear();
			Cross::GetPlatformConfigDir(config_path);
			Cross::GetPlatformConfigName(config_file);
			config_path += config_file;
			control->ParseConfigFile(config_path.c_str());
			if (!control->configfiles.size()) {
				//Try to create the userlevel configfile.
				config_file.clear();
				Cross::CreatePlatformConfigDir(config_path);
				Cross::GetPlatformConfigName(config_file);
				config_path += config_file;
				if (control->PrintConfig(config_path.c_str())) {
					LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s", config_path.c_str());
					//Load them as well. Makes relative paths much easier
					control->ParseConfigFile(config_path.c_str());
				}
			}
		}

		//Second parse -conf switches
		while (control->cmdline->FindString("-conf", config_file, true)) {
			if (!control->ParseConfigFile(config_file.c_str())) {
				// try to load it from the user directory
				if (!control->ParseConfigFile((config_path + config_file).c_str())) {
					LOG_MSG("CONFIG: Can't open specified config file: %s", config_file.c_str());
				}
			}
		}
		// if none found => parse localdir conf
		if (!control->configfiles.size()) control->ParseConfigFile("dosbox.conf");

		// if none found => parse userlevel conf
		if (!control->configfiles.size()) {
			config_file.clear();
			Cross::GetPlatformConfigName(config_file);
			control->ParseConfigFile((config_path + config_file).c_str());
		}

		if (!control->configfiles.size()) {
			//Try to create the userlevel configfile.
			config_file.clear();
			Cross::CreatePlatformConfigDir(config_path);
			Cross::GetPlatformConfigName(config_file);
			config_path += config_file;
			if (control->PrintConfig(config_path.c_str())) {
				LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s", config_path.c_str());
				//Load them as well. Makes relative paths much easier
				control->ParseConfigFile(config_path.c_str());
			}
			else {
				LOG_MSG("CONFIG: Using default settings. Create a configfile to change them");
			}
		}


#if (ENVIRON_LINKED)
		control->ParseEnv(environ);
#endif
		//		UI_Init();
		//		if (control->cmdline->FindExist("-startui")) UI_Run(false);
		/* Init all the sections */
		control->Init();
		/* Some extra SDL Functions */
		Section_prop * sdl_sec = static_cast<Section_prop *>(control->GetSection("sdl"));

		//if (control->cmdline->FindExist("-fullscreen") || sdl_sec->Get_bool("fullscreen")) {
		//	if (!sdl.desktop.fullscreen) { //only switch if not already in fullscreen
		//		GFX_SwitchFullScreen();
		//	}
		//}

		/* Init the keyMapper */
		MAPPER_Init();
		if (control->cmdline->FindExist("-startmapper")) MAPPER_RunInternal();
		/* Start up main machine */
		control->StartUp();
	}
	catch (char * error)
	{
	#if defined (WIN32)
		sticky_keys(true);
	#endif
		Console_ShowMsg("Exit to error: %s", error);
		fflush(NULL);
		if (s_Sdl.wait_on_error) 
		{
			//TODO Maybe look for some way to show message in linux?
	#if (C_DEBUG)
			Console_ShowMsg("Press enter to continue");
			fflush(NULL);
			fgetc(stdin);
	#elif defined(WIN32)
			Sleep(5000);
	#endif
		}

	}
	catch (int) {
		; //nothing, pressed killswitch
	}
	catch (...) {
		; // Unknown error, let's just exit.
	}
}

#pragma region Config
void Config_Add_SDL()
{
	Section_prop * sdl_sec = control->AddSection_prop("sdl", &GUI_StartUp);
	sdl_sec->AddInitFunction(&MAPPER_StartUp);
	Prop_bool* Pbool;
	Prop_string* Pstring;
	Prop_int* Pint;
	Prop_multival* Pmulti;

	Pbool = sdl_sec->Add_bool("fullscreen", Property::Changeable::Always, false);
	Pbool->Set_help("Start dosbox directly in fullscreen. (Press ALT-Enter to go back)");

	Pbool = sdl_sec->Add_bool("fulldouble", Property::Changeable::Always, false);
	Pbool->Set_help("Use double buffering in fullscreen. It can reduce screen flickering, but it can also result in a slow DOSBox.");

	Pstring = sdl_sec->Add_string("fullresolution", Property::Changeable::Always, "original");
	Pstring->Set_help("What resolution to use for fullscreen: original, desktop or a fixed size (e.g. 1024x768).\n"
		"Using your monitor's native resolution with aspect=true might give the best results.\n"
		"If you end up with small window on a large screen, try an output different from surface."
		"On Windows 10 with display scaling (Scale and layout) set to a value above 100%, it is recommended\n"
		"to use a lower full/windowresolution, in order to avoid window size problems.");

	Pstring = sdl_sec->Add_string("windowresolution", Property::Changeable::Always, "original");
	Pstring->Set_help("Scale the window to this size IF the output device supports hardware scaling.\n"
		"(output=surface does not!)");

	const char* outputs[] = {
		"surface", "overlay",
#if C_OPENGL
		"opengl", "openglnb",
#endif
#if C_DDRAW
		"ddraw",
#endif
		0 };
	Pstring = sdl_sec->Add_string("output", Property::Changeable::Always, "surface");
	Pstring->Set_help("What video system to use for output.");
	Pstring->Set_values(outputs);

	Pbool = sdl_sec->Add_bool("autolock", Property::Changeable::Always, true);
	Pbool->Set_help("Mouse will automatically lock, if you click on the screen. (Press CTRL-F10 to unlock)");

	Pint = sdl_sec->Add_int("sensitivity", Property::Changeable::Always, 100);
	Pint->SetMinMax(1, 1000);
	Pint->Set_help("Mouse sensitivity.");

	Pbool = sdl_sec->Add_bool("waitonerror", Property::Changeable::Always, true);
	Pbool->Set_help("Wait before closing the console if dosbox has an error.");

	Pmulti = sdl_sec->Add_multi("priority", Property::Changeable::Always, ",");
	Pmulti->SetValue("higher,normal");
	Pmulti->Set_help("Priority levels for dosbox. Second entry behind the comma is for when dosbox is not focused/minimized.\n"
		"pause is only valid for the second entry.");

	const char* actt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0 };
	Pstring = Pmulti->GetSection()->Add_string("active", Property::Changeable::Always, "higher");
	Pstring->Set_values(actt);

	const char* inactt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0 };
	Pstring = Pmulti->GetSection()->Add_string("inactive", Property::Changeable::Always, "normal");
	Pstring->Set_values(inactt);

	Pstring = sdl_sec->Add_path("mapperfile", Property::Changeable::Always, MAPPERFILE);
	Pstring->Set_help("File used to load/save the key/event mappings from. Resetmapper only works with the default value.");

	Pbool = sdl_sec->Add_bool("usescancodes", Property::Changeable::Always, true);
	Pbool->Set_help("Avoid usage of symkeys, might not work on all operating systems.");
}
#pragma endregion

#pragma region Restart
#if C_DEBUG
extern void DEBUG_ShutDown(Section * /*sec*/);
#endif

void MIXER_CloseAudioDevice();

void restart_program(std::vector<std::string> & parameters) {
	char** newargs = new char*[parameters.size() + 1];
	// parameter 0 is the executable path
	// contents of the vector follow
	// last one is NULL
	for (Bitu i = 0; i < parameters.size(); i++) newargs[i] = (char*)parameters[i].c_str();
	newargs[parameters.size()] = NULL;
	MIXER_CloseAudioDevice();
	SDL_Delay(50);
	SDL_Quit();
#if C_DEBUG
	// shutdown curses
	DEBUG_ShutDown(NULL);
#endif

	if (execvp(newargs[0], newargs) == -1) {
#ifdef WIN32
		if (newargs[0][0] == '\"') {
			//everything specifies quotes around it if it contains a space, however my system disagrees
			std::string edit = parameters[0];
			edit.erase(0, 1);edit.erase(edit.length() - 1, 1);
			//However keep the first argument of the passed argv (newargs) with quotes, as else repeated restarts go wrong.
			if (execvp(edit.c_str(), newargs) == -1) E_Exit("Restarting failed");
		}
#endif
		E_Exit("Restarting failed");
	}
	free(newargs);
}
void Restart(bool pressed) 
{ // mapper handler
	restart_program(control->startup_params);
}
#pragma endregion

#pragma region Console

#if defined (WIN32)
static BOOL WINAPI ConsoleEventHandler(DWORD event)
{
	switch (event) {
	case CTRL_SHUTDOWN_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_CLOSE_EVENT:
	case CTRL_BREAK_EVENT:
		raise(SIGTERM);
		return TRUE;
	case CTRL_C_EVENT:
	default: //pass to the next handler
		return FALSE;
	}
}
#endif

/* static variable to show wether there is not a valid stdout.
* Fixes some bugs when -noconsole is used in a read only directory */
static bool no_stdout = false;
void Console_ShowMsg(char const* format, ...)
{
	char buf[512];
	va_list msg;
	va_start(msg, format);
	vsprintf(buf, format, msg);
	strcat(buf, "\n");
	va_end(msg);
	if (!no_stdout) printf("%s", buf); //Else buf is parsed again.
}

#pragma endregion

static void KillSwitch(bool pressed) {
	if (!pressed)
		return;
	throw 1;
}