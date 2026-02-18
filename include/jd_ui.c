jd_ReadOnly static u64 _jd_ui_internal_default_seed = 104720809;

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
    jd_DArray* font_id_stack; // jd_String*
    jd_DArray* style_stack; // jd_UIStyle*
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
        .key = hash
    };
    
    return t;
}

jd_ExportFn jd_ForceInline void jd_UISeedPushTag(jd_UITag tag) {
    jd_DArrayPushBack(_jd_internal_ui_state.seeds, &tag.key);
}

jd_ExportFn jd_ForceInline void jd_UISeedPushString(jd_String string) {
    u32* seed = jd_DArrayGetBack(_jd_internal_ui_state.seeds);
    if (!seed) jd_LogError("UI Error: Seed stack is empty!", jd_Error_APIMisuse, jd_Error_Fatal);
    u32 hash = jd_HashStrToU32(string, *seed);
    jd_DArrayPushBack(_jd_internal_ui_state.seeds, &hash);
}

jd_ExportFn jd_ForceInline void jd_UISeedPushU32(u32 v) {
    u32* seed = jd_DArrayGetBack(_jd_internal_ui_state.seeds);
    if (!seed) jd_LogError("UI Error: Seed stack is empty!", jd_Error_APIMisuse, jd_Error_Fatal);
    u32 hash = jd_HashU32toU32(v, *seed);
    jd_DArrayPushBack(_jd_internal_ui_state.seeds, &hash);
}

