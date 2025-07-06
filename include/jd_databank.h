/* date = June 13th 2024 9:52 pm */

#ifndef JD_FS_H
#define JD_FS_H

#ifndef JD_UNITY_H
#include "jd_sysinfo.h"
#include "jd_helpers.h"
#include "jd_hash.h"
#include "jd_locks.h"
#include "jd_error.h"
#include "jd_unicode.h"
#include "jd_data_structures.h"
#include "jd_string.h"
#include "jd_file.h"
#include "jd_disk.h"
#endif

typedef enum jd_DataType {
    jd_DataType_None = 0,
    jd_DataType_String =  1 << 0,
    jd_DataType_Bin =     1 << 1,
    jd_DataType_u64 =     1 << 2,
    jd_DataType_u32 =     1 << 3,
    jd_DataType_b32 =     1 << 4,
    jd_DataType_c8  =     1 << 5,
    jd_DataType_i64 =     1 << 6,
    jd_DataType_i32 =     1 << 7,
    jd_DataType_f32 =     1 << 8,
    jd_DataType_f64 =     1 << 9,
    jd_DataType_Record =  1 << 10,
    jd_DataType_Root   =  1 << 11,
    jd_DataType_Count
} jd_DataType;

typedef struct jd_Value {
    jd_DataType type;
    union {
        jd_String string;
        jd_View   bin;
        u64 U64;
        u32 U32;
        b8  B32;
        c8  C8;
        i64 I64;
        i32 I32;
        f32 F32;
        f64 F64;
    };
} jd_Value;

typedef struct jd_DataNodeOptions {
    jd_String display;
} jd_DataNodeOptions;

typedef struct jd_DataNode {
    struct jd_DataBank* bank;
    
    jd_RWLock* lock;
    
    jd_String key;
    jd_Value  value;
    
    jd_String display;
    
    u32 slot_taken;
    
    struct jd_DataNode* next_with_same_hash;
    
    jd_Node(jd_DataNode);
} jd_DataNode;

typedef struct jd_DataBank {
    jd_Arena* arena;
    jd_RWLock* lock;
    
    u64 primary_key_index;
    
    jd_DataType disabled_types;
    
    jd_DataNode* primary_key_hash_table;
    u64           primary_key_hash_table_slot_count;
    
    jd_DataNode* root;
} jd_DataBank;

typedef struct jd_DataBankConfig {
    jd_Arena* arena;
    jd_String name;
    jd_DataType disabled_types; // |= types to this flag to disable them
    u64 total_memory_cap;
    u64 primary_key_hash_table_slot_count;
    u64 primary_key_index;
} jd_DataBankConfig;

typedef enum jd_DataFilterRule {
    jd_FilterRule_None,
    jd_FilterRule_GreaterThan,
    jd_FilterRule_LessThan,
    jd_FilterRule_GreaterThanOrEq,
    jd_FilterRule_LessThanOrEq,
    jd_FilterRule_Equals, 
    jd_FilterRule_DoesNotEqual,
    jd_FilterRule_Contains,
    jd_FilterRule_DoesNotContain,
    jd_FilterRule_Count
} jd_DataFilterRule;

typedef struct jd_DataFilter {
    jd_String key;
    jd_Value  value;
    jd_DataFilterRule rule;
    jd_Node(jd_DataFilter);
} jd_DataFilter;

jd_ExportFn jd_DataFilter* jd_DataFilterCreate(jd_Arena* arena, jd_String key);
jd_ExportFn jd_DataFilter* jd_DataFilterPush(jd_Arena* arena, jd_DataFilter* parent, jd_String key, jd_Value value, jd_DataFilterRule rule);
jd_ExportFn b32                 jd_DataFilterEvaluate(jd_DataFilter* filter, jd_DataNode* n, b32 case_sensitive);

jd_ExportFn jd_DataBank*  jd_DataBankCreate(jd_DataBankConfig* config);
jd_ExportFn jd_DFile*     jd_DataBankSerialize(jd_DataBank* bank);
jd_ExportFn jd_DataBank*  jd_DataBankDeserialize(jd_File view);

jd_ExportFn jd_DataNode*   jd_DataBankGetRoot(jd_DataBank* bank);

jd_ExportFn jd_DataNode*   jd_DataBankAddRecord(jd_DataNode* parent, jd_String key);
jd_ExportFn jd_DataNode*   jd_DataBankAddRecordWithPK(jd_DataNode* parent, jd_String key, u64 primary_key);
jd_ExportFn jd_DataNode*   jd_DataPointSet(jd_DataNode* record, jd_String key, jd_Value value);
jd_ExportFn jd_DataNode*   jd_DataPointGet(jd_DataNode* record, jd_String key);
jd_ExportFn jd_Value       jd_DataPointGetValue(jd_DataNode* record, jd_String key);
jd_ExportFn jd_DataNode*   jd_DataBankGetRecordWithID(jd_DataBank* bank, u64 primary_key);
jd_ExportFn jd_DataNode*   jd_DataBankCopySubtree(jd_Arena arena, jd_DataNode* subtree_root);

jd_ExportFn jd_ForceInline jd_Value jd_ValueCastString(jd_String string);
jd_ExportFn jd_ForceInline jd_Value jd_ValueCastBin(jd_View view);
jd_ExportFn jd_ForceInline jd_Value jd_ValueCastU64(u64 val);
jd_ExportFn jd_ForceInline jd_Value jd_ValueCastU32(u32 val);
jd_ExportFn jd_ForceInline jd_Value jd_ValueCastB32(b32 val);
jd_ExportFn jd_ForceInline jd_Value jd_ValueCastC8(c8 val);
jd_ExportFn jd_ForceInline jd_Value jd_ValueCastI64(i64 val);
jd_ExportFn jd_ForceInline jd_Value jd_ValueCastI32(i32 val);
jd_ExportFn jd_ForceInline jd_Value jd_ValueCastF32(f32 val);
jd_ExportFn jd_ForceInline jd_Value jd_ValueCastF64(f64 val);

jd_ExportFn jd_ForceInline jd_String jd_ValueString(jd_Value v);
jd_ExportFn jd_ForceInline jd_View   jd_ValueBin(jd_Value v);
jd_ExportFn jd_ForceInline u64       jd_ValueU64(jd_Value v);
jd_ExportFn jd_ForceInline u32       jd_ValueU32(jd_Value v);
jd_ExportFn jd_ForceInline b32       jd_ValueB32(jd_Value v);
jd_ExportFn jd_ForceInline c8        jd_ValueC8 (jd_Value v);
jd_ExportFn jd_ForceInline i64       jd_ValueI64(jd_Value v);
jd_ExportFn jd_ForceInline i32       jd_ValueI32(jd_Value v);
jd_ExportFn jd_ForceInline f32       jd_ValueF32(jd_Value v);
jd_ExportFn jd_ForceInline f64       jd_ValueF64(jd_Value v);

#ifdef JD_IMPLEMENTATION
#include "jd_databank.c"
#endif


#endif //JD_FS_H
