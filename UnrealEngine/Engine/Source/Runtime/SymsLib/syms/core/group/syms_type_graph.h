// Copyright Epic Games, Inc. All Rights Reserved.
/* date = February 16th 2022 1:40 pm */

#ifndef SYMS_TYPE_GRAPH_H
#define SYMS_TYPE_GRAPH_H

////////////////////////////////
// NOTE(allen): Syms Type Graph

typedef struct SYMS_TypeMember{
  SYMS_MemKind kind;
  SYMS_MemVisibility visibility;
  SYMS_MemFlags flags;
  SYMS_String8 name;
  SYMS_U32 off;
  SYMS_U32 virtual_off;
  struct SYMS_TypeNode *type;
} SYMS_TypeMember;

typedef struct SYMS_TypeSrcCoord{
  SYMS_USID usid;
  SYMS_FileID file_id;
  SYMS_U32 line;
  SYMS_U32 col;
} SYMS_TypeSrcCoord;

typedef struct SYMS_TypeMemberArray{
  SYMS_TypeMember *mems;
  SYMS_U64 count;
} SYMS_TypeMemberArray;

typedef struct SYMS_TypeNode{
  // SYMS_TypeNode extends and completes the information from SYMS_TypeInfo.
  // See SYMS_TypeInfo for more interpretation info.
  
  SYMS_TypeKind kind;
  SYMS_String8 name;
  SYMS_U64 byte_size;
  
  // when non-null contains the source location of the type's definition
  SYMS_TypeSrcCoord *src_coord;
  
  // (in addition to interpretations of SYMS_TypeInfo 'direct_type')
  //  SYMS_TypeKind_Forward*   -> the concrete type referenced by the forward reference
  struct SYMS_TypeNode *direct_type;
  
  // 'this_type' meaning depends on kind:
  //  SYMS_TypeKind_MemberPtr  -> the container type of the member pointer
  //  SYMS_TypeKind_Proc       -> if non-nil this is the type of an implicit 'this' in a C++ method
  struct SYMS_TypeNode *this_type;
  
  union{
    // kind: SYMS_TypeKind_Modifier
    SYMS_TypeModifiers mods;
    
    // kind: SYMS_TypeKind_Array
    SYMS_U64 array_count;
    
    // kind: SYMS_TypeKind_Proc
    struct{
      struct SYMS_TypeNode **params;
      SYMS_U64 param_count;
    } proc;
    
    // opaque pointer for lazy eval attachments to the type node.
    void *lazy_ptr;
  };
} SYMS_TypeNode;

typedef struct SYMS_TypeContentNode{
  struct SYMS_TypeContentNode *next;
  SYMS_String8 key;
  SYMS_U64 hash;
  SYMS_TypeNode *type;
} SYMS_TypeContentNode;

typedef struct SYMS_TypeContentBuckets{
  SYMS_TypeContentNode **buckets;
  SYMS_U64 bucket_count;
} SYMS_TypeContentBuckets;

typedef struct SYMS_TypeUSIDNode{
  struct SYMS_TypeUSIDNode *next;
  SYMS_USID key;
  SYMS_TypeNode *type;
} SYMS_TypeUSIDNode;

typedef struct SYMS_TypeUSIDBuckets{
  SYMS_TypeUSIDNode **buckets;
  SYMS_U64 bucket_count;
} SYMS_TypeUSIDBuckets;

typedef struct SYMS_TypeChainNode{
  struct SYMS_TypeChainNode *next;
  SYMS_TypeNode *type;
} SYMS_TypeChainNode;

typedef struct SYMS_TypeChain{
  SYMS_TypeChainNode *first;
  SYMS_TypeChainNode *last;
  SYMS_U64 count;
} SYMS_TypeChain;

typedef struct SYMS_TypeNameNode{
  struct SYMS_TypeNameNode *next;
  SYMS_U8 *name_ptr;
  SYMS_TypeChain chain;
} SYMS_TypeNameNode;

typedef struct SYMS_TypeNameBuckets{
  SYMS_TypeNameNode **buckets;
  SYMS_U64 bucket_count;
} SYMS_TypeNameBuckets;

typedef struct SYMS_TypeGraph{
  SYMS_Arena *arena;
  SYMS_StringCons *string_cons;
  SYMS_U64 address_size;
  SYMS_TypeUSIDBuckets type_usid_buckets;
  SYMS_TypeContentBuckets type_content_buckets;
  SYMS_TypeNameBuckets type_name_buckets;
} SYMS_TypeGraph;

////////////////////////////////
// NOTE(allen): Nil Type Node

SYMS_READ_ONLY SYMS_GLOBAL SYMS_TypeNode syms_type_graph_nil = {
  SYMS_TypeKind_Null,     // kind
  {(SYMS_U8*)"(nil)", 5}, // name
  0,                      // byte_size
  0,                      // src_coord
  &syms_type_graph_nil,   // direct_type
  &syms_type_graph_nil,   // this_type
};

////////////////////////////////
// NOTE(allen): Type Graph Setup Functions

// The graph_arena is a "permanent arena" that is, it is used
// for all memory allocations on the graph. It should not be
// popped or released until the graph is no longer in use. It
// should not be a scratch arena.
// Similarly with the string cons structure.

SYMS_API void            syms_type_graph_init(SYMS_TypeGraph *graph,
                                              SYMS_Arena *graph_arena,
                                              SYMS_StringCons *graph_string_cons,
                                              SYMS_U64 address_size);

