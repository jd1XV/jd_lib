#include "jd_xml.h"

static void jd_XMLParserSkipWhitespace(jd_XMLParser* parser) {
    while (parser->index < parser->input.count && (parser->input.mem[parser->index] == ' '  ||
                                                   parser->input.mem[parser->index] == '\n' ||
                                                   parser->input.mem[parser->index] == '\r' ||
                                                   parser->input.mem[parser->index] == '\t' )) {
        parser->index++;
    }
}

static b32 jd_XMLParserSeekChars(jd_XMLParser* parser, jd_String break_on, b32 skip_whitespace) {
    if (skip_whitespace) jd_XMLParserSkipWhitespace(parser);
    parser->token.mem = parser->input.mem + parser->index;
    parser->token.count = 0;
    
    while (parser->index < parser->input.count) {
        for (u64 i = 0; i < break_on.count; i++) {
            if (parser->input.mem[parser->index] == break_on.mem[i]) {
                parser->matched_char = break_on.mem[i];
                return true;
            }
        }
        
        parser->index++;
        parser->token.count++;
    }
    
    return false;
}

static void jd_XMLParserGetNextKey(jd_XMLParser* parser) {
    jd_XMLParserSkipWhitespace(parser);
    parser->token.mem = parser->input.mem + parser->index;
    parser->token.count = 0;
    
    if (parser->token.mem[0] == '/') {
        parser->index++;
        parser->token.count++;
    }
    
    while (parser->index < parser->input.count && 
           parser->input.mem[parser->index] != ' ' &&
           parser->input.mem[parser->index] != '>' &&
           parser->input.mem[parser->index] != '/') {
        parser->index++;
        parser->token.count++;
    }
    
    parser->matched_char = parser->input.mem[parser->index];
}

static jd_String jd_XMLGetAttributeKey(jd_XMLParser* parser) {
    jd_String ret = {0};
    jd_XMLParserSkipWhitespace(parser);
    
    if (parser->index >= parser->input.count || 
        parser->input.mem[parser->index] == '/' || 
        parser->input.mem[parser->index] == '>' ||
        parser->input.mem[parser->index] == '?') return ret;
    
    parser->token.mem = parser->input.mem + parser->index;
    parser->token.count = 0;
    
    while (parser->index < parser->input.count && 
           parser->input.mem[parser->index] != '=') {
        parser->index++;
        parser->token.count++;
    }
    
    parser->index++; //skip the equals
    
    return parser->token;
}

static jd_String jd_XMLGetAttributeVal(jd_XMLParser* parser) {
    jd_String ret = {0};
    while (parser->index < parser->input.count && parser->input.mem[parser->index] != '"') {
        parser->index++;
    }
    
    parser->index++; // skip the quote itself
    
    if (parser->index >= parser->input.count) return ret;
    
    parser->token.mem = parser->input.mem + parser->index;
    parser->token.count = 0;
    while (parser->index < parser->input.count && 
           parser->input.mem[parser->index] != '"') {
        parser->index++;
        parser->token.count++;
    }
    
    parser->index++; // skip the ending quote
    
    return parser->token;
}


static jd_XMLAttr jd_XMLParseAttribute(jd_XMLParser* parser) {
    jd_XMLAttr attr = {0};
    jd_String key = jd_XMLGetAttributeKey(parser);
    if (key.count == 0) return attr;
    jd_String val = jd_XMLGetAttributeVal(parser);
    if (val.count == 0) return attr;
    
    attr.key = key;
    attr.val = val;
    return attr;
}

