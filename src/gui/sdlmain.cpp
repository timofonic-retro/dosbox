/*
 *  Copyright (C) 2002-2015  The DOSBox Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even th: implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <GL/glu.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#ifdef WIN32
#include <signal.h>
#include <process.h>
#endif

#include "cross.h"
#include "SDL.h"
#include "../sdl_cdrom/compat_SDL_cdrom.h"

#include "dosbox.h"
#include "video.h"
#include "mouse.h"
#include "pic.h"
#include "timer.h"
#include "setup.h"
#include "support.h"
#include "debug.h"
#include "mapper.h"
#include "vga.h"
#include "keyboard.h"
#include "cpu.h"
#include "cross.h"
#include "control.h"

#define MAPPERFILE "mapper-sdl2-" VERSION ".map"
//#define DISABLE_JOYSTICK

#if C_OPENGL
#include "SDL_opengl.h"
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object 1
typedef void (APIENTRYP PFNGLGENBUFFERSPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLDELETEBUFFERSPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP PFNGLBUFFERDATAPROC) (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef GLvoid* (APIENTRYP PFNGLMAPBUFFERPROC) (GLenum target, GLenum access);
typedef GLboolean (APIENTRYP PFNGLUNMAPBUFFERPROC) (GLenum target);
#endif

PFNGLGENBUFFERSPROC glGenBuffers = NULL;
PFNGLBINDBUFFERPROC glBindBuffer = NULL;
PFNGLDELETEBUFFERSPROC glDeleteBuffers = NULL;
PFNGLBUFFERDATAPROC glBufferData = NULL;
PFNGLMAPBUFFERPROC glMapBuffer = NULL;
PFNGLUNMAPBUFFERPROC glUnmapBuffer = NULL;

PFNGLATTACHSHADERPROC glAttachShader = NULL;
PFNGLCOMPILESHADERPROC glCompileShader = NULL;
PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
PFNGLCREATESHADERPROC glCreateShader = NULL;
PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;
PFNGLDELETESHADERPROC glDeleteShader = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
PFNGLSHADERSOURCEPROC glShaderSource = NULL;
PFNGLUNIFORM2FPROC glUniform2f = NULL;
PFNGLUNIFORM1IPROC glUniform1i = NULL;
PFNGLUSEPROGRAMPROC glUseProgram = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;
PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays = NULL;

#endif //C_OPENGL

#if !(ENVIRON_INCLUDED)
extern char** environ;
#endif

#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#if (HAVE_DDRAW_H)
#include <ddraw.h>
struct private_hwdata {
	LPDIRECTDRAWSURFACE3 dd_surface;
	LPDIRECTDRAWSURFACE3 dd_writebuf;
};
#endif

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

enum SCREEN_TYPES {
	SCREEN_SURFACE,
	SCREEN_TEXTURE,
	SCREEN_OPENGL
};

enum PRIORITY_LEVELS {
	PRIORITY_LEVEL_PAUSE,
	PRIORITY_LEVEL_LOWEST,
	PRIORITY_LEVEL_LOWER,
	PRIORITY_LEVEL_NORMAL,
	PRIORITY_LEVEL_HIGHER,
	PRIORITY_LEVEL_HIGHEST
};

struct SDL_Block {
	bool inited;
	bool active;							//If this isn't set don't draw
	bool updating;
	bool update_display_contents;
	bool update_window;
	int window_desired_width, window_desired_height;
	struct {
		Bit32u width;
		Bit32u height;
		Bitu flags;
		double scalex,scaley;
		GFX_CallBack_t callback;
	} draw;
	bool wait_on_error;
	struct {
		struct {
			Bit16u width, height;
			bool fixed;
			bool display_res;
		} full;
		struct {
			Bit16u width, height;
		} window;
		Bit8u bpp;
		Bit32u sdl2pixelFormat;
		bool fullscreen;
		bool lazy_fullscreen;
		bool lazy_fullscreen_req;
		bool vsync;
		SCREEN_TYPES type;
		SCREEN_TYPES want_type;
	} desktop;
#if C_OPENGL
	struct SDL_OpenGL_Block {
		SDL_GLContext context;
		Bitu pitch;
		void * framebuf;
		GLuint buffer;
		GLuint texture;
		GLint max_texsize;
		bool bilinear;

		static char *const vertex_shader_default_src;
		static char *const fragment_shader_default_src;
		char *vertex_shader_src, *fragment_shader_src;
		GLuint program_object;
		GLuint vao;
		GLuint vertex_vbo;
		GLuint texture_vbo;

		GLuint color_vbo;

		struct {
			// GLint position;
			//GLint tex_coord;
			struct {
				// GLint texture;
				GLint texture_size;
				GLint input_size;
				GLint output_size;
				GLint frame_count;
			} ruby;
		} program_arguments;
		GLint actual_frame_count;
		GLfloat vertex_data[12];
		GLfloat texture_data[8];
		GLfloat color_data[12];
		static const GLushort vertex_data_indices[6];
	} opengl;
#endif	// C_OPENGL

	struct {
		PRIORITY_LEVELS focus;
		PRIORITY_LEVELS nofocus;
	} priority;
	SDL_Rect clip;
	SDL_Surface * surface;
	SDL_Window * window;
	SDL_Renderer * renderer;
	const char * rendererDriver;
	int displayNumber;
	struct {
		SDL_Texture * texture;
		SDL_PixelFormat * pixelFormat;
	} texture;
	SDL_cond *cond;
	struct {
		bool autolock;
		bool autoenable;
		bool requestlock;
		bool locked;
		Bitu sensitivity;
	} mouse;
	SDL_Rect updateRects[1024];
	Bitu num_joysticks;
#if defined (WIN32)
	bool using_windib;
	// Time when sdl regains focus (alt-tab) in windowed mode
	Bit32u focus_ticks;
#endif
	// state of alt-keys for certain special handlings
	SDL_EventType laltstate;
	SDL_EventType raltstate;
};

char *const SDL_Block::SDL_OpenGL_Block::vertex_shader_default_src =
      "#version 330 core\n"
      "in vec4 a_position;\n"
      "out vec2 v_texCoord;\n"
      "uniform vec2 rubyTextureSize;\n"
      "uniform vec2 rubyInputSize;\n"
      "uniform vec2 rubyOutputSize;\n"
      "uniform int rubyFrameCount;\n"
      "\n"
      "void main()\n"
      "{\n"
      "  gl_Position = a_position;\n"
      "  v_texCoord = vec2((a_position.x+1.0)/2.0*rubyInputSize.x/rubyTextureSize.x,(1.0-a_position.y)/2.0*rubyInputSize.y/rubyTextureSize.y);\n"
      "}\n";
char *const SDL_Block::SDL_OpenGL_Block::fragment_shader_default_src =
      "#version 330 core\n"
      "in vec2 v_texCoord;\n"
	  "out color;\n"
      "uniform sampler2D rubyTexture;\n"
      "\n"
      "void main()\n"
      "{\n"
      "  color = texture2D(rubyTexture, v_texCoord);\n"
      "}\n";
const GLushort SDL_Block::SDL_OpenGL_Block::vertex_data_indices[6] = { 0, 1, 2, 0, 2, 3 };

static SDL_Block sdl;

static int SDL_Init_Wrapper(void) {
	int result = ( SDL_Init( SDL_INIT_AUDIO|SDL_INIT_VIDEO|SDL_INIT_TIMER
		|SDL_INIT_NOPARACHUTE
	));
#if C_PHYSICAL_CDROM_MOUNT
	return (result < 0) ? result : Compat_SDL_CDROMInit();
#else
	return result;
#endif
}

static void SDL_Quit_Wrapper(void) {
#if C_PHYSICAL_CDROM_MOUNT
	Compat_SDL_CDROMQuit();
#endif
	SDL_Quit();
}

extern const char* RunningProgram;
extern bool CPU_CycleAutoAdjust;
//Globals for keyboard initialisation
bool startup_state_numlock=false;
bool startup_state_capslock=false;

void GFX_SetTitle(Bit32s cycles,Bits frameskip,bool paused) {
	char title[200]={0};
	static Bit32s internal_cycles=0;
	static Bit32s internal_frameskip=0;
	if(cycles != -1) internal_cycles = cycles;
	if(frameskip != -1) internal_frameskip = frameskip;
	if(CPU_CycleAutoAdjust) {
		sprintf(title,"DOSBox %s, CPU speed: max %3d%% cycles, Frameskip %2d, Program: %8s",VERSION,internal_cycles,internal_frameskip,RunningProgram);
	} else {
		sprintf(title,"DOSBox %s, CPU speed: %8d cycles, Frameskip %2d, Program: %8s",VERSION,internal_cycles,internal_frameskip,RunningProgram);
	}

	if(paused) strcat(title," PAUSED");
	SDL_SetWindowTitle(sdl.window,title); // VERSION is gone...
}

static unsigned char logo[32*32*4]= {
#include "dosbox_logo.h"
};
static void GFX_SetIcon() {
#if !defined(MACOSX)
	/* Set Icon (must be done before any sdl_setvideomode call) */
	/* But don't set it on OS X, as we use a nicer external icon there. */
	/* Made into a separate call, so it can be called again when we restart the graphics output on win32 */
#if WORDS_BIGENDIAN
	SDL_Surface* logos= SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0xff000000,0x00ff0000,0x0000ff00,0);
#else
	SDL_Surface* logos= SDL_CreateRGBSurfaceFrom((void*)logo,32,32,32,128,0x000000ff,0x0000ff00,0x00ff0000,0);
#endif // WORDS_BIGENDIAN
	SDL_SetWindowIcon(sdl.window, logos);
#endif // !defined(MACOSX)
}


static void KillSwitch(bool pressed) {
	if (!pressed)
		return;
	throw 1;
}

static void PauseDOSBox(bool pressed) {
	if (!pressed)
		return;
	GFX_SetTitle(-1,-1,true);
	bool paused = true;
	KEYBOARD_ClrBuffer();
	SDL_Delay(500);
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		// flush event queue.
	}
	/* NOTE: This is one of the few places where we use SDL key codes
	with SDL 2.0, rather than scan codes. Is that the correct behavior? */
	while (paused) {
		SDL_WaitEvent(&event);    // since we're not polling, cpu usage drops to 0.
		switch (event.type) {

			case SDL_QUIT: KillSwitch(true); break;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_RESTORED) {
					// We may need to re-create a texture and more
					GFX_ResetScreen();
				}
				break;
			case SDL_KEYDOWN:   // Must use Pause/Break Key to resume.
			case SDL_KEYUP:
			if(event.key.keysym.sym == SDLK_PAUSE) {

				paused = false;
				GFX_SetTitle(-1,-1,false);
				break;
			}
#if defined (MACOSX)
			if (event.key.keysym.sym == SDLK_q &&
			    (event.key.keysym.mod == KMOD_RGUI ||
			     event.key.keysym.mod == KMOD_LGUI)
			   ) {
				/* On macs, all aps exit when pressing cmd-q */
				KillSwitch(true);
				break;
			}
#endif
		}
	}
}

