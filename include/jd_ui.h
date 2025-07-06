/* date = April 15th 2024 1:23 am */

#ifndef JD_UI_H
#define JD_UI_H

#ifndef JD_UNITY_H
#include "jd_app.h"
#endif

typedef struct jd_UITag {
    u32 key;
    u32 seed;
} jd_UITag;

typedef struct jd_UIStyle {
    jd_String id;
    jd_V2F    label_anchor;
    jd_V4F    label_color;
    jd_V4F    color_bg;
    jd_V4F    color_border;
    jd_V4F    color_label;
    jd_V4F    color_button;
    jd_V4F    color_menu;
    jd_V4F    color_hover_mod;
    jd_V4F    color_active_mod;
    jd_V2F    padding;
    f32       corner_radius;
    f32       box_softness;
    f32       border;
    f32       box_thickness;
} jd_UIStyle;

typedef struct jd_UIBoxRec {
    jd_UITag   tag;
    jd_RectF32 rect;
    jd_V2F     pos; // platform window space
    jd_V2F     label_anchor;
    jd_String  label;
    
    struct jd_UIViewport* vp;
    
    struct jd_UIBoxRec* next_with_same_hash;
    
    u64 frame_last_touched;
    
    jd_Node(jd_UIBoxRec);
} jd_UIBoxRec;

typedef struct jd_UIViewport {
    jd_V2F size;
    jd_UIBoxRec* root;
    jd_UIBoxRec* popup_root;
    jd_UIBoxRec* menu_root;
    jd_UIBoxRec* titlebar_root;
    
    jd_UIBoxRec* root_new;
    jd_UIBoxRec* popup_root_new;
    jd_UIBoxRec* menu_root_new;
    jd_UIBoxRec* titlebar_root_new;
    
    jd_UIBoxRec* hot;
    jd_UIBoxRec* active;
    jd_UIBoxRec* last_active;
    
    jd_V2F client_size;
    
    jd_PlatformWindow* window;
    
    b32 roots_init;
    
    jd_InputEventSlice new_inputs;
    jd_InputEventSlice old_inputs;
} jd_UIViewport;

typedef struct jd_UIResult {
    jd_UIBoxRec* box;
    
    b8 l_clicked;
    b8 r_clicked;
    b8 m_clicked;
    
    b8 control_clicked;
    b8 alt_clicked;
    b8 shift_clicked;
    
    b8 text_input;
    
    b8 drag_drop_recieved;
    b8 drag_drop_sent;
    
    b8 hovered;
} jd_UIResult;

typedef struct jd_UIBoxConfig {
    jd_UIBoxRec*  parent;
    jd_UIStyle*   style;
    jd_String     label;
    jd_V2F        label_alignment;
    jd_String     string_id;
    jd_RectF32    rect;
    
    b8            clickable;
    b8            use_padding;
    b8            act_on_click;
    b8            static_color;
    b8            disabled;
    b8            label_selectable;
    jd_Cursor     cursor;
    
    // TODO: Texture information? I feel like there's no reason to not have it this low level with this design,
    // and it makes textured rectangles a first class citizen. Thereotically, with the right application of these
    // flags, we can have skinning by default.
} jd_UIBoxConfig;

typedef jd_UIBoxConfig jd_UIButtonConfig;

jd_ExportFn jd_UIResult jd_UIBox(jd_UIBoxConfig* cfg);
jd_ExportFn jd_UIViewport* jd_UIBeginViewport(jd_PlatformWindow* window);
jd_ExportFn jd_ForceInline void jd_UISeedPushPtr(void* ptr);
jd_ExportFn jd_ForceInline void jd_UISeedPop();
jd_ExportFn jd_ForceInline void jd_UIStylePush(jd_UIStyle* style);
jd_ExportFn jd_ForceInline void jd_UIStylePop();
jd_ExportFn jd_ForceInline void jd_UIFontPush(jd_String font_id);
jd_ExportFn jd_ForceInline void jd_UIPopFont();
jd_ExportFn jd_V2F jd_UIParentSize(jd_UIBoxRec* box);

static jd_UIStyle jd_default_style_dark = {
    .id = jd_StrConst("jd_default_style_dark"),
    .label_anchor = {0},
    .label_color = {.98f, .98f, .98f, 1.0f},
    .color_bg = {.08f, .07f, .07f, 1.0f},
    .color_border = {.15f, .15f, .15f, 1.0f},
    .color_label = {.95f, .95f, .92f, 1.0f},
    .color_button = {.10f, .10f, .08f, 1.0f},
    .color_menu = {.09f, .09f, .06f, 1.0f},
    .color_hover_mod = {1.30, 1.30f, 1.30f, 1.30f},
    .color_active_mod = {.80f, .80f, .80f, 1.0f},
    .padding = {8.0f, 8.f},
    .corner_radius = 4.0f,
    .box_softness = 0.0f,
    .border = 3.0f,
    .box_thickness = 0.0f
};

#ifdef JD_IMPLEMENTATION
#include "jd_ui.c"
#endif

#endif //JD_UI_H