jd_String jd_XMLStringPushEscaped(jd_Arena* arena, jd_String str) {
    const jd_String escape_sequences[] = {
        jd_StrConst("&quot;"),
        jd_StrConst("&apos;"),
        jd_StrConst("&lt;"),
        jd_StrConst("&gt;"),
        jd_StrConst("&amp;")
    };
    
    const c8 escape_chars[] = {
        '"',
        '\'',
        '<',
        '>',
        '&'
    };
    
    u64 escape_sequences_count = jd_ArrayCount(escape_sequences);
    u64 escape_chars_count = jd_ArrayCount(escape_chars);
    jd_Assert(escape_sequences_count == escape_chars_count);
    
    jd_String new_str = {
        .mem = jd_ArenaAlloc(arena, str.count + 1),
        .count = 0
    };
    
    u64 copy_index = 0;
    
    for (u64 i = 0; i < str.count; i++) {
        u64 match_char = 0;
        for (u64 j = 0; j < escape_sequences_count; j++) {
            if (i + escape_sequences[j].count <= str.count) {
                if (jd_MemCmp(escape_sequences[j].mem, &str.mem[i], escape_sequences[j].count)) {
                    match_char = j;
                    i += escape_sequences[j].count - 1;
                    break;
                }
            }
        }
        
        if (match_char > 0) {
            new_str.mem[copy_index++] = escape_chars[match_char];
            new_str.count++;
        } else {
            new_str.mem[copy_index++] = str.mem[i];
            new_str.count++;
        }
    }
    
    return new_str;
}

jd_XMLAttr* jd_XMLAttrPushBack(jd_XMLNode* node, jd_XMLAttr* in) {
    if (!in) return NULL;
    
    jd_XMLAttr* attr = jd_ArenaAlloc(node->arena, sizeof(jd_XMLAttr));
    
    if (!node->attr) node->attr = attr;
    else {
        jd_XMLAttr* a = node->attr;
        while (a->next != 0) {
            a = a->next;
        }
        
        jd_DLinkNext(a, attr);
    }
    
    attr->key = jd_XMLStringPushEscaped(node->arena, in->key);
    attr->val = jd_XMLStringPushEscaped(node->arena, in->val);
    
    return attr;
}

jd_XMLNode* jd_XMLNodePushEmpty(jd_XMLNode* parent) {
    if (!parent) return NULL;
    
    jd_XMLNode* child = jd_ArenaAlloc(parent->arena, sizeof(jd_XMLNode));
    child->arena = parent->arena;
    child->parent = parent;
    child->decl = parent->decl;
    
    jd_TreeLinkLastChild(parent, child);
    return child;
}