/* Reset the screen with current values in the sdl structure */
Bitu GFX_GetBestMode(Bitu flags) {
	/* For simplicity, with SDL 2.0 we accept
	the desktop's color depth only for now */
	switch (sdl.desktop.want_type) {
	case SCREEN_SURFACE:
	case SCREEN_TEXTURE:
check_surface:
		flags &= ~GFX_LOVE_8;		//Disable love for 8bpp modes
		/* If we can't get our favorite mode check for another working one */
		switch (sdl.desktop.bpp)
		{
		case 8:
			if (flags & GFX_CAN_8) flags&=~(GFX_CAN_15|GFX_CAN_16|GFX_CAN_32);
			break;
		case 15:
			if (flags & GFX_CAN_15) flags&=~(GFX_CAN_8|GFX_CAN_16|GFX_CAN_32);
			break;
		case 16:
			if (flags & GFX_CAN_16) flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_32);
			break;
		case 24:
		case 32:
			if (flags & GFX_CAN_32) flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
			break;
		}
		if (sdl.desktop.want_type == SCREEN_TEXTURE)
			flags |= GFX_SCALING; // We want texture...
		else	// Or we want/FORCE surface (e.g. for most scalers)
			flags |= GFX_CAN_RANDOM;
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
		// BGRA. We're not on Android
		if (flags & GFX_RGBONLY || !(flags&GFX_CAN_32)) goto check_surface;
		flags|=GFX_SCALING;
		flags&=~(GFX_CAN_8|GFX_CAN_15|GFX_CAN_16);
		break;
#endif
	default:
		goto check_surface;
		break;
	}
	return flags;
}


void GFX_ResetScreen(void) {
	GFX_Stop();
	if (sdl.draw.callback)
		(sdl.draw.callback)( GFX_CallBackReset );
	GFX_Start();
	CPU_Reset_AutoAdjust();
}

void GFX_ForceFullscreenExit(void) {
	if (sdl.desktop.lazy_fullscreen) {
		LOG_MSG("GFX LF: invalid screen change");
	} else {
		sdl.desktop.fullscreen=false;
		GFX_ResetScreen();
	}
}

static int int_log2 (int val) {
    int log = 0;
    while ((val >>= 1) != 0)
	log++;
    return log;
}

static SDL_Window * GFX_SetSDLWindowMode(Bit16u width, Bit16u height, bool fullscreen, SCREEN_TYPES screenType) {
	static SCREEN_TYPES lastType = SCREEN_SURFACE;
	if (sdl.renderer) {
		SDL_DestroyRenderer(sdl.renderer);
		sdl.renderer=0;
	}
	if (sdl.texture.pixelFormat) {
		SDL_FreeFormat(sdl.texture.pixelFormat);
		sdl.texture.pixelFormat = 0;
	}
	if (sdl.texture.texture) {
		SDL_DestroyTexture(sdl.texture.texture);
		sdl.texture.texture=0;
	}
#if C_OPENGL
	if (sdl.opengl.context) {
		SDL_GL_DeleteContext(sdl.opengl.context);
		sdl.opengl.context=0;
	}

	if (sdl.opengl.vao) {
		glBindVertexArray(0);
		glDeleteVertexArrays(1, &sdl.opengl.vao);
		sdl.opengl.vao = 0;
	}

	if (sdl.opengl.vertex_vbo) {
		glDeleteBuffers(1, &sdl.opengl.vertex_vbo);
		sdl.opengl.vertex_vbo = 0;
	}

	if (sdl.opengl.texture_vbo) {
		glDeleteBuffers(1, &sdl.opengl.texture_vbo);
		sdl.opengl.texture_vbo = 0;
	}

	if (sdl.opengl.color_vbo) {
		glDeleteBuffers(1, &sdl.opengl.color_vbo);
		sdl.opengl.color_vbo = 0;
	}

	if (sdl.opengl.program_object) {
		glUseProgram(0);
		glDeleteProgram(sdl.opengl.program_object);
		sdl.opengl.program_object = 0;
	}

#endif // C_OPENGL
	sdl.window_desired_width = width;
	sdl.window_desired_height = height;
	int currWidth, currHeight;
	if (sdl.window) {
		if (!sdl.update_window) {
			SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
			sdl.update_display_contents = ((width == currWidth) && (height == currHeight));
			return sdl.window;
		}
	}
	/* If we change screen type, recreate the window. Furthermore, if
	 * it is our very first time then we simply create a new window.
	 */
	if (!sdl.window || (lastType != screenType)) {
		lastType = screenType;
		if (sdl.window) {
			SDL_DestroyWindow(sdl.window);
		}

		sdl.window = SDL_CreateWindow("",
		                 SDL_WINDOWPOS_UNDEFINED_DISPLAY(sdl.displayNumber),
		                 SDL_WINDOWPOS_UNDEFINED_DISPLAY(sdl.displayNumber),
		                 width, height,
		                 (fullscreen ? (sdl.desktop.full.display_res ? SDL_WINDOW_FULLSCREEN_DESKTOP : SDL_WINDOW_FULLSCREEN) : 0)
		                 | ((screenType == SCREEN_OPENGL) ? SDL_WINDOW_OPENGL : 0) | SDL_WINDOW_SHOWN);
		if (sdl.window)
			GFX_SetTitle(-1,-1,false); //refresh title.
		SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
		sdl.update_display_contents = ((width == currWidth) && (height == currHeight));
		return sdl.window;
	}
	/* Fullscreen mode switching has its limits, and is also problematic on
	 * some window managers. For now, the following may work up to some
	 * level. On X11, SDL_VIDEO_X11_LEGACY_FULLSCREEN=1 can also help,
	 * although it has its own issues.
	 * Suggestion: Use the desktop res if possible, with output=surface
	 * if one is not interested in scaling.
	 * On Android, desktop res is the only way.
	 */
	if (fullscreen) {
		SDL_DisplayMode displayMode;
		SDL_GetWindowDisplayMode(sdl.window, &displayMode);
		displayMode.w = width;
		displayMode.h = height;
		SDL_SetWindowDisplayMode(sdl.window, &displayMode);

		SDL_SetWindowFullscreen(sdl.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
	} else {
		SDL_SetWindowFullscreen(sdl.window, 0);

		SDL_SetWindowSize(sdl.window, width, height);
	}
	// Maybe some requested fullscreen resolution is unsupported?
	SDL_GetWindowSize(sdl.window, &currWidth, &currHeight);
	sdl.update_display_contents = ((width == currWidth) && (height == currHeight));
	return sdl.window;
}

// Used for the mapper UI and more: Creates a fullscreen window with desktop res
// on Android, and a non-fullscreen window with the input dimensions otherwise.
SDL_Window * GFX_SetSDLSurfaceWindow(Bit16u width, Bit16u height) {
	return GFX_SetSDLWindowMode(width, height, false, SCREEN_SURFACE);
}

// Returns the rectangle in the current window to be used for scaling a
// sub-window with the given dimensions, like the mapper UI.
SDL_Rect GFX_GetSDLSurfaceSubwindowDims(Bit16u width, Bit16u height) {
	SDL_Rect rect;
	rect.x=rect.y=0;
	rect.w=width;
	rect.h=height;
	return rect;
}

// CuPerhaps we can can talk about something for the week after that?Perhaps we can can talk about something for the week after that?rrently used for an initial test here
static SDL_Window * GFX_SetSDLOpenGLWindow(Bit16u width, Bit16u height) {
	return GFX_SetSDLWindowMode(width, height, false, SCREEN_OPENGL);
}

static SDL_Window * GFX_SetupWindowScaled(SCREEN_TYPES screenType) {
	LOG_MSG("Calling GFX_SetupWindowScaled");
	Bit16u fixedWidth;
	Bit16u fixedHeight;

	if (sdl.desktop.fullscreen) {
		fixedWidth = sdl.desktop.full.fixed ? sdl.desktop.full.width : 0;
		fixedHeight = sdl.desktop.full.fixed ? sdl.desktop.full.height : 0;
	} else {
		fixedWidth = sdl.desktop.window.width;
		fixedHeight = sdl.desktop.window.height;
	}
	if (fixedWidth && fixedHeight) {
		double ratio_w=(double)fixedWidth/(sdl.draw.width*sdl.draw.scalex);
		double ratio_h=(double)fixedHeight/(sdl.draw.height*sdl.draw.scaley);
		if ( ratio_w < ratio_h) {
			sdl.clip.w=fixedWidth;
			sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley*ratio_w + 0.1); //possible rounding issues
		} else {
			/*
			 * The 0.4 is there to correct for rounding issues.
			 * (partly caused by the rounding issues fix in RENDER_SetSize)
			 */
			sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex*ratio_h + 0.4);
			sdl.clip.h=(Bit16u)fixedHeight;
		}
		if (sdl.desktop.fullscreen) {
			sdl.window = GFX_SetSDLWindowMode(fixedWidth, fixedHeight, sdl.desktop.fullscreen, screenType);
		} else {
			sdl.window = GFX_SetSDLWindowMode(sdl.clip.w, sdl.clip.h, sdl.desktop.fullscreen, screenType);
		}
		if (sdl.window && SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_FULLSCREEN) {
			int windowWidth;
			SDL_GetWindowSize(sdl.window, &windowWidth, NULL);
			sdl.clip.x=(Sint16)((windowWidth-sdl.clip.w)/2);
			sdl.clip.y=(Sint16)((fixedHeight-sdl.clip.h)/2);
		} else {
			sdl.clip.x = 0;
			sdl.clip.y = 0;
		}
		return sdl.window;
	} else {
		sdl.clip.x=0;sdl.clip.y=0;
		sdl.clip.w=(Bit16u)(sdl.draw.width*sdl.draw.scalex);
		sdl.clip.h=(Bit16u)(sdl.draw.height*sdl.draw.scaley);
		sdl.window = GFX_SetSDLWindowMode(sdl.clip.w, sdl.clip.h, sdl.desktop.fullscreen, screenType);
		return sdl.window;
	}
}

/* Create a GLSL shader object, load the shader source, and compile the shader.
 */
GLuint GFX_LoadGLShader(GLenum type, const char *shaderSrc) {

	// Create the shader object
	GLuint shader = 0;
	if (!(shader = glCreateShader(type))) {
		LOG_MSG("%s", SDL_GetError());
		return 0;
	}

	// Load the shader source
	glShaderSource(shader, 1, &shaderSrc, NULL);

	// Compile the shader
	glCompileShader(shader);

	// Check the compile status
	GLint compiled = 0;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
	if (!compiled) {
		GLint infoLen = 0;

		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen > 1) {
			GLchar *infoLog = new GLchar[infoLen];
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			LOG_MSG("%s", infoLog);
			delete[] infoLog;
		}

		glDeleteShader(shader);
		return 0;
	}
	return shader;
}