////////////////////////////////
// NOTE(allen): Type Content Hash Functions

SYMS_API SYMS_U64       syms_type_content_hash(SYMS_String8 data);
SYMS_API SYMS_TypeNode* syms_type_from_content(SYMS_TypeContentBuckets *buckets, SYMS_String8 data);
SYMS_API SYMS_String8   syms_type_content_buckets_insert(SYMS_Arena *arena, SYMS_TypeContentBuckets *buckets,
                                                         SYMS_String8 key, SYMS_TypeNode *type);

////////////////////////////////
// NOTE(allen): Type USID Hash Functions

SYMS_API SYMS_U64       syms_type_usid_hash(SYMS_USID usid);
SYMS_API SYMS_TypeNode* syms_type_from_usid(SYMS_TypeUSIDBuckets *buckets,
                                            SYMS_USID usid);
SYMS_API void           syms_type_usid_buckets_insert(SYMS_Arena *arena,
                                                      SYMS_TypeUSIDBuckets *buckets,
                                                      SYMS_USID key,
                                                      SYMS_TypeNode *type);

////////////////////////////////
// NOTE(allen): Type Name Hash Functions

SYMS_API SYMS_U64        syms_type_name_hash(SYMS_U8 *ptr);
SYMS_API SYMS_TypeChain* syms_type_chain_from_name(SYMS_TypeNameBuckets *buckets,
                                                   SYMS_U8 *name_ptr);
SYMS_API void            syms_type_name_buckets_insert(SYMS_Arena *arena,
                                                       SYMS_TypeNameBuckets *buckets,
                                                       SYMS_U8 *name_ptr, SYMS_TypeNode *type);

////////////////////////////////
// NOTE(allen): Type Name Lookup Functions

SYMS_API SYMS_TypeChain syms_type_from_name(SYMS_TypeGraph *graph, SYMS_String8 name);

////////////////////////////////
// NOTE(allen): Type Node Constructors

SYMS_API SYMS_TypeNode* syms_type_basic(SYMS_TypeGraph *graph,
                                        SYMS_TypeKind basic_kind, SYMS_U64 size,
                                        SYMS_String8 name);
SYMS_API SYMS_TypeNode* syms_type_mod_from_type(SYMS_TypeGraph *graph,
                                                SYMS_TypeNode *type, SYMS_TypeModifiers mods);
SYMS_API SYMS_TypeNode* syms_type_ptr_from_type(SYMS_TypeGraph *graph,
                                                SYMS_TypeKind ptr_kind, SYMS_TypeNode *type);
SYMS_API SYMS_TypeNode* syms_type_array_from_type(SYMS_TypeGraph *graph,
                                                  SYMS_TypeNode *type, SYMS_U64 count);
SYMS_API SYMS_TypeNode* syms_type_proc_from_type(SYMS_TypeGraph *graph,
                                                 SYMS_TypeNode *ret_type,
                                                 SYMS_TypeNode *this_type,
                                                 SYMS_TypeNode **params, SYMS_U64 count);
SYMS_API SYMS_TypeNode* syms_type_member_ptr_from_type(SYMS_TypeGraph *graph,
                                                       SYMS_TypeNode *container,
                                                       SYMS_TypeNode *type);

////////////////////////////////
// NOTE(allen): Type Info Operators

SYMS_API SYMS_TypeNode* syms_type_resolved(SYMS_TypeNode *type);
SYMS_API SYMS_B32       syms_type_node_match(SYMS_TypeNode *l, SYMS_TypeNode *r);

SYMS_API SYMS_TypeNode* syms_type_resolve_enum_to_basic(SYMS_TypeGraph *graph, SYMS_TypeNode *t);

SYMS_API SYMS_TypeNode* syms_type_promoted_from_type_node(SYMS_TypeGraph *graph,
                                                          SYMS_TypeNode *c);
SYMS_API SYMS_TypeNode* syms_type_auto_casted_from_type_nodes(SYMS_TypeGraph *graph,
                                                              SYMS_TypeNode *l,
                                                              SYMS_TypeNode *r);

////////////////////////////////
// NOTE(allen): Type Stringizing

SYMS_API SYMS_String8   syms_string_from_type(SYMS_Arena *arena, SYMS_TypeNode *type);

SYMS_API void           syms_lhs_string_from_type(SYMS_Arena *arena, SYMS_TypeNode *type,
                                                  SYMS_String8List *out);
SYMS_API void           syms_rhs_string_from_type(SYMS_Arena *arena, SYMS_TypeNode *type,
                                                  SYMS_String8List *out);

SYMS_API void           syms_lhs_string_from_type_skip_return(SYMS_Arena *arena,
                                                              SYMS_TypeNode *type,
                                                              SYMS_String8List *out);

SYMS_API void           syms_lhs_string_from_type__internal(SYMS_Arena *arena,
                                                            SYMS_TypeNode *type,
                                                            SYMS_String8List *out,
                                                            SYMS_U32 prec, SYMS_B32 skip);
SYMS_API void           syms_rhs_string_from_type__internal(SYMS_Arena *arena,
                                                            SYMS_TypeNode *type,
                                                            SYMS_String8List *out,
                                                            SYMS_U32 prec);

#endif //SYMS_TYPE_GRAPH_H
