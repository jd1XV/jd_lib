typedef struct jd_Internal_UIState {
    jd_Arena* arena;
    
    jd_UIBoxRec* box_array;
    u64          box_array_size;
    jd_UIBoxRec* box_free_slot;
    
    jd_UIViewport viewports[256];
    u64 viewport_count;
    u64 active_viewport;
    
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
    jd_DArrayPushBack(_jd_internal_ui_state.style_stack, &ui_style);
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
    if (!parent) {
        parent = *(jd_UIBoxRec**)jd_DArrayGetBack(_jd_internal_ui_state.parent_stack);
    }
    
    if (!parent) {
        parent = vp->root_new;
    }
    
    jd_UIStyle* style = config->style;
    
    if (!style) {
        style = *(jd_UIStyle**)jd_DArrayGetBack(_jd_internal_ui_state.style_stack);
    }
    
    if (!style) {
        style = &jd_default_style_dark;
        jd_LogError("No style specified! Using default style", jd_Error_APIMisuse, jd_Error_Warning);
    }
    
    b8 act_on_click = config->act_on_click;
    b8 clickable    = config->clickable;
    
    jd_UITag tag = jd_UITagFromString(config->string_id);
    jd_UIBoxRec* b = jd_UIBoxGetByTag(tag);
    
    b->vp = jd_UIViewportGetCurrent();
    
    jd_UIResult result = {0};
    result.box = b;
    
    jd_V4F color         = style->bg_color;
    jd_V4F hovered_color = style->bg_color_hovered;
    jd_V4F active_color  = style->bg_color_active;
    
    // input handling
    if (!config->disabled) {
        if (b == vp->hot) {
            result.hovered = true;
            if (!config->static_color)
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
            
            if (!config->static_color)
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
    
    b->style = style;
    b->fixed_position = config->fixed_position;
    b->label = config->label;
    b->label_alignment = config->label_alignment;
    b->color = color;
    b->layout = &config->layout;
    b->layout_axis = ((b->layout->dir == jd_UILayout_LeftToRight) || b->layout->dir == jd_UILayout_RightToLeft) ? jd_UIAxis_X : jd_UIAxis_Y;
    b->size = config->size;
    b->font_id = jd_DArrayGetBack(_jd_internal_ui_state.font_id_stack);
    b->cursor = config->cursor;
    
    jd_TreeLinkLastChild(parent, b);
    
    jd_UIParentPush(b);
    
    return result;
}

void jd_UIBoxEnd() {
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    
    jd_UIBoxRec** bp = jd_DArrayGetBack(_jd_internal_ui_state.parent_stack);
    jd_UIBoxRec* b = *bp;
    
    jd_UIParentPop();
}

jd_UIViewport* jd_UIBegin(jd_UIViewport* viewport) {
    if (!_jd_internal_ui_state.arena) { // not yet initialized
        jd_LogError("Call jd_UIInitForWindow() before jd_UIBegin", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    jd_UIViewport* vp = viewport;
    vp->size = jd_WindowGetDrawSize(viewport->window);
    
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
    
    vp->position_offset = jd_WindowGetDrawOrigin(viewport->window);
    
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
    
    
    
    jd_UISeedPushPtr(vp->window);
    return vp;
}


void jd_UI_Internal_SolveFixedSizes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    while (b != 0) {
        if (b->size.rule[axis] == jd_UISizeRule_Fixed) {
            b->rect.max.val[axis] = b->size.fixed_size.val[axis];
        }
        
        else if (b->size.rule[axis] == jd_UISizeRule_FitText) {
            b->rect.max.val[axis] = jd_CalcStringSizeUTF8(*b->font_id, b->label, 0.0f).val[axis]; 
            b->rect.max.val[axis] += b->style->label_padding.val[axis];
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
        f32 allowed_size = b->rect.max.val[axis];
        
        for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next) {
            if (axis != b->layout_axis) {
                c->rect.max.val[axis] = jd_Min(c->rect.max.val[axis], allowed_size);
            }
            
            else {
                f32 requested_size = 0.0f;
                u32 grow_children = 0;
                
                for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next) {
                    requested_size += c->rect.max.val[axis];
                    if (c->size.rule[axis] == jd_UISizeRule_Grow) {
                        grow_children++;
                    }
                }
                
                f32 over = requested_size - allowed_size;
                if (over > 0) {
                    for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next) {
                        if (c->size.rule[axis] == jd_UISizeRule_Grow) {
                            c->rect.max.val[axis] -= (f32)(over / grow_children);
                            c->rect.max.val[axis] = jd_Max(c->rect.max.val[axis], 0);
                        }
                    }
                }
                
            }
            
        }
        
        for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next) {
            if (c->size.rule[axis] == jd_UISizeRule_PctParent) {
                c->rect.max.val[axis] = b->rect.max.val[axis] * c->size.pct_of_parent.val[axis];
            }
        }
        
        jd_TreeTraversePreorder(b);
    }
}

void jd_UI_Internal_PositionBoxes(jd_UIBoxRec* root, jd_UIAxis axis) {
    jd_UIBoxRec* b = root;
    jd_UIViewport* vp = b->vp;
    f32 position = vp->position_offset.val[axis];
    
    while (b) {
        if (b->parent) position = b->parent->rect.min.val[axis];
        
        for (jd_UIBoxRec* c = b->first_child; c != 0; c = c->next) {
            if (c->fixed_position.val[axis] == 0) {
                c->rect.min.val[axis] = position;
                if (axis == b->layout_axis)
                    position += c->rect.max.val[axis];
                
                c->rect.min.val[axis] = b->rect.min.val[axis] + c->rect.min.val[axis];
            } else {
                c->rect.min.val[axis] = c->fixed_position.val[axis];
            }
        }
        
        jd_TreeTraversePreorder(b);
    }
}

