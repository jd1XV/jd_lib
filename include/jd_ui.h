/* date = April 15th 2024 1:23 am */

#ifndef JD_UI_H
#define JD_UI_H

#ifndef JD_UNITY_H
#include "jd_app.h"
#endif

typedef struct jd_UITag {
    u32  key;
} jd_UITag;

typedef struct jd_UIStyle {
    jd_V4F bg_color;
    jd_V4F bg_color_hovered;
    jd_V4F bg_color_active;
    
    jd_V4F label_color;
    jd_V2F label_padding;
    
    jd_V2F padding;
    
    f32    softness;
    f32    rounding;
    f32    thickness;
} jd_UIStyle;

typedef enum jd_UILayoutDir {
    jd_UILayout_LeftToRight,
    jd_UILayout_RightToLeft,
    jd_UILayout_TopToBottom,
    jd_UILayout_BottomToTop,
    jd_UILayout_Count
} jd_UILayoutDir;

typedef enum jd_UIAxis {
    jd_UIAxis_X,
    jd_UIAxis_Y,
    jd_UIAxis_Count
} jd_UIAxis;

typedef enum jd_UISizeRule {
    jd_UISizeRule_Grow,
    jd_UISizeRule_Fixed,
    jd_UISizeRule_FitChildren,
    jd_UISizeRule_PctParent,
    jd_UISizeRule_FitText,
    jd_UISizeRule_Count
} jd_UISizeRule;

typedef struct jd_UISize {
    jd_UISizeRule rule[2];
    jd_V2F fixed_size;
    jd_V2F min_size;
    jd_V2F pct_of_parent;
} jd_UISize;

typedef struct jd_UITextBox {
    u64 cursor;
    u64 selection;
    jd_V2F last_cursor_pos;
    
    jd_String* string;
    u64 max;
} jd_UITextBox;

typedef enum jd_UIBoxFlags {
    jd_UIBoxFlags_None                 = 0,
    jd_UIBoxFlags_Clickable            = 1 << 0,
    jd_UIBoxFlags_ActOnClick           = 1 << 1,
    jd_UIBoxFlags_MatchSiblingsOffAxis = 1 << 2,
    jd_UIBoxFlags_StaticColor          = 1 << 3,
    jd_UIBoxFlags_Disabled             = 1 << 4,
    jd_UIBoxFlags_SelectableLabel      = 1 << 5,
    jd_UIBoxFlags_TextEdit             = 1 << 6,
    jd_UIBoxFlags_ScrollX              = 1 << 7,
    jd_UIBoxFlags_ScrollY              = 1 << 8,
    jd_UIBoxFlags_DragandDrop          = 1 << 9
} jd_UIBoxFlags;


typedef struct jd_UIBoxRec {
    jd_UITag   tag;
    jd_String  string_id;
    jd_RectF32 rect;
    jd_V2F     pos; // platform window space
    jd_V2F     label_alignment;
    jd_String  label;
    jd_String  font_id;
    jd_V4F     color;
    jd_Cursor  cursor;
    jd_V2F     scroll;
    jd_V2F     scroll_max;
    
    jd_UIStyle style;
    
    u64 last_touched;
    b8  being_dragged;
    
    jd_UIBoxFlags flags;
    
    u32 draw_index;
    
    jd_UITextBox text_box;
    
    jd_UISize size;
    jd_UIAxis layout_axis;
    f32       layout_gap;
    jd_V2F    fixed_position;
    jd_V2F    reference_point;
    
    struct jd_UIBoxRec* anchor_box;
    jd_V2F anchor_reference_point;
    
    struct jd_UIViewport* vp;
    struct jd_UIBoxRec* next_with_same_hash;
    
    u64 frame_last_touched;
    
    jd_Node(jd_UIBoxRec);
} jd_UIBoxRec;

typedef struct jd_UIViewport {
    jd_V2F size;
    jd_V2F minimum_size;
    
    jd_UIBoxRec* root;
    jd_UIBoxRec* popup_root;
    jd_UIBoxRec* titlebar_root;
    
    jd_UIBoxRec* root_new;
    jd_UIBoxRec* popup_root_new;
    jd_UIBoxRec* titlebar_root_new;
    
    jd_UIBoxRec* hot;
    jd_UIBoxRec* active;
    jd_UIBoxRec* last_active;
    jd_UIBoxRec* drag_box;
    
    jd_UIBoxRec* builder_parent;
    jd_UIBoxRec* builder_last_box;
    
    jd_Window* window;
    
    b32 roots_init;
    
    u32 box_count;
    
    u64 frame_counter;
    
    jd_InputEventSlice new_inputs;
    jd_InputEventSlice old_inputs;
} jd_UIViewport;