jd_XMLNode* jd_XMLTreeFromString(jd_XMLParser* parser, jd_Arena* arena, jd_String xml) {
    jd_XMLNode* root = jd_ArenaAlloc(arena, sizeof(jd_XMLNode));
    root->arena = arena;
    parser->input = xml;
    parser->index = 0;
    parser->unclosed_tags = jd_DArrayCreate(256, sizeof(jd_String));
    parser->node = root;
    parser->root = root;
    parser->arena = arena;
    parser->matched_char = 0;
    jd_String break_str = jd_StrLit("<");
    
    while (jd_XMLParserSeekChars(parser, jd_StrLit("<"), false)) {
        if (parser->token.count > 0) {
            parser->node->text = jd_XMLStringPushEscaped(arena, parser->token);
        }
        
        parser->index++;
        jd_XMLParserGetNextKey(parser);
        
        // declaration parsing
        if (root->decl == NULL && jd_StringMatch(parser->token, jd_StrLit("?xml"))) {
            root->decl = jd_ArenaAlloc(arena, sizeof(jd_XMLDecl));
            jd_XMLAttr attr = {0};
            attr = jd_XMLParseAttribute(parser);
            while (attr.key.count > 0 && attr.val.count > 0) {
                if (jd_StringMatch(attr.key, jd_StrLit("version"))) {
                    root->decl->version = jd_XMLStringPushEscaped(arena, attr.val);
                }
                
                else if (jd_StringMatch(attr.key, jd_StrLit("encoding"))) {
                    root->decl->encoding = jd_XMLStringPushEscaped(arena, attr.val);
                }
                
                attr = jd_XMLParseAttribute(parser);
            }
            
            jd_XMLParserSeekChars(parser, jd_StrLit("?"), true);
            jd_XMLParserSeekChars(parser, jd_StrLit(">"), true);
            parser->index++;
        }
        
        else if (jd_StringBeginsWith(parser->token, jd_StrLit("/"))) {
            jd_String trunc_str = parser->token;
            trunc_str.mem++;
            trunc_str.count--;
            jd_String* last_tag = jd_DArrayGetBack(parser->unclosed_tags);
            if (jd_StringMatch(trunc_str, *last_tag)) {
                jd_DArrayPopBack(parser->unclosed_tags);
                parser->node = parser->node->parent;
            } else {
                jd_DArrayRelease(parser->unclosed_tags);
                return NULL;
            }
            
            jd_XMLParserSeekChars(parser, jd_StrLit(">"), true);
            parser->index++;
        }
        
        else {
            jd_String key = parser->token;
            if (key.count == 0) {
                jd_LogError("Unable to parse a key from the XML input file", jd_Error_BadInput, jd_Error_Common);
                parser->error = true;
                break;
            }
            
            jd_XMLNode* child = jd_XMLNodePushEmpty(parser->node);
            child->key = jd_XMLStringPushEscaped(arena, key);
            if (parser->matched_char == ' ') {
                jd_XMLAttr attr = {0};
                attr = jd_XMLParseAttribute(parser);
                while (attr.key.count > 0 && attr.val.count > 0) {
                    jd_XMLAttrPushBack(child, &attr);
                    attr = jd_XMLParseAttribute(parser);
                }
            }
            
            jd_XMLParserSeekChars(parser, jd_StrLit("/>"), true);
            if (parser->matched_char == '/') {
                child->is_empty = true;
            } else {
                jd_DArrayPushBack(parser->unclosed_tags, &child->key);
                parser->node = child;
            }
            
            jd_XMLParserSeekChars(parser, jd_StrLit(">"), true);
            parser->index++;
        }
        
    }
    
    if (parser->unclosed_tags->count > 0) {
        jd_LogError("Missing closing tag in XML file", jd_Error_BadInput, jd_Error_Common);
        parser->error = true;
        //jd_XML2SetError(parser->root, XML_MISSING_CLOSING_TAG, parser->index);
    }
    
    if (root->first_child == NULL) {
        jd_LogError("No tree generated by XML input file", jd_Error_BadInput, jd_Error_Common);
        parser->error = true;
    }
    
    jd_DArrayRelease(parser->unclosed_tags);
    
    return root;
}

jd_XMLNode* jd_XMLGetFirstNodeWithKey(jd_XMLNode* node, jd_String string) {
    jd_XMLNode* n = node;
    while (n) {
        if (jd_StringMatch(n->key, string)) {
            return n;
        }
        jd_TreeTraversePreorder(n);
    }
    
    return n;
}

jd_XMLNode* jd_XMLGetNextNodeWithKey(jd_XMLNode* node, jd_String string) {
    jd_XMLNode* n = node;
    if (n && jd_StringMatch(n->key, string)) {
        jd_TreeTraversePreorder(n);
    }
    
    while (n) {
        if (jd_StringMatch(n->key, string)) {
            return n;
        }
        
        jd_TreeTraversePreorder(n);
    }
    
    return n;
}

jd_XMLNode* jd_XMLGetFirstChildWithKey(jd_XMLNode* parent, jd_String string) {
    jd_XMLNode* n = parent->first_child;
    while (n) {
        if (jd_StringMatch(n->key, string)) {
            return n;
        }
        
        n = n->next;
    }
    
    return n;
}

jd_XMLNode* jd_XMLGetNextSiblingWithKey(jd_XMLNode* node, jd_String string) {
    jd_XMLNode* n = node;
    if (n && jd_StringMatch(n->key, string)) {
        n = n->next;
    }
    
    while (n) {
        if (jd_StringMatch(n->key, string)) {
            return n;
        }
        
        n = n->next;
    }
    
    return n;
}
