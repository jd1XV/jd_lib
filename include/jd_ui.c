typedef struct jd_Internal_UIState {
    jd_Arena* arena;
    
    jd_UIBoxRec* box_array;
    u64          box_array_size;
    jd_UIBoxRec* box_free_slot;
    
    jd_UIViewport viewports[256];
    u64 viewport_count;
    u64 active_viewport;
    
    jd_DArray* seeds;       // u32
    jd_DArray* parent_stack;
    jd_DArray* font_id_stack; // jd_String
} jd_Internal_UIState;

static jd_Internal_UIState _jd_internal_ui_state = {0};

#define jd_UIViewports_Max      256
#define jd_UIBox_HashTable_Size KILOBYTES(1)

jd_ExportFn jd_V2F jd_UIParentSize(jd_UIBoxRec* box) {
    if (box->parent) {
        return box->parent->rect.max;
    } else return box->vp->size;
}

jd_ForceInline jd_UIViewport* jd_UIViewportGetCurrent() {
    return &_jd_internal_ui_state.viewports[_jd_internal_ui_state.active_viewport];
}

jd_ForceInline jd_String _jd_UIStringGetDisplayPart(jd_String str) {
    return jd_StringGetPrefix(str, jd_StrLit("##"));
}

jd_ForceInline jd_String _jd_UIStringGetHashPart(jd_String str) {
    return jd_StringGetPostfix(str, jd_StrLit("###"));
}

jd_ExportFn jd_UITag jd_UITagFromString(jd_String str) {
    if (!str.count) jd_LogError("UI Error: Cannot create a tag from an empty string", jd_Error_APIMisuse, jd_Error_Fatal);
    u32* seed = jd_DArrayGetBack(_jd_internal_ui_state.seeds);
    if (!seed) jd_LogError("UI Error: Seed stack is empty!", jd_Error_MissingResource, jd_Error_Fatal);
    u32 hash = jd_HashStrToU32(str, *seed);
    
    jd_UITag t = {
        .key = hash,
        .seed = *seed
    };
    
    return t;
}

jd_ExportFn jd_ForceInline void jd_UISeedPushPtr(void* ptr) {
    u64 ptru = (u64)ptr;
    u32 hash = jd_HashU64toU32(ptru, 7);
    jd_DArrayPushBack(_jd_internal_ui_state.seeds, &hash);
}

jd_ExportFn jd_ForceInline void jd_UISeedPop() {
#ifdef JD_DEBUG
    if (!_jd_internal_ui_state.seeds->count)
        jd_LogError("Seed stack is empty! Mismatched Push/Pop pair", jd_Error_APIMisuse, jd_Error_Fatal);
#endif
    jd_DArrayPopBack(_jd_internal_ui_state.seeds);
}

jd_ExportFn jd_UIBoxRec* jd_UIBoxGetByTag(jd_UITag tag) {
    jd_UIBoxRec* b = &(_jd_internal_ui_state.box_array[tag.key % _jd_internal_ui_state.box_array_size]);
    if (b->tag.key == 0 && b->tag.seed == 0) {
        b->tag.key = tag.key;
        b->tag.seed = tag.seed;
        return b;
    }
    
    while (b->next_with_same_hash != 0) {
        if (b->tag.key == tag.key && b->tag.seed == tag.seed)
            break;
        b = b->next_with_same_hash;
    }
    
    if (b->tag.key != tag.key || b->tag.seed != tag.seed) {
        if (_jd_internal_ui_state.box_free_slot) {
            b = _jd_internal_ui_state.box_free_slot;
            _jd_internal_ui_state.box_free_slot = b->next;
            jd_DLinksClear(b);
        } else {
            b->next_with_same_hash = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec));
            b = b->next_with_same_hash;
        }
        
        b->tag.key = tag.key;
        b->tag.seed = tag.seed;
    }
    
    return b;
}

jd_ForceInline b32 jd_UIRectContainsPoint(jd_UIViewport* vp, jd_RectF32 r, jd_V2F p) {
    jd_V2F min = {r.min.x, r.min.y};
    jd_V2F max = {(r.min.x) + (r.max.x), (r.min.y) + (r.max.y)};
    return ((p.x > min.x && p.x < max.x) && (p.y > min.y && p.y < max.y));
}

