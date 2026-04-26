
jd_ReadOnly static u32 _jd_ui_internal_default_seed = 104720809;

typedef struct jd_Internal_UIState {
    jd_Arena* arena;
    
    jd_UIBoxRec* box_array;
    u64          box_array_size;
    jd_UIBoxRec* box_free_slot;
    
    jd_UIViewport viewports[256];
    u32 viewport_count;
    u32 active_viewport;
    
    jd_DArray* seeds;       // u32
    jd_DArray* parent_stack; // jd_UIBoxRec*
    jd_DArray* font_stack; // jd_FontHandle
    jd_DArray* color_stack; // jd_UIColors
    jd_DArray* layout_stack;
    jd_DArray* shape_stack;
} jd_Internal_UIState;

static jd_Internal_UIState _jd_internal_ui_state = {0};

#define jd_UIViewports_Max      256
#define jd_UIBox_HashTable_Size KILOBYTES(1)

jd_V2F jd_UIParentSize(jd_UIBoxRec* box) {
    if (box->parent) {
        return box->parent->rect.max;
    } else return (jd_V2F){box->vp->size.x, box->vp->size.y};
}

jd_UIViewport* jd_UIViewportGetCurrent() {
    return &_jd_internal_ui_state.viewports[_jd_internal_ui_state.active_viewport];
}

jd_String _jd_UIStringGetDisplayPart(jd_String str) {
    return jd_StringGetPrefix(str, jd_StrLit("##"));
}

jd_String _jd_UIStringGetHashPart(jd_String str) {
    return jd_StringGetPostfix(str, jd_StrLit("###"));
}

jd_UITag jd_UITagFromString(jd_String str) {
    if (!str.count) jd_LogError("UI Error: Cannot create a tag from an empty string", jd_Error_APIMisuse, jd_Error_Fatal);
    u32* seed = jd_DArrayGetBack(_jd_internal_ui_state.seeds);
    if (!seed) jd_LogError("UI Error: Seed stack is empty!", jd_Error_MissingResource, jd_Error_Fatal);
    u32 hash = jd_HashStrToU32(str, *seed);
    
    jd_UITag t = {
        .key = hash
    };
    
    return t;
}

void jd_UISeedPushTag(jd_UITag tag) {
    jd_DArrayPushBack(_jd_internal_ui_state.seeds, &tag.key);
}

void jd_UISeedPushString(jd_String string) {
    u32* seed = jd_DArrayGetBack(_jd_internal_ui_state.seeds);
    if (!seed) jd_LogError("UI Error: Seed stack is empty!", jd_Error_APIMisuse, jd_Error_Fatal);
    u32 hash = jd_HashStrToU32(string, *seed);
    jd_DArrayPushBack(_jd_internal_ui_state.seeds, &hash);
}

void jd_UISeedPushU32(u32 v) {
    u32* seed = jd_DArrayGetBack(_jd_internal_ui_state.seeds);
    if (!seed) jd_LogError("UI Error: Seed stack is empty!", jd_Error_APIMisuse, jd_Error_Fatal);
    u32 hash = jd_HashU32toU32(v, *seed);
    jd_DArrayPushBack(_jd_internal_ui_state.seeds, &hash);
}

void jd_UISeedPushU64(u64 v) {
    u32* seed = jd_DArrayGetBack(_jd_internal_ui_state.seeds);
    if (!seed) jd_LogError("UI Error: Seed stack is empty!", jd_Error_APIMisuse, jd_Error_Fatal);
    u32 hash = jd_HashU64toU32(v, *seed);
    jd_DArrayPushBack(_jd_internal_ui_state.seeds, &hash);
}

void jd_UISeedPushPtr(void* ptr) {
    u64 p = (u64)ptr;
    jd_UISeedPushU64(p);
}

void jd_UISeedPushStringF(jd_String fmt, ...) {
    jd_ScratchArena s = jd_ScratchArenaCreate(_jd_internal_ui_state.arena);
    va_list list = {0};
    va_start(list, fmt);
    jd_String str = jd_StringPushVAList(s.arena, fmt, list);
    jd_UISeedPushString(str);
    jd_ScratchArenaRelease(s);
    va_end(list);
}

void jd_UISeedPop() {
#ifdef JD_DEBUG
    if (!_jd_internal_ui_state.seeds->count)
        jd_LogError("Seed stack is empty! Mismatched Push/Pop pair", jd_Error_APIMisuse, jd_Error_Fatal);
#endif
    jd_DArrayPopBack(_jd_internal_ui_state.seeds);
}

jd_UIBoxRec* jd_UIBoxGetByTag(jd_UITag tag) {
    jd_UIBoxRec* b = &(_jd_internal_ui_state.box_array[tag.key % _jd_internal_ui_state.box_array_size]);
    if (b->tag.key == 0) {
        b->tag.key = tag.key;
        return b;
    }
    
    while (b->next_with_same_hash != 0) {
        if (b->tag.key == tag.key)
            break;
        b = b->next_with_same_hash;
    }
    
    if (b->tag.key != tag.key) {
        if (_jd_internal_ui_state.box_free_slot) {
            b = _jd_internal_ui_state.box_free_slot;
            _jd_internal_ui_state.box_free_slot = b->next;
            jd_TreeLinksClear(b);
        } else {
            b->next_with_same_hash = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec));
            b = b->next_with_same_hash;
        }
        
        b->tag.key = tag.key;
    }
    
    return b;
}

b32 jd_UIRectContainsPoint(jd_UIViewport* vp, jd_V4F r, jd_V2F p) {
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
    
    return ret;
}

void jd_UI_Internal_InsertCodepoint(jd_UITextBox* textbox, u32 codepoint) {
    c8 utf8_buf[4] = {0};
    u32 write_size = jd_UnicodeUTF32toUTF8(codepoint, utf8_buf, 4);
    jd_String* input_text_string = textbox->string;
    if (input_text_string->count + write_size < textbox->max) {
        if (input_text_string->count > 0 && textbox->cursor < input_text_string->count - 1) {
            jd_MemMove(&input_text_string->mem[textbox->cursor + write_size], &input_text_string->mem[textbox->cursor], input_text_string->count - textbox->cursor);
        }
        
        jd_MemMove(&input_text_string->mem[textbox->cursor], utf8_buf, write_size);
        input_text_string->count += write_size;
        textbox->cursor += write_size;
    }
}

void jd_UI_Internal_DeleteCodepoint(jd_UITextBox* textbox) {
    if (textbox->cursor == 0) return;
    u64 c = textbox->cursor;
    
    jd_String* string = textbox->string;
    
    jd_UnicodeSeekDeltaUTF8(*string, &c, -1);
    
    u64 size = string->count - c;
    
    jd_MemMove(&string->mem[c], &string->mem[textbox->cursor], size);
    string->mem[string->count] = 0;
    string->count = string->count - (textbox->cursor - c);
    textbox->cursor = c;
}

void jd_UI_Internal_DeleteWord(jd_UITextBox* textbox) {
    if (textbox->cursor == 0) return;
    while (textbox->cursor > 0 && textbox->string->mem[textbox->cursor - 1] == ' ') {
        jd_UI_Internal_DeleteCodepoint(textbox);
    }
    while (textbox->cursor > 0 && textbox->string->mem[textbox->cursor - 1] != ' ') {
        jd_UI_Internal_DeleteCodepoint(textbox);
    }
}

