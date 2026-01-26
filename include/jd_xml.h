/* date = January 18th 2026 1:06 am */

#ifndef JD_XML_H
#define JD_XML_H

typedef struct jd_XMLAttr {
    jd_String key;
    jd_String val;
    
    jd_Node(jd_XMLAttr);
} jd_XMLAttr;

typedef struct jd_XMLNode {
    jd_Arena* arena;
    
    jd_String key;
    jd_String text;
    
    jd_XMLAttr* attr; // doubly linked list
    
    struct jd_XMLDecl* decl;
    
    b32 is_empty;
    
    jd_Node(jd_XMLNode);
} jd_XMLNode;

typedef struct jd_XMLDecl {
    jd_String version;
    jd_String encoding;
} jd_XMLDecl;

typedef struct jd_XMLParser {
    jd_Arena* arena;
    jd_String input;
    u64 index;
    
    jd_String token;
    jd_DArray* unclosed_tags;
    
    jd_XMLNode* root;
    jd_XMLNode* node;
    
    b32 error;
    
    c8 matched_char;
    b8 closing_tag;
} jd_XMLParser;

jd_XMLNode* jd_XMLTreeFromString(jd_XMLParser* parser, jd_Arena* arena, jd_String xml);
jd_XMLNode* jd_XMLGetFirstNodeWithKey(jd_XMLNode* node, jd_String string);
jd_XMLNode* jd_XMLGetNextNodeWithKey(jd_XMLNode* node, jd_String string);
jd_XMLNode* jd_XMLGetFirstChildWithKey(jd_XMLNode* parent, jd_String string);
jd_XMLNode* jd_XMLGetNextSiblingWithKey(jd_XMLNode* node, jd_String string);

#endif //JD_XML_H
