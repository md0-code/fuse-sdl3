#ifndef FUSE_LIBRETRO_API_H
#define FUSE_LIBRETRO_API_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RETRO_API_VERSION 1

#if defined(_WIN32)
#define RETRO_CALLCONV __cdecl
#define RETRO_API __declspec(dllexport)
#else
#define RETRO_CALLCONV
#define RETRO_API __attribute__((visibility("default")))
#endif

enum retro_region {
  RETRO_REGION_NTSC = 0,
  RETRO_REGION_PAL = 1
};

enum retro_device {
  RETRO_DEVICE_NONE = 0,
  RETRO_DEVICE_JOYPAD = 1,
  RETRO_DEVICE_MOUSE = 2,
  RETRO_DEVICE_KEYBOARD = 3
};

enum retro_device_id_joypad {
  RETRO_DEVICE_ID_JOYPAD_B = 0,
  RETRO_DEVICE_ID_JOYPAD_Y = 1,
  RETRO_DEVICE_ID_JOYPAD_SELECT = 2,
  RETRO_DEVICE_ID_JOYPAD_START = 3,
  RETRO_DEVICE_ID_JOYPAD_UP = 4,
  RETRO_DEVICE_ID_JOYPAD_DOWN = 5,
  RETRO_DEVICE_ID_JOYPAD_LEFT = 6,
  RETRO_DEVICE_ID_JOYPAD_RIGHT = 7,
  RETRO_DEVICE_ID_JOYPAD_A = 8,
  RETRO_DEVICE_ID_JOYPAD_X = 9,
  RETRO_DEVICE_ID_JOYPAD_L = 10,
  RETRO_DEVICE_ID_JOYPAD_R = 11,
  RETRO_DEVICE_ID_JOYPAD_L2 = 12,
  RETRO_DEVICE_ID_JOYPAD_R2 = 13,
  RETRO_DEVICE_ID_JOYPAD_L3 = 14,
  RETRO_DEVICE_ID_JOYPAD_R3 = 15,
  RETRO_DEVICE_ID_JOYPAD_MASK = 256
};

enum retro_pixel_format {
  RETRO_PIXEL_FORMAT_0RGB1555 = 0,
  RETRO_PIXEL_FORMAT_XRGB8888 = 1,
  RETRO_PIXEL_FORMAT_RGB565 = 2,
  RETRO_PIXEL_FORMAT_UNKNOWN = INT32_MAX
};

enum retro_environment {
  RETRO_ENVIRONMENT_SET_ROTATION = 1,
  RETRO_ENVIRONMENT_GET_OVERSCAN = 2,
  RETRO_ENVIRONMENT_GET_CAN_DUPE = 3,
  RETRO_ENVIRONMENT_SET_MESSAGE = 6,
  RETRO_ENVIRONMENT_SHUTDOWN = 7,
  RETRO_ENVIRONMENT_SET_PERFORMANCE_LEVEL = 8,
  RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY = 9,
  RETRO_ENVIRONMENT_SET_PIXEL_FORMAT = 10,
  RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS = 11,
  RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK = 12,
  RETRO_ENVIRONMENT_SET_DISK_CONTROL_INTERFACE = 13,
  RETRO_ENVIRONMENT_SET_HW_RENDER = 14,
  RETRO_ENVIRONMENT_GET_VARIABLE = 15,
  RETRO_ENVIRONMENT_SET_VARIABLES = 16,
  RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE = 17,
  RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME = 18,
  RETRO_ENVIRONMENT_GET_LIBRETRO_PATH = 19,
  RETRO_ENVIRONMENT_SET_FRAME_TIME_CALLBACK = 21,
  RETRO_ENVIRONMENT_SET_AUDIO_CALLBACK = 22,
  RETRO_ENVIRONMENT_GET_RUMBLE_INTERFACE = 23,
  RETRO_ENVIRONMENT_GET_INPUT_DEVICE_CAPABILITIES = 24,
  RETRO_ENVIRONMENT_GET_SENSOR_INTERFACE = 25,
  RETRO_ENVIRONMENT_GET_CAMERA_INTERFACE = 26,
  RETRO_ENVIRONMENT_GET_LOG_INTERFACE = 27,
  RETRO_ENVIRONMENT_GET_PERF_INTERFACE = 28,
  RETRO_ENVIRONMENT_GET_LOCATION_INTERFACE = 29,
  RETRO_ENVIRONMENT_GET_CORE_ASSETS_DIRECTORY = 30,
  RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY = 31,
  RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO = 32,
  RETRO_ENVIRONMENT_SET_PROC_ADDRESS_CALLBACK = 33,
  RETRO_ENVIRONMENT_SET_SUBSYSTEM_INFO = 34,
  RETRO_ENVIRONMENT_SET_CONTROLLER_INFO = 35,
  RETRO_ENVIRONMENT_SET_MEMORY_MAPS = 36,
  RETRO_ENVIRONMENT_SET_GEOMETRY = 37,
  RETRO_ENVIRONMENT_GET_USERNAME = 38,
  RETRO_ENVIRONMENT_GET_LANGUAGE = 39
};

enum retro_log_level {
  RETRO_LOG_DEBUG = 0,
  RETRO_LOG_INFO = 1,
  RETRO_LOG_WARN = 2,
  RETRO_LOG_ERROR = 3
};