void jd_UI_Internal_GetAlphaNumericalDelta(jd_UITextBox* textbox, i32 input) {
    if (input < 0 && textbox->cursor == 0) return;
    if (input > 0 && textbox->cursor == textbox->string->count - 1) return;
    
    if (input > 0) {
        while (textbox->cursor < textbox->string->count && textbox->string->mem[textbox->cursor] == ' ') {
            textbox->cursor++;
        }
        for (u64 i = textbox->cursor; i < textbox->string->count; i++) {
            if (textbox->string->mem[i] == ' ') {
                break;
            } else textbox->cursor++;
        }
    } else {
        while (textbox->cursor > 0 && textbox->string->mem[textbox->cursor] == ' ') {
            textbox->cursor--;
        }
        for (u64 i = textbox->cursor; i > 0; i--) {
            if (textbox->string->mem[i] == ' ') {
                break;
            } else textbox->cursor--;
        }
    }
}

void jd_UIPickActiveBox(jd_UIViewport* vp) {
    jd_V2F mouse_delta = {0};
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
                    vp->drag_box = 0;
                } else {
                    vp->active = jd_UIPickBoxForPos(vp, e->mouse_pos);
                    vp->drag_box = vp->active;
                }
            } break;
        }
        
        if (vp->drag_box) {
            mouse_delta.x += e->mouse_delta.x;
            mouse_delta.y += e->mouse_delta.y;
        }
    }
    
    if (vp->drag_box && !jd_V2FEq(mouse_delta, (jd_V2F){0})) {
        vp->drag_box->drag_delta = mouse_delta;
    }
}

void jd_UIFontPush(jd_Font* font, u16 point_size) {
    jd_UISizedFont f = {
        .font = font,
        .point_size = point_size
    };
    
    jd_DArrayPushBack(_jd_internal_ui_state.font_stack, &f);
}

