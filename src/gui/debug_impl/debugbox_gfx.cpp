#include "dosbox.h"
#include "video.h"
#include "cpu.h"
#include "mapper.h"

#include "SDL.h"

extern const char* RunningProgram;
extern bool CPU_CycleAutoAdjust;
void INPUT_LosingFocus();

void GFX_SetTitle(Bit32s cycles, Bits frameskip, bool paused)
{
	char title[200] = { 0 };
	static Bit32s internal_cycles = 0;
	static Bit32s internal_frameskip = 0;
	if (cycles != -1) internal_cycles = cycles;
	if (frameskip != -1) internal_frameskip = frameskip;
	if (CPU_CycleAutoAdjust) {
		sprintf(title, "DOSBox %s, CPU speed: max %3d%% cycles, Frameskip %2d, Program: %8s", VERSION, internal_cycles, internal_frameskip, RunningProgram);
	}
	else {
		sprintf(title, "DOSBox %s, CPU speed: %8d cycles, Frameskip %2d, Program: %8s", VERSION, internal_cycles, internal_frameskip, RunningProgram);
	}

	if (paused) strcat(title, " PAUSED");
	s_Sdl.DosBoxWindowTitle = title;
}

void GFX_SetPalette(Bitu start, Bitu count, GFX_PalEntry * entries)
{
	//if (!SDL_SetPaletteColors(sdl.surface->format->palette, (SDL_Color *)entries, start, count)) {
	//	E_Exit("SDL:Can't set palette");
	//}
}

Bitu GFX_GetBestMode(Bitu flags) 
{
	flags |= GFX_SCALING;
	flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
	return flags;
}

Bitu GFX_GetRGB(Bit8u red, Bit8u green, Bit8u blue) 
{
	//		return ((red << 0) | (green << 8) | (blue << 16)) | (255 << 24);
	//USE BGRA
	return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
}

static int int_log2(int val) {
	int log = 0;
	while ((val >>= 1) != 0)
		log++;
	return log;
}

Bitu GFX_SetSize(Bitu width, Bitu height, Bitu flags, double scalex, double scaley, GFX_CallBack_t callback) 
{
	if (s_Sdl.updating)
		GFX_EndUpdate(0);

	s_Sdl.Draw.width = width;
	s_Sdl.Draw.height = height;
	s_Sdl.Draw.callback = callback;
	s_Sdl.Draw.scalex = scalex;
	s_Sdl.Draw.scaley = scaley;

	int bpp = 0;
	Bitu retFlags = 0;

		if (s_Sdl.Opengl.pixel_buffer_object)
		{
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
			if (s_Sdl.Opengl.buffer) glDeleteBuffers(1, &s_Sdl.Opengl.buffer);
		}
		else if (s_Sdl.Opengl.framebuf)
		{
			free(s_Sdl.Opengl.framebuf);
		}
		s_Sdl.Opengl.framebuf = 0;

		if ((flags&GFX_CAN_32) == 0)
		{
			E_Exit("SDL:OPENGL: Only support 32bits color (?)");
		}

		int texsize = 2 << int_log2(width > height ? width : height);
		if (texsize>s_Sdl.Opengl.max_texsize)
		{
			LOG_MSG("SDL:OPENGL: No support for texturesize of %d, falling back to surface", texsize);
		}
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#if SDL_VERSION_ATLEAST(1, 2, 11)
		//SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0);
#endif
		//GFX_SetupSurfaceScaled(SDL_OPENGL, 0);
		//if (!s_Sdl.surface || s_Sdl.surface->format->BitsPerPixel<15)
		//{
		//	E_Exit("SDL:OPENGL: Can't open drawing surface, are you running in 16bpp (or higher) mode?");
		//}
		/* Create the texture and display list */
		if (s_Sdl.Opengl.pixel_buffer_object)
		{
			glGenBuffers(1, &s_Sdl.Opengl.buffer);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, s_Sdl.Opengl.buffer);
			glBufferData(GL_PIXEL_UNPACK_BUFFER, width*height * 4, NULL, GL_STREAM_DRAW);
			glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		}
		else 
		{
			s_Sdl.Opengl.framebuf = malloc(width*height * 4);		//32 bit color
		}
		s_Sdl.Opengl.pitch = width * 4;

		glBindTexture(GL_TEXTURE_2D, s_Sdl.DosBoxRenderTextureId);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);

		retFlags = GFX_CAN_32 | GFX_SCALING;
		if (s_Sdl.Opengl.pixel_buffer_object)
			retFlags |= GFX_HARDWARE;

	if (retFlags)
		GFX_Start();
	if (!s_Sdl.Mouse.autoenable) SDL_ShowCursor(s_Sdl.Mouse.autolock ? SDL_DISABLE : SDL_ENABLE);
	return retFlags;
}

