/* date = April 15th 2024 1:23 am */

#ifndef JD_UI_H
#define JD_UI_H

#ifndef JD_UNITY_H
#include "jd_app.h"
#include "jd_font.h"
#endif

typedef struct jd_UITag {
    u32  key;
} jd_UITag;

typedef enum jd_UICorner {
    jd_UICorner_None,
    jd_UICorner_TL,
    jd_UICorner_TR,
    jd_UICorner_BL,
    jd_UICorner_BR
} jd_UICorner;

typedef enum jd_UISizeKind {
    jd_UISizeKind_Grow,
    jd_UISizeKind_Fixed,
    jd_UISizeKind_Fit,
    jd_UISizeKind_PctParent,
    jd_UISizeKind_FitText,
    jd_UISizeKind_Count
} jd_UISizeKind;

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
    jd_UIBoxFlags_DragandDrop          = 1 << 9,
    jd_UIBoxFlags_Multiline            = 1 << 10,
    jd_UIBoxFlags_InvisibleBG          = 1 << 11,
    jd_UIBoxFlags_NoClip               = 1 << 12
} jd_UIBoxFlags;

typedef struct jd_UISizeRule {
    jd_UISizeKind kind;
    f32 minimum_size;
    f32 fixed_size;
    f32 pct_parent;
} jd_UISizeRule;

typedef struct jd_UIShape {
    jd_UISizeRule rules[jd_UIAxis_Count];
    jd_V2F padding;
    f32 softness;
    f32 corner_radius;
    f32 thickness;
    f32 stroke_width;
    b32 defined;
} jd_UIShape;

typedef struct jd_UIColors {
    jd_V4F bg_color;
    jd_V4F bg_color_hovered;
    jd_V4F bg_color_active;
    jd_V4F label;
    jd_V4F stroke;
    jd_V4F draggables;
    b32 defined;
} jd_UIColors;

typedef struct jd_UILayout {
    jd_UILayoutDir dir;
    f32 gap;
    jd_UIAxis axis;
    b32 defined;
} jd_UILayout;


#define jd_UIFixed(x)     (jd_UISizeRule){jd_UISizeKind_Fixed, 0, x, 0}
#define jd_UIGrow         (jd_UISizeRule){jd_UISizeKind_Grow, 0, 0, 0}
#define jd_UIGrowMin(x)   (jd_UISizeRule){jd_UISizeKind_Grow, x, 0, 0}
#define jd_UIFit          (jd_UISizeRule){jd_UISizeKind_Fit, 0, 0, 0}
#define jd_UIFitText      (jd_UISizeRule){jd_UISizeKind_FitText}
#define jd_UIPctParent(x) (jd_UISizeRule){jd_UISizeKind_PctParent, 0, 0, x}

jd_ExportFn jd_UIShape  jd_UIMakeShape(jd_UISizeRule rule_x, jd_UISizeRule rule_y);
jd_ExportFn jd_UIShape  jd_UIMakeShapeEx(jd_UISizeRule rule_x, jd_UISizeRule rule_y, f32 padding_x, f32 padding_y, f32 softness, f32 corner_radius, f32 thickness, f32 stroke_width);
jd_ExportFn jd_UIColors jd_UIMakeColors(jd_V4F bg, jd_V4F bg_hovered, jd_V4F bg_active, jd_V4F label, jd_V4F stroke, jd_V4F draggables);
jd_ExportFn jd_UILayout jd_UIMakeLayout(jd_UILayoutDir dir, f32 gap);

jd_ExportFn jd_V4F jd_UIColorHexToV4F(u32 rgba);
//
//#define jd_UIMakeShape(rule_x, rule_y) {{rule_x, rule_y}}
//#define jd_UIMakeShapeEx(rule_x, rule_y, padding_x, padding_y, softness, corner_radius, thickness, stroke_width) {{rule_x, rule_y}, {padding_x, padding_y}, softness, corner_radius, thickness, stroke_width}
//#define jd_UIMakeColors(bg, bg_hovered, bg_active, label, stroke) {bg, bg_hovered, bg_active, label, stroke}
//#define jd_UIMakeLayout(dir, gap) {dir, gap}

typedef struct jd_UITextBox {
    jd_String hint;
    u64 cursor;
    u64 selection;
    jd_V2F last_cursor_pos;
    
    jd_String* string;
    u64 max;
} jd_UITextBox;