Bitu GFX_SetSize(Bitu width,Bitu height,Bitu flags, double scalex, double scaley, GFX_CallBack_t callback) {
	LOG_MSG("width: %lu", width);
	LOG_MSG("height: %lu", height);
	LOG_MSG("scalex: %f", scalex);
	LOG_MSG("scaley: %f", scaley);

	if (sdl.updating)
		GFX_EndUpdate( 0 );

	sdl.draw.width=width;
	sdl.draw.height=height;
	sdl.draw.callback=callback;
	sdl.draw.scalex=scalex;
	sdl.draw.scaley=scaley;

	Bitu retFlags = 0;

	switch (sdl.desktop.want_type) {
	case SCREEN_SURFACE:
dosurface:
		sdl.desktop.type=SCREEN_SURFACE;
		sdl.clip.w=width;
		sdl.clip.h=height;
		if (sdl.desktop.fullscreen) {
			if (sdl.desktop.full.fixed) {
				sdl.clip.x=(Sint16)((sdl.desktop.full.width-width)/2);
				sdl.clip.y=(Sint16)((sdl.desktop.full.height-height)/2);
				sdl.window = GFX_SetSDLWindowMode(sdl.desktop.full.width,
				                                  sdl.desktop.full.height,
				                                  sdl.desktop.fullscreen, sdl.desktop.type);
				if (sdl.window == NULL)
					E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",sdl.desktop.full.width,sdl.desktop.full.height,sdl.desktop.bpp,SDL_GetError());
			} else {
				sdl.clip.x=0;sdl.clip.y=0;
				sdl.window = GFX_SetSDLWindowMode(width, height,
				                                  sdl.desktop.fullscreen, sdl.desktop.type);
				if (sdl.window == NULL)
					E_Exit("Could not set fullscreen video mode %ix%i-%i: %s",(int)width,(int)height,sdl.desktop.bpp,SDL_GetError());
			}
		} else {
			sdl.clip.x=0;sdl.clip.y=0;
			sdl.window = GFX_SetSDLWindowMode(width, height,
			                                  sdl.desktop.fullscreen, sdl.desktop.type);
			if (sdl.window == NULL)
				E_Exit("Could not set windowed video mode %ix%i-%i: %s",(int)width,(int)height,sdl.desktop.bpp,SDL_GetError());
		}
		sdl.surface = SDL_GetWindowSurface(sdl.window);
		if (sdl.surface == NULL)
				E_Exit("Could not retrieve window surface: %s",SDL_GetError());
		switch (sdl.surface->format->BitsPerPixel) {
			case 8:
				retFlags = GFX_CAN_8;
				break;
			case 15:
				retFlags = GFX_CAN_15;
				break;
			case 16:
				retFlags = GFX_CAN_16;
				break;
			case 32:
				retFlags = GFX_CAN_32;
				break;
		}
		/* Fix a glitch with aspect=true occuring when
		changing between modes with different dimensions */
		SDL_FillRect(sdl.surface, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
		SDL_UpdateWindowSurface(sdl.window);
		break;
	case SCREEN_TEXTURE:
	{
		if (!GFX_SetupWindowScaled(sdl.desktop.want_type)) {
			LOG_MSG("SDL:Can't set video mode, falling back to surface");
			goto dosurface;
		}
		if (strcmp(sdl.rendererDriver, "auto"))
			SDL_SetHint(SDL_HINT_RENDER_DRIVER, sdl.rendererDriver);
		sdl.renderer = SDL_CreateRenderer(sdl.window, -1,
		                                  SDL_RENDERER_ACCELERATED |
		                                  (sdl.desktop.vsync ? SDL_RENDERER_PRESENTVSYNC : 0));
		if (!sdl.renderer) {
			LOG_MSG("%s", SDL_GetError());
			LOG_MSG("SDL:Can't create renderer, falling back to surface");
			goto dosurface;
		}
		/* SDL_PIXELFORMAT_ARGB8888 is possible with most
		rendering drivers, "opengles" being a notable exception */
		sdl.texture.texture = SDL_CreateTexture(sdl.renderer, SDL_PIXELFORMAT_ARGB8888,
		                                        SDL_TEXTUREACCESS_STREAMING, width, height);
		/* SDL_PIXELFORMAT_ABGR8888 (not RGB) is the
		only supported format for the "opengles" driver */
		if (!sdl.texture.texture) {
			if (flags & GFX_RGBONLY) goto dosurface;
			sdl.texture.texture = SDL_CreateTexture(sdl.renderer, SDL_PIXELFORMAT_ABGR8888,
			                                        SDL_TEXTUREACCESS_STREAMING, width, height);
		}
		if (!sdl.texture.texture) {
			SDL_DestroyRenderer(sdl.renderer);
			sdl.renderer = NULL;
			LOG_MSG("SDL:Can't create texture, falling back to surface");
			goto dosurface;
		}
		SDL_SetRenderDrawColor(sdl.renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
		sdl.desktop.type=SCREEN_TEXTURE;
		Uint32 pixelFormat;
		SDL_QueryTexture(sdl.texture.texture, &pixelFormat, NULL, NULL, NULL);
		sdl.texture.pixelFormat = SDL_AllocFormat(pixelFormat);
		switch (SDL_BITSPERPIXEL(pixelFormat)) {
			case 8:
				retFlags = GFX_CAN_8;
				break;
			case 15:
				retFlags = GFX_CAN_15;
				break;
			case 16:
				retFlags = GFX_CAN_16;
				break;
			case 24: /* SDL_BYTESPERPIXEL is probably 4, though. */
			case 32:
				retFlags = GFX_CAN_32;
				break;
		}
		retFlags |= GFX_SCALING;
		SDL_RendererInfo rendererInfo;
		SDL_GetRendererInfo(sdl.renderer, &rendererInfo);
		LOG_MSG("Using driver \"%s\" for renderer", rendererInfo.name);
		if (rendererInfo.flags & SDL_RENDERER_ACCELERATED)
			retFlags |= GFX_HARDWARE;
		break;
	}
#if C_OPENGL
	case SCREEN_OPENGL:
	{
		if (sdl.opengl.framebuf) {
			free(sdl.opengl.framebuf);
		}
		sdl.opengl.framebuf=0;
		if (!(flags&GFX_CAN_32) || (flags & GFX_RGBONLY)) goto dosurface; // BGRA except on Android.
		int texsize=2 << int_log2(width > height ? width : height);
		if (texsize>sdl.opengl.max_texsize) {
			LOG_MSG("SDL:OPENGL:No support for texturesize of %d, falling back to surface",texsize);
			goto dosurface;
		}
		SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);

		GFX_SetupWindowScaled(sdl.desktop.want_type);
		/* We may simply use SDL_BYTESPERPIXEL
		here rather than SDL_BITSPERPIXEL   */
		if (!sdl.window || SDL_BYTESPERPIXEL(SDL_GetWindowPixelFormat(sdl.window))<2) {
			LOG_MSG("SDL:OPENGL:Can't open drawing window, are you running in 16bpp(or higher) mode?");
			goto dosurface;
		}

		sdl.opengl.context = SDL_GL_CreateContext(sdl.window);
		if (sdl.opengl.context == NULL) {
			LOG_MSG("%s", SDL_GetError());
			LOG_MSG("SDL:OPENGL:Can't create OpenGL context, falling back to surface");
			goto dosurface;
		}

		int major_version = 0;
		if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &major_version) != 0) {
			LOG_MSG("%s", SDL_GetError());
		}
		int minor_version = 0;
		if (SDL_GL_GetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, &minor_version) != 0) {
			LOG_MSG("%s", SDL_GetError());
		}
		if (major_version < 3 || minor_version < 3) {
			LOG_MSG("SDL:OPENGL:Can't create OpenGL 3.3 context, falling back to surface.");
			goto dosurface;
		}

		/* Sync to VBlank if desired */
		SDL_GL_SetSwapInterval(sdl.desktop.vsync ? 1 : 0);
		/* Create the texture and display list */
		sdl.opengl.framebuf=malloc(width*height*4);		//32 bit color
		sdl.opengl.pitch=width*4;

		int windowWidth = 0;
		int windowHeight = 0;
		SDL_GetWindowSize(sdl.window, &windowWidth, &windowHeight);

		GLuint vertexShader = GFX_LoadGLShader(GL_VERTEX_SHADER, sdl.opengl.vertex_shader_src);
		if (!vertexShader) {
			LOG_MSG("SDL:OPENGL:Can't compile vertex shader, falling back to surface.");
			goto dosurface;
		}
		GLuint fragmentShader = GFX_LoadGLShader(GL_FRAGMENT_SHADER, sdl.opengl.fragment_shader_src);
		if (!fragmentShader) {
			glDeleteShader(vertexShader);
			LOG_MSG("SDL:OPENGL:Can't compile fragment shader, falling back to surface.");
			goto dosurface;
		}
		sdl.opengl.program_object = glCreateProgram();

		if (!sdl.opengl.program_object) {
			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);
			goto dosurface;
		}
		glAttachShader (sdl.opengl.program_object, vertexShader);
		glAttachShader (sdl.opengl.program_object, fragmentShader);
		// Link the program
		glLinkProgram (sdl.opengl.program_object);
		// Even if we *are* successful, we may delete the shader objects
		glDeleteShader(vertexShader);
		glDeleteShader(fragmentShader);

		// Check the link status
		GLint isProgramLinked;
		glGetProgramiv (sdl.opengl.program_object, GL_LINK_STATUS, &isProgramLinked);

		if (!isProgramLinked)  {
			GLint infoLen = 0;

			glGetProgramiv(sdl.opengl.program_object, GL_INFO_LOG_LENGTH, &infoLen);

			if (infoLen > 1) {
				char* infoLog = (char *) malloc (sizeof(char) * infoLen);

				glGetProgramInfoLog (sdl.opengl.program_object, infoLen, NULL, infoLog);
				LOG_MSG("SDL:OPENGL:Error linking program: %s", infoLog);
				free ( infoLog );
			}

			glDeleteProgram( sdl.opengl.program_object);
			goto dosurface;
		}

		LOG_MSG("sdl.clip.x: %d", sdl.clip.x);
		LOG_MSG("sdl.clip.y: %d", sdl.clip.y);
		LOG_MSG("windowHeight: %d", windowHeight);
		LOG_MSG("sdl.clip.w: %d", sdl.clip.w);
		LOG_MSG("sdl.clip.h: %d", sdl.clip.h);

		glViewport(0, 0, sdl.clip.w, sdl.clip.h);
		// glViewport(sdl.clip.x, windowHeight - (sdl.clip.y + sdl.clip.h), sdl.clip.w, sdl.clip.h);
		glDeleteTextures(1, &sdl.opengl.texture);
 		glGenTextures(1, &sdl.opengl.texture);
		glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);

		// No borders
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		if (sdl.opengl.bilinear) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		} else {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		}

		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize, 0, GL_BGRA, GL_UNSIGNED_BYTE, 0);

		// Time to take advantage of the shader now
		glUseProgram(sdl.opengl.program_object);

		// Get the attribute locations
		// sdl.opengl.program_arguments.position = glGetAttribLocation ( sdl.opengl.program_object, "a_position" );
		// Get uniform locations and set some of them
		// sdl.opengl.program_arguments.ruby.texture = glGetUniformLocation ( sdl.opengl.program_object, "rubyTexture" );
		// glUniform1i (sdl.opengl.program_arguments.ruby.texture, 0);
		sdl.opengl.program_arguments.ruby.texture_size = glGetUniformLocation ( sdl.opengl.program_object, "rubyTextureSize" );
		glUniform2f (sdl.opengl.program_arguments.ruby.texture_size, texsize, texsize);
		sdl.opengl.program_arguments.ruby.input_size = glGetUniformLocation ( sdl.opengl.program_object, "rubyInputSize" );
		glUniform2f (sdl.opengl.program_arguments.ruby.input_size, width, height);
		sdl.opengl.program_arguments.ruby.output_size = glGetUniformLocation ( sdl.opengl.program_object, "rubyOutputSize" );
		glUniform2f (sdl.opengl.program_arguments.ruby.output_size, sdl.clip.w, sdl.clip.h);
		// The following uniform is *not* set right now
		sdl.opengl.actual_frame_count = 0;
		sdl.opengl.program_arguments.ruby.frame_count = glGetUniformLocation ( sdl.opengl.program_object, "rubyFrameCount" );

		glGenVertexArrays(1, &sdl.opengl.vao);
		glBindVertexArray(sdl.opengl.vao);

		// Vertex coordinates

		// lower left
		sdl.opengl.vertex_data[0] = -1.0f;
		sdl.opengl.vertex_data[1] = -1.0f;
		sdl.opengl.vertex_data[2] = 0.0f;
		// lower right
		sdl.opengl.vertex_data[3] = 1.0f;
		sdl.opengl.vertex_data[4] = -1.0f;
		sdl.opengl.vertex_data[5] = 0.0f;
		// upper right
		sdl.opengl.vertex_data[6] = 1.0f;
		sdl.opengl.vertex_data[7] = 1.0f;
		sdl.opengl.vertex_data[8] = 0.0f;
		// upper left
		sdl.opengl.vertex_data[9] = -1.0f;
		sdl.opengl.vertex_data[10] = 1.0f;
		sdl.opengl.vertex_data[11] = 0.0f;

		glGenBuffers(1, &sdl.opengl.vertex_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, sdl.opengl.vertex_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(sdl.opengl.vertex_data), sdl.opengl.vertex_data, GL_STATIC_DRAW);

		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof (GLfloat), (GLvoid *)0);
		glEnableVertexAttribArray(0);

		// Textures now. With DosBox's framebuffer, the origin is at the top left, and the t
		// axis is reversed.

		// lower left
		sdl.opengl.texture_data[0] = 0.0f;
		sdl.opengl.texture_data[1] = 1.0f;
		// lower right
		sdl.opengl.texture_data[2] = 1.0f;
		sdl.opengl.texture_data[3] = 1.0f;
		// upper right
		sdl.opengl.texture_data[4] = 1.0f;
		sdl.opengl.texture_data[5] = 0.0f;
		// upper left
		sdl.opengl.texture_data[6] = 0.0f;
		sdl.opengl.texture_data[7] = 0.0f;

		glGenBuffers(1, &sdl.opengl.texture_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, sdl.opengl.texture_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(sdl.opengl.texture_data), sdl.opengl.texture_data, GL_STATIC_DRAW);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_TRUE, 2 * sizeof (GLfloat), (GLvoid *)0);
		glEnableVertexAttribArray(1);

		// Colors

		// lower left
		sdl.opengl.color_data[0] = 1.0f;
		sdl.opengl.color_data[1] = 0.0f;
		sdl.opengl.color_data[2] = 0.0f;

		// lower right
		sdl.opengl.color_data[3] = 0.0f;
		sdl.opengl.color_data[4] = 1.0f;
		sdl.opengl.color_data[5] = 0.0f;

		// upper right
		sdl.opengl.color_data[6] = 0.0f;
		sdl.opengl.color_data[7] = 0.0f;
		sdl.opengl.color_data[8] = 1.0f;

		// upper left
		sdl.opengl.color_data[9] = 1.0f;
		sdl.opengl.color_data[10] = 1.0f;
		sdl.opengl.color_data[11] = 0.0f;

		glGenBuffers(1, &sdl.opengl.color_vbo);
		glBindBuffer(GL_ARRAY_BUFFER, sdl.opengl.color_vbo);
		glBufferData(GL_ARRAY_BUFFER, sizeof(sdl.opengl.color_data), sdl.opengl.color_data, GL_STATIC_DRAW);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_TRUE, 3 * sizeof (GLfloat), (GLvoid *)0);
		glEnableVertexAttribArray(2);

		sdl.desktop.type=SCREEN_OPENGL;
		retFlags = GFX_CAN_32 | GFX_SCALING;
	break;
		}//OPENGL