bool GFX_StartUpdate(Bit8u * & pixels, Bitu & pitch)
{
	if (!s_Sdl.active || s_Sdl.updating)
		return false;

	if (s_Sdl.Opengl.pixel_buffer_object)
	{
		glBindBuffer(GL_PIXEL_UNPACK_BUFFER, s_Sdl.Opengl.buffer);
		pixels = (Bit8u *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
	}
	else
	{
		pixels = (Bit8u *)s_Sdl.Opengl.framebuf;
	}
	pitch = s_Sdl.Opengl.pitch;
	s_Sdl.updating = true;
	return true;
}

void GFX_EndUpdate(const Bit16u *changedLines)
{
	if (!s_Sdl.updating)
		return;
	s_Sdl.updating = false;

	if (changedLines)
	{
		Bitu y = 0, index = 0;
		glBindTexture(GL_TEXTURE_2D, s_Sdl.DosBoxRenderTextureId);
		while (y < s_Sdl.Draw.height) {
			if (!(index & 1)) 
			{
				y += changedLines[index];
			}
			else 
			{
				Bit8u *pixels = (Bit8u *)s_Sdl.Opengl.framebuf + y * s_Sdl.Opengl.pitch;
				Bitu height = changedLines[index];
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y,
					s_Sdl.Draw.width, height, GL_BGRA,
					GL_UNSIGNED_INT_8_8_8_8_REV, pixels);

				y += height;
			}
			index++;
		}
	}

	Debug_UpdateAndDraw(true);
}

void GFX_ResetScreen(void) 
{
	GFX_Stop();
	if (s_Sdl.Draw.callback)
		(s_Sdl.Draw.callback)(GFX_CallBackReset);
	GFX_Start();
	CPU_Reset_AutoAdjust();
}

void GFX_Start() 
{
	s_Sdl.active = true;
}

void GFX_Stop() 
{
	if (s_Sdl.updating)
		GFX_EndUpdate(0);
	s_Sdl.active = false;
}

static unsigned char logo[32 * 32 * 4] = {
#include "dosbox_logo.h"
};
static void GFX_SetIcon() {
#if !defined(MACOSX)
	/* Set Icon (must be done before any sdl_setvideomode call) */
	/* But don't set it on OS X, as we use a nicer external icon there. */
	/* Made into a separate call, so it can be called again when we restart the graphics output on win32 */
#if WORDS_BIGENDIAN
	SDL_Surface* logos = SDL_CreateRGBSurfaceFrom((void*)logo, 32, 32, 32, 128, 0xff000000, 0x00ff0000, 0x0000ff00, 0);
#else
	SDL_Surface* logos = SDL_CreateRGBSurfaceFrom((void*)logo, 32, 32, 32, 128, 0x000000ff, 0x0000ff00, 0x00ff0000, 0);
#endif
	SDL_SetWindowIcon(s_Sdl.Window, logos);
#endif
}

void GFX_LosingFocus(void) 
{
	s_Sdl.laltstate = SDL_KEYUP;
	s_Sdl.raltstate = SDL_KEYUP;
	INPUT_LosingFocus();
}

SDL_Surface* SDL_SetVideoMode_Wrap(int width, int height, int bpp, Bit32u flags) 
{
	return nullptr;
}

void CreateDosBoxRenderTexture()
{
	GLuint my_opengl_texture;
	glGenTextures(1, &my_opengl_texture);
	s_Sdl.DosBoxRenderTextureId = my_opengl_texture;
}