typedef struct jd_UIBoxRec {
    jd_UITag   tag;
    jd_String  string_id;
    jd_V4F     rect;
    jd_V4F     rect_clipped;
    jd_V2F     pos; // platform window space
    jd_V2F     label_alignment;
    jd_String  label;
    jd_V4F     color;
    jd_Cursor  cursor;
    jd_V2F     scroll;
    jd_V2F     scroll_max;
    jd_V2F     slider_range;
    jd_UIAxis  slider_axis;
    f64        slider_pos;
    jd_V2F     drag_delta;
    
    u16 font_size;
    jd_Font2* font;
    
    jd_UIColors colors;
    jd_UILayout layout;
    jd_UIShape  shape;
    
    u64 last_touched;
    b8  being_dragged;
    
    jd_UIBoxFlags flags;
    
    u32 draw_index;
    
    jd_UITextBox text_box;
    
    jd_V2F    fixed_position;
    jd_V2F    reference_point;
    jd_V2F    requested_size;
    
    struct jd_UIBoxRec* anchor_box;
    jd_V2F anchor_reference_point;
    
    struct jd_UIViewport* vp;
    struct jd_UIBoxRec* next_with_same_hash;
    
    jd_Node(jd_UIBoxRec);
} jd_UIBoxRec;

typedef struct jd_UIViewport {
    jd_V2I size;
    jd_V2F minimum_size;
    
    jd_UIBoxRec* root;
    jd_UIBoxRec* popup_root;
    
    jd_UIBoxRec* root_new;
    jd_UIBoxRec* popup_root_new;
    
    jd_UIBoxRec* hot;
    jd_UIBoxRec* active;
    jd_UIBoxRec* last_active;
    jd_UIBoxRec* drag_box;
    jd_UIBoxRec* previous_last_active;
    
    jd_UIBoxRec* builder_parent;
    jd_UIBoxRec* builder_last_box;
    
    jd_Window* window;
    
    b32 roots_init;
    
    u32 box_count;
    u64 frame_counter;
    
    b8  debug_mode;
    jd_UIBoxRec* debug_box;
    
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
    
    jd_V2F drag_delta;
    f64 slider_position;
    
    b8 drag_drop_recieved;
    b8 drag_drop_sent;
    
    b8 hovered;
} jd_UIResult;

typedef struct jd_UISizedFont {
    jd_Font2* font;
    u16       point_size;
} jd_UISizedFont;

typedef struct jd_UIBoxConfig {
    jd_UIBoxRec*  parent;
    
    jd_V2F        fixed_position;
    jd_V2F        reference_point; // 0, 0 = Top Left TODO: Rethink the names of (probably) most things in this struct, especially this.
    jd_UIAxis     slider_axis;
    
    jd_UIBoxRec*  anchor_box;
    jd_V2F        anchor_reference_point;
    
    jd_String     label;
    jd_V2F        label_alignment;
    jd_String     string_id;
    
    jd_UIBoxFlags flags;
    
    jd_Cursor     cursor;
    jd_UITextBox  text_box;
    
    jd_UIShape  shape;
    jd_UIColors colors;
    jd_UILayout layout;
    
    b32 draggable;
    
    // TODO: Texture information? I feel like there's no reason to not have it this low level with this design,
    // and it makes textured rectangles a first class citizen. Thereotically, with the right application of these
    // flags, we can have skinning by default.
} jd_UIBoxConfig;

typedef jd_UIBoxConfig jd_UIButtonConfig;

jd_ExportFn jd_UIViewport* jd_UIInitForWindow(jd_Window* window);

jd_ExportFn jd_UIViewport* jd_UIBegin(jd_UIViewport* viewport);
jd_ExportFn void           jd_UIEnd();

jd_ExportFn jd_UIResult    jd_UIBoxBegin(jd_UIBoxConfig* cfg);
jd_ExportFn void           jd_UIBoxEnd();
jd_ExportFn jd_UIResult    jd_UIRegionBeginAnchored(jd_String string_id, jd_UIShape shape, jd_UILayout layout, jd_UIBoxRec* anchor_box, jd_V2F anchor_to, jd_V2F anchor_from, jd_UIBoxFlags flags);
jd_ExportFn jd_UIResult    jd_UIRegionBegin(jd_String string_id, jd_UIShape shape, jd_UILayout layout, jd_UIBoxFlags flags);
jd_ExportFn void           jd_UIRegionEnd();

jd_ExportFn jd_ForceInline void jd_UISeedPushU32(u32 v);
jd_ExportFn jd_ForceInline void jd_UISeedPushU64(u64 v);
jd_ExportFn jd_ForceInline void jd_UISeedPushString(jd_String string);
jd_ExportFn jd_ForceInline void jd_UISeedPushStringF(jd_String fmt, ...);
jd_ExportFn jd_ForceInline void jd_UISeedPop();