void jd_UI_Internal_RenderBoxes(jd_UIBoxRec* root) {
    jd_UIBoxRec* b = root;
    while (b) {
        jd_UIViewport* vp = b->vp;
        
        f64 dpi_sf = jd_WindowGetDPIScale(vp->window);
        
        jd_UIStyle* style = b->style;
        if (!style) style = &jd_default_style_dark;
        
        jd_V4F color        = b->color;
        
        f32 softness        = style->softness;
        f32 corner_radius   = style->rounding;
        f32 thickness       = style->thickness;
        
        jd_V2F rect_pos = b->rect.min;
        jd_V2F rect_size = b->rect.max;
        
        jd_DrawRect(rect_pos, rect_size, color, corner_radius, softness, thickness);
        if (b->label.count > 0) {
            jd_String* font_id = jd_DArrayGetBack(_jd_internal_ui_state.font_id_stack);
            jd_V2F string_pos = rect_pos;
            
            for (u32 i = 0; i < jd_UIAxis_Count; i++) {
                if (b->size.rule[i] == jd_UISizeRule_FitText)
                    string_pos.val[i] += b->style->label_padding.val[i] / 2.0f;
            }
            
            f32 style_scaling = dpi_sf;
            jd_V2F string_size = jd_CalcStringSizeUTF8(*font_id, b->label, 0.0f);
            string_pos.x += ((rect_size.x - string_size.x) * b->label_alignment.x);
            string_pos.y += ((rect_size.y - string_size.y) * b->label_alignment.y);
            jd_DrawString(*font_id, b->label, string_pos, jd_TextOrigin_TopLeft, style->label_color, 0.0f);
        }
        
        jd_TreeTraversePreorder(b);
    }
}

void jd_UIEnd() {
    jd_UISeedPop();
    if (!_jd_internal_ui_state.arena) {
        jd_LogError("jd_UIEnd() called before jd_UIBegin()", jd_Error_APIMisuse, jd_Error_Fatal);
    }
    
    // Solve sizes and positions
    
    jd_UIViewport* vp = jd_UIViewportGetCurrent();
    
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
        jd_UI_Internal_RenderBoxes(b);
    }
    
}

jd_UIViewport* jd_UIInitForWindow(jd_Window* window) {
    if (!_jd_internal_ui_state.arena) {
        _jd_internal_ui_state.arena = jd_ArenaCreate(GIGABYTES(2), KILOBYTES(4));
        _jd_internal_ui_state.box_array_size = jd_UIBox_HashTable_Size;
        _jd_internal_ui_state.box_array = jd_ArenaAlloc(_jd_internal_ui_state.arena, sizeof(jd_UIBoxRec) * jd_UIBox_HashTable_Size);
        _jd_internal_ui_state.seeds = jd_DArrayCreate(2048, sizeof(u32));
        _jd_internal_ui_state.font_id_stack = jd_DArrayCreate(2048, sizeof(jd_String));
        _jd_internal_ui_state.parent_stack = jd_DArrayCreate(256, sizeof(jd_UIBoxRec*));
        _jd_internal_ui_state.style_stack = jd_DArrayCreate(256, sizeof(jd_UIStyle*));
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

jd_UIResult jd_UILabelButton(jd_String label) {
    jd_UIBoxConfig config = {0};
    config.clickable = true;
    config.string_id = label;
    config.label = label;
    config.size.rule[jd_UIAxis_X] = jd_UISizeRule_FitText;
    config.size.rule[jd_UIAxis_Y] = jd_UISizeRule_FitText;
    config.cursor = jd_Cursor_Hand;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIFixedSizeButton(jd_String label, jd_V2F size, jd_V2F label_alignment) {
    jd_UIBoxConfig config = {0};
    config.clickable = true;
    config.string_id = label;
    config.label = label;
    config.size.rule[jd_UIAxis_X] = jd_UISizeRule_Fixed;
    config.size.rule[jd_UIAxis_Y] = jd_UISizeRule_Fixed;
    config.size.fixed_size = size;
    config.cursor = jd_Cursor_Hand;
    config.label_alignment = label_alignment;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIButton(jd_String label, jd_UISize size, b8 act_on_click, b8 static_color) {
    jd_UIBoxConfig config = {0};
    config.clickable = true;
    config.string_id = label;
    config.label = label;
    config.size = size;
    config.cursor = jd_Cursor_Hand;
    config.label_alignment = (jd_V2F){0.5f, 0.5f};
    config.act_on_click = act_on_click;
    config.static_color = static_color;
    jd_UIResult result = jd_UIBoxBegin(&config);
    jd_UIBoxEnd();
    
    return result;
}

jd_UIResult jd_UIRegionBegin(jd_String string_id, jd_UIStyle* style, jd_UISize size, jd_UILayoutDir dir, f32 gap, b8 clickable) {
    jd_UIBoxConfig config = {0};
    config.clickable = clickable;
    if (config.clickable)
        config.act_on_click = true;
    config.string_id  = string_id;
    config.layout.dir = dir;
    config.layout.gap = gap;
    config.size = size;
    config.static_color = true;
    config.cursor = jd_Cursor_Default;
    config.style = style;
    
    jd_UIResult result = jd_UIBoxBegin(&config);
    
    return result;
}

void jd_UIRegionEnd() {
    jd_UIBoxEnd();
}