void jd_UIFontPop() {
    if (_jd_internal_ui_state.font_stack->count == 0) {
        jd_LogError("Mismatched calls to jd_UIFontPop()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    jd_DArrayPopBack(_jd_internal_ui_state.font_stack);
}

void jd_UIParentPush(jd_UIBoxRec* box) {
    jd_DArrayPushBack(_jd_internal_ui_state.parent_stack, &box);
}

void jd_UIParentPop() {
    if (_jd_internal_ui_state.parent_stack->count == 0) {
        jd_LogError("Mismatched calls to jd_UIParentPop()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    jd_DArrayPopBack(_jd_internal_ui_state.parent_stack);
}

void jd_UILayoutPush(jd_UILayout layout) {
    jd_DArrayPushBack(_jd_internal_ui_state.layout_stack, &layout);
}

void jd_UILayoutPop() {
    if (_jd_internal_ui_state.layout_stack->count == 0) {
        jd_LogError("Mismatched calls to jd_UILayoutPop()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    jd_DArrayPopBack(_jd_internal_ui_state.layout_stack);
}

void jd_UIShapePush(jd_UIShape shape) {
    jd_DArrayPushBack(_jd_internal_ui_state.shape_stack, &shape);
}

void jd_UIShapePop() {
    if (_jd_internal_ui_state.shape_stack->count == 0) {
        jd_LogError("Mismatched calls to jd_UIShapePop()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    jd_DArrayPopBack(_jd_internal_ui_state.shape_stack);
}

void jd_UIColorsPush(jd_UIColors colors) {
    jd_DArrayPushBack(_jd_internal_ui_state.color_stack, &colors);
}

void jd_UIColorsPop() {
    if (_jd_internal_ui_state.color_stack->count == 0) {
        jd_LogError("Mismatched calls to jd_UIColorsPop()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    jd_DArrayPopBack(_jd_internal_ui_state.color_stack);
}

jd_UIResult jd_UIBoxBegin(jd_UIBoxConfig* config) {
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    
    jd_UIBoxRec* parent = config->parent;
    if (!parent && _jd_internal_ui_state.parent_stack->count > 0) {
        parent = *(jd_UIBoxRec**)jd_DArrayGetBack(_jd_internal_ui_state.parent_stack);
    }
    
    if (!parent) {
        parent = vp->root_new;
    }
    
    jd_UILayout layout = config->layout;
    jd_UIShape  shape =  config->shape;
    jd_UIColors colors = config->colors;
    
    if (!colors.defined) {
        jd_UIColors* colorsp = jd_DArrayGetBack(_jd_internal_ui_state.color_stack);
        colors = *colorsp;
    } 
    
    if (!layout.defined) {
        jd_UILayout* layoutp = jd_DArrayGetBack(_jd_internal_ui_state.layout_stack);
        layout = *layoutp;
    }
    
    if (!shape.defined) {
        jd_UIShape* shapep = jd_DArrayGetBack(_jd_internal_ui_state.shape_stack);
        shape = *shapep;
    }
    
    b8 act_on_click = (config->flags & jd_UIBoxFlags_ActOnClick);
    b8 clickable    = (config->flags & jd_UIBoxFlags_Clickable);
    b8 text_edit   = (config->flags & jd_UIBoxFlags_TextEdit);
    
    jd_UITag tag = jd_UITagFromString(config->string_id);
    jd_UIBoxRec* b = jd_UIBoxGetByTag(tag);
    
    jd_TreeLinksClear(b);
    
    b->vp = jd_UIViewportGetCurrent();
    
    jd_UIResult result = {0};
    result.box = b;
    result.last_active = (b == vp->last_active);
    
    jd_V4F color         = colors.bg_color;
    jd_V4F hovered_color = colors.bg_color_hovered;
    jd_V4F active_color  = colors.bg_color_active;
    
    if (config->draggable) {
        color = colors.draggables;
        hovered_color = colors.draggables;
        active_color = colors.draggables;
    }
    
    if (b->anchor_box) {
        parent = vp->popup_root_new;
    }
    
    vp->box_count++;
    
    // input handling
    if (!(config->flags & jd_UIBoxFlags_Disabled)) {
        if (b == vp->hot) {
            result.hovered = true;
            if (!(config->flags & jd_UIBoxFlags_StaticColor))
                color = hovered_color;
            
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
            
            if (!(config->flags & jd_UIBoxFlags_StaticColor))
                color = active_color;
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
                            break;
                        }
                    }
                }
                
                if (text_edit) {
                    result.text_input = true;
                }
            }
            
        }
        
        if (b == vp->drag_box) {
            result.drag_delta = b->drag_delta;
            b->drag_delta.x = 0;
            b->drag_delta.y = 0;
        }
    }
    
    b->shape = shape;
    b->layout = layout;
    b->colors = colors;
    b->flags = config->flags;
    b->text_box.string  = config->text_box.string;
    if (config->text_box.string && config->text_box.string->count < b->text_box.cursor) b->text_box.cursor = 0;
    b->text_box.max = config->text_box.max;
    b->text_box.hint = config->text_box.hint;
    b->last_touched = jd_AppCurrentFrame(vp->window->app);
    b->fixed_position = config->fixed_position;
    b->anchor_box = config->anchor_box;
    b->label = config->label;
    b->label_alignment = config->label_alignment;
    b->color = color;
    b->anchor_reference_point = config->anchor_reference_point;
    b->string_id = config->string_id;
    b->layout.axis = ((layout.dir == jd_UILayout_LeftToRight) || layout.dir == jd_UILayout_RightToLeft) ? jd_UIAxis_X : jd_UIAxis_Y;
    b->slider_axis = config->slider_axis;
    jd_UISizedFont* sized_font = jd_DArrayGetBack(_jd_internal_ui_state.font_stack);
    b->font = sized_font->font;
    b->font_size = sized_font->point_size;
    b->cursor = config->cursor;
    if (b->flags & jd_UIBoxFlags_Textured) {
        b->texture = config->texture;
    }
    
    jd_TreeLinkLastChild(parent, b);
    
    b->slider_range = (jd_V2F){0, b->rect.max.val[b->slider_axis]};
    
    vp->builder_last_box = b;
    
    jd_UIParentPush(b);
    jd_UISeedPushTag(b->tag);
    
    return result;
}

void jd_UIBoxEnd() {
    jd_UISeedPop();
    jd_UIParentPop();
}

void jd_UIPruneUnusedBoxes() {
    jd_Internal_UIState state = _jd_internal_ui_state;
    jd_App* app = jd_UIViewportGetCurrent()->window->app;
    if (jd_AppCurrentFrame(app) < 2) {
        return;
    }
    for (u64 i = 0; i < state.box_array_size; i++) {
        jd_UIBoxRec* box = &state.box_array[i];
        jd_App* app = jd_UIViewportGetCurrent()->window->app;
        if (box->tag.key > 0 && box->last_touched < (jd_AppCurrentFrame(app) - 2)) {
            box->tag.key = 0;
            jd_TreeLinksClear(box);
        }
        
        jd_UIBoxRec* h = box->next_with_same_hash;
        while (h) {
            jd_UIBoxRec* next = h->next;
            if (h->tag.key > 0 && h->last_touched < (jd_AppCurrentFrame(app) - 2)) {
                h->tag.key = 0;
                
                jd_TreeLinksClear(h);
                
                if (state.box_free_slot) {
                    jd_DLinkPrev(state.box_free_slot, h);
                } 
                
                state.box_free_slot = h;
            }
            
            h = next;
        }
    }
}

jd_UIViewport* jd_UIBegin(jd_UIViewport* viewport) {
    if (!_jd_internal_ui_state.arena) { // not yet initialized
        jd_LogError("Call jd_UIInitForWindow() before jd_UIBegin", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    
    jd_UIViewport* vp = viewport;
    vp->size = jd_WindowGetDrawSize(viewport->window);
    jd_WindowSetMinimumSize(vp->window, (jd_V2I){vp->minimum_size.x, vp->minimum_size.y});
    
    for (u64 i = 0; i < _jd_internal_ui_state.viewport_count; i++) {
        if (&_jd_internal_ui_state.viewports[i] == viewport) {
            _jd_internal_ui_state.active_viewport = i;
            break;
        }
    }
    
    vp->root->rect.max = (jd_V2F){vp->window->size.x, vp->window->size.y};
    vp->old_inputs = vp->new_inputs;
    vp->new_inputs.index = vp->old_inputs.range;
    vp->new_inputs.range = vp->old_inputs.array->count;
    
    vp->hot = 0;
    
    jd_V2F mouse_pos = jd_AppGetMousePos(vp->window); // Real time mouse information used for setting the hot item
    
    vp->box_count = 0;
    
    jd_UIBoxRec* old_root = vp->root;
    vp->root = vp->root_new;
    vp->root_new = old_root;
    jd_TreeLinksClear(vp->root_new);
    
    jd_UIBoxRec* old_popup_root = vp->popup_root;
    vp->popup_root = vp->popup_root_new;
    vp->popup_root_new = old_popup_root;
    jd_TreeLinksClear(vp->popup_root_new);
    
    vp->hot = jd_UIPickBoxForPos(vp, mouse_pos);
    if (vp->hot) jd_AppSetCursor(vp->hot->cursor);
    jd_UIPickActiveBox(vp);
    
    jd_UIPruneUnusedBoxes();
    
    jd_UISeedPushU32(_jd_internal_ui_state.active_viewport);
    
    return vp;
}

void jd_UI_Internal_SolveFixedSizes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    while (b != 0) {
        if (b->shape.rules[axis].kind == jd_UISizeKind_Fixed) {
            b->rect.max.val[axis] = b->shape.rules[axis].fixed_size;
        }
        
        else if (b->shape.rules[axis].kind == jd_UISizeKind_FitText) {
            jd_String s = b->label;
            if (b->flags & jd_UIBoxFlags_TextEdit) {
                s = *(b->text_box.string);
                if (!s.count) {
                    s = b->text_box.hint;
                }
            }
            
            jd_String fallback = jd_StrLit("|");
            if (!s.count) {
                s = fallback;
            }
            if (s.count)
                b->rect.max.val[axis] = jd_FontGetTextLayoutExtent(b->font, b->font_size, s, 0.0f, false).val[axis];
            
            b->rect.max.val[axis] += (b->shape.padding.val[axis] * 2);
        }
        
        b->requested_size.val[axis] = b->rect.max.val[axis];
        
        jd_TreeTraversePreorder(b);
    }
}

void jd_UI_Internal_SolveGrowSizes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    while (b != 0) {
        if (b->shape.rules[axis].kind != jd_UISizeKind_Grow) {
            jd_TreeTraversePreorder(b);
            continue;
        }
        if (b == vp->root_new) {
            b->rect.max.val[axis] = vp->size.val[axis];
        }  else {
            jd_UIBoxRec* p = b->parent;
            if (axis == p->layout.axis) {
                u64 grow_siblings = 0;
                u64 sibling_count = 0;
                f32 taken = 0.0f;
                for (jd_UIBoxRec* s = p->first_child; s != 0; s = s->next) {
                    if (s != b) {
                        sibling_count++;
                        if (s->shape.rules[axis].kind == jd_UISizeKind_Grow)
                            grow_siblings++;
                        else
                            taken += s->rect.max.val[axis];
                    }
                }
                
                f32 total_grow_size = (p->rect.max.val[axis] - (p->shape.padding.val[axis] * 2));
                total_grow_size -= (p->layout.gap * sibling_count);
                total_grow_size -= taken;
                
                f32 per_grow_size = (total_grow_size / (1+grow_siblings));
                
                b->rect.max.val[axis] = jd_Max(0.0f, per_grow_size);
            } else {
                b->rect.max.val[axis] = p->rect.max.val[axis] - (p->shape.padding.val[axis] * 2);
            }
            
        }
        
        jd_TreeTraversePreorder(b);
    }
}

void jd_UI_Internal_SolvePctParentSizes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    while (b != 0) {
        if (b->shape.rules[axis].kind == jd_UISizeKind_PctParent) {
            jd_UIBoxRec* p = b->parent;
            
            if (!p) {
                b->rect.max.val[axis] = b->vp->size.val[axis] * b->shape.rules[axis].pct_parent;
            } else {
                b->rect.max.val[axis] = (p->rect.max.val[axis] * b->shape.rules[axis].pct_parent);
            }
        }
        
        b->requested_size.val[axis] = b->rect.max.val[axis];
        jd_TreeTraversePreorder(b);
    }
}

void jd_UI_Internal_SolveFitChildrenSizes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    jd_TreeGetPostorderRoot(b);
    
    while (b != 0) {
        if (b->shape.rules[axis].kind == jd_UISizeKind_Fit ||
            b->shape.rules[axis].kind == jd_UISizeKind_Grow) {
            jd_UIBoxRec* c = b->first_child;
            f32 size = 0.0;
            u64 child_count = 0;
            f32 largest = 0.0f;
            for (; c != 0; c = c->next) {
                if (c->flags & jd_UIBoxFlags_NoClip) {
                    continue;
                }
                if (b->layout.axis == axis) {
                    size += c->rect.max.val[axis];
                }
                
                else
                    size = jd_Max(c->rect.max.val[axis], size);
                
                child_count++;
            }
            if (child_count && b->layout.axis == axis)
                b->rect.max.val[axis] += (child_count - 1) * b->layout.gap;
            
            b->rect.max.val[axis] += jd_Max(size, 0) + (b->shape.padding.val[axis] * 2);
            
            if (!child_count && b->label.count) {
                b->rect.max.val[axis] += jd_FontGetTextLayoutExtent(b->font, b->font_size, b->label, 0.0f, 0).val[axis];
            }
            b->requested_size.val[axis] = b->rect.max.val[axis];
        }
        jd_TreeTraversePostorder(b);
    }
}

void jd_UI_Internal_FinalizeSizes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    b = root;
    
    while (b) {
        f32 size = 0.0f;
        f32 allowed_size = b->rect.max.val[axis] - (b->shape.padding.val[axis] * 2);
        
        u32 child_count = 0;
        for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next, child_count++);
        if (axis == b->layout.axis) {
            allowed_size -= (b->layout.gap * (child_count - 1));
        }
        
        for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next) {
            if (axis != b->layout.axis) {
                if (c->flags & jd_UIBoxFlags_MatchSiblingsOffAxis) {
                    c->rect.max.val[axis] = allowed_size;
                }
                
                size = jd_Max(c->rect.max.val[axis], size);
            } else {
                size += c->rect.max.val[axis];
            }
            
            if (c->shape.rules[axis].minimum_size > 0.0f) {
                c->rect.max.val[axis] = jd_Max(c->shape.rules[axis].minimum_size, c->rect.max.val[axis]);
            }
            
        }
        
        f32 diff = (size - allowed_size);
        b->scroll_max.val[axis] = (diff > 0) ? diff : 0;
        if (b->scroll.val[axis] > b->scroll_max.val[axis]) {
            b->scroll.val[axis] = b->scroll_max.val[axis];
        }
        
        b->rect.min.x = jd_F32RoundUp(b->rect.min.x);
        b->rect.min.y = jd_F32RoundUp(b->rect.min.y);
        b->rect.max.x = jd_F32RoundUp(b->rect.max.x);
        b->rect.max.y = jd_F32RoundUp(b->rect.max.y);
        
        jd_TreeTraversePreorder(b);
    }
} 

void jd_UI_Internal_PositionBoxes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    jd_UIViewport* vp = b->vp;
    f32 position = 0;
    
    while (b) {
        f32 position = 0;
        for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next) {
            if (c->anchor_box) {
                c->rect.min.val[axis] =  c->anchor_box->rect.min.val[axis];
                c->rect.min.val[axis] += c->anchor_box->rect.max.val[axis] * c->anchor_reference_point.val[axis]; 
                c->rect.min.val[axis] -= c->rect.max.val[axis] * c->reference_point.val[axis];
            }
            else if (c->fixed_position.val[axis] == 0) {
                c->rect.min.val[axis] = (b->rect.min.val[axis] + position);
                if (c == b->first_child) {
                    c->rect.min.val[axis] += b->shape.padding.val[axis];
                }
                
                if (axis == b->layout.axis) {
                    if (c == b->first_child) {
                        position += b->shape.padding.val[axis];
                    } 
                    position += c->rect.max.val[axis] + b->layout.gap;
                }
                else {
                    position = jd_Max(position, b->shape.padding.val[axis]);
                }
                
            } else {
                c->rect.min.val[axis] = c->fixed_position.val[axis] - (c->rect.max.val[axis] * c->reference_point.val[axis]);
            }
            
            if (b->flags & (jd_UIBoxFlags_ScrollX << axis)) {
                c->rect.min.val[axis] -= b->scroll.val[axis];
            }
        }
        
        
        jd_TreeTraversePreorder(b);
    }
}