enum retro_key {
  RETROK_BACKSPACE = 8,
  RETROK_TAB = 9,
  RETROK_CLEAR = 12,
  RETROK_RETURN = 13,
  RETROK_PAUSE = 19,
  RETROK_ESCAPE = 27,
  RETROK_SPACE = 32,
  RETROK_EXCLAIM = 33,
  RETROK_QUOTEDBL = 34,
  RETROK_HASH = 35,
  RETROK_DOLLAR = 36,
  RETROK_AMPERSAND = 38,
  RETROK_QUOTE = 39,
  RETROK_LEFTPAREN = 40,
  RETROK_RIGHTPAREN = 41,
  RETROK_ASTERISK = 42,
  RETROK_PLUS = 43,
  RETROK_COMMA = 44,
  RETROK_MINUS = 45,
  RETROK_PERIOD = 46,
  RETROK_SLASH = 47,
  RETROK_0 = 48,
  RETROK_1 = 49,
  RETROK_2 = 50,
  RETROK_3 = 51,
  RETROK_4 = 52,
  RETROK_5 = 53,
  RETROK_6 = 54,
  RETROK_7 = 55,
  RETROK_8 = 56,
  RETROK_9 = 57,
  RETROK_COLON = 58,
  RETROK_SEMICOLON = 59,
  RETROK_LESS = 60,
  RETROK_EQUALS = 61,
  RETROK_GREATER = 62,
  RETROK_QUESTION = 63,
  RETROK_AT = 64,
  RETROK_LEFTBRACKET = 91,
  RETROK_BACKSLASH = 92,
  RETROK_RIGHTBRACKET = 93,
  RETROK_CARET = 94,
  RETROK_UNDERSCORE = 95,
  RETROK_BACKQUOTE = 96,
  RETROK_a = 97,
  RETROK_b = 98,
  RETROK_c = 99,
  RETROK_d = 100,
  RETROK_e = 101,
  RETROK_f = 102,
  RETROK_g = 103,
  RETROK_h = 104,
  RETROK_i = 105,
  RETROK_j = 106,
  RETROK_k = 107,
  RETROK_l = 108,
  RETROK_m = 109,
  RETROK_n = 110,
  RETROK_o = 111,
  RETROK_p = 112,
  RETROK_q = 113,
  RETROK_r = 114,
  RETROK_s = 115,
  RETROK_t = 116,
  RETROK_u = 117,
  RETROK_v = 118,
  RETROK_w = 119,
  RETROK_x = 120,
  RETROK_y = 121,
  RETROK_z = 122,
  RETROK_DELETE = 127,
  RETROK_KP_ENTER = 271,
  RETROK_UP = 273,
  RETROK_DOWN = 274,
  RETROK_RIGHT = 275,
  RETROK_LEFT = 276,
  RETROK_INSERT = 277,
  RETROK_HOME = 278,
  RETROK_END = 279,
  RETROK_PAGEUP = 280,
  RETROK_PAGEDOWN = 281,
  RETROK_F1 = 282,
  RETROK_F2 = 283,
  RETROK_F3 = 284,
  RETROK_F4 = 285,
  RETROK_F5 = 286,
  RETROK_F6 = 287,
  RETROK_F7 = 288,
  RETROK_F8 = 289,
  RETROK_F9 = 290,
  RETROK_F10 = 291,
  RETROK_F11 = 292,
  RETROK_F12 = 293,
  RETROK_NUMLOCK = 300,
  RETROK_CAPSLOCK = 301,
  RETROK_SCROLLOCK = 302,
  RETROK_RSHIFT = 303,
  RETROK_LSHIFT = 304,
  RETROK_RCTRL = 305,
  RETROK_LCTRL = 306,
  RETROK_RALT = 307,
  RETROK_LALT = 308,
  RETROK_RMETA = 309,
  RETROK_LMETA = 310,
  RETROK_LSUPER = 311,
  RETROK_RSUPER = 312,
  RETROK_MODE = 313
};

struct retro_game_info {
  const char *path;
  const void *data;
  size_t size;
  const char *meta;
};

struct retro_system_info {
  const char *library_name;
  const char *library_version;
  const char *valid_extensions;
  bool need_fullpath;
  bool block_extract;
};

struct retro_game_geometry {
  unsigned base_width;
  unsigned base_height;
  unsigned max_width;
  unsigned max_height;
  float aspect_ratio;
};

struct retro_system_timing {
  double fps;
  double sample_rate;
};

struct retro_system_av_info {
  struct retro_game_geometry geometry;
  struct retro_system_timing timing;
};

struct retro_variable {
  const char *key;
  const char *value;
};

struct retro_log_callback {
  void (RETRO_CALLCONV *log)( enum retro_log_level level,
                              const char *fmt, ... );
};

struct retro_keyboard_callback {
  void (RETRO_CALLCONV *callback)( bool down, unsigned keycode,
                                   uint32_t character,
                                   uint16_t key_modifiers );
};

typedef bool (RETRO_CALLCONV *retro_environment_t)( unsigned cmd, void *data );
typedef void (RETRO_CALLCONV *retro_video_refresh_t)( const void *data,
                                                      unsigned width,
                                                      unsigned height,
                                                      size_t pitch );
typedef void (RETRO_CALLCONV *retro_audio_sample_t)( int16_t left,
                                                     int16_t right );
typedef size_t (RETRO_CALLCONV *retro_audio_sample_batch_t)(
  const int16_t *data, size_t frames );
typedef void (RETRO_CALLCONV *retro_input_poll_t)( void );
typedef int16_t (RETRO_CALLCONV *retro_input_state_t)( unsigned port,
                                                       unsigned device,
                                                       unsigned index,
                                                       unsigned id );

#endif