jd_ExportFn jd_ForceInline void jd_UISeedPushStringF(jd_String fmt, ...) {
    jd_ScratchArena s = jd_ScratchArenaCreate(_jd_internal_ui_state.arena);
    va_list list = {0};
    va_start(list, fmt);
    jd_String str = jd_StringPushVAList(s.arena, fmt, list);
    jd_UISeedPushString(str);
    jd_ScratchArenaRelease(s);
    va_end(list);
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

void jd_UIBoxPrune() {
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    for (u64 i = 0; i < _jd_internal_ui_state.box_array_size; i++) {
        jd_UIBoxRec* b = &_jd_internal_ui_state.box_array[i];
        
        if (b->tag.key != 0 && b->last_touched < (jd_AppCurrentFrame(vp->window->app) - 2)) {
            jd_ZeroMemory(b, sizeof(*b));
        }
    }
}


jd_ExportFn void jd_UIMemoryCleanup() {
    jd_UIBoxPrune();
};

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

void jd_UIPickActiveBox(jd_UIViewport* vp) {
    jd_InputSliceForEach(i, vp->old_inputs) {
        jd_InputEvent* e = jd_DArrayGetIndex(vp->old_inputs.array, i);
        if (!e) { // Sanity Check
            jd_LogError("Incorrect input slice range!", jd_Error_APIMisuse, jd_Error_Fatal);
        }
        
        i64 delta = 0;
        
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
                
                
            } break;
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

void jd_UIParentPush(jd_UIBoxRec* box) {
    jd_DArrayPushBack(_jd_internal_ui_state.parent_stack, &box);
}

void jd_UIParentPop() {
    if (_jd_internal_ui_state.parent_stack->count == 0) {
        jd_LogError("Mismatched calls to jd_UIParentPop()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    jd_DArrayPopBack(_jd_internal_ui_state.parent_stack);
}

void jd_UIStylePush(jd_UIStyle* ui_style) {
    jd_DArrayPushBack(_jd_internal_ui_state.style_stack, ui_style);
}

void jd_UIStylePop() {
    if (_jd_internal_ui_state.style_stack->count == 0) {
        jd_LogError("Mismatched calls to jd_UIStylePop()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    jd_DArrayPopBack(_jd_internal_ui_state.style_stack);
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
    
    jd_UIStyle* style = config->style;
    
    if (!style) {
        style = jd_DArrayGetBack(_jd_internal_ui_state.style_stack);
    }
    
    if (!style) {
        style = &jd_default_style_dark;
        jd_LogError("No style specified! Using default style", jd_Error_APIMisuse, jd_Error_Warning);
    }
    
    b8 act_on_click = (config->flags & jd_UIBoxFlags_ActOnClick);
    b8 clickable    = (config->flags & jd_UIBoxFlags_Clickable);
    
    jd_UITag tag = jd_UITagFromString(config->string_id);
    jd_UIBoxRec* b = jd_UIBoxGetByTag(tag);
    
    jd_TreeLinksClear(b);
    
    b->vp = jd_UIViewportGetCurrent();
    
    jd_UIResult result = {0};
    result.box = b;
    result.last_active = (b == vp->last_active);
    
    jd_V4F color         = style->bg_color;
    jd_V4F hovered_color = style->bg_color_hovered;
    jd_V4F active_color  = style->bg_color_active;
    
    
    if (b->anchor_box) {
        parent = vp->popup_root_new;
    }
    
    b->draw_index = vp->box_count;
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
            }
        }
        
    }
    
    b->style = *style;
    b->flags = config->flags;
    b->text_box.string  = config->text_box.string;
    if (config->text_box.string && config->text_box.string->count < b->text_box.cursor) b->text_box.cursor = 0;
    b->last_touched = jd_AppCurrentFrame(vp->window->app);
    b->text_box.max = config->text_box.max;
    b->fixed_position = config->fixed_position;
    b->anchor_box = config->anchor_box;
    b->label = config->label;
    b->label_alignment = config->label_alignment;
    b->color = color;
    b->anchor_reference_point = config->anchor_reference_point;
    b->string_id = config->string_id;
    b->layout_gap = config->layout_gap;
    b->layout_axis = ((config->layout_dir == jd_UILayout_LeftToRight) || config->layout_dir == jd_UILayout_RightToLeft) ? jd_UIAxis_X : jd_UIAxis_Y;
    b->size = config->size;
    b->font_id = *(jd_String*)jd_DArrayGetBack(_jd_internal_ui_state.font_id_stack);
    b->cursor = config->cursor;
    
    jd_TreeLinkLastChild(parent, b);
    
    vp->builder_last_box = b;
    
    jd_UIParentPush(b);
    jd_UISeedPushTag(b->tag);
    
    return result;
}

void jd_UIBoxEnd() {
    jd_UISeedPop();
    jd_UIParentPop();
}

jd_UIViewport* jd_UIBegin(jd_UIViewport* viewport) {
    if (!_jd_internal_ui_state.arena) { // not yet initialized
        jd_LogError("Call jd_UIInitForWindow() before jd_UIBegin", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    jd_UIViewport* vp = viewport;
    vp->size = jd_WindowGetDrawSize(viewport->window);
    jd_WindowSetMinimumSize(vp->window, vp->minimum_size);
    
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
    
    vp->box_count = 0;
    
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
    
    if (jd_AppWindowIsActive(viewport->window)) {
        vp->hot = jd_UIPickBoxForPos(vp, mouse_pos);
        if (vp->hot) jd_AppSetCursor(vp->hot->cursor);
        jd_UIPickActiveBox(vp);
    }
    
    jd_UISeedPushU32(_jd_internal_ui_state.active_viewport);
    
    return vp;
}


void jd_UI_Internal_SolveFixedSizes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    while (b != 0) {
        if (b->size.rule[axis] == jd_UISizeRule_Fixed) {
            b->rect.max.val[axis] = b->size.fixed_size.val[axis];
        }
        
        else if (b->size.rule[axis] == jd_UISizeRule_FitText) {
            b->rect.max.val[axis] = jd_CalcStringSizeUTF8(b->font_id, b->label, 0.0f).val[axis]; 
            b->rect.max.val[axis] += b->style.label_padding.val[axis];
        }
        
        jd_TreeTraversePreorder(b);
    }
}

void jd_UI_Internal_SolveGrowSizes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    while (b != 0) {
        if (b->size.rule[axis] == jd_UISizeRule_Grow) {
            jd_UIBoxRec* p = b->parent;
            while (p != 0 && p->size.rule[axis] != jd_UISizeRule_Fixed && p->size.rule[axis] != jd_UISizeRule_Grow) {
                p = p->parent;
            }
            
            if (!p) {
                b->rect.max.val[axis] = b->vp->size.val[axis];
            } else {
                b->rect.max.val[axis] = p->rect.max.val[axis];
            }
            
        }
        jd_TreeTraversePreorder(b);
    }
}

void jd_UI_Internal_SolvePctParentSizes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    while (b != 0) {
        if (b->size.rule[axis] == jd_UISizeRule_PctParent) {
            jd_UIBoxRec* p = b->parent;
            while (p != 0 && 
                   p->size.rule[axis] != jd_UISizeRule_Fixed && 
                   p->size.rule[axis] != jd_UISizeRule_PctParent) {
                p = p->parent;
            }
            
            if (!p) {
                b->rect.max.val[axis] = b->vp->size.val[axis] * b->size.pct_of_parent.val[axis];
            } else {
                b->rect.max.val[axis] = p->rect.max.val[axis] * b->size.pct_of_parent.val[axis];
            }
        }
        
        jd_TreeTraversePreorder(b);
    }
}

void jd_UI_Internal_SolveFitChildrenSizes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    jd_TreeGetPostorderRoot(b);
    
    while (b != 0) {
        if (b->size.rule[axis] == jd_UISizeRule_FitChildren) {
            jd_UIBoxRec* c = b->first_child;
            f32 size = 0.0;
            for (; c != 0; c = c->next) {
                if (b->layout_axis == axis)
                    size += c->rect.max.val[axis];
                else
                    size = jd_Max(c->rect.max.val[axis], size);
            }
            
            b->rect.max.val[axis] = size;
        }
        jd_TreeTraversePostorder(b);
    }
}

void jd_UI_Internal_SolveConflicts(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    while (b) {
        f32 size = 0.0f;
        f32 allowed_size = b->rect.max.val[axis];
        b->rect.max.val[axis] += b->style.padding.val[axis] * 2;
        
        u32 child_count = 0;
        for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next, child_count++);
        if (axis == b->layout_axis) {
            allowed_size -= (b->layout_gap * (child_count - 1));
        }
        
        for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next) {
            if (axis != b->layout_axis) {
                if (c->flags & jd_UIBoxFlags_MatchSiblingsOffAxis) {
                    c->rect.max.val[axis] = allowed_size;
                } else {
                    c->rect.max.val[axis] = jd_Min(c->rect.max.val[axis], allowed_size);
                }
                
                size = jd_Max(c->rect.max.val[axis], size);
            }
            
            else {
                f32 requested_size = 0.0f;
                u32 grow_children = 0;
                
                for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next) {
                    if (!c->anchor_box) {
                        requested_size += c->rect.max.val[axis];
                    }
                    
                    if (c->size.rule[axis] == jd_UISizeRule_Grow) {
                        grow_children++;
                    }
                }
                
                f32 over = requested_size - allowed_size;
                if (over > 0 && !b->anchor_box) {
                    for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next) {
                        if (c->size.rule[axis] == jd_UISizeRule_Grow) {
                            c->rect.max.val[axis] -= (f32)(over / grow_children);
                            c->rect.max.val[axis] = jd_Max(c->rect.max.val[axis], 0);
                        }
                    }
                }
            }
            
            if (c->size.min_size.val[axis] > 0.0f) {
                c->rect.max.val[axis] = jd_Max(c->size.min_size.val[axis], c->rect.max.val[axis]);
            }
            size += c->rect.max.val[axis];
            
        }
        
        for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next) {
            if (c->size.rule[axis] == jd_UISizeRule_PctParent) {
                c->rect.max.val[axis] = b->rect.max.val[axis] * c->size.pct_of_parent.val[axis];
            }
        }
        
        f32 diff = (size - allowed_size);
        b->scroll_max.val[axis] = diff ? diff : 0;
        
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
                if (c != b->first_child) {
                    if (axis == b->layout_axis) {
                        c->rect.min.val[axis] += b->layout_gap;
                    }
                } else {
                    c->rect.min.val[axis] += b->style.padding.val[axis];
                }
                
                if (axis == b->layout_axis)
                    position += c->rect.max.val[axis] + b->layout_gap;
                else {
                    position = jd_Max(position, b->style.padding.val[axis]);
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

void jd_UI_Internal_RenderBoxes(jd_UIBoxRec* root, b32 clip_parent) {
    jd_UIBoxRec* b = root;
    jd_UIViewport* vp = b->vp;
    f64 dpi_sf = jd_WindowGetDPIScale(vp->window);
    
    while (b) {
        jd_UIStyle style = b->style;
        
        f32 z = (root == vp->popup_root_new) ? 0.0 : (1.0 / (b->draw_index + 1));
        
        jd_V4F color        = b->color;
        
        f32 softness        = style.softness;
        f32 corner_radius   = style.rounding;
        f32 thickness       = style.thickness;
        
        jd_V3F rect_pos = {b->rect.min.x, b->rect.min.y, z};
        jd_V2F rect_size = b->rect.max;
        
        jd_RectF32 clipping_rectangle_self = {
            .min = b->rect.min,
            .max = {b->rect.min.x + b->rect.max.x, b->rect.min.y + b->rect.max.y}
        };
        
        jd_RectF32 clipping_rectangle_parent = {0};
        if (clip_parent && b->parent) {
            clipping_rectangle_parent.min = b->parent->rect.min;
            clipping_rectangle_parent.max = (jd_V2F){b->parent->rect.min.x + b->parent->rect.max.x, b->parent->rect.min.y + b->parent->rect.max.y};
        } else {
            clipping_rectangle_parent = clipping_rectangle_self;
        }
        
        jd_DrawRectWithZ(rect_pos, rect_size, color, corner_radius, softness, thickness, clipping_rectangle_parent);
        if (b->flags & jd_UIBoxFlags_TextEdit) {
            jd_V2F string_size = jd_CalcStringSizeUTF8(b->font_id, *b->text_box.string, b->rect.max.x);
            
            jd_V2F string_pos = {rect_pos.x, rect_pos.y};
            for (u32 i = 0; i < jd_UIAxis_Count; i++) {
                if (b->size.rule[i] == jd_UISizeRule_FitText)
                    string_pos.val[i] += b->style.label_padding.val[i] / 2.0f;
            }
            
            if (b->flags & jd_UIBoxFlags_ScrollX) {
                string_pos.x -= b->scroll.x;
                b->scroll_max.x = string_size.x - rect_size.x;
            }
            
            if (b->flags & jd_UIBoxFlags_ScrollY) {
                string_pos.y -= b->scroll.y;
                b->scroll_max.y = string_size.y - rect_size.y;
            }
            
            jd_V3F final_pos = {string_pos.x, string_pos.y, z};
            jd_DrawStringWithZ(b->font_id, *b->text_box.string, final_pos, jd_TextOrigin_TopLeft, style.label_color, rect_size.x, clipping_rectangle_self);
            if (b == vp->last_active) {
                jd_V2F cur_pos_v2 = jd_CalcCursorPosUTF8(b->font_id, *b->text_box.string, b->rect.max.x, b->text_box.cursor);
                
                if (b->flags & jd_UIBoxFlags_ScrollX) cur_pos_v2.x -= b->scroll.x;
                if (b->flags & jd_UIBoxFlags_ScrollY) cur_pos_v2.y -= b->scroll.y;
                
                jd_V3F cur_pos = {rect_pos.x + cur_pos_v2.x, rect_pos.y + cur_pos_v2.y, z};
                b->text_box.last_cursor_pos = (jd_V2F){cur_pos_v2.x, cur_pos_v2.y};
                jd_V2F cur_size = {2.0f, jd_FontGetLineHeightForCodepoint(b->font_id, 41)};
                jd_DrawRectWithZ(cur_pos, cur_size, (jd_V4F){1.0f, 1.0f, 1.0f, 1.0f}, 0.0, 0.0, 0.0, clipping_rectangle_self);
            }
        }
        else if (b->label.count > 0) {
            jd_V2F string_pos = {rect_pos.x, rect_pos.y};
            
            for (u32 i = 0; i < jd_UIAxis_Count; i++) {
                if (b->size.rule[i] == jd_UISizeRule_FitText)
                    string_pos.val[i] += b->style.label_padding.val[i] / 2.0f;
            }
            
            jd_V2F string_size = jd_CalcStringSizeUTF8(b->font_id, b->label, rect_size.x);
            string_pos.x += ((rect_size.x - string_size.x) * b->label_alignment.x);
            string_pos.y += ((rect_size.y - string_size.y) * b->label_alignment.y);
            
            jd_V3F final_pos = {string_pos.x, string_pos.y, z};
            jd_DrawStringWithZ(b->font_id, b->label, final_pos, jd_TextOrigin_TopLeft, style.label_color, 0.0f, clipping_rectangle_parent);
        }
        
        b->rect = jd_RectClip(b->rect, clipping_rectangle_parent);
        
        jd_TreeTraversePreorder(b);
    }
}

void jd_UIEnd() {
    u32 last_codepoint = 41; // Used to infer line height for cursor movement
    jd_UISeedPop();
    if (!_jd_internal_ui_state.arena) {
        jd_LogError("jd_UIEnd() called before jd_UIBegin()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    // Solve sizes and positions
    
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    
    f32 scroll_step = 30.0f * jd_WindowGetDPIScale(vp->window);
    
    jd_InputSliceForEach(i, vp->old_inputs) {
        jd_InputEvent* e = jd_DArrayGetIndex(vp->old_inputs.array, i);
        if (!e) { // Sanity Check
            jd_LogError("Incorrect input slice range!", jd_Error_APIMisuse, jd_Error_Fatal);
        }
        
        b8 text_edit_active = (vp->last_active && (vp->last_active->flags & jd_UIBoxFlags_TextEdit));
        
        i64 delta = 0;
        
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
                        delta -= 5;
                    } else {
                        delta -= 1;
                    }
                } break;
                
                case jd_Key_Right: {
                    if (jd_InputHasMod(e, jd_KeyMod_Ctrl)) {
                        delta += 5;
                    } else {
                        delta += 1;
                    }
                } break;
                
                case jd_Key_Back: 
                case jd_Key_Delete: {
                    jd_UI_Internal_DeleteCodepoint(&b->text_box);
                } break;
                
                case jd_Key_Up: 
                case jd_Key_Down: {
                    f32 line_adv = jd_FontGetLineAdvForCodepoint(b->font_id, last_codepoint);
                    
                    if (e->key == jd_Key_Up)
                        line_adv = -line_adv;
                    
                    jd_V2F current_pos = b->text_box.last_cursor_pos;
                    jd_V2F new_pos = {current_pos.x + b->scroll.x, current_pos.y + b->scroll.y + line_adv};
                    u64 cursor = b->text_box.cursor;
                    u64 new_cursor = jd_CalcCursorIndexUTF8(b->font_id, *b->text_box.string, b->rect.max.x, new_pos);
                    i64 delta = new_cursor - cursor;
                    
                    jd_UnicodeSeekDeltaUTF8(*b->text_box.string, &cursor, delta);
                    b->text_box.cursor = cursor;
                } break;
                
                case jd_Key_Return: {
                    jd_UI_Internal_InsertCodepoint(&b->text_box, 0x0A);
                } break;
                
                case jd_Key_Tab: {
                    //jd_UI_Internal_InsertCodepoint(&b->text_box, 0x09);
                    if (!(b->flags & jd_UIBoxFlags_ScrollY))
                        break;
                    if (jd_InputHasMod(e, jd_KeyMod_Shift)) {
                        b->scroll.y -= 25.0f;
                    } else {
                        b->scroll.y += 25.0f;
                    }
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
        for (u64 i = 0; i < jd_UIAxis_Count; i++) {
            
            jd_UI_Internal_SolveFixedSizes(b, i);
            jd_UI_Internal_SolveGrowSizes(b, i);
            jd_UI_Internal_SolvePctParentSizes(b, i);
            jd_UI_Internal_SolveFitChildrenSizes(b, i);
            jd_UI_Internal_SolveConflicts(b, i);
            jd_UI_Internal_PositionBoxes(b, i);
        }
        jd_UI_Internal_RenderBoxes(b, true);
    }
    
    {
        jd_UIBoxRec* b = vp->popup_root_new;
        for (u64 i = 0; i < jd_UIAxis_Count; i++) {
            
            jd_UI_Internal_SolveFixedSizes(b, i);
            jd_UI_Internal_SolvePctParentSizes(b, i);
            jd_UI_Internal_SolveFitChildrenSizes(b, i);
            jd_UI_Internal_SolveConflicts(b, i);
            jd_UI_Internal_PositionBoxes(b, i);
        }
        jd_UI_Internal_RenderBoxes(b, false);
    }
    
    {
        jd_UIBoxRec* b = vp->root_new;
        while (b) {
            vp->minimum_size.x = jd_Max(b->size.min_size.x, vp->minimum_size.x);
            vp->minimum_size.y = jd_Max(b->size.min_size.y, vp->minimum_size.y);
            jd_TreeTraversePreorder(b);
        }
    }
    
    jd_UIBoxPrune();
}

jd_UIViewport* jd_UIInitForWindow(jd_Window* window) {
    if (!_jd_internal_ui_state.arena) {
        _jd_internal_ui_state.arena = jd_ArenaCreate(GIGABYTES(2), KILOBYTES(4));
        _jd_internal_ui_state.box_array_size = jd_UIBox_HashTable_Size;
        _jd_internal_ui_state.box_array = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec) * jd_UIBox_HashTable_Size);
        _jd_internal_ui_state.seeds = jd_DArrayCreate(2048, sizeof(u32));
        jd_DArrayPushBack(_jd_internal_ui_state.seeds, &_jd_ui_internal_default_seed);
        _jd_internal_ui_state.font_id_stack = jd_DArrayCreate(2048, sizeof(jd_String));
        _jd_internal_ui_state.parent_stack = jd_DArrayCreate(256, sizeof(jd_UIBoxRec*));
        _jd_internal_ui_state.style_stack = jd_DArrayCreate(256, sizeof(jd_UIStyle));
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
    
    vp->root->vp = vp;
    vp->popup_root->vp = vp;
    vp->titlebar_root->vp = vp;
    
    vp->root_new->vp = vp;
    vp->popup_root_new->vp = vp;
    vp->titlebar_root_new->vp = vp;
    
    vp->roots_init = true;
    return vp;
}

//~ User-facing functions

jd_UIResult jd_UILabel(jd_String label) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_StaticColor;
    config.string_id = _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    config.size.rule[jd_UIAxis_X] = jd_UISizeRule_FitText;
    config.size.rule[jd_UIAxis_Y] = jd_UISizeRule_FitText;
    config.cursor = jd_Cursor_Default;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UILabelButton(jd_String label) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_Clickable;
    config.string_id = _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    config.size.rule[jd_UIAxis_X] = jd_UISizeRule_FitText;
    config.size.rule[jd_UIAxis_Y] = jd_UISizeRule_FitText;
    config.cursor = jd_Cursor_Hand;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIListButton(jd_String label) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_Clickable|jd_UIBoxFlags_MatchSiblingsOffAxis;
    config.string_id = _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    config.size.rule[jd_UIAxis_X] = jd_UISizeRule_FitText;
    config.size.rule[jd_UIAxis_Y] = jd_UISizeRule_FitText;
    config.cursor = jd_Cursor_Hand;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIFixedSizeButton(jd_String label, jd_V2F size, jd_V2F label_alignment) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_Clickable;
    config.string_id =  _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    config.size.rule[jd_UIAxis_X] = jd_UISizeRule_Fixed;
    config.size.rule[jd_UIAxis_Y] = jd_UISizeRule_Fixed;
    config.size.fixed_size = size;
    config.cursor = jd_Cursor_Hand;
    config.label_alignment = label_alignment;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIButton(jd_String label, jd_UISize size, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags;
    config.flags |= jd_UIBoxFlags_Clickable;
    config.string_id =  _jd_UIStringGetHashPart(label);
    config.label = _jd_UIStringGetDisplayPart(label);
    config.size = size;
    config.cursor = jd_Cursor_Hand;
    config.label_alignment = (jd_V2F){0.5f, 0.5f};
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIRegionBegin(jd_String string_id, jd_UIStyle* style, jd_UISize size, jd_UILayoutDir dir, f32 gap, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags;
    config.flags |= jd_UIBoxFlags_StaticColor;
    config.string_id  = string_id;
    config.layout_dir = dir;
    config.layout_gap = gap;
    config.size = size;
    config.cursor = jd_Cursor_Default;
    config.style = style;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    
    return result;
}

jd_UIResult jd_UIRegionBeginAnchored(jd_String string_id, jd_UIStyle* style, jd_UIBoxRec* anchor_box, jd_V2F anchor_to, jd_V2F anchor_from, jd_UISize size, jd_UILayoutDir dir, f32 gap, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags;
    config.flags |= jd_UIBoxFlags_StaticColor;
    config.string_id  = string_id;
    config.layout_dir = dir;
    config.layout_gap = gap;
    config.anchor_box = anchor_box;
    config.anchor_reference_point = anchor_to;
    config.reference_point = anchor_from;
    config.size = size;
    config.cursor = jd_Cursor_Default;
    config.style = style;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    
    return result;
}

jd_UIResult jd_UIInputTextBox(jd_String string_id, jd_String* string, u64 max_string_size, jd_UIStyle* style, jd_UISize size) {
    jd_UIBoxConfig config = {0};
    config.flags = jd_UIBoxFlags_Clickable|jd_UIBoxFlags_TextEdit|jd_UIBoxFlags_StaticColor|jd_UIBoxFlags_ScrollY;
    config.string_id  = string_id;
    config.size = size;
    config.cursor = jd_Cursor_Default;
    config.style = style;
    config.text_box = (jd_UITextBox){ .string = string, .max = max_string_size };
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIRowGrowBegin(jd_String string_id, jd_UIStyle* style, f32 gap, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags;
    config.flags |= jd_UIBoxFlags_StaticColor;
    config.string_id  = string_id;
    config.layout_dir = jd_UILayout_LeftToRight;
    config.layout_gap = gap;
    config.size = (jd_UISize){.rule = {jd_UISizeRule_Grow, jd_UISizeRule_FitChildren}};
    config.cursor = jd_Cursor_Default;
    config.style = style;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    
    return result;
}

jd_UIResult jd_UIColGrowBegin(jd_String string_id, jd_UIStyle* style, f32 gap, jd_UIBoxFlags flags) {
    jd_UIBoxConfig config = {0};
    config.flags = flags;
    config.flags |= jd_UIBoxFlags_StaticColor;
    config.string_id = string_id;
    config.layout_dir = jd_UILayout_TopToBottom;
    config.layout_gap = gap;
    config.size = (jd_UISize){.rule = {jd_UISizeRule_FitChildren, jd_UISizeRule_Grow}};
    config.cursor = jd_Cursor_Default;
    config.style = style;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    
    return result;
}

jd_UIResult jd_UIWindowRegionBegin(jd_V2F min_size, jd_UIStyle* style, jd_UILayoutDir dir, f32 gap) {
    jd_UIBoxConfig config = {0};
    config.string_id = jd_StrLit("jd_internal_global_window_parent");
    config.flags = 0;
    config.size = (jd_UISize) {
        .rule = {jd_UISizeRule_Grow, jd_UISizeRule_Grow},
        .min_size = min_size
    };
    config.flags |= jd_UIBoxFlags_StaticColor;
    config.layout_dir = dir;
    config.layout_gap = gap;
    config.cursor = jd_Cursor_Default;
    config.style = style;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    
    return result;
}

jd_UIResult jd_UIGrowPadding(jd_String string_id) {
    jd_UIBoxConfig config = {0};
    config.string_id = string_id;
    config.flags = 0;
    config.size = (jd_UISize){jd_UISizeRule_Grow, jd_UISizeRule_Grow};
    config.flags |= jd_UIBoxFlags_StaticColor;
    config.cursor = jd_Cursor_Default;
    config.style = &jd_invisible_style;
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    return result;
}

jd_UIBoxRec* jd_UIGetLastBox() {
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    return vp->builder_last_box;
}

void jd_UIRegionEnd() {
    jd_UIBoxEnd();
}