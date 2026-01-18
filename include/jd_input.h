/* date = March 7th 2024 11:15 pm */

#ifndef JD_INPUT_H
#define JD_INPUT_H

#define _jd_InputMap(key, i) key = i

typedef enum jd_Key {
    _jd_InputMap(jd_MB_Left,            0x01),
    _jd_InputMap(jd_MB_Right,           0x02),
    _jd_InputMap(jd_MB_Middle,          0x04),
    _jd_InputMap(jd_MB_X1,              0x05),
    _jd_InputMap(jd_MB_X2,              0x06),
    _jd_InputMap(jd_Key_Back,           0x08),
    _jd_InputMap(jd_Key_Tab,            0x09),
    _jd_InputMap(jd_Key_Clear,          0x0C),
    _jd_InputMap(jd_Key_Return,         0x0D),
    _jd_InputMap(jd_Key_Shift,          0x10),
    _jd_InputMap(jd_Key_Control,        0x11),
    _jd_InputMap(jd_Key_Menu,           0x12),
    _jd_InputMap(jd_Key_Pause,          0x13),
    _jd_InputMap(jd_Key_Capital,        0x14),
    _jd_InputMap(jd_Key_Space,          0x20),
    _jd_InputMap(jd_Key_Prior,          0x21),
    _jd_InputMap(jd_Key_Next,           0x22),
    _jd_InputMap(jd_Key_End,            0x23),
    _jd_InputMap(jd_Key_Home,           0x24),
    _jd_InputMap(jd_Key_Left,           0x25),
    _jd_InputMap(jd_Key_Up,             0x26),
    _jd_InputMap(jd_Key_Right,          0x27),
    _jd_InputMap(jd_Key_Down,           0x28),
    _jd_InputMap(jd_Key_Select,         0x29),
    _jd_InputMap(jd_Key_Print,          0x2A),
    _jd_InputMap(jd_Key_Execute,        0x2B),
    _jd_InputMap(jd_Key_Snapshot,       0x2C),
    _jd_InputMap(jd_Key_Insert,         0x2D),
    _jd_InputMap(jd_Key_Delete,         0x2E),
    _jd_InputMap(jd_Key_Help,           0x2F),
    _jd_InputMap(jd_Key_0,              0x30),
    _jd_InputMap(jd_Key_1,              0x31),
    _jd_InputMap(jd_Key_2,              0x32),
    _jd_InputMap(jd_Key_3,              0x33),
    _jd_InputMap(jd_Key_4,              0x34),
    _jd_InputMap(jd_Key_5,              0x35),
    _jd_InputMap(jd_Key_6,              0x36),
    _jd_InputMap(jd_Key_7,              0x37),
    _jd_InputMap(jd_Key_8,              0x38),
    _jd_InputMap(jd_Key_9,              0x39),
    _jd_InputMap(jd_Key_A,              0x41),
    _jd_InputMap(jd_Key_B,              0x42),
    _jd_InputMap(jd_Key_C,              0x43),
    _jd_InputMap(jd_Key_D,              0x44),
    _jd_InputMap(jd_Key_E,              0x45),
    _jd_InputMap(jd_Key_F,              0x46),
    _jd_InputMap(jd_Key_G,              0x47),
    _jd_InputMap(jd_Key_H,              0x48),
    _jd_InputMap(jd_Key_I,              0x49),
    _jd_InputMap(jd_Key_J,              0x4A),
    _jd_InputMap(jd_Key_K,              0x4B),
    _jd_InputMap(jd_Key_L,              0x4C),
    _jd_InputMap(jd_Key_M,              0x4D),
    _jd_InputMap(jd_Key_N,              0x4E),
    _jd_InputMap(jd_Key_O,              0x4F),
    _jd_InputMap(jd_Key_P,              0x50),
    _jd_InputMap(jd_Key_Q,              0x51),
    _jd_InputMap(jd_Key_R,              0x52),
    _jd_InputMap(jd_Key_S,              0x53),
    _jd_InputMap(jd_Key_T,              0x54),
    _jd_InputMap(jd_Key_U,              0x55),
    _jd_InputMap(jd_Key_V,              0x56),
    _jd_InputMap(jd_Key_W,              0x57),
    _jd_InputMap(jd_Key_X,              0x58),
    _jd_InputMap(jd_Key_Y,              0x59),
    _jd_InputMap(jd_Key_Z,              0x5A),
    _jd_InputMap(jd_Key_LWin,           0x5B),
    _jd_InputMap(jd_Key_RWin,           0x5C),
    _jd_InputMap(jd_Key_Apps,           0x5D),
    _jd_InputMap(jd_Key_Sleep,          0x5F),
    _jd_InputMap(jd_Key_Numpad0,        0x60),
    _jd_InputMap(jd_Key_Numpad1,        0x61),
    _jd_InputMap(jd_Key_Numpad2,        0x62),
    _jd_InputMap(jd_Key_Numpad3,        0x63),
    _jd_InputMap(jd_Key_Numpad4,        0x64),
    _jd_InputMap(jd_Key_Numpad5,        0x65),
    _jd_InputMap(jd_Key_Numpad6,        0x66),
    _jd_InputMap(jd_Key_Numpad7,        0x67),
    _jd_InputMap(jd_Key_Numpad8,        0x68),
    _jd_InputMap(jd_Key_Numpad9,        0x69),
    _jd_InputMap(jd_Key_Multiply,       0x6A),
    _jd_InputMap(jd_Key_Add,            0x6B),
    _jd_InputMap(jd_Key_Separator,      0x6C),
    _jd_InputMap(jd_Key_Subtract,       0x6D),
    _jd_InputMap(jd_Key_Decimal,        0x6E),
    _jd_InputMap(jd_Key_Divide,         0x6F),
    _jd_InputMap(jd_Key_F1,             0x70),
    _jd_InputMap(jd_Key_F2,             0x71),
    _jd_InputMap(jd_Key_F3,             0x72),
    _jd_InputMap(jd_Key_F4,             0x73),
    _jd_InputMap(jd_Key_F5,             0x74),
    _jd_InputMap(jd_Key_F6,             0x75),
    _jd_InputMap(jd_Key_F7,             0x76),
    _jd_InputMap(jd_Key_F8,             0x77),
    _jd_InputMap(jd_Key_F9,             0x78),
    _jd_InputMap(jd_Key_F10,            0x79),
    _jd_InputMap(jd_Key_F11,            0x7A),
    _jd_InputMap(jd_Key_F12,            0x7B),
    _jd_InputMap(jd_Key_F13,            0x7C),
    _jd_InputMap(jd_Key_F14,            0x7D),
    _jd_InputMap(jd_Key_F15,            0x7E),
    _jd_InputMap(jd_Key_F16,            0x7F),
    _jd_InputMap(jd_Key_F17,            0x80),
    _jd_InputMap(jd_Key_F18,            0x81),
    _jd_InputMap(jd_Key_F19,            0x82),
    _jd_InputMap(jd_Key_F20,            0x83),
    _jd_InputMap(jd_Key_F21,            0x84),
    _jd_InputMap(jd_Key_F22,            0x85),
    _jd_InputMap(jd_Key_F23,            0x86),
    _jd_InputMap(jd_Key_F24,            0x87),
    _jd_InputMap(jd_Key_Numlock,        0x90),
    _jd_InputMap(jd_Key_Scroll,         0x91),
    _jd_InputMap(jd_Key_NumpadEqual,    0x92),   // '=' key on numpad
    _jd_InputMap(jd_Key_LShift,         0xA0),
    _jd_InputMap(jd_Key_RShift,         0xA1),
    _jd_InputMap(jd_Key_LControl,       0xA2),
    _jd_InputMap(jd_Key_RControl,       0xA3),
    _jd_InputMap(jd_Key_LMenu,          0xA4),
    _jd_InputMap(jd_Key_RMenu,          0xA5),
    
    _jd_InputMap(jd_Key_Media_Next,         0xB0),
    _jd_InputMap(jd_Key_Media_Prev,         0xB1),
    _jd_InputMap(jd_Key_Media_Stop,         0xB2),
    _jd_InputMap(jd_Key_Media_PlayPause,    0xB3),
    
    _jd_InputMap(jd_Key_SemiColon_Colon,    0xBA),   // ';:' for US
    _jd_InputMap(jd_Key_Equals_Plus,        0xBB), // '+' any country
    _jd_InputMap(jd_Key_Comma_LessThan,     0xBC),   // ',' any country
    _jd_InputMap(jd_Key_Hyphen_UnderScore,  0xBD),   // '-' any country
    _jd_InputMap(jd_Key_Period_GreaterThan, 0xBE),   // '.' any country
    _jd_InputMap(jd_Key_FSlash_QMark,       0xBF),   // '/?' for US
    _jd_InputMap(jd_Key_Grave_Tilde,        0xC0),   // '`~' for US
    _jd_InputMap(jd_Key_LBrack_LBrace,      0xDB),  //  '[{' for US
    _jd_InputMap(jd_Key_BSlash_VBar,        0xDC),  //  '\|' for US
    _jd_InputMap(jd_Key_RBrack_RBrace,      0xDD),  //  ']}' for US
    _jd_InputMap(jd_Key_Apos_Comma,         0xDE),  //  ''"' for US
    jd_Key_Count
} jd_Key;