jd_UIBoxRec* jd_UIPickBoxForPos(jd_UIViewport* vp, jd_V2F pos) {
    jd_UIBoxRec* ret = 0;
    jd_UIBoxRec* b = vp->root;
    {
        while (b != 0) {
            if (jd_UIRectContainsPoint(vp, b->rect, pos)) {
                ret = b;
            } 
            jd_TreeTraversePreorder(b);
        }
        
    }
    
    b = vp->popup_root;
    {
        while (b != 0) {
            if (jd_UIRectContainsPoint(vp, b->rect, pos)) {
                ret = b;
            } 
            jd_TreeTraversePreorder(b);
        }
        
    }
    
    b = vp->titlebar_root;
    {
        while (b != 0) {
            if (jd_UIRectContainsPoint(vp, b->rect, pos)) {
                ret = b;
            } 
            jd_TreeTraversePreorder(b);
        }
        
    }
    
    
    return ret;
}

void jd_UIPickActiveBox(jd_UIViewport* vp) {
    jd_InputSliceForEach(i, vp->old_inputs) {
        jd_InputEvent* e = jd_DArrayGetIndex(vp->old_inputs.array, i);
        if (!e) { // Sanity Check
            jd_LogError("Incorrect input slice range!", jd_Error_APIMisuse, jd_Error_Fatal);
        }
        
        switch (e->key) {
            case jd_MB_Left:
            case jd_MB_Right:
            case jd_MB_Middle: {
                if (e->release_event) {
                    vp->last_active = vp->active;
                    vp->active = 0;
                } else {
                    vp->active = jd_UIPickBoxForPos(vp, e->mouse_pos);
                }
            }
        }
    }
}

void jd_UIFontPush(jd_String font_id) {
    jd_DArrayPushBack(_jd_internal_ui_state.font_id_stack, &font_id);
}