#endif	//C_OPENGL
	default:
		goto dosurface;
		break;
	}//CASE
	if (retFlags)
		GFX_Start();
	if (!sdl.mouse.autoenable) SDL_ShowCursor(sdl.mouse.autolock?SDL_DISABLE:SDL_ENABLE);
	return retFlags;
}


void GFX_CaptureMouse(void) {
	sdl.mouse.locked=!sdl.mouse.locked;
	if (sdl.mouse.locked) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_SetRelativeMouseMode(SDL_FALSE);
		if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
        mouselocked=sdl.mouse.locked;
}

void GFX_UpdateSDLCaptureState(void) {
	if (sdl.mouse.locked) {
		SDL_SetRelativeMouseMode(SDL_TRUE);
		SDL_ShowCursor(SDL_DISABLE);
	} else {
		SDL_SetRelativeMouseMode(SDL_FALSE);
		if (sdl.mouse.autoenable || !sdl.mouse.autolock) SDL_ShowCursor(SDL_ENABLE);
	}
	CPU_Reset_AutoAdjust();
	GFX_SetTitle(-1,-1,false);
}

bool mouselocked; //Global variable for mapper
static void CaptureMouse(bool pressed) {
	if (!pressed)
		return;
	GFX_CaptureMouse();
}

#if defined (WIN32)
STICKYKEYS stick_keys = {sizeof(STICKYKEYS), 0};
void sticky_keys(bool restore){
	static bool inited = false;
	if (!inited){
		inited = true;
		SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
	}
	if (restore) {
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &stick_keys, 0);
		return;
	}
	//Get current sticky keys layout:
	STICKYKEYS s = {sizeof(STICKYKEYS), 0};
	SystemParametersInfo(SPI_GETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	if ( !(s.dwFlags & SKF_STICKYKEYSON)) { //Not on already
		s.dwFlags &= ~SKF_HOTKEYACTIVE;
		SystemParametersInfo(SPI_SETSTICKYKEYS, sizeof(STICKYKEYS), &s, 0);
	}
}
#endif

void GFX_SwitchFullScreen(void) {
	sdl.desktop.fullscreen=!sdl.desktop.fullscreen;
	if (sdl.desktop.fullscreen) {
		if (!sdl.mouse.locked) GFX_CaptureMouse();
#if defined (WIN32)
		sticky_keys(false); //disable sticky keys in fullscreen mode
#endif
	} else {
		if (sdl.mouse.locked) GFX_CaptureMouse();
#if defined (WIN32)
		sticky_keys(true); //restore sticky keys to default state in windowed mode.
#endif
	}
	GFX_ResetScreen();
}

static void SwitchFullScreen(bool pressed) {
	if (!pressed)
		return;

	if (sdl.desktop.lazy_fullscreen) {
		LOG_MSG("GFX LF: fullscreen switching not supported");
	} else {
		GFX_SwitchFullScreen();
	}
}

void GFX_SwitchLazyFullscreen(bool lazy) {
	sdl.desktop.lazy_fullscreen=lazy;
	sdl.desktop.lazy_fullscreen_req=false;
}

void GFX_SwitchFullscreenNoReset(void) {
	sdl.desktop.fullscreen=!sdl.desktop.fullscreen;
}

bool GFX_LazyFullscreenRequested(void) {
	if (sdl.desktop.lazy_fullscreen) return sdl.desktop.lazy_fullscreen_req;
	return false;
}

void GFX_RestoreMode(void) {
	GFX_SetSize(sdl.draw.width,sdl.draw.height,sdl.draw.flags,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback);
	GFX_UpdateSDLCaptureState();
}

void GFX_DrawGLTexture(void) {
	glUniform1i (sdl.opengl.program_arguments.ruby.frame_count, sdl.opengl.actual_frame_count);
	sdl.opengl.actual_frame_count++;
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, sdl.opengl.vertex_data_indices);
}

bool GFX_StartUpdate(Bit8u * & pixels,Bitu & pitch) {
	if (!sdl.update_display_contents)
		return false;
	if (!sdl.active || sdl.updating)
		return false;
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		{
			pixels=(Bit8u *)sdl.surface->pixels;
			pixels+=sdl.clip.y*sdl.surface->pitch;
			pixels+=sdl.clip.x*sdl.surface->format->BytesPerPixel;
			pitch=sdl.surface->pitch;
		}
		sdl.updating=true;
		return true;
	case SCREEN_TEXTURE:
	{
		void * texPixels;
		int texPitch;
		if (SDL_LockTexture(sdl.texture.texture, NULL, &texPixels, &texPitch) < 0)
			return false;
		pixels = (Bit8u *)texPixels;
		pitch = texPitch;
		sdl.updating=true;
		return true;
	}
#if C_OPENGL
	case SCREEN_OPENGL:
		pixels=(Bit8u *)sdl.opengl.framebuf;
		pitch=sdl.opengl.pitch;
		sdl.updating=true;
		return true;
#endif
	default:
		break;
	}
	return false;
}


void GFX_EndUpdate( const Bit16u *changedLines ) {
#if (HAVE_DDRAW_H) && defined(WIN32)
	int ret;
#endif
	if (!sdl.update_display_contents)
		return;
	if (!sdl.updating)
		return;
	sdl.updating=false;
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		if (changedLines) {
			Bitu y = 0, index = 0, rectCount = 0;
			while (y < sdl.draw.height) {
				if (!(index & 1)) {
					y += changedLines[index];
				} else {
					SDL_Rect *rect = &sdl.updateRects[rectCount++];
					rect->x = sdl.clip.x;
					rect->y = sdl.clip.y + y;
					rect->w = (Bit16u)sdl.draw.width;
					rect->h = changedLines[index];
					y += changedLines[index];
				}
				index++;
			}
			if (rectCount)
				SDL_UpdateWindowSurfaceRects( sdl.window, sdl.updateRects, rectCount );
		}
		break;
	case SCREEN_TEXTURE:
		SDL_UnlockTexture(sdl.texture.texture);
		SDL_RenderClear(sdl.renderer);
		SDL_RenderCopy(sdl.renderer, sdl.texture.texture, NULL, &sdl.clip);
		SDL_RenderPresent(sdl.renderer);
		break;
#if C_OPENGL
	case SCREEN_OPENGL:
		if (changedLines) {
			Bitu y = 0, index = 0;
			glBindTexture(GL_TEXTURE_2D, sdl.opengl.texture);
			while (y < sdl.draw.height) {
				if (!(index & 1)) {
					y += changedLines[index];
				} else {
					Bit8u *pixels = (Bit8u *)sdl.opengl.framebuf + y * sdl.opengl.pitch;
					Bitu height = changedLines[index];
					glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y,
						sdl.draw.width, height, GL_BGRA,
						GL_UNSIGNED_INT_8_8_8_8_REV, pixels );
					y += height;
				}
				index++;
			}
			GFX_DrawGLTexture();
			SDL_GL_SwapWindow(sdl.window);
		}
		break;
#endif
	default:
		break;
	}
}

Bitu GFX_GetRGB(Bit8u red,Bit8u green,Bit8u blue) {
	switch (sdl.desktop.type) {
	case SCREEN_SURFACE:
		return SDL_MapRGB(sdl.surface->format,red,green,blue);
	case SCREEN_TEXTURE:
		return SDL_MapRGB(sdl.texture.pixelFormat,red,green,blue);
	case SCREEN_OPENGL:
		// USE BGRA on desktops.
		return ((blue << 0) | (green << 8) | (red << 16)) | (255 << 24);
	}
	return 0;
}

void GFX_Stop() {
	if (sdl.updating)
		GFX_EndUpdate( 0 );
	sdl.active=false;
}

void GFX_Start() {
	sdl.active=true;
}

/* NOTE: The following appears to do its job on Android only *before*
 * a screen rotation occurs. After that, the old dimensions are retrieved.
 * For the updated dimensions we should listen to a window resize event.
 */
void GFX_ObtainDisplayDimensions() {
	SDL_Rect displayDimensions;
	SDL_GetDisplayBounds(sdl.displayNumber, &displayDimensions);
	sdl.desktop.full.width = displayDimensions.w;
	sdl.desktop.full.height = displayDimensions.h;

}