typedef enum jd_KeyMod {
    jd_KeyMod_None   = 0,
    jd_KeyMod_Ctrl   = (1 << 0),
    jd_KeyMod_Alt    = (1 << 1),
    jd_KeyMod_Shift  = (1 << 2),
    jd_KeyMod_LCtrl  = (1 << 3),
    jd_KeyMod_RCtrl  = (1 << 4),
    jd_KeyMod_LAlt   = (1 << 5),
    jd_KeyMod_RAlt   = (1 << 6),
    jd_KeyMod_LShift = (1 << 7),
    jd_KeyMod_RShift = (1 << 8)
} jd_KeyMod;

typedef struct jd_InputEvent {
    jd_Key    key;
    u8        scancode;
    u32       mods;
    jd_V2F    mouse_drag_start;
    jd_V2F    mouse_pos;
    jd_V2F    mouse_delta;
    jd_V2F    scroll_delta;
    jd_String drag_drop_data;
    u32       codepoint;
    b8        release_event; // 0 means it's a down event, and I feel fine assuming that's the usual case
} jd_InputEvent;

typedef struct jd_InputSlice {
    jd_DArray* array;
    u64 index;
    u64 range;
} jd_InputEventSlice;

#define jd_InputSliceSize(x) (x.range - x.index)
#define jd_InputSliceForEach(iden, x) for (u64 iden = x.index; iden < x.range; iden++)

#define jd_InputHasMod(e, flag) (e->mods & flag)

#ifdef JD_IMPLEMENTATION
#include "jd_input.c"
#endif

#endif //JD_INPUT_H