b32 jd_UI_Internal_BoxIsOffScreen(jd_UIBoxRec* b) {
    if (b->rect.min.x > b->vp->size.x) return true;
    if (b->rect.min.y > b->vp->size.y) return true;
    return false;
}

void jd_UI_Internal_RenderBoxes(jd_UIBoxRec* root, b32 clip_parent) {
    jd_UIBoxRec* b = root;
    jd_UIViewport* vp = b->vp;
    f64 dpi_sf = jd_WindowGetDPIScale(vp->window);
    
    b8 debug = vp->debug_mode;
    
    while (b) {
        if (jd_UI_Internal_BoxIsOffScreen(b)) {
            jd_TreeTraversePreorder(b);
            continue;
        }
        
        jd_UIColors colors = b->colors;
        jd_UILayout layout = b->layout;
        jd_UIShape shape  = b->shape;
        
        f32 z = 0.0;
        
        jd_V4F color        = b->color;
        
        f32 softness        = shape.softness;
        f32 corner_radius   = shape.corner_radius;
        f32 thickness       = shape.thickness;
        
        jd_V2F rect_pos = {b->rect.min.x, b->rect.min.y};
        jd_V2F rect_size = b->rect.max;
        
        jd_V4F clipping_rectangle_self = {
            .min = {b->rect.min.x, b->rect.min.y},
            .max = {b->rect.min.x + (b->rect.max.x), b->rect.min.y + (b->rect.max.y)}
        };
        
        jd_V4F clipping_rectangle_parent = {0};
        if (clip_parent && b->parent) {
            clipping_rectangle_parent = b->parent->rect_clipped;
        } else {
            clipping_rectangle_parent = clipping_rectangle_self;
        }
        
        if (b->flags & jd_UIBoxFlags_NoClip) {
            clipping_rectangle_parent.min = (jd_V2F){0, 0};
            clipping_rectangle_parent.max = (jd_V2F){vp->size.x, vp->size.y};
        }
        
        b->rect_clipped = jd_2DRectClip(clipping_rectangle_self, clipping_rectangle_parent);
        
        jd_V2F string_pos = {rect_pos.x + b->shape.padding.x, rect_pos.y + b->shape.padding.y};
        if (!(b->flags & jd_UIBoxFlags_InvisibleBG))
            jd_2DColorRect(rect_pos, rect_size, color, corner_radius, softness, thickness, clipping_rectangle_parent); 
        if (b->shape.stroke_width > 0) {
            jd_2DColorRect(rect_pos, rect_size, colors.stroke, corner_radius, 1.0f, b->shape.stroke_width, clipping_rectangle_parent); 
        }
        
        if (b->flags & jd_UIBoxFlags_Textured) {
            jd_V2F image_position = rect_pos;
            f32 tex_width  = b->texture.uvw_max.x - b->texture.uvw_min.x;
            f32 tex_height = b->texture.uvw_max.y - b->texture.uvw_min.y;
            b8 portrait = tex_width < tex_height;
            f32 aspect_ratio = tex_width / tex_height;
            
            f32 width = 0;
            f32 height = 0;
            
            if (portrait) {
                height = b->rect.max.y;
                width  = height * aspect_ratio;
                image_position.x += (b->rect.max.x - width) / 2;
            } else {
                width  = b->rect.max.x;
                height = width / aspect_ratio;
                image_position.y += (b->rect.max.y - height) / 2;
            }
            
            jd_2DTextureRect(b->texture, image_position, jd_V2F(width, height), corner_radius, softness, thickness, clipping_rectangle_self);
        }
        
        if (b->flags & jd_UIBoxFlags_TextEdit) {
            f32 wrap = b->rect.max.x;
            if (!(b->flags & jd_UIBoxFlags_Multiline)) {
                wrap = 0;
            }
            
            jd_V2F string_size = {0};
            jd_String backup_string = jd_StrLit("M");
            if (b->text_box.string->count == 0) {
                string_size = jd_FontGetTextLayoutExtent(b->font, b->font_size, backup_string, wrap, (wrap));
            } else {
                string_size = jd_FontGetTextLayoutExtent(b->font, b->font_size, *b->text_box.string, wrap, (wrap));
            }
            
            string_size.x = jd_Min(b->rect.max.x, string_size.x);
            string_size.y = jd_Min(b->rect.max.y, string_size.y);
            
            string_pos.x += (((rect_size.x - (b->shape.padding.x * 2)) - string_size.x) * b->label_alignment.x);
            string_pos.y += (((rect_size.y - (b->shape.padding.y * 2)) - string_size.y) * b->label_alignment.y);
            
            if (b->flags & jd_UIBoxFlags_ScrollX) {
                string_pos.x -= b->scroll.x;
                b->scroll_max.x = string_size.x - rect_size.x;
            }
            
            if (b->flags & jd_UIBoxFlags_ScrollY) {
                string_pos.y -= b->scroll.y;
                b->scroll_max.y = string_size.y - rect_size.y;
            }
            
            jd_V2F final_pos = {string_pos.x, string_pos.y};
            
            if (b->text_box.string->count)
                jd_2DString(b->font, *b->text_box.string, b->font_size, final_pos, colors.label, clipping_rectangle_self, wrap, true);
            else if (b != vp->last_active) {
                jd_2DString(b->font, b->text_box.hint, b->font_size, final_pos, colors.label, clipping_rectangle_self, wrap, true);
            }
            
            if (b == vp->last_active) {
                jd_V2F cur_pos_v2 = jd_FontGetPenPositionForIndex(b->font, b->font_size, *b->text_box.string, b->text_box.cursor, wrap, (b->flags & jd_UIBoxFlags_Multiline));
                
                if (b->flags & jd_UIBoxFlags_ScrollX) cur_pos_v2.x -= b->scroll.x;
                if (b->flags & jd_UIBoxFlags_ScrollY) cur_pos_v2.y -= b->scroll.y;
                
                jd_V2F cur_pos = {final_pos.x + cur_pos_v2.x, final_pos.y + cur_pos_v2.y};
                b->text_box.last_cursor_pos = (jd_V2F){cur_pos_v2.x, cur_pos_v2.y};
                jd_V2F cur_size = {2.0f, (b->font->layout_line_height * b->font_size) * 1.2f};
                jd_2DColorRect(cur_pos, cur_size, jd_V4F(1.0f, 1.0f, 1.0f, 1.0f), 0, 0, 0, clipping_rectangle_self);
            }
        }
        
        else if (b->label.count > 0) {
            jd_V2F string_size = jd_FontGetTextLayoutExtent(b->font, b->font_size, b->label, 0.0f, false);
            string_size.x = jd_Min(b->rect.max.x, string_size.x);
            string_size.y = jd_Min(b->rect.max.y, string_size.y);
            string_pos.x += (((rect_size.x - (b->shape.padding.x * 2)) - string_size.x) * b->label_alignment.x);
            string_pos.y += (((rect_size.y - (b->shape.padding.y * 2)) - string_size.y) * b->label_alignment.y);
            jd_V3F final_pos = {string_pos.x, string_pos.y, z};
            jd_2DString(b->font, b->label, b->font_size, string_pos, colors.label, clipping_rectangle_parent, 0.0f, false);
        }
        
        b->rect = jd_2DRectClip(b->rect, clipping_rectangle_parent);
        
        if (debug) {
            jd_V4F debug_color        = {1.0f, 0.0, 0.0, 0.5};
            jd_V4F debug_color_active = {1.0f, 1.0, 0.0, 1.0};
            jd_V4F c = (b == vp->last_active) ? debug_color_active : debug_color;
            if (b == vp->last_active) vp->debug_box = b;
            jd_2DColorRect(rect_pos, rect_size, c, 0, 0, 1.0f, clipping_rectangle_parent);
        }
        
        jd_TreeTraversePreorder(b);
    }
}