/* Manually update display dimensions in case of a window resize,
 * IF there is the need for that ("yes" on Android, "no" otherwise).
 * Used for the mapper UI on Android.
 * Reason is the usage of GFX_GetSDLSurfaceSubwindowDims, as well as a
 * mere notification of the fact that the window's dimensions are modified.
 */
void GFX_UpdateDisplayDimensions(int width, int height) {
	if (sdl.desktop.full.display_res && sdl.desktop.fullscreen) {
		/* Note: We should not use GFX_ObtainDisplayDimensions
		(SDL_GetDisplayBounds) on Android after a screen rotation:
		The older values from application startup are returned. */
		sdl.desktop.full.width = width;
		sdl.desktop.full.height = height;
	}
}

static void GUI_ShutDown(Section * /*sec*/) {
	GFX_Stop();
	if (sdl.draw.callback) (sdl.draw.callback)( GFX_CallBackStop );
	if (sdl.mouse.locked) GFX_CaptureMouse();
	if (sdl.desktop.fullscreen) GFX_SwitchFullScreen();
}


static void SetPriority(PRIORITY_LEVELS level) {

#if C_SET_PRIORITY
// Do nothing if priorties are not the same and not root, else the highest
// priority can not be set as users can only lower priority (not restore it)

	if((sdl.priority.focus != sdl.priority.nofocus ) &&
		(getuid()!=0) ) return;

#endif
	switch (level) {
#ifdef WIN32
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_LOWER:
		SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_NORMAL:
		SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHER:
		SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS);
		break;
	case PRIORITY_LEVEL_HIGHEST:
		SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
		break;
#elif C_SET_PRIORITY
/* Linux use group as dosbox has mulitple threads under linux */
	case PRIORITY_LEVEL_PAUSE:	// if DOSBox is paused, assume idle priority
	case PRIORITY_LEVEL_LOWEST:
		setpriority (PRIO_PGRP, 0,PRIO_MAX);
		break;
	case PRIORITY_LEVEL_LOWER:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/3));
		break;
	case PRIORITY_LEVEL_NORMAL:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-(PRIO_TOTAL/2));
		break;
	case PRIORITY_LEVEL_HIGHER:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/5) );
		break;
	case PRIORITY_LEVEL_HIGHEST:
		setpriority (PRIO_PGRP, 0,PRIO_MAX-((3*PRIO_TOTAL)/4) );
		break;
#endif
	default:
		break;
	}
}

extern Bit8u int10_font_14[256 * 14];
static void OutputString(Bitu x,Bitu y,const char * text,Bit32u color,Bit32u color2,SDL_Surface * output_surface) {
	Bit32u * draw=(Bit32u*)(((Bit8u *)output_surface->pixels)+((y)*output_surface->pitch))+x;
	while (*text) {
		Bit8u * font=&int10_font_14[(*text)*14];
		Bitu i,j;
		Bit32u * draw_line=draw;
		for (i=0;i<14;i++) {
			Bit8u map=*font++;
			for (j=0;j<8;j++) {
				if (map & 0x80) *((Bit32u*)(draw_line+j))=color; else *((Bit32u*)(draw_line+j))=color2;
				map<<=1;
			}
			draw_line+=output_surface->pitch/4;
		}
		text++;
		draw+=8;
	}
}

#include "dosbox_splash.h"

void Restart(bool pressed);

static void GUI_StartUp(Section * sec) {
	sec->AddDestroyFunction(&GUI_ShutDown);
	Section_prop * section=static_cast<Section_prop *>(sec);
	sdl.active=false;
	sdl.updating=false;
	sdl.update_window=true;
	sdl.update_display_contents=true;

	GFX_SetIcon();

	sdl.desktop.lazy_fullscreen=false;
	sdl.desktop.lazy_fullscreen_req=false;

	sdl.desktop.fullscreen=section->Get_bool("fullscreen");
	sdl.wait_on_error=section->Get_bool("waitonerror");

	Prop_multival* p=section->Get_multival("priority");
	std::string focus = p->GetSection()->Get_string("active");
	std::string notfocus = p->GetSection()->Get_string("inactive");

	if      (focus == "lowest")  { sdl.priority.focus = PRIORITY_LEVEL_LOWEST;  }
	else if (focus == "lower")   { sdl.priority.focus = PRIORITY_LEVEL_LOWER;   }
	else if (focus == "normal")  { sdl.priority.focus = PRIORITY_LEVEL_NORMAL;  }
	else if (focus == "higher")  { sdl.priority.focus = PRIORITY_LEVEL_HIGHER;  }
	else if (focus == "highest") { sdl.priority.focus = PRIORITY_LEVEL_HIGHEST; }

	if      (notfocus == "lowest")  { sdl.priority.nofocus=PRIORITY_LEVEL_LOWEST;  }
	else if (notfocus == "lower")   { sdl.priority.nofocus=PRIORITY_LEVEL_LOWER;   }
	else if (notfocus == "normal")  { sdl.priority.nofocus=PRIORITY_LEVEL_NORMAL;  }
	else if (notfocus == "higher")  { sdl.priority.nofocus=PRIORITY_LEVEL_HIGHER;  }
	else if (notfocus == "highest") { sdl.priority.nofocus=PRIORITY_LEVEL_HIGHEST; }
	else if (notfocus == "pause")   {
		/* we only check for pause here, because it makes no sense
		 * for DOSBox to be paused while it has focus
		 */
		sdl.priority.nofocus=PRIORITY_LEVEL_PAUSE;
	}

	SetPriority(sdl.priority.focus); //Assume focus on startup
	sdl.mouse.locked=false;
	mouselocked=false; //Global for mapper
	sdl.mouse.requestlock=false;
	sdl.desktop.full.fixed=false;
	const char* fullresolution=section->Get_string("fullresolution");
	sdl.desktop.full.width  = 0;
	sdl.desktop.full.height = 0;

	if(fullresolution && *fullresolution) {
		char res[100];
		safe_strncpy( res, fullresolution, sizeof( res ));
		fullresolution = lowcase (res);//so x and X are allowed
		if (strcmp(fullresolution,"original")) {
			sdl.desktop.full.fixed = true;
			if (strcmp(fullresolution,"desktop")) { //desktop = 0x0
				char* height = const_cast<char*>(strchr(fullresolution,'x'));
				if (height && * height) {
					*height = 0;
					sdl.desktop.full.height = (Bit16u)atoi(height+1);
					sdl.desktop.full.width  = (Bit16u)atoi(res);
				}
			}
		}
	}

	sdl.desktop.window.width  = 0;
	sdl.desktop.window.height = 0;

	const char* windowresolution=section->Get_string("windowresolution");
	if(windowresolution && *windowresolution) {
		char res[100];
		safe_strncpy( res,windowresolution, sizeof( res ));
		windowresolution = lowcase (res);//so x and X are allowed
		if(strcmp(windowresolution,"original")) {
			char* height = const_cast<char*>(strchr(windowresolution,'x'));
			if(height && *height) {
				*height = 0;
				sdl.desktop.window.height = (Bit16u)atoi(height+1);
				sdl.desktop.window.width  = (Bit16u)atoi(res);
			}
		}
	}

	sdl.desktop.vsync=section->Get_bool("vsync");

	sdl.displayNumber=section->Get_int("display");
	if ((sdl.displayNumber < 0) || (sdl.displayNumber >= SDL_GetNumVideoDisplays())) {
		sdl.displayNumber = 0;
		LOG_MSG("SDL:Display number out of bounds, switching back to 0");
	}
	sdl.desktop.full.display_res = sdl.desktop.full.fixed && (!sdl.desktop.full.width || !sdl.desktop.full.height);
	if (sdl.desktop.full.display_res) {
		GFX_ObtainDisplayDimensions();
	}

	sdl.mouse.autoenable=section->Get_bool("autolock");
	if (!sdl.mouse.autoenable) SDL_ShowCursor(SDL_DISABLE);
	sdl.mouse.autolock=false;
	sdl.mouse.sensitivity=section->Get_int("sensitivity");
	std::string output=section->Get_string("output");

	/* Setup Mouse correctly if fullscreen */
	if(sdl.desktop.fullscreen) GFX_CaptureMouse();

	if (output == "surface") {
		sdl.desktop.want_type=SCREEN_SURFACE;
	} else if (output == "texture") {
		sdl.desktop.want_type=SCREEN_TEXTURE;
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	} else if (output == "texturenb") {
		sdl.desktop.want_type=SCREEN_TEXTURE;
		// Currently the default, but... oh well
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
#if C_OPENGL
	} else if (output == "opengl") {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.opengl.bilinear=true;
	} else if (output == "openglnb") {
		sdl.desktop.want_type=SCREEN_OPENGL;
		sdl.opengl.bilinear=false;
#endif
	} else {
		LOG_MSG("SDL:Unsupported output device %s, switching back to surface",output.c_str());
		sdl.desktop.want_type=SCREEN_SURFACE;//SHOULDN'T BE POSSIBLE anymore
	}

	sdl.texture.texture=0;
	sdl.texture.pixelFormat=0;
	sdl.window=0;
	sdl.renderer=0;
	sdl.rendererDriver = section->Get_string("renderer");

#if C_OPENGL
	if(sdl.desktop.want_type==SCREEN_OPENGL) { /* OPENGL is requested */
		if (!GFX_SetSDLOpenGLWindow(640,400)) {
			LOG_MSG("Could not create OpenGL window, switching back to surface");
			sdl.desktop.want_type=SCREEN_SURFACE;
		} else {
			sdl.opengl.context = SDL_GL_CreateContext(sdl.window);
			if (sdl.opengl.context == 0) {
				LOG_MSG("Could not create OpenGL context, switching back to surface");
				sdl.desktop.want_type=SCREEN_SURFACE;
			}
		}

		if (sdl.desktop.want_type==SCREEN_OPENGL) {
			sdl.opengl.program_object=0;
			sdl.opengl.vao = 0;
			sdl.opengl.vertex_vbo = 0;
			sdl.opengl.vertex_shader_src = sdl.opengl.vertex_shader_default_src;
			sdl.opengl.fragment_shader_src = sdl.opengl.fragment_shader_default_src;
			std::string glshader_filename=section->Get_string("glshader");
			if (!glshader_filename.empty()) {
				std::string config_path;
				Cross::GetPlatformConfigDir(config_path);
				size_t file_size;
				FILE *fp = fopen((config_path + "glshaders" + CROSS_FILESPLIT + glshader_filename + ".vsh").c_str(), "rb");

				if (fp) {
					fseek(fp, 0, SEEK_END);
					file_size = ftell(fp);
					// DO NOT FORGET TO ADD ONE MORE FOR THE NULL CHAR!
					sdl.opengl.vertex_shader_src = (char *)malloc(file_size+1);
					fseek(fp, 0, SEEK_SET);
					fread(sdl.opengl.vertex_shader_src, file_size, 1, fp);
					fclose(fp);
					// DO NOT FORGET THIS!
					sdl.opengl.vertex_shader_src[file_size] = '\0';
				}

				fp = fopen((config_path + "glshaders" + CROSS_FILESPLIT + glshader_filename + ".fsh").c_str(), "rb");
				if (fp) {
					fseek(fp, 0, SEEK_END);
					file_size = ftell(fp);
					// DO NOT FORGET TO ADD ONE MORE FOR THE NULL CHAR!
					sdl.opengl.fragment_shader_src = (char *)malloc(file_size+1);
					fseek(fp, 0, SEEK_SET);
					fread(sdl.opengl.fragment_shader_src, file_size, 1, fp);
					fclose(fp);
					// DO NOT FORGET THIS!
					sdl.opengl.fragment_shader_src[file_size] = '\0';
				}
			}

			sdl.opengl.buffer=0;
			sdl.opengl.framebuf=0;
			sdl.opengl.texture=0;

			glGetIntegerv (GL_MAX_TEXTURE_SIZE, &sdl.opengl.max_texsize);
			glGenBuffers = (PFNGLGENBUFFERSPROC)SDL_GL_GetProcAddress("glGenBuffers");
			glBindBuffer = (PFNGLBINDBUFFERPROC)SDL_GL_GetProcAddress("glBindBuffer");
			glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)SDL_GL_GetProcAddress("glDeleteBuffers");
			glBufferData = (PFNGLBUFFERDATAPROC)SDL_GL_GetProcAddress("glBufferData");
			glMapBuffer = (PFNGLMAPBUFFERPROC)SDL_GL_GetProcAddress("glMapBuffer");
			glUnmapBuffer = (PFNGLUNMAPBUFFERPROC)SDL_GL_GetProcAddress("glUnmapBuffer");

			glAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
			glCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
			glCreateProgram = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
			glCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
			glDeleteProgram = (PFNGLDELETEPROGRAMPROC)SDL_GL_GetProcAddress("glDeleteProgram");
			glDeleteShader = (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader");
			glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
			glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)SDL_GL_GetProcAddress("glGetAttribLocation");
			glGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
			glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
			glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
			glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
			glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation");
			glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
			glShaderSource = (PFNGLSHADERSOURCEPROC)SDL_GL_GetProcAddress("glShaderSource");
			glUniform2f = (PFNGLUNIFORM2FPROC)SDL_GL_GetProcAddress("glUniform2f");
			glUniform1i = (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i");
			glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");
			glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer");
			glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glGenVertexArrays");
			glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)SDL_GL_GetProcAddress("glBindVertexArray");
			glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)SDL_GL_GetProcAddress("glDeleteVertexArrays");
		}
	} /* OPENGL is requested end */

