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

typedef struct jd_UIState {
    jd_Window* window;
} jd_UIState;


typedef struct jd_UIBoxRec {
    jd_UITag   tag;
    jd_RectF32 rect;
    jd_V2F     pos; // platform window space
    jd_V2F     label_anchor;
    jd_String  label;
    
    struct jd_UIStyle* style;
    struct jd_UILayout* layout;
    struct jd_UISize* size;
    
    struct jd_UIViewport* vp;
    
    struct jd_UIBoxRec* next_with_same_hash;
    
    u64 frame_last_touched;
    
    jd_Node(jd_UIBoxRec);
} jd_UIBoxRec;

typedef struct jd_UIViewport {
    jd_V2F size;
    
    jd_UIBoxRec* root;
    jd_UIBoxRec* popup_root;
    jd_UIBoxRec* titlebar_root;
    
    jd_UIBoxRec* root_new;
    jd_UIBoxRec* popup_root_new;
    jd_UIBoxRec* titlebar_root_new;
    
    jd_UIBoxRec* hot;
    jd_UIBoxRec* active;
    jd_UIBoxRec* last_active;
    
    jd_UIBoxRec* builder_parent;
    
    jd_Window* window;
    
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

typedef struct jd_UIStyle {
    jd_V4F bg_color;
    jd_V4F bg_color_hovered;
    jd_V4F bg_color_active;
    jd_V4F label_color;
    f32    softness;
    f32    rounding;
    f32    thickness;
} jd_UIStyle;

typedef enum jd_UILayoutDir {
    jd_UILayout_None,
    jd_UILayout_LR,
    jd_UILayout_RL,
    jd_UILayout_TB,
    jd_UILayout_BT,
    jd_UILayout_Count
} jd_UILayoutDir;

typedef struct jd_UILayout {
    jd_UILayoutDir dir;
    jd_V2F         padding;
    f32            gap;
} jd_UILayout;

typedef enum jd_UISizeRule {
    jd_UISizeRule_Fixed,
    jd_UISizeRule_SizeByChildren,
    jd_UISizeRule_SizeByParent,
    jd_UISizeRule_SizeGrow,
    jd_UISizeRule_Count
} jd_UISizeRule;

typedef struct jd_UISize {
    jd_UISizeRule rule;
    jd_V2F fixed_size;
    f32    pct_of_parent;
} jd_UISize;

typedef struct jd_UIBoxConfig {
    jd_UIBoxRec*  parent;
    jd_UIStyle*   style;
    jd_UISize*    size;
    
    jd_String     label;
    jd_V2F        label_alignment;
    jd_String     string_id;
    
    jd_RectF32    rect;
    
    b8            clickable;
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

jd_ExportFn jd_UIResult    jd_UIBox(jd_UIBoxConfig* cfg);
jd_ExportFn jd_UIViewport* jd_UIBegin(jd_UIViewport* viewport);
jd_ExportFn void           jd_UIEnd();
jd_ExportFn jd_UIViewport* jd_UIInitForWindow(jd_Window* window);
jd_ExportFn jd_ForceInline void jd_UISeedPushPtr(void* ptr);
jd_ExportFn jd_ForceInline void jd_UISeedPop();
jd_ExportFn jd_ForceInline void jd_UIStylePush(jd_UIStyle* style);
jd_ExportFn jd_ForceInline void jd_UIStylePop();
jd_ExportFn jd_ForceInline void jd_UIFontPush(jd_String font_id);
jd_ExportFn jd_ForceInline void jd_UIFontPop();
jd_ExportFn jd_V2F jd_UIParentSize(jd_UIBoxRec* box);

static jd_UIStyle jd_default_style_dark = {
    .bg_color = {.08f, .07f, .07f, 1.0f},
    .bg_color_hovered = {.09f, .08f, .08f, 1.0f},
    .bg_color_active = {.06f, .06f, .06f, 1.0f},
    .label_color = {.9f, .9f, .9f, 1.0f},
    .softness = 0.0f,
    .rounding = 0.0f,
    .thickness = 0.0f
};

#ifdef JD_IMPLEMENTATION



#include "jd_ui.c"
#endif

#endif //JD_UI_H