void jd_UI_Internal_ResetSizes(jd_UIBoxRec* root) {
    jd_UIBoxRec* b = root;
    while (b) {
        for (u64 i = 0; i < jd_UIAxis_Count; i++) {
            b->rect.max.val[i] = 0.0f;
            b->rect.min.val[i] = 0.0f;
        }
        jd_TreeTraversePreorder(b);
    }
}

void jd_UIEnd() {
    u32 last_codepoint = 41; // Used to infer line height for cursor movement
    jd_UISeedPop();
    
    if (_jd_internal_ui_state.seeds->count > 1 || // There is always a seed in the seed stack!
        _jd_internal_ui_state.parent_stack->count > 0 ||
        _jd_internal_ui_state.font_stack->count > 0 ||
        _jd_internal_ui_state.color_stack->count > 1) {
        jd_LogError("A UI stack is not empty at jd_UIEnd()! Mismatched Push/Pop.", jd_Error_APIMisuse, jd_Error_Warning);
    }
    
    if (!_jd_internal_ui_state.arena) {
        jd_LogError("jd_UIEnd() called before jd_UIBegin()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    // Solve sizes and positions
    
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    
    f32 scroll_step = 30.0f/* * jd_WindowGetDPIScale(vp->window)*/;
    
    jd_InputSliceForEach(i, vp->old_inputs) {
        jd_InputEvent* e = jd_DArrayGetIndex(vp->old_inputs.array, i);
        if (!e) { // Sanity Check
            jd_LogError("Incorrect input slice range!", jd_Error_APIMisuse, jd_Error_Fatal);
        }
        
        b8 text_edit_active = (vp->last_active && (vp->last_active->flags & jd_UIBoxFlags_TextEdit));
        
        i64 delta = 0;
        i64 word_delta = 0;
        
        if (vp->hot && !(e->scroll_delta.x == 0 && e->scroll_delta.y == 0)) {
            jd_UIBoxRec* sb = vp->hot;
            while (sb && !(sb->flags & jd_UIBoxFlags_ScrollY) && !(sb->flags & jd_UIBoxFlags_ScrollX)) {
                sb = sb->parent;
            }
            
            if (sb) {
                if (sb->flags & jd_UIBoxFlags_ScrollY) {
                    sb->scroll.y += (e->scroll_delta.y * scroll_step);
                    if (sb->scroll.y < 0.0f) sb->scroll.y = 0;
                    if (sb->scroll.y > sb->scroll_max.y) sb->scroll.y = sb->scroll_max.y;
                }
                
                if (sb->flags & jd_UIBoxFlags_ScrollX) {
                    sb->scroll.x += e->scroll_delta.x * scroll_step;
                    if (sb->scroll.x < 0.0f) sb->scroll.x = 0;
                    if (sb->scroll.x > sb->scroll_max.x) sb->scroll.x = sb->scroll_max.x;
                }
            }
        }
        
        if (text_edit_active) {
            jd_UIBoxRec* b = vp->last_active;
            switch (e->key) {
                case jd_Key_Left: {
                    if (jd_InputHasMod(e, jd_KeyMod_Ctrl)) {
                        jd_UI_Internal_GetAlphaNumericalDelta(&b->text_box, -1);
                    } else {
                        delta += -1;
                    }
                } break;
                
                case jd_Key_Right: {
                    if (jd_InputHasMod(e, jd_KeyMod_Ctrl)) {
                        jd_UI_Internal_GetAlphaNumericalDelta(&b->text_box, 1);
                    } else {
                        delta += 1;
                    }
                } break;
                
                case jd_Key_Back: 
                case jd_Key_Delete: {
                    if (jd_InputHasMod(e, jd_KeyMod_Ctrl)) {
                        jd_UI_Internal_DeleteWord(&b->text_box);
                    } else {
                        jd_UI_Internal_DeleteCodepoint(&b->text_box);
                    }
                } break;
                
                case jd_Key_Up: 
                case jd_Key_Down: {
                    if (b->flags & jd_UIBoxFlags_Multiline) {
                        f32 line_adv = b->font->line_adv * b->font_size;
                        
                        if (e->key == jd_Key_Up)
                            line_adv = -line_adv;
                        
                        jd_V2F current_pos = b->text_box.last_cursor_pos;
                        jd_V2F new_pos = {current_pos.x + b->scroll.x, current_pos.y + b->scroll.y + line_adv};
                        u64 cursor = b->text_box.cursor;
                        u64 new_cursor = jd_FontGetIndexForPenPosition(b->font, b->font_size, *b->text_box.string, new_pos, b->rect.max.x, true);
                        i64 delta = new_cursor - cursor;
                        
                        jd_UnicodeSeekDeltaUTF8(*b->text_box.string, &cursor, delta);
                        b->text_box.cursor = cursor;
                    }
                    
                } break;
                
                case jd_Key_Return: {
                    if (b->flags & jd_UIBoxFlags_Multiline)
                        jd_UI_Internal_InsertCodepoint(&b->text_box, 0x0A);
                } break;
                
                case jd_Key_Tab: {
                    
                } break;
                
                default: {
                    if (e->codepoint > 0) {
                        jd_UI_Internal_InsertCodepoint(&b->text_box, e->codepoint);
                        last_codepoint = e->codepoint;
                    }
                }
            }
        }
        
        if (delta != 0) {
            jd_UIBoxRec* b = vp->last_active;
            jd_UnicodeSeekDeltaUTF8(*b->text_box.string, &b->text_box.cursor, delta);
        }
        
    }
    
    {
        jd_UIBoxRec* b = vp->root_new;
        jd_UI_Internal_ResetSizes(b);
        for (u64 i = 0; i < jd_UIAxis_Count; i++) {
            
            jd_UI_Internal_SolveFixedSizes(b, i);
            jd_UI_Internal_SolveFitChildrenSizes(b, i);
            jd_UI_Internal_SolveGrowSizes(b, i);
            jd_UI_Internal_SolvePctParentSizes(b, i);
            jd_UI_Internal_FinalizeSizes(b, i);
            jd_UI_Internal_PositionBoxes(b, i);
        }
        jd_UI_Internal_RenderBoxes(b, true);
    }
    
    {
        jd_UIBoxRec* b = vp->popup_root_new;
        jd_UI_Internal_ResetSizes(b);
        for (u64 i = 0; i < jd_UIAxis_Count; i++) {
            
            jd_UI_Internal_SolveFixedSizes(b, i);
            jd_UI_Internal_SolveFitChildrenSizes(b, i);
            jd_UI_Internal_SolvePctParentSizes(b, i);
            jd_UI_Internal_FinalizeSizes(b, i);
            jd_UI_Internal_PositionBoxes(b, i);
        }
        jd_UI_Internal_RenderBoxes(b, false);
    }
    
    {
        jd_UIBoxRec* b = vp->root_new;
        while (b) {
            vp->minimum_size.x = jd_Max(b->shape.rules[jd_UIAxis_X].minimum_size, vp->minimum_size.x);
            vp->minimum_size.y = jd_Max(b->shape.rules[jd_UIAxis_Y].minimum_size, vp->minimum_size.y);
            
            jd_TreeTraversePreorder(b);
        }
    }
}

jd_UIViewport* jd_UIInitForWindow(jd_Window* window) {
    if (!_jd_internal_ui_state.arena) {
        _jd_internal_ui_state.arena = jd_ArenaCreate(GIGABYTES(2), KILOBYTES(4));
        _jd_internal_ui_state.box_array_size = jd_UIBox_HashTable_Size;
        _jd_internal_ui_state.box_array = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec) * jd_UIBox_HashTable_Size);
        _jd_internal_ui_state.seeds = jd_DArrayCreate(2048, sizeof(u32));
        jd_DArrayPushBack(_jd_internal_ui_state.seeds, &_jd_ui_internal_default_seed);
        _jd_internal_ui_state.font_stack = jd_DArrayCreate(2048, sizeof(jd_UISizedFont));
        _jd_internal_ui_state.parent_stack = jd_DArrayCreate(256, sizeof(jd_UIBoxRec*));
        _jd_internal_ui_state.color_stack = jd_DArrayCreate(256, sizeof(jd_UIColors));
        _jd_internal_ui_state.layout_stack = jd_DArrayCreate(256, sizeof(jd_UILayout));
        _jd_internal_ui_state.shape_stack  = jd_DArrayCreate(256, sizeof(jd_UIShape));
        
        jd_UIColors jd_default_ui_colors = jd_UIMakeColors(jd_V4F(0.1, 0.1, 0.1, 1.0f), 
                                                           jd_V4F(0.2, 0.2, 0.2, 1.0f), 
                                                           jd_V4F(.05, .05, .05, 1.0f), 
                                                           jd_V4F(.90, .90, .90, 1.0f), 
                                                           jd_V4F(0.0, 0.0, 0.0, 1.0f),
                                                           jd_V4F(0.9, 0.9, 0.9, 1.0f));
        
        jd_UIShape jd_default_ui_shape  = jd_UIMakeShape(jd_UIGrow, jd_UIGrow); 
        
        jd_UILayout jd_default_ui_layout = jd_UIMakeLayout(jd_UILayout_LeftToRight, 0.0f);
        
        jd_DArrayPushBack(_jd_internal_ui_state.color_stack, &jd_default_ui_colors);
        jd_DArrayPushBack(_jd_internal_ui_state.layout_stack, &jd_default_ui_layout);
        jd_DArrayPushBack(_jd_internal_ui_state.shape_stack, &jd_default_ui_shape);
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
    
    vp->root_new = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec));
    vp->popup_root_new = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec));
    
    vp->root->vp = vp;
    vp->popup_root->vp = vp;
    
    vp->root_new->vp = vp;
    vp->popup_root_new->vp = vp;
    
    vp->roots_init = true;
    return vp;
}