#endif	//OPENGL
	/* Initialize screen for first time */
	if (!GFX_SetSDLSurfaceWindow(640,400))
		E_Exit("Could not initialize video: %s",SDL_GetError());
	sdl.surface = SDL_GetWindowSurface(sdl.window);
	SDL_Rect splash_rect=GFX_GetSDLSurfaceSubwindowDims(640,400);
	sdl.desktop.sdl2pixelFormat = SDL_GetWindowPixelFormat(sdl.window);
	LOG_MSG("SDL:Current window pixel format: %s", SDL_GetPixelFormatName(sdl.desktop.sdl2pixelFormat));
	/* Do NOT use SDL_BITSPERPIXEL here - It returns 24 for
	SDL_PIXELFORMAT_RGB888, while SDL_BYTESPERPIXEL returns 4.
	To compare, with SDL 1.2 the detected desktop color depth is 32 bpp. */
	sdl.desktop.bpp=8*SDL_BYTESPERPIXEL(sdl.desktop.sdl2pixelFormat);
	if (sdl.desktop.bpp==24) {
		LOG_MSG("SDL:You are running in 24 bpp mode, this will slow down things!");
	}
	GFX_Stop();
	SDL_SetWindowTitle(sdl.window,"DOSBox"); // VERSION is gone...

/* The endian part is intentionally disabled as somehow it produces correct results without according to rhoenie*/
//#if SDL_BYTEORDER == SDL_BIG_ENDIAN
//    Bit32u rmask = 0xff000000;
//    Bit32u gmask = 0x00ff0000;
//    Bit32u bmask = 0x0000ff00;
//#else
    Bit32u rmask = 0x000000ff;
    Bit32u gmask = 0x0000ff00;
    Bit32u bmask = 0x00ff0000;
//#endif