jd_ExportFn jd_ForceInline void jd_UIFontPush(jd_Font2* font, u16 size);
jd_ExportFn jd_ForceInline void jd_UIFontPop();

jd_ExportFn jd_ForceInline void jd_UILayoutPush(jd_UILayout layout);
jd_ExportFn jd_ForceInline void jd_UILayoutPop();

jd_ExportFn jd_ForceInline void jd_UIColorsPush(jd_UIColors colors);
jd_ExportFn jd_ForceInline void jd_UIColorsPop();

jd_ExportFn jd_ForceInline void jd_UIShapePush(jd_UIShape colors);
jd_ExportFn jd_ForceInline void jd_UIShapePop();

jd_ExportFn jd_ForceInline jd_UIViewport* jd_UIViewportGetCurrent();
jd_ExportFn jd_ForceInline jd_V2F         jd_UIParentSize(jd_UIBoxRec* box);
jd_ExportFn jd_ForceInline jd_UIBoxRec*   jd_UIGetLastBox();
jd_ExportFn jd_ForceInline void           jd_UIResetScroll(jd_UIBoxRec* b);
jd_ExportFn jd_ForceInline jd_UIBoxRec*   jd_UIGetDebugBox();

jd_ExportFn jd_UIResult jd_UIButtonEx(jd_String label, jd_UIShape shape, jd_UIColors colors, jd_V2F label_alignment, jd_UIBoxFlags flags);
jd_ExportFn jd_UIResult jd_UISliderF32(jd_String string_id, jd_UIShape shape, jd_UIShape shape_button, jd_UIAxis axis, f32* v, f32 min, f32 max);
jd_ExportFn jd_UIResult jd_UISliderU64(jd_String string_id, jd_UIShape shape, jd_UIShape shape_button, jd_UIAxis axis, u64* v, u64 min, u64 max);
jd_ExportFn jd_UIResult jd_UILabel(jd_String label);
jd_ExportFn jd_UIResult jd_UILabelEx(jd_String label, jd_UIShape shape, jd_UIColors colors, jd_V2F alignment, jd_UIBoxFlags flags);
jd_ExportFn jd_UIResult jd_UILabelButton(jd_String label, jd_V2F padding, jd_UIBoxFlags flags);
jd_ExportFn jd_UIResult jd_UIListButton(jd_String label, jd_V2F alignment, jd_UIBoxFlags flags);
jd_ExportFn jd_UIResult jd_UIFixedSizeButton(jd_String label, jd_V2F size, jd_V2F label_alignment);
jd_ExportFn jd_UIResult jd_UIRowGrowBegin(jd_String string_id, jd_V2F padding, f32 gap, jd_UIBoxFlags flags);
jd_ExportFn jd_UIResult jd_UIColGrowBegin(jd_String string_id, jd_V2F padding, f32 gap, jd_UIBoxFlags flags);
jd_ExportFn jd_UIResult jd_UIInputTextBox(jd_String string_id, jd_UIShape shape, jd_String* string, u64 max_string_size );
jd_ExportFn jd_UIResult jd_UIInputTextBoxAligned(jd_String string_id, jd_UIShape shape, jd_String* string, u64 max_string_size, jd_V2F alignment);
jd_ExportFn jd_UIResult jd_UIInputTextBoxAlignedWithHint(jd_String string_id, jd_UIShape shape, jd_String* string, jd_String hint, u64 max_string_size, jd_V2F alignment);
jd_ExportFn jd_UIResult jd_UIInputTextBoxWithHint(jd_String string_id, jd_UIShape shape, jd_String* string, jd_String hint, u64 max_string_size);
jd_ExportFn jd_UIResult jd_UIInputTextBoxMultiline(jd_String string_id, jd_UIShape shape, jd_String* string, jd_String hint, u64 max_string_size);
jd_ExportFn jd_UIResult jd_UIWindowRegionBegin(jd_V2F min_size, jd_UILayoutDir dir, f32 gap);
jd_ExportFn jd_UIResult jd_UIGrowPadding(jd_String string_id);
jd_ExportFn jd_UIResult jd_UIFixedPadding(jd_String string_id, jd_V2F fixed_size);
jd_ExportFn jd_UIResult jd_UIFixedSizeLabel(jd_String label, jd_V2F alignment, jd_V2F size);


#ifdef JD_IMPLEMENTATION

#include "jd_ui.c"
#endif

#endif //JD_UI_H

