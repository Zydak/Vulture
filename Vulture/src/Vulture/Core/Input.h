#pragma once
#include "pch.h"
#include "glfw/glfw3.h"

#define VL_KEY_SPACE              32
#define VL_KEY_APOSTROPHE         39  /* ' */
#define VL_KEY_COMMA              44  /* , */
#define VL_KEY_MINUS              45  /* - */
#define VL_KEY_PERIOD             46  /* . */
#define VL_KEY_SLASH              47  /* / */
#define VL_KEY_0                  48
#define VL_KEY_1                  49
#define VL_KEY_2                  50
#define VL_KEY_3                  51
#define VL_KEY_4                  52
#define VL_KEY_5                  53
#define VL_KEY_6                  54
#define VL_KEY_7                  55
#define VL_KEY_8                  56
#define VL_KEY_9                  57
#define VL_KEY_SEMICOLON          59  /* ; */
#define VL_KEY_EQUAL              61  /* = */
#define VL_KEY_A                  65
#define VL_KEY_B                  66
#define VL_KEY_C                  67
#define VL_KEY_D                  68
#define VL_KEY_E                  69
#define VL_KEY_F                  70
#define VL_KEY_G                  71
#define VL_KEY_H                  72
#define VL_KEY_I                  73
#define VL_KEY_J                  74
#define VL_KEY_K                  75
#define VL_KEY_L                  76
#define VL_KEY_M                  77
#define VL_KEY_N                  78
#define VL_KEY_O                  79
#define VL_KEY_P                  80
#define VL_KEY_Q                  81
#define VL_KEY_R                  82
#define VL_KEY_S                  83
#define VL_KEY_T                  84
#define VL_KEY_U                  85
#define VL_KEY_V                  86
#define VL_KEY_W                  87
#define VL_KEY_X                  88
#define VL_KEY_Y                  89
#define VL_KEY_Z                  90
#define VL_KEY_LEFT_BRACKET       91  /* [ */
#define VL_KEY_BACKSLASH          92  /* \ */
#define VL_KEY_RIGHT_BRACKET      93  /* ] */
#define VL_KEY_GRAVE_ACCENT       96  /* ` */
#define VL_KEY_WORLD_1            161 /* non-US #1 */
#define VL_KEY_WORLD_2            162 /* non-US #2 */

/* Function keys */
#define VL_KEY_ESCAPE             256
#define VL_KEY_ENTER              257
#define VL_KEY_TAB                258
#define VL_KEY_BACKSPACE          259
#define VL_KEY_INSERT             260
#define VL_KEY_DELETE             261
#define VL_KEY_RIGHT              262
#define VL_KEY_LEFT               263
#define VL_KEY_DOWN               264
#define VL_KEY_UP                 265
#define VL_KEY_PAGE_UP            266
#define VL_KEY_PAGE_DOWN          267
#define VL_KEY_HOME               268
#define VL_KEY_END                269
#define VL_KEY_CAPS_LOCK          280
#define VL_KEY_SCROLL_LOCK        281
#define VL_KEY_NUM_LOCK           282
#define VL_KEY_PRINT_SCREEN       283
#define VL_KEY_PAUSE              284
#define VL_KEY_F1                 290
#define VL_KEY_F2                 291
#define VL_KEY_F3                 292
#define VL_KEY_F4                 293
#define VL_KEY_F5                 294
#define VL_KEY_F6                 295
#define VL_KEY_F7                 296
#define VL_KEY_F8                 297
#define VL_KEY_F9                 298
#define VL_KEY_F10                299
#define VL_KEY_F11                300
#define VL_KEY_F12                301
#define VL_KEY_F13                302
#define VL_KEY_F14                303
#define VL_KEY_F15                304
#define VL_KEY_F16                305
#define VL_KEY_F17                306
#define VL_KEY_F18                307
#define VL_KEY_F19                308
#define VL_KEY_F20                309
#define VL_KEY_F21                310
#define VL_KEY_F22                311
#define VL_KEY_F23                312
#define VL_KEY_F24                313
#define VL_KEY_F25                314
#define VL_KEY_KP_0               320
#define VL_KEY_KP_1               321
#define VL_KEY_KP_2               322
#define VL_KEY_KP_3               323
#define VL_KEY_KP_4               324
#define VL_KEY_KP_5               325
#define VL_KEY_KP_6               326
#define VL_KEY_KP_7               327
#define VL_KEY_KP_8               328
#define VL_KEY_KP_9               329
#define VL_KEY_KP_DECIMAL         330
#define VL_KEY_KP_DIVIDE          331
#define VL_KEY_KP_MULTIPLY        332
#define VL_KEY_KP_SUBTRACT        333
#define VL_KEY_KP_ADD             334
#define VL_KEY_KP_ENTER           335
#define VL_KEY_KP_EQUAL           336
#define VL_KEY_LEFT_SHIFT         340
#define VL_KEY_LEFT_CONTROL       341
#define VL_KEY_LEFT_ALT           342
#define VL_KEY_LEFT_SUPER         343
#define VL_KEY_RIGHT_SHIFT        344
#define VL_KEY_RIGHT_CONTROL      345
#define VL_KEY_RIGHT_ALT          346
#define VL_KEY_RIGHT_SUPER        347
#define VL_KEY_MENU               348

namespace Vulture
{


	class Input
	{
	public:
		static void Init(GLFWwindow* window);
		static bool IsKeyPressed(int keyCode);

	private:
		static GLFWwindow* s_Window;
	};
}