void jd_UIFontPop() {
    if (_jd_internal_ui_state.font_id_stack->count == 0) {
        jd_LogError("Mismatched calls to jd_UIFontPop()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    jd_DArrayPopBack(_jd_internal_ui_state.font_id_stack);
}

/*
ideal usage code:

typedef struct jd_UILayout {
	jd_UILayoutDir dir;
	jd_V2F padding;
	f32 gap;
} jd_UILayout;

typedef struct jd_UISize {
	jd_UISizeRule rule;
	jd_V2F fixed_size;
} jd_UISize;

typedef struct jd_UIStyle {
	jd_V4F bg_color;
	f32 softness;
	f32 rounding;
	f32 thickness;
jd_V4F label_color;
} jd_UIStyle;


static b32 file_menu_open = false;

jd_UIBegin();

jd_UISize size = {
	.rule = jd_SizeRule_SizeByChildren
};

jd_UILayout layout = {
.dir = jd_UILayourDir_LR,
.padding = 3.0f,
.gap = 2.0f
};

jd_UIStyle style = {
.bg_color = {1.0, 1.0, 1.0, 1.0}
}

jd_UIStyle button_style = {
.rounding = 1.0f,
.softness = 1.0f,
.bg_color = {1.0, 1.0, 1.0, 1.0}
};

jd_UIBeginContainer(jd_StrLit("##MenuBar"), &layout, &size, &style);

if (jd_UILabelButton(jd_StrLit("File"), &button_style).l_clicked) {
	  file_menu_open = true;
}

...

jd_UIEndContainer();

if (file_menu_open) {
 jd_V2F origin = jd_UIBoxGetBottomLeft("##MenuBar");

jd_UISize = {
.rule = jd_SizeRule_SizeByChildren
}

jd_UILayout layout = {
.dir = jd_UILayoutDir_TB,
.padding = 3.0f,
.gap = 2.0f
};

jd_UIBeginPopupContainer(jd_StrLit("##FileMenu"), &layout, &size, &style)

if (jd_UILabelButton("Open", &button_style).l_clicked) {
 ...
}

if (jd_UILabelButton("Save", &button_style).l_clicked) {
...
}

jd_UIEndPopupContainer();

jd_UIEnd();


*/


jd_UIResult jd_UIBox(jd_UIBoxConfig* config) {
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    
    jd_UIBoxRec* parent = config->parent;
    if (!parent) {
        parent = vp->root_new;
    }
    
    jd_UIStyle* style = config->style;
    
    if (!style) {
        jd_LogError("No style specified!", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    b8 act_on_click = config->act_on_click;
    b8 clickable    = config->clickable;
    
    jd_UITag tag = jd_UITagFromString(config->string_id);
    jd_UIBoxRec* b = jd_UIBoxGetByTag(tag);
    
    jd_UIResult result = {0};
    result.box = b;
    
    jd_V4F color         = style->bg_color;
    jd_V4F hovered_color = style->bg_color_hovered;
    jd_V4F active_color  = style->bg_color_active;
    
    f32 softness        = style->softness;
    f32 corner_radius   = style->rounding;
    f32 thickness       = style->thickness;
    
    // input handling
    if (!config->disabled) {
        if (b == vp->hot) {
            result.hovered = true;
            if (!config->static_color)
                color = hovered_color;
            
            jd_AppSetCursor(config->cursor);
        }
        
        if (b == vp->active) {
            jd_InputSliceForEach(i, vp->old_inputs) {
                jd_InputEvent* e = jd_DArrayGetIndex(vp->old_inputs.array, i);
                if (!e) { // Sanity Check
                    jd_LogError("Incorrect input slice range!", jd_Error_APIMisuse, jd_Error_Fatal);
                }
                
                if (act_on_click && clickable) {
                    switch (e->key) {
                        case jd_MB_Left:
                        case jd_MB_Right:
                        case jd_MB_Middle: {
                            result.control_clicked = jd_InputHasMod(e, jd_KeyMod_Ctrl);
                            result.alt_clicked     = jd_InputHasMod(e, jd_KeyMod_Alt);
                            result.shift_clicked   = jd_InputHasMod(e, jd_KeyMod_Shift);
                            result.l_clicked       = (e->key == jd_MB_Left);
                            result.m_clicked       = (e->key == jd_MB_Middle);
                            result.r_clicked       = (e->key == jd_MB_Right);
                            break;
                        }
                    }
                }
            }
            
            if (!config->static_color)
                color = active_color;
            
            //jd_AppSetCursor(config->cursor);
        }
        
        if (b == vp->last_active) {
            jd_InputSliceForEach(i, vp->old_inputs) {
                jd_InputEvent* e = jd_DArrayGetIndex(vp->old_inputs.array, i);
                if (!e) { // Sanity Check
                    jd_LogError("Incorrect input slice range!", jd_Error_APIMisuse, jd_Error_Fatal);
                }
                if (clickable) {
                    switch (e->key) {
                        
                        case jd_MB_Left:
                        case jd_MB_Right:
                        case jd_MB_Middle: {
                            if (!act_on_click) {
                                if (e->release_event && vp->hot == b) {
                                    result.control_clicked = jd_InputHasMod(e, jd_KeyMod_Ctrl);
                                    result.alt_clicked     = jd_InputHasMod(e, jd_KeyMod_Alt);
                                    result.shift_clicked   = jd_InputHasMod(e, jd_KeyMod_Shift);
                                    result.l_clicked       = (e->key == jd_MB_Left);
                                    result.m_clicked       = (e->key == jd_MB_Middle);
                                    result.r_clicked       = (e->key == jd_MB_Right);
                                    break;
                                }
                            }
                            
                        }
                        
                    }
                }
            }
        }
        
    }
    
    jd_TreeLinkLastChild(parent, b);
    
    f64 dpi_sf = jd_WindowGetDPIScale(vp->window);
    
    b->rect = config->rect;
    b->rect.max = (jd_V2F){b->rect.max.x * dpi_sf, b->rect.max.y * dpi_sf};
    b->rect.min = (jd_V2F){b->rect.min.x * dpi_sf, b->rect.min.y * dpi_sf};
    
    b->rect.min.y = jd_Max(vp->window->custom_titlebar_size, b->rect.min.y);
    
    jd_V2F rect_pos = b->rect.min;
    jd_V2F rect_size = b->rect.max;
    
    jd_DrawRect(rect_pos, rect_size, color, corner_radius, softness, thickness);
    if (config->label.count > 0) {
        jd_String* font_id = jd_DArrayGetBack(_jd_internal_ui_state.font_id_stack);
        jd_V2F string_pos = rect_pos;
        
        f32 style_scaling = dpi_sf;
        jd_V2F string_size = jd_CalcStringSizeUTF8(*font_id, config->label, 0.0f);
        string_pos.x += ((rect_size.x - string_size.x) * config->label_alignment.x);
        string_pos.y += ((rect_size.y - string_size.y) * config->label_alignment.y);
        jd_DrawString(*font_id, config->label, string_pos, jd_TextOrigin_TopLeft, style->label_color, 0.0f);
    }
    
    return result;
}

jd_UIViewport* jd_UIBegin(jd_UIViewport* viewport) {
    if (!_jd_internal_ui_state.arena) { // not yet initialized
        jd_LogError("Call jd_UIInitForWindow() before jd_UIBegin", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    jd_UIViewport* vp = viewport;
    
    for (u64 i = 0; i < _jd_internal_ui_state.viewport_count; i++) {
        if (&_jd_internal_ui_state.viewports[i] == viewport) {
            _jd_internal_ui_state.active_viewport = i;
            break;
        }
    }
    
    vp->root->rect.max = vp->window->size;
    
    // Get new inputs
    vp->old_inputs = vp->new_inputs;
    vp->new_inputs.index = vp->old_inputs.range;
    vp->new_inputs.range = vp->old_inputs.array->count;
    
    vp->hot = 0;
    
    jd_V2F mouse_pos = jd_AppGetMousePos(vp->window); // Real time mouse information used for setting the hot item
    vp->hot          = jd_UIPickBoxForPos(vp, mouse_pos);
    
    jd_UIBoxRec* old_root = vp->root;
    vp->root = vp->root_new;
    vp->root_new = old_root;
    jd_TreeLinksClear(vp->root_new);
    
    jd_UIBoxRec* old_popup_root = vp->popup_root;
    vp->popup_root = vp->popup_root_new;
    vp->popup_root_new = old_popup_root;
    jd_TreeLinksClear(vp->popup_root_new);
    
    jd_UIBoxRec* old_titlebar_root = vp->titlebar_root;
    vp->titlebar_root = vp->titlebar_root_new;
    vp->titlebar_root_new = old_titlebar_root;
    jd_TreeLinksClear(vp->titlebar_root_new);
    
    jd_UIPickActiveBox(vp);
    
    jd_UISeedPushPtr(vp->window);
    return vp;
}

void jd_UIEnd() {
    jd_UISeedPop();
    if (!_jd_internal_ui_state.arena) {
        jd_LogError("jd_UIEnd() called before jd_UIBegin()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
}

jd_UIViewport* jd_UIInitForWindow(jd_Window* window) {
    if (!_jd_internal_ui_state.arena) { // not yet initialized
        _jd_internal_ui_state.arena = jd_ArenaCreate(GIGABYTES(2), KILOBYTES(4));
        _jd_internal_ui_state.box_array_size = jd_UIBox_HashTable_Size;
        _jd_internal_ui_state.box_array = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec) * jd_UIBox_HashTable_Size);
        _jd_internal_ui_state.seeds = jd_DArrayCreate(2048, sizeof(u32));
        _jd_internal_ui_state.font_id_stack = jd_DArrayCreate(2048, sizeof(jd_String));
    }
    
    jd_Assert(_jd_internal_ui_state.viewport_count + 1 < jd_UIViewports_Max);
    jd_UIViewport* vp = &_jd_internal_ui_state.viewports[_jd_internal_ui_state.viewport_count++];
    
    if (!vp) { // Sanity check
        jd_LogError("Could not allocate a viewport!", jd_Error_OutOfMemory, jd_Error_Fatal);
    } 
    
    
    vp->window = window;
    vp->old_inputs.array = window->input_events;
    vp->new_inputs.array = window->input_events;
    
    vp->root = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec));
    vp->popup_root = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec));
    vp->titlebar_root = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec));
    
    vp->root_new = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec));
    vp->popup_root_new = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec));
    vp->titlebar_root_new = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec));
    
    vp->roots_init = true;
    
    return vp;
}