/* Please leave the Splash screen stuff in working order in DOSBox. We spend a lot of time making DOSBox. */
	SDL_Surface* splash_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 400, 32, rmask, gmask, bmask, 0);
	if (splash_surf) {
		SDL_SetSurfaceBlendMode(splash_surf, SDL_BLENDMODE_BLEND);
		SDL_FillRect(splash_surf, NULL, SDL_MapRGB(splash_surf->format, 0, 0, 0));

		Bit8u* tmpbufp = new Bit8u[640*400*3];
		GIMP_IMAGE_RUN_LENGTH_DECODE(tmpbufp,gimp_image.rle_pixel_data,640*400,3);
		for (Bitu y=0; y<400; y++) {

			Bit8u* tmpbuf = tmpbufp + y*640*3;
			Bit32u * draw=(Bit32u*)(((Bit8u *)splash_surf->pixels)+((y)*splash_surf->pitch));
			for (Bitu x=0; x<640; x++) {
//#if SDL_BYTEORDER == SDL_BIG_ENDIAN
//				*draw++ = tmpbuf[x*3+2]+tmpbuf[x*3+1]*0x100+tmpbuf[x*3+0]*0x10000+0x00000000;
//#else
				*draw++ = tmpbuf[x*3+0]+tmpbuf[x*3+1]*0x100+tmpbuf[x*3+2]*0x10000+0x00000000;
//#endif
			}
		}

		bool exit_splash = false;

		static Bitu max_splash_loop = 600;
		static Bitu splash_fade = 100;
		static bool use_fadeout = true;

		for (Bit32u ct = 0,startticks = GetTicks();ct < max_splash_loop;ct = GetTicks()-startticks) {
			SDL_Event evt;
			while (SDL_PollEvent(&evt)) {
				if (evt.type == SDL_QUIT) {
					exit_splash = true;
					break;
				}
			}
			if (exit_splash) break;

			if (ct<1) {
				SDL_FillRect(sdl.surface, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
				SDL_SetSurfaceAlphaMod(splash_surf, 255);
				SDL_BlitScaled(splash_surf, NULL, sdl.surface, &splash_rect);
				SDL_UpdateWindowSurface(sdl.window);
			} else if (ct>=max_splash_loop-splash_fade) {
				if (use_fadeout) {
					SDL_FillRect(sdl.surface, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
					SDL_SetSurfaceAlphaMod(splash_surf, (Bit8u)((max_splash_loop-1-ct)*255/(splash_fade-1)));
					SDL_BlitScaled(splash_surf, NULL, sdl.surface, &splash_rect);
					SDL_UpdateWindowSurface(sdl.window);
				}
			} else { // Fix a possible glitch
				SDL_UpdateWindowSurface(sdl.window);
			}

		}

		if (use_fadeout) {
			SDL_FillRect(sdl.surface, NULL, SDL_MapRGB(sdl.surface->format, 0, 0, 0));
			SDL_UpdateWindowSurface(sdl.window);
		}
		SDL_FreeSurface(splash_surf);
		delete [] tmpbufp;

	}

	/* Get some Event handlers */
	MAPPER_AddHandler(KillSwitch,MK_f9,MMOD1,"shutdown","ShutDown");
	MAPPER_AddHandler(CaptureMouse,MK_f10,MMOD1,"capmouse","Cap Mouse");
	MAPPER_AddHandler(SwitchFullScreen,MK_return,MMOD2,"fullscr","Fullscreen");
	MAPPER_AddHandler(Restart,MK_home,MMOD1|MMOD2,"restart","Restart");
#if C_DEBUG
	/* Pause binds with activate-debugger */
#else
	MAPPER_AddHandler(&PauseDOSBox, MK_pause, MMOD2, "pause", "Pause");
#endif
	/* Get Keyboard state of numlock and capslock */
	SDL_Keymod keystate = SDL_GetModState();
	if(keystate&KMOD_NUM) startup_state_numlock = true;
	if(keystate&KMOD_CAPS) startup_state_capslock = true;
}

void Mouse_AutoLock(bool enable) {
	sdl.mouse.autolock=enable;
	if (sdl.mouse.autoenable) sdl.mouse.requestlock=enable;
	else {
		SDL_ShowCursor(enable?SDL_DISABLE:SDL_ENABLE);
		sdl.mouse.requestlock=false;
	}
}

static void HandleMouseMotion(SDL_MouseMotionEvent * motion) {
	if (sdl.mouse.locked || !sdl.mouse.autoenable)
		Mouse_CursorMoved((float)motion->xrel*sdl.mouse.sensitivity/100.0f,
						  (float)motion->yrel*sdl.mouse.sensitivity/100.0f,
						  (float)(motion->x-sdl.clip.x)/(sdl.clip.w-1)*sdl.mouse.sensitivity/100.0f,
						  (float)(motion->y-sdl.clip.y)/(sdl.clip.h-1)*sdl.mouse.sensitivity/100.0f,
						  sdl.mouse.locked);
}

static void HandleMouseButton(SDL_MouseButtonEvent * button) {
	switch (button->state) {
	case SDL_PRESSED:
		if (sdl.mouse.requestlock && !sdl.mouse.locked) {
			GFX_CaptureMouse();
			// Dont pass klick to mouse handler
			break;
		}
		if (!sdl.mouse.autoenable && sdl.mouse.autolock && button->button == SDL_BUTTON_MIDDLE) {
			GFX_CaptureMouse();
			break;
		}
		switch (button->button) {
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
		switch (button->button) {
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

void GFX_LosingFocus(void) {
	sdl.laltstate=SDL_KEYUP;
	sdl.raltstate=SDL_KEYUP;
	MAPPER_LosingFocus();
}

bool GFX_IsFullscreen(void) {
	return sdl.desktop.fullscreen;
}

void GFX_HandleVideoResize(int width, int height) {
	/* Maybe a screen rotation has just occurred, so we simply resize.
	There may be a different cause for a forced resized, though.    */
	if (sdl.desktop.full.display_res && sdl.desktop.fullscreen) {
		/* Note: We should not use GFX_ObtainDisplayDimensions
		(SDL_GetDisplayBounds) on Android after a screen rotation:
		The older values from application startup are returned. */
		sdl.desktop.full.width = width;
		sdl.desktop.full.height = height;
	}
	/* Even if the new window's dimensions are actually the desired ones
	 * we may still need to re-obtain a new window surface or do
	 * a different thing. So we basically call GFX_SetSize, but without
	 * touching the window itself (or else we may end in an infinite loop).
	 *
	 * Furthermore, if the new dimensions are *not* the desired ones, we
	 * don't fight it. Rather than attempting to resize it back, we simply
	 * keep the window as-is and disable screen updates. This is done
	 * in SDL_SetSDLWindowSurface by setting sdl.update_display_contents
	 * to false.
	 */
	sdl.update_window = false;
	GFX_ResetScreen();
	sdl.update_window = true;
}

void GFX_Events() {
	SDL_Event event;
#if defined (REDUCE_JOYSTICK_POLLING)
	static int poll_delay=0;
	int time=GetTicks();
	if (time-poll_delay>20) {
		poll_delay=time;
		if (sdl.num_joysticks>0) SDL_JoystickUpdate();
		MAPPER_UpdateJoysticks();
	}
#endif
	while (SDL_PollEvent(&event)) {
		switch (event.type) {
		case SDL_WINDOWEVENT:
			switch (event.window.event) {
				case SDL_WINDOWEVENT_RESTORED:
					/* We may need to re-create a texture
					 * and more on Android. Another case:
					 * Update surface while using X11.
					 */
					GFX_ResetScreen();
					continue;
				case SDL_WINDOWEVENT_RESIZED:
					GFX_HandleVideoResize(event.window.data1, event.window.data2);
					continue;
				case SDL_WINDOWEVENT_EXPOSED:
					if (sdl.draw.callback) sdl.draw.callback( GFX_CallBackRedraw );
					continue;
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					if (sdl.desktop.fullscreen && !sdl.mouse.locked)
						GFX_CaptureMouse();
					SetPriority(sdl.priority.focus);
					CPU_Disable_SkipAutoAdjust();
					break;
				case SDL_WINDOWEVENT_FOCUS_LOST:
					if (sdl.mouse.locked) {
#ifdef WIN32
						if (sdl.desktop.fullscreen) {
							VGA_KillDrawing();
							GFX_ForceFullscreenExit();
						}
#endif
						GFX_CaptureMouse();
					}
					SetPriority(sdl.priority.nofocus);
					GFX_LosingFocus();
					CPU_Enable_SkipAutoAdjust();
					break;
				default: ;
			}

			/* Non-focus priority is set to pause; check to see if we've lost window or input focus
			 * i.e. has the window been minimised or made inactive?
			 */
			if (sdl.priority.nofocus == PRIORITY_LEVEL_PAUSE) {
				if ((event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) || (event.window.event == SDL_WINDOWEVENT_MINIMIZED)) {
					/* Window has lost focus, pause the emulator.
					 * This is similar to what PauseDOSBox() does, but the exit criteria is different.
					 * Instead of waiting for the user to hit Alt-Break, we wait for the window to
					 * regain window or input focus.
					 */
					bool paused = true;
					SDL_Event ev;

					GFX_SetTitle(-1,-1,true);
					KEYBOARD_ClrBuffer();

					while (paused) {
						// WaitEvent waits for an event rather than polling, so CPU usage drops to zero
						SDL_WaitEvent(&ev);

						switch (ev.type) {
						case SDL_QUIT: throw(0); break; // a bit redundant at linux at least as the active events gets before the quit event.
						case SDL_WINDOWEVENT:     // wait until we get window focus back
							if ((ev.window.event == SDL_WINDOWEVENT_FOCUS_LOST) || (ev.window.event == SDL_WINDOWEVENT_MINIMIZED) || (ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) || (ev.window.event == SDL_WINDOWEVENT_RESTORED) || (ev.window.event == SDL_WINDOWEVENT_EXPOSED)) {
								// We've got focus back, so unpause and break out of the loop
								if ((ev.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) || (ev.window.event == SDL_WINDOWEVENT_RESTORED) || (ev.window.event == SDL_WINDOWEVENT_EXPOSED)) {
									paused = false;
									GFX_SetTitle(-1,-1,false);
								}

								/* Now poke a "release ALT" command into the keyboard buffer
								 * we have to do this, otherwise ALT will 'stick' and cause
								 * problems with the app running in the DOSBox.
								 */
								KEYBOARD_AddKey(KBD_leftalt, false);
								KEYBOARD_AddKey(KBD_rightalt, false);
								if (ev.window.event == SDL_WINDOWEVENT_RESTORED) {
									// We may need to re-create a texture and more
									GFX_ResetScreen();
								}
							}
							break;
						}
					}
				}
			}
			break;
		case SDL_MOUSEMOTION:
			HandleMouseMotion(&event.motion);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			HandleMouseButton(&event.button);
			break;
		case SDL_QUIT:
			throw(0);
			break;
#ifdef WIN32
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			// ignore event alt+tab
			if (event.key.keysym.sym==SDLK_LALT) sdl.laltstate = (SDL_EventType)event.key.type;
			if (event.key.keysym.sym==SDLK_RALT) sdl.raltstate = (SDL_EventType)event.key.type;
			if (((event.key.keysym.sym==SDLK_TAB)) &&
				((sdl.laltstate==SDL_KEYDOWN) || (sdl.raltstate==SDL_KEYDOWN))) break;
			// This can happen as well.
			if (((event.key.keysym.sym == SDLK_TAB )) && (event.key.keysym.mod & KMOD_ALT)) break;
			// ignore tab events that arrive just after regaining focus. (likely the result of alt-tab)
			if ((event.key.keysym.sym == SDLK_TAB) && (GetTicks() - sdl.focus_ticks < 2)) break;
#endif
#if defined (MACOSX)
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			/* On macs CMD-Q is the default key to close an application */
			if (event.key.keysym.sym == SDLK_q &&
			    (event.key.keysym.mod == KMOD_RGUI ||
			     event.key.keysym.mod == KMOD_LGUI)
			    ) {
				KillSwitch(true);
				break;
			}
#endif
		default:
			void MAPPER_CheckEvent(SDL_Event * event);
			MAPPER_CheckEvent(&event);
		}
	}
}

#if defined (WIN32)
static BOOL WINAPI ConsoleEventHandler(DWORD event) {
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
void GFX_ShowMsg(char const* format,...) {
	char buf[512];
	va_list msg;
	va_start(msg,format);
	vsprintf(buf,format,msg);
        strcat(buf,"\n");
	va_end(msg);
	if(!no_stdout) printf("%s",buf); //Else buf is parsed again.
}


void Config_Add_SDL() {
	Section_prop * sdl_sec=control->AddSection_prop("sdl", &GUI_StartUp);
	sdl_sec->AddInitFunction(&MAPPER_StartUp);
	Prop_bool* Pbool;
	Prop_string* Pstring;
	Prop_int* Pint;
	Prop_multival* Pmulti;

	Pbool = sdl_sec->Add_bool("fullscreen",Property::Changeable::Always,false);
	Pbool->Set_help("Start dosbox directly in fullscreen. (Press ALT-Enter to go back)");

	Pbool = sdl_sec->Add_bool("vsync",Property::Changeable::Always,false);
	Pbool->Set_help("Sync to Vblank IF supported by the output device and renderer (if relevant).\n"
	                "It can reduce screen flickering, but it can also result in a slow DOSBox.");

	Pstring = sdl_sec->Add_string("fullresolution",Property::Changeable::Always,"0x0");
	Pstring->Set_help("What resolution to use for fullscreen: original, desktop or a fixed size (e.g. 1024x768).\n"
	                  "  Using your monitor's native resolution with aspect=true might give the best results.\n"
			  "  If you end up with small window on a large screen, try an output different from surface.");

	Pstring = sdl_sec->Add_string("windowresolution",Property::Changeable::Always,"original");
	Pstring->Set_help("Scale the window to this size IF the output device supports hardware scaling.\n"
	                  "  (output=surface does not!)");

	const char* outputs[] = {
		"surface",
		"texture",
		"texturenb",
#if C_OPENGL
		"opengl", "openglnb",
#endif
		0 };
	Pstring = sdl_sec->Add_string("output",Property::Changeable::Always,"texture");
	Pstring->Set_help("What video system to use for output.");
	Pstring->Set_values(outputs);

	Pstring = sdl_sec->Add_string("glshader",Property::Changeable::Always,"");
	Pstring->Set_help("What set of GLSL shaders to use with an OpenGL output. Keep empty if this is not desired.\n"
	                  "  Note that in case it is used, the respective shader files must be found in the \"glshaders\" subdirectory\n"
	                  "  relatively to where the default DOSBox configuration fiel is stored. For shader file naming convention,\n"
	                  "  suppose that you have a pair of shader files ready: mysample.vsh and mysample.fsh.\n"
	                  "  Then glshader=mysample should be set.\n");

	const char* renderers[] = {
		"auto",
#ifdef WIN32
		"direct3d",
#endif
		"opengl",
		"software",
		0 };
	Pstring = sdl_sec->Add_string("renderer",Property::Changeable::Always,"auto");
	Pstring->Set_help("Choose a renderer driver if output=texture or output=texturenb. Use output=auto for an automatic choice.");
	Pstring->Set_values(renderers);

	Pbool = sdl_sec->Add_bool("autolock",Property::Changeable::Always,true);
	Pbool->Set_help("Mouse will automatically lock, if you click on the screen. (Press CTRL-F10 to unlock)");

	Pint = sdl_sec->Add_int("sensitivity",Property::Changeable::Always,100);
	Pint->SetMinMax(1,1000);
	Pint->Set_help("Mouse sensitivity.");

	Pbool = sdl_sec->Add_bool("waitonerror",Property::Changeable::Always, true);
	Pbool->Set_help("Wait before closing the console if dosbox has an error.");

	Pmulti = sdl_sec->Add_multi("priority", Property::Changeable::Always, ",");
	Pmulti->SetValue("higher,normal");
	Pmulti->Set_help("Priority levels for dosbox. Second entry behind the comma is for when dosbox is not focused/minimized.\n"
	                 "  pause is only valid for the second entry.");

	const char* actt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0};
	Pstring = Pmulti->GetSection()->Add_string("active",Property::Changeable::Always,"higher");
	Pstring->Set_values(actt);

	const char* inactt[] = { "lowest", "lower", "normal", "higher", "highest", "pause", 0};
	Pstring = Pmulti->GetSection()->Add_string("inactive",Property::Changeable::Always,"normal");
	Pstring->Set_values(inactt);

	Pstring = sdl_sec->Add_path("mapperfile",Property::Changeable::Always,MAPPERFILE);
	Pstring->Set_help("File used to load/save the key/event mappings from. Resetmapper only works with the default value.");

	Pbool = sdl_sec->Add_bool("usescancodes",Property::Changeable::Always,true);
	Pbool->Set_help("Avoid usage of symkeys, might not work on all operating systems.");
}

static void show_warning(char const * const message) {
	bool textonly = true;
#ifdef WIN32
	textonly = false;
	if ( !sdl.inited && SDL_Init(SDL_INIT_VIDEO|SDL_INIT_NOPARACHUTE) < 0 ) textonly = true;
	sdl.inited = true;
#endif
	printf("%s",message);
	if(textonly) return;
	if (!sdl.window)
		if (!GFX_SetSDLSurfaceWindow(640,400)) return;
	sdl.surface = SDL_GetWindowSurface(sdl.window);
	if(!sdl.surface) return;

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
	Bit32u rmask = 0xff000000;
	Bit32u gmask = 0x00ff0000;
	Bit32u bmask = 0x0000ff00;
#else
	Bit32u rmask = 0x000000ff;
	Bit32u gmask = 0x0000ff00;
	Bit32u bmask = 0x00ff0000;
#endif
	SDL_Surface* splash_surf = SDL_CreateRGBSurface(SDL_SWSURFACE, 640, 400, 32, rmask, gmask, bmask, 0);
	if (!splash_surf) return;

	int x = 120,y = 20;
	std::string m(message),m2;
	std::string::size_type a,b,c,d;

	while(m.size()) { //Max 50 characters. break on space before or on a newline
		c = m.find('\n');
		d = m.rfind(' ',50);
		if(c>d) a=b=d; else a=b=c;
		if( a != std::string::npos) b++;
		m2 = m.substr(0,a); m.erase(0,b);
		OutputString(x,y,m2.c_str(),0xffffffff,0,splash_surf);
		y += 20;
	}

	SDL_BlitSurface(splash_surf, NULL, sdl.surface, NULL);
	SDL_UpdateWindowSurface(sdl.window);
	SDL_Delay(12000);
}

static void launcheditor() {
	std::string path,file;
	Cross::CreatePlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;
	FILE* f = fopen(path.c_str(),"r");
	if(!f && !control->PrintConfig(path.c_str())) {
		printf("tried creating %s. but failed.\n",path.c_str());
		exit(1);
	}
	if(f) fclose(f);
	std::string edit;
	while(control->cmdline->FindString("-editconf",edit,true)) //Loop until one succeeds
		execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
	//if you get here the launching failed!
	printf("can't find editor(s) specified at the command line.\n");
	exit(1);
}
#if C_DEBUG
extern void DEBUG_ShutDown(Section * /*sec*/);
#endif

void restart_program(std::vector<std::string> & parameters) {
	char** newargs = new char* [parameters.size()+1];
	// parameter 0 is the executable path
	// contents of the vector follow
	// last one is NULL
	for(Bitu i = 0; i < parameters.size(); i++) newargs[i]=(char*)parameters[i].c_str();
	newargs[parameters.size()] = NULL;
	SDL_CloseAudio();
	SDL_Delay(50);
	SDL_Quit_Wrapper();
#if C_DEBUG
	// shutdown curses
	DEBUG_ShutDown(NULL);
#endif

	execvp(newargs[0], newargs);
	free(newargs);
}
void Restart(bool pressed) { // mapper handler
	restart_program(control->startup_params);
}

static void launchcaptures(std::string const& edit) {
	std::string path,file;
	Section* t = control->GetSection("dosbox");
	if(t) file = t->GetPropValue("captures");
	if(!t || file == NO_SUCH_PROPERTY) {
		printf("Config system messed up.\n");
		exit(1);
	}
	Cross::CreatePlatformConfigDir(path);
	path += file;
	Cross::CreateDir(path);
	struct stat cstat;
	if(stat(path.c_str(),&cstat) || (cstat.st_mode & S_IFDIR) == 0) {
		printf("%s doesn't exists or isn't a directory.\n",path.c_str());
		exit(1);
	}

	execlp(edit.c_str(),edit.c_str(),path.c_str(),(char*) 0);
	//if you get here the launching failed!
	printf("can't find filemanager %s\n",edit.c_str());
	exit(1);
}

static void printconfiglocation() {
	std::string path,file;
	Cross::CreatePlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;

	FILE* f = fopen(path.c_str(),"r");
	if(!f && !control->PrintConfig(path.c_str())) {
		printf("tried creating %s. but failed",path.c_str());
		exit(1);
	}
	if(f) fclose(f);
	printf("%s\n",path.c_str());
	exit(0);
}

static void eraseconfigfile() {
	FILE* f = fopen("dosbox.conf","r");
	if(f) {
		fclose(f);
		show_warning("Warning: dosbox.conf exists in current working directory.\nThis will override the configuration file at runtime.\n");
	}
	std::string path,file;
	Cross::GetPlatformConfigDir(path);
	Cross::GetPlatformConfigName(file);
	path += file;
	f = fopen(path.c_str(),"r");
	if(!f) exit(0);
	fclose(f);
	unlink(path.c_str());
	exit(0);
}

static void erasemapperfile() {
	FILE* g = fopen("dosbox.conf","r");
	if(g) {
		fclose(g);
		show_warning("Warning: dosbox.conf exists in current working directory.\nKeymapping might not be properly reset.\n"
		             "Please reset configuration as well and delete the dosbox.conf.\n");
	}

	std::string path,file=MAPPERFILE;
	Cross::GetPlatformConfigDir(path);
	path += file;
	FILE* f = fopen(path.c_str(),"r");
	if(!f) exit(0);
	fclose(f);
	unlink(path.c_str());
	exit(0);
}


int main(int argc, char* argv[]) {
	try {
		CommandLine com_line(argc,argv);
		Config myconf(&com_line);
		control=&myconf;
		/* Init the configuration system and add default values */
		Config_Add_SDL();
		DOSBOX_Init();

		std::string editor;
		if(control->cmdline->FindString("-editconf",editor,false)) launcheditor();
		if(control->cmdline->FindString("-opencaptures",editor,true)) launchcaptures(editor);
		if(control->cmdline->FindExist("-eraseconf")) eraseconfigfile();
		if(control->cmdline->FindExist("-resetconf")) eraseconfigfile();
		if(control->cmdline->FindExist("-erasemapper")) erasemapperfile();
		if(control->cmdline->FindExist("-resetmapper")) erasemapperfile();

		/* Can't disable the console with debugger enabled */
#if defined(WIN32) && !(C_DEBUG)
		if (control->cmdline->FindExist("-noconsole")) {
			FreeConsole();
			/* Redirect standard input and standard output */
			if(freopen(STDOUT_FILE, "w", stdout) == NULL)
				no_stdout = true; // No stdout so don't write messages
			freopen(STDERR_FILE, "w", stderr);
			setvbuf(stdout, NULL, _IOLBF, BUFSIZ);	/* Line buffered */
			setbuf(stderr, NULL);					/* No buffering */
		} else {
			if (AllocConsole()) {
				fclose(stdin);
				fclose(stdout);
				fclose(stderr);
				freopen("CONIN$","r",stdin);
				freopen("CONOUT$","w",stdout);
				freopen("CONOUT$","w",stderr);
			}
			SetConsoleTitle("DOSBox Status Window");
		}
#endif  //defined(WIN32) && !(C_DEBUG)
		if (control->cmdline->FindExist("-version") ||
		    control->cmdline->FindExist("--version") ) {
			printf("\nDOSBox version %s, copyright 2002-2015 DOSBox Team.\n\n",VERSION);
			printf("DOSBox is written by the DOSBox Team (See AUTHORS file))\n");
			printf("DOSBox comes with ABSOLUTELY NO WARRANTY.  This is free software,\n");
			printf("and you are welcome to redistribute it under certain conditions;\n");
			printf("please read the COPYING file thoroughly before doing so.\n\n");
			return 0;
		}
		if(control->cmdline->FindExist("-printconf")) printconfiglocation();

#if C_DEBUG
		DEBUG_SetupConsole();
#endif

#if defined(WIN32)
	SetConsoleCtrlHandler((PHANDLER_ROUTINE) ConsoleEventHandler,TRUE);
#endif

#ifdef OS2
        PPIB pib;
        PTIB tib;
        DosGetInfoBlocks(&tib, &pib);
        if (pib->pib_ultype == 2) pib->pib_ultype = 3;
        setbuf(stdout, NULL);
        setbuf(stderr, NULL);
#endif

	/* Display Welcometext in the console */
	LOG_MSG("DOSBox version %s",VERSION);
	LOG_MSG("Copyright 2002-2015 DOSBox Team, published under GNU GPL.");
	LOG_MSG("---");

	/* Init SDL */
	/* Or debian/ubuntu with older libsdl version as they have done this themselves, but then differently.
	 * with this variable they will work correctly. I've only tested the 1.2.14 behaviour against the windows version
	 * of libsdl
	 */
	putenv(const_cast<char*>("SDL_DISABLE_LOCK_KEYS=1"));
	if ( SDL_Init_Wrapper() < 0 )
		E_Exit("Can't init SDL %s",SDL_GetError());
	sdl.inited = true;

#ifndef DISABLE_JOYSTICK
	//Initialise Joystick separately. This way we can warn when it fails instead
	//of exiting the application
	if( SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0 ) LOG_MSG("Failed to init joystick support");
#endif

	sdl.laltstate = SDL_KEYUP;
	sdl.raltstate = SDL_KEYUP;

	sdl.num_joysticks=SDL_NumJoysticks();

	/* Parse configuration files */
	std::string config_file,config_path;
	Cross::GetPlatformConfigDir(config_path);

	//First parse -userconf
	if(control->cmdline->FindExist("-userconf",true)){
		config_file.clear();
		Cross::GetPlatformConfigDir(config_path);
		Cross::GetPlatformConfigName(config_file);
		config_path += config_file;
		control->ParseConfigFile(config_path.c_str());
		if(!control->configfiles.size()) {
			//Try to create the userlevel configfile.
			config_file.clear();
			Cross::CreatePlatformConfigDir(config_path);
			Cross::GetPlatformConfigName(config_file);
			config_path += config_file;
			if(control->PrintConfig(config_path.c_str())) {
				LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s",config_path.c_str());
				//Load them as well. Makes relative paths much easier
				control->ParseConfigFile(config_path.c_str());
			}
		}
	}

	//Second parse -conf switches
	while(control->cmdline->FindString("-conf",config_file,true)) {
		if(!control->ParseConfigFile(config_file.c_str())) {
			// try to load it from the user directory
			control->ParseConfigFile((config_path + config_file).c_str());
		}
	}
	// if none found => parse localdir conf
	if(!control->configfiles.size()) control->ParseConfigFile("dosbox.conf");

	// if none found => parse userlevel conf
	if(!control->configfiles.size()) {
		config_file.clear();
		Cross::GetPlatformConfigName(config_file);
		control->ParseConfigFile((config_path + config_file).c_str());
	}

	if(!control->configfiles.size()) {
		//Try to create the userlevel configfile.
		config_file.clear();
		Cross::CreatePlatformConfigDir(config_path);
		Cross::GetPlatformConfigName(config_file);
		config_path += config_file;
		if(control->PrintConfig(config_path.c_str())) {
			LOG_MSG("CONFIG: Generating default configuration.\nWriting it to %s",config_path.c_str());
			//Load them as well. Makes relative paths much easier
			control->ParseConfigFile(config_path.c_str());
		} else {
			LOG_MSG("CONFIG: Using default settings. Create a configfile to change them");
		}
	}


#if (ENVIRON_LINKED)
		control->ParseEnv(environ);
#endif
		/* Init all the sections */
		control->Init();
		/* Some extra SDL Functions */
		Section_prop * sdl_sec=static_cast<Section_prop *>(control->GetSection("sdl"));

		if (control->cmdline->FindExist("-fullscreen") || sdl_sec->Get_bool("fullscreen")) {
			if(!sdl.desktop.fullscreen) { //only switch if not already in fullscreen
				GFX_SwitchFullScreen();
			}
		}

		/* Init the keyMapper */
		MAPPER_Init();
		if (control->cmdline->FindExist("-startmapper")) MAPPER_RunInternal();
		/* Start up main machine */
		control->StartUp();
		/* Shutdown everything */
	} catch (char * error) {
#if defined (WIN32)
		sticky_keys(true);
#endif
		GFX_ShowMsg("Exit to error: %s",error);
		fflush(NULL);
		if(sdl.wait_on_error) {
			//TODO Maybe look for some way to show message in linux?
#if (C_DEBUG)
			GFX_ShowMsg("Press enter to continue");
			fflush(NULL);
			fgetc(stdin);
#elif defined(WIN32)
			Sleep(5000);
#endif
		}

	}
	catch (int){
		; //nothing, pressed killswitch
	}
	catch(...){
		; // Unknown error, let's just exit.
	}
#if defined (WIN32)
	sticky_keys(true); //Might not be needed if the shutdown function switches to windowed mode, but it doesn't hurt
#endif
	//Force visible mouse to end user. Somehow this sometimes doesn't happen
	SDL_SetRelativeMouseMode(SDL_FALSE);
	SDL_ShowCursor(SDL_ENABLE);

	SDL_Quit_Wrapper();//Let's hope sdl will quit as well when it catches an exception
	return 0;
}

void GFX_GetSize(int &width, int &height, bool &fullscreen) {
	width = sdl.draw.width;
	height = sdl.draw.height;
	fullscreen = sdl.desktop.fullscreen;
}