typedef struct jd_UIResult {
    jd_UIBoxRec* box;
    
    b8 l_clicked;
    b8 r_clicked;
    b8 m_clicked;
    
    b8 last_active;
    
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
    jd_UISize     size;
    jd_V2F        fixed_position;
    jd_V2F        reference_point; // 0, 0 = Top Left TODO: Rethink the names of (probably) most things in this struct, especially this.
    f32           layout_gap;
    f32           layout_dir;
    
    jd_UIBoxRec*  anchor_box;
    jd_V2F        anchor_reference_point;
    
    jd_String     label;
    jd_V2F        label_alignment;
    jd_String     string_id;
    
    jd_RectF32    rect;
    jd_V4F        color;
    
    jd_UIBoxFlags flags;
    
    jd_Cursor     cursor;
    jd_UITextBox  text_box;
    
    // TODO: Texture information? I feel like there's no reason to not have it this low level with this design,
    // and it makes textured rectangles a first class citizen. Thereotically, with the right application of these
    // flags, we can have skinning by default.
} jd_UIBoxConfig;

typedef jd_UIBoxConfig jd_UIButtonConfig;

jd_ExportFn jd_UIResult    jd_UIBox(jd_UIBoxConfig* cfg);
jd_ExportFn jd_UIResult    jd_UIBoxBegin(jd_UIBoxConfig* cfg);
jd_ExportFn void           jd_UIBoxEnd();
jd_ExportFn jd_UIViewport* jd_UIBegin(jd_UIViewport* viewport);
jd_ExportFn void           jd_UIEnd();
jd_ExportFn jd_UIViewport* jd_UIInitForWindow(jd_Window* window);
jd_ExportFn jd_ForceInline void jd_UISeedPushU32(u32 v);
jd_ExportFn jd_ForceInline void jd_UISeedPushString(jd_String string);
jd_ExportFn jd_ForceInline void jd_UISeedPushStringF(jd_String fmt, ...);
jd_ExportFn jd_ForceInline void jd_UISeedPop();
jd_ExportFn jd_ForceInline void jd_UIStylePush(jd_UIStyle* style);
jd_ExportFn jd_ForceInline void jd_UIStylePop();
jd_ExportFn jd_ForceInline void jd_UIStyleGet();
jd_ExportFn jd_ForceInline void jd_UIFontPush(jd_String font_id);
jd_ExportFn jd_ForceInline void jd_UIFontPop();
jd_ExportFn jd_ForceInline jd_UIViewport* jd_UIViewportGetCurrent();
jd_ExportFn jd_V2F jd_UIParentSize(jd_UIBoxRec* box);
jd_ExportFn jd_UIResult jd_UIButton(jd_String label, jd_UISize size, jd_UIBoxFlags flags);
jd_ExportFn jd_UIResult jd_UILabel(jd_String label);
jd_ExportFn jd_UIResult jd_UILabelButton(jd_String label);
jd_ExportFn jd_UIResult jd_UIListButton(jd_String label);
jd_ExportFn jd_UIResult jd_UIFixedSizeButton(jd_String label, jd_V2F size, jd_V2F label_alignment);
jd_ExportFn jd_UIResult jd_UIRegionBegin(jd_String string_id, jd_UIStyle* style, jd_UISize size, jd_UILayoutDir dir, f32 gap, jd_UIBoxFlags flags);
jd_ExportFn jd_UIResult jd_UIRegionBeginAnchored(jd_String string_id, jd_UIStyle* style, jd_UIBoxRec* anchor_box, jd_V2F anchor_to, jd_V2F anchor_from, jd_UISize size, jd_UILayoutDir dir, f32 gap, b8 clickable);
jd_ExportFn jd_UIResult jd_UIRowGrowBegin(jd_String string_id, jd_UIStyle* style, f32 gap, jd_UIBoxFlags flags);
jd_ExportFn jd_UIResult jd_UIColGrowBegin(jd_String string_id, jd_UIStyle* style, f32 gap, jd_UIBoxFlags flags);
jd_ExportFn jd_UIResult jd_UIInputTextBox(jd_String string_id, jd_String* string, u64 max_string_size, jd_UIStyle* style, jd_UISize size);
jd_ExportFn jd_UIResult jd_UIWindowRegionBegin(jd_V2F min_size, jd_UIStyle* style, jd_UILayoutDir dir, f32 gap);
jd_ExportFn jd_UIResult jd_UIGrowPadding(jd_String string_id);
jd_ExportFn jd_UIBoxRec* jd_UIGetLastBox();
jd_ExportFn void        jd_UIRegionEnd();

static jd_UIStyle jd_default_style_dark = {
    .bg_color = {.1f, .1f, .1f, 1.0f},
    .bg_color_hovered = {.2f, .2f, .2f, 1.0f},
    .bg_color_active = {.00f, .00f, .00f, 1.0f},
    .label_color = {.9f, .9f, .9f, 1.0f},
    .label_padding = {10.0f, 10.0f},
    .softness = 0.0f,
    .rounding = 0.0f,
    .thickness = 0.0f
};

static jd_UIStyle jd_invisible_style = {
    0
};

#ifdef JD_IMPLEMENTATION

#include "jd_ui.c"
#endif

#endif //JD_UI_H