//~ User-facing functions
jd_UIShape jd_UIMakeShape(jd_UISizeRule rule_x, jd_UISizeRule rule_y) {
    jd_UIShape shape = {
        .rules = {rule_x, rule_y},
        .defined = true
    };
    return shape;
}

jd_UIShape jd_UIMakeShapeEx(jd_UISizeRule rule_x, jd_UISizeRule rule_y, f32 padding_x, f32 padding_y, f32 softness, f32 corner_radius, f32 thickness, f32 stroke_width) {
    jd_UIShape shape = {
        .rules = {rule_x, rule_y},
        .padding = {padding_x, padding_y},
        .softness = softness,
        .corner_radius = corner_radius,
        .thickness = thickness,
        .stroke_width = stroke_width,
        .defined = true
    };
    return shape;
}

jd_UIColors jd_UIMakeColors(jd_V4F bg, jd_V4F bg_hovered, jd_V4F bg_active, jd_V4F label, jd_V4F stroke, jd_V4F draggables) {
    jd_UIColors colors = {
        .bg_color = bg,
        .bg_color_hovered = bg_hovered,
        .bg_color_active = bg_active,
        .label = label,
        .stroke = stroke,
        .draggables = draggables,
        .defined = true
    };
    return colors;
}

jd_UILayout jd_UIMakeLayout(jd_UILayoutDir dir, f32 gap) {
    jd_UILayout layout = {
        .dir = dir,
        .gap = gap,
        .axis = (dir < jd_UILayout_TopToBottom) ? jd_UIAxis_X : jd_UIAxis_Y,
        .defined = true
    };
    return layout;
}

jd_UIResult jd_UILabel(jd_String label) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_StaticColor|jd_UIBoxFlags_InvisibleBG;
    config.string_id = _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    jd_UIShape shape = jd_UIMakeShape(jd_UIFitText, jd_UIFitText);
    config.shape = shape;
    config.cursor = jd_Cursor_Default;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UILabelEx(jd_String label, jd_UIShape shape, jd_UIColors colors, jd_V2F alignment, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags;
    config.flags |= jd_UIBoxFlags_StaticColor|jd_UIBoxFlags_InvisibleBG;
    config.string_id = _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    config.shape = shape;
    config.cursor = jd_Cursor_Default;
    config.label_alignment = alignment;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIFixedSizeLabel(jd_String label, jd_V2F alignment, jd_V2F size) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_StaticColor|jd_UIBoxFlags_InvisibleBG;
    config.string_id = _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    config.cursor = jd_Cursor_Default;
    config.label_alignment = alignment;
    
    jd_UIShape shape = jd_UIMakeShape(jd_UIFixed(size.x), jd_UIFixed(size.y));
    config.shape = shape;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UILabelButton(jd_String label, jd_V2F padding, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_Clickable;
    config.string_id = _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    config.cursor = jd_Cursor_Hand;
    
    jd_UIShape shape = jd_UIMakeShapeEx(jd_UIFitText, jd_UIFitText, padding.x, padding.y, 0, 0, 0, 0);
    config.shape = shape;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIListButton(jd_String label, jd_V2F alignment, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags|jd_UIBoxFlags_Clickable|jd_UIBoxFlags_MatchSiblingsOffAxis;
    config.string_id = _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    jd_UIShape shape = jd_UIMakeShape(jd_UIFitText, jd_UIFitText);
    config.shape = shape;
    config.cursor = jd_Cursor_Hand;
    config.label_alignment = alignment;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIFixedSizeButton(jd_String label, jd_V2F size, jd_V2F label_alignment) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_Clickable;
    config.string_id =  _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    
    jd_UIShape shape = jd_UIMakeShape(jd_UIFixed(size.x), jd_UIFixed(size.y));
    config.shape = shape;
    config.cursor = jd_Cursor_Hand;
    config.label_alignment = label_alignment;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIButtonEx(jd_String label, jd_UIShape shape, jd_UIColors colors, jd_V2F label_alignment, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags;
    config.flags |= jd_UIBoxFlags_Clickable;
    config.string_id =  _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    config.shape = shape;
    config.colors = colors;
    config.cursor = jd_Cursor_Hand;
    config.label_alignment = label_alignment;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UISlider(jd_String string_id, jd_UIShape shape, jd_UIShape shape_button, jd_UIAxis axis) {
    jd_UIBoxConfig underbox = {0};
    underbox.flags = jd_UIBoxFlags_StaticColor;
    underbox.string_id = string_id;
    underbox.shape = shape;
    underbox.cursor = jd_Cursor_Default;
    
    jd_UIResult underbox_result = jd_UIBoxBegin(&underbox);
    
    jd_UIBoxRec* under = jd_UIGetLastBox();
    
    jd_UIBoxConfig slider_config = {0};
    slider_config.flags = jd_UIBoxFlags_Clickable|jd_UIBoxFlags_ActOnClick|jd_UIBoxFlags_NoClip;
    slider_config.string_id = jd_StrLit("jd_internal_slider_child");
    slider_config.shape = shape_button;
    slider_config.cursor = jd_Cursor_Hand;
    slider_config.reference_point = (jd_V2F){0.5, 0.5};
    slider_config.draggable = true;
    
    slider_config.fixed_position.val[axis] = under->rect.min.val[axis] + ((under->rect.max.val[axis] - shape_button.rules[axis].fixed_size) * under->slider_pos);
    f32 offaxis_diff = (under->rect.max.val[!axis] - shape.rules[!axis].fixed_size) * slider_config.reference_point.val[!axis];
    slider_config.fixed_position.val[!axis] = under->rect.min.val[!axis] + offaxis_diff;
    
    jd_UIResult slider_result = jd_UIBoxBegin(&slider_config);
    jd_UIBoxEnd();
    
    if (slider_result.drag_delta.val[axis] != 0) {
        jd_UIBoxRec* b = slider_result.box;
        
        f32 converted_delta = slider_result.drag_delta.val[b->slider_axis] / under->slider_range.val[1];
        under->slider_pos = jd_ClampToRange(under->slider_pos + converted_delta, 0, 1);
        slider_result.slider_position = under->slider_pos;
    }
    
    slider_result.box = under;
    
    jd_UIBoxEnd();
    return slider_result;
}

jd_UIResult jd_UISliderF32(jd_String string_id, jd_UIShape shape, jd_UIShape shape_button, jd_UIAxis axis, f32* v, f32 min, f32 max) {
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    jd_UIResult result = jd_UISlider(string_id, shape, shape_button, axis);
    if (result.drag_delta.val[axis] != 0) {
        *v = min + (max * result.slider_position);
    } else if (!vp->drag_box) {
        result.box->slider_pos = jd_ClampToRange((*v / max), 0, 1);
    }
    return result;
}

jd_UIResult jd_UISliderU64(jd_String string_id, jd_UIShape shape, jd_UIShape shape_button, jd_UIAxis axis, u64* v, u64 min, u64 max) {
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    jd_UIResult result = jd_UISlider(string_id, shape, shape_button, axis);
    if (result.drag_delta.val[axis] != 0) {
        *v = min + (max * result.slider_position);
    } else if (!vp->drag_box) {
        u64 value = *v;
        if (max != 0) {
            result.box->slider_pos = (f32)jd_ClampToRange(((f32)value / (f32)max), 0, 1);
        }
    }
    return result;
}

jd_UIResult jd_UIRegionBegin(jd_String string_id, jd_UIShape shape, jd_UILayout layout, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags;
    config.flags |= jd_UIBoxFlags_StaticColor;
    config.string_id  = string_id;
    config.shape = shape;
    config.layout = layout;
    config.cursor = jd_Cursor_Default;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    
    return result;
}

jd_UIResult jd_UIRegionBeginAnchored(jd_String string_id, jd_UIShape shape, jd_UILayout layout, jd_UIBoxRec* anchor_box, jd_V2F anchor_to, jd_V2F anchor_from, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags;
    config.flags |= jd_UIBoxFlags_StaticColor;
    config.string_id  = string_id;
    config.anchor_box = anchor_box;
    config.anchor_reference_point = anchor_to;
    config.reference_point = anchor_from;
    config.shape = shape;
    config.layout = layout;
    config.cursor = jd_Cursor_Default;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    
    return result;
}

jd_UIResult jd_UIInputTextBoxAligned(jd_String string_id, jd_UIShape shape, jd_String* string, u64 max_string_size, jd_V2F alignment) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_Clickable|jd_UIBoxFlags_TextEdit|jd_UIBoxFlags_StaticColor;
    config.string_id  = string_id;
    config.shape = shape;
    config.cursor = jd_Cursor_Default;
    config.text_box = (jd_UITextBox){ .string = string, .max = max_string_size};
    config.label_alignment = alignment;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIInputTextBoxAlignedWithHint(jd_String string_id, jd_UIShape shape, jd_String* string, jd_String hint, u64 max_string_size, jd_V2F alignment) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_Clickable|jd_UIBoxFlags_TextEdit|jd_UIBoxFlags_StaticColor;
    config.string_id  = string_id;
    config.shape = shape;
    config.cursor = jd_Cursor_Default;
    config.text_box = (jd_UITextBox){ .string = string, .max = max_string_size, .hint = hint};
    config.label_alignment = alignment;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIInputTextBoxWithHint(jd_String string_id, jd_UIShape shape, jd_String* string, jd_String hint, u64 max_string_size) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_Clickable|jd_UIBoxFlags_TextEdit|jd_UIBoxFlags_StaticColor;
    config.string_id  = string_id;
    config.shape = shape;
    config.cursor = jd_Cursor_Default;
    config.text_box = (jd_UITextBox){ .string = string, .max = max_string_size, .hint = hint };
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIInputTextBoxMultiline(jd_String string_id, jd_UIShape shape, jd_String* string, jd_String hint, u64 max_string_size) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_Clickable|jd_UIBoxFlags_TextEdit|jd_UIBoxFlags_Multiline;
    config.string_id = string_id;
    config.shape = shape;
    config.cursor = jd_Cursor_Default;
    config.text_box = (jd_UITextBox){ .string = string, .max = max_string_size, .hint = hint };
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIRowGrowBegin(jd_String string_id, jd_V2F padding, f32 gap, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags;
    config.flags |= jd_UIBoxFlags_StaticColor;
    config.string_id  = string_id;
    jd_UILayout layout = jd_UIMakeLayout(jd_UILayout_LeftToRight, gap);
    jd_UIShape  shape  = jd_UIMakeShapeEx(jd_UIGrow, jd_UIFit, padding.x, padding.y, 0.0f, 0.0f, 0.0f, 0.0f); 
    config.layout = layout;
    config.shape = shape;
    config.cursor = jd_Cursor_Default;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    
    return result;
}

jd_UIResult jd_UIColGrowBegin(jd_String string_id, jd_V2F padding, f32 gap, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags;
    config.flags |= jd_UIBoxFlags_StaticColor;
    config.string_id = string_id;
    jd_UILayout layout = jd_UIMakeLayout(jd_UILayout_TopToBottom,gap);
    jd_UIShape  shape  = jd_UIMakeShapeEx(jd_UIFit, jd_UIGrow, padding.x, padding.y, 0.0f, 0.0f, 0.0f, 0.0f); 
    config.layout = layout;
    config.shape = shape;
    config.cursor = jd_Cursor_Default;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    
    return result;
}

jd_UIResult jd_UIWindowRegionBegin(jd_V2F min_size, jd_UILayoutDir dir, f32 gap) {
    jd_UIBoxConfig config = {0};
    config.string_id = jd_StrLit("jd_internal_global_window_parent");
    config.flags |= jd_UIBoxFlags_StaticColor|jd_UIBoxFlags_InvisibleBG;
    jd_UILayout layout = jd_UIMakeLayout(dir, gap); 
    jd_UIShape  shape  = jd_UIMakeShape(jd_UIGrowMin(min_size.x), jd_UIGrowMin(min_size.y));
    config.cursor = jd_Cursor_Default;
    config.shape = shape;
    config.layout = layout;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    
    return result;
}

jd_UIResult jd_UIFixedPadding(jd_String string_id, jd_V2F fixed_size) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_InvisibleBG;
    config.flags |= jd_UIBoxFlags_StaticColor;
    config.string_id = string_id;
    jd_UIShape shape = jd_UIMakeShape(jd_UIFixed(fixed_size.x), jd_UIFixed(fixed_size.y));
    config.shape = shape;
    config.cursor = jd_Cursor_Default;
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    return result;
}

jd_UIResult jd_UIGrowPadding(jd_String string_id) {
    jd_UIBoxConfig config = {0};
    config.flags = 0;
    config.flags |= jd_UIBoxFlags_StaticColor|jd_UIBoxFlags_InvisibleBG;
    config.cursor = jd_Cursor_Default;
    config.string_id = string_id;
    jd_UIShape shape = jd_UIMakeShape(jd_UIGrow, jd_UIGrow);
    config.shape = shape;
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    return result;
}

jd_UIResult jd_UIImage(jd_String string_id, jd_UIShape shape, jd_Texture texture, b32 maintain_aspect_ratio) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_Textured|jd_UIBoxFlags_InvisibleBG;
    config.texture = texture;
    config.string_id = string_id;
    config.shape = shape;
    config.cursor = jd_Cursor_Default;
    config.maintain_texture_aspect_ratio = maintain_aspect_ratio;
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    return result;
}

jd_UIBoxRec* jd_UIGetLastBox() {
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    return vp->builder_last_box;
}

void jd_UIResetScroll(jd_UIBoxRec* b) {
    b->scroll_max.x = 0;
    b->scroll_max.y = 0;
    b->scroll.x = 0;
    b->scroll.y = 0;
}

jd_UIBoxRec* jd_UIGetDebugBox() {
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    return vp->debug_box;
}

void jd_UIRegionEnd() {
    jd_UIBoxEnd();
}

jd_V4F jd_UIColorHexToV4F(u32 rgba) {
    u8 r = (rgba >> 24) & 0xFF;
    u8 g = (rgba >> 16) & 0xFF;
    u8 b = (rgba >>  8) & 0xFF;
    u8 a = rgba & 0xff;
    
    f32 rf = (f32)r / 255;
    f32 gf = (f32)g / 255;
    f32 bf = (f32)b / 255;
    f32 af = (f32)a / 255;
    
    jd_V4F ret = {rf, gf, bf, af};
    return ret;
}