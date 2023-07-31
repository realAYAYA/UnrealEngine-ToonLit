// Copyright Epic Games, Inc. All Rights Reserved.

#ifndef SYMS_TYPE_GRAPH_C
#define SYMS_TYPE_GRAPH_C

////////////////////////////////
// NOTE(allen): Type Graph Setup Functions

SYMS_API void
syms_type_graph_init(SYMS_TypeGraph *graph,
                     SYMS_Arena *graph_arena,
                     SYMS_StringCons *graph_string_cons,
                     SYMS_U64 address_size){
  SYMS_ProfBegin("syms_type_graph_init");
  
  graph->arena = graph_arena;
  graph->string_cons = graph_string_cons;
  graph->address_size = address_size;
  
  graph->type_usid_buckets.bucket_count = 4093;
  graph->type_usid_buckets.buckets = syms_push_array_zero(graph->arena, SYMS_TypeUSIDNode*, 4093);
  
  graph->type_content_buckets.bucket_count = 4093;
  graph->type_content_buckets.buckets = syms_push_array_zero(graph->arena, SYMS_TypeContentNode*, 4093);
  
  graph->type_name_buckets.bucket_count = 4093;
  graph->type_name_buckets.buckets = syms_push_array_zero(graph->arena, SYMS_TypeNameNode*, 4093);
  
  SYMS_ProfEnd();
}

////////////////////////////////
// NOTE(allen): Type Content Hash Functions

SYMS_API SYMS_U64
syms_type_content_hash(SYMS_String8 data){
  SYMS_U64 result = syms_hash_djb2(data);
  return(result);
}

SYMS_API SYMS_TypeNode*
syms_type_from_content(SYMS_TypeContentBuckets *buckets, SYMS_String8 data){
  SYMS_ProfBegin("syms_type_from_content");
  SYMS_TypeNode *result = 0;
  if (buckets->bucket_count != 0){
    SYMS_U64 hash = syms_type_content_hash(data);
    SYMS_U64 bucket_index = hash%buckets->bucket_count;
    for (SYMS_TypeContentNode *node = buckets->buckets[bucket_index];
         node != 0;
         node = node->next){
      if (node->hash == hash && syms_string_match(node->key, data, 0)){
        result = node->type;
        break;
      }
    }
  }
  SYMS_ProfEnd();
  return(result);
}

SYMS_API SYMS_String8
syms_type_content_buckets_insert(SYMS_Arena *arena, SYMS_TypeContentBuckets *buckets,
                                 SYMS_String8 key, SYMS_TypeNode *type){
  SYMS_ProfBegin("syms_type_content_buckets_insert");
  SYMS_U64 hash = syms_type_content_hash(key);
  SYMS_String8 result = {0};
  if (buckets->bucket_count > 0){
    SYMS_U64 bucket_index = hash%buckets->bucket_count;
    
    result = syms_push_string_copy(arena, key);
    
    SYMS_TypeContentNode *new_node = syms_push_array(arena, SYMS_TypeContentNode, 1);
    SYMS_StackPush(buckets->buckets[bucket_index], new_node);
    new_node->key = result;
    new_node->hash = hash;
    new_node->type = type;
  }
  SYMS_ProfEnd();
  return(result);
}

////////////////////////////////
// NOTE(allen): Type USID Hash Functions

SYMS_API SYMS_U64
syms_type_usid_hash(SYMS_USID usid){
  SYMS_U64 result = syms_hash_u64(usid.sid + usid.uid*97);
  return(result);
}

SYMS_API SYMS_TypeNode*
syms_type_from_usid(SYMS_TypeUSIDBuckets *buckets, SYMS_USID usid){
  SYMS_ProfBegin("syms_type_from_usid");
  SYMS_TypeNode *result = 0;
  if (buckets->bucket_count > 0){
    SYMS_U64 hash = syms_type_usid_hash(usid);
    SYMS_U64 bucket_index = hash%buckets->bucket_count;
    for (SYMS_TypeUSIDNode *node = buckets->buckets[bucket_index];
         node != 0;
         node = node->next){
      SYMS_USID key = node->key;
      if (key.uid == usid.uid && key.sid == usid.sid){
        result = node->type;
        break;
      }
    }
  }
  SYMS_ProfEnd();
  return(result);
}

SYMS_API void
syms_type_usid_buckets_insert(SYMS_Arena *arena, SYMS_TypeUSIDBuckets *buckets,
                              SYMS_USID key,
                              SYMS_TypeNode *type){
  SYMS_ProfBegin("syms_type_usid_buckets_insert");
  if (buckets->bucket_count > 0){
    SYMS_U64 hash = syms_type_usid_hash(key);
    SYMS_U64 bucket_index = hash%buckets->bucket_count;
    SYMS_TypeUSIDNode *new_node = syms_push_array(arena, SYMS_TypeUSIDNode, 1);
    SYMS_StackPush(buckets->buckets[bucket_index], new_node);
    new_node->key = key;
    new_node->type = type;
  }
  SYMS_ProfEnd();
}

////////////////////////////////
// NOTE(allen): Type Name Hash Functions

SYMS_API SYMS_U64
syms_type_name_hash(SYMS_U8 *ptr){
  SYMS_U64 u64 = (SYMS_U64)(ptr);
  SYMS_U64 bits = u64 >> 3;
  SYMS_U64 result = bits ^ (bits << 32);
  return(result);
}

SYMS_API SYMS_TypeChain*
syms_type_chain_from_name(SYMS_TypeNameBuckets *buckets, SYMS_U8 *name_ptr){
  SYMS_TypeChain *result = 0;
  SYMS_ProfBegin("syms_type_chain_from_name");
  if (buckets->bucket_count > 0){
    SYMS_U64 hash = syms_type_name_hash(name_ptr);
    SYMS_U64 bucket_index = hash%buckets->bucket_count;
    for (SYMS_TypeNameNode *node = buckets->buckets[bucket_index];
         node != 0;
         node = node->next){
      if (node->name_ptr == name_ptr){
        result = &node->chain;
        break;
      }
    }
  }
  SYMS_ProfEnd();
  return(result);
}

SYMS_API void
syms_type_name_buckets_insert(SYMS_Arena *arena, SYMS_TypeNameBuckets *buckets,
                              SYMS_U8 *name_ptr, SYMS_TypeNode *type){
  SYMS_ProfBegin("syms_type_chain_from_name");
  if (buckets->bucket_count > 0){
    // find existing name node
    SYMS_TypeNameNode *match = 0;
    SYMS_U64 hash = syms_type_name_hash(name_ptr);
    SYMS_U64 bucket_index = hash%buckets->bucket_count;
    for (SYMS_TypeNameNode *node = buckets->buckets[bucket_index];
         node != 0;
         node = node->next){
      if (node->name_ptr == name_ptr){
        match = node;
        break;
      }
    }
    
    // new name node if necessary
    if (match == 0){
      SYMS_TypeNameNode *new_node = syms_push_array_zero(arena, SYMS_TypeNameNode, 1);
      SYMS_StackPush(buckets->buckets[bucket_index], new_node);
      new_node->name_ptr = name_ptr;
      
      match = new_node;
    }
    
    // new type chain node
    {
      SYMS_TypeChainNode *new_chain = syms_push_array(arena, SYMS_TypeChainNode, 1);
      SYMS_QueuePush(match->chain.first, match->chain.last, new_chain);
      match->chain.count += 1;
      new_chain->type = type;
    }
  }
  SYMS_ProfEnd();
}

////////////////////////////////
// NOTE(allen): Type Name Lookup Functions

SYMS_API SYMS_TypeChain
syms_type_from_name(SYMS_TypeGraph *graph, SYMS_String8 name){
  SYMS_TypeChain result = {0};
  SYMS_String8 name_cons = syms_string_cons(graph->arena, graph->string_cons, name);
  SYMS_TypeChain *chain = syms_type_chain_from_name(&graph->type_name_buckets, name_cons.str);
  if (chain != 0){
    syms_memmove(&result, chain, sizeof(*chain));
  }
  return(result);
}

////////////////////////////////
// NOTE(allen): Type Node Constructors

SYMS_API SYMS_TypeNode*
syms_type_basic(SYMS_TypeGraph *graph, SYMS_TypeKind basic_kind,
                SYMS_U64 size, SYMS_String8 name){
  SYMS_ProfBegin("syms_type_basic");
  
  SYMS_TypeNode *result = &syms_type_graph_nil;
  
  SYMS_String8 name_cons = syms_string_cons(graph->arena, graph->string_cons, name);
  
  SYMS_U64 content[3];
  content[0] = basic_kind;
  content[1] = size;
  content[2] = (SYMS_U64)name_cons.str;
  SYMS_String8 data = {(SYMS_U8*)content, sizeof(content)};
  result = syms_type_from_content(&graph->type_content_buckets, data);
  
  if (result == 0){
    result = syms_push_array_zero(graph->arena, SYMS_TypeNode, 1);
    syms_type_content_buckets_insert(graph->arena, &graph->type_content_buckets, data, result);
    syms_type_name_buckets_insert(graph->arena, &graph->type_name_buckets,
                                  name_cons.str, result);
    
    result->kind = basic_kind;
    result->name = name_cons;
    result->byte_size = size;
    result->direct_type = &syms_type_graph_nil;
    result->this_type = &syms_type_graph_nil;
  }
  
  SYMS_ProfEnd();
  return(result);
}

SYMS_API SYMS_TypeNode*
syms_type_mod_from_type(SYMS_TypeGraph *graph, SYMS_TypeNode *type, SYMS_TypeModifiers mods){
  SYMS_ProfBegin("syms_type_mod_from_type");
  
  SYMS_TypeNode *result = &syms_type_graph_nil;
  
  SYMS_U64 content[3];
  content[0] = SYMS_TypeKind_Modifier;
  content[1] = mods;
  content[2] = (SYMS_U64)type;
  SYMS_String8 data = {(SYMS_U8*)content, sizeof(content)};
  result = syms_type_from_content(&graph->type_content_buckets, data);
  
  if (result == 0){
    result = syms_push_array_zero(graph->arena, SYMS_TypeNode, 1);
    syms_type_content_buckets_insert(graph->arena, &graph->type_content_buckets, data, result);
    
    result->kind = SYMS_TypeKind_Modifier;
    result->byte_size = type->byte_size;
    result->direct_type = type;
    result->this_type = &syms_type_graph_nil;
    result->mods = mods;
  }
  
  SYMS_ProfEnd();
  return(result);
}

SYMS_API SYMS_TypeNode*
syms_type_ptr_from_type(SYMS_TypeGraph *graph, SYMS_TypeKind ptr_kind, SYMS_TypeNode *type){
  SYMS_ProfBegin("syms_type_ptr_from_type");
  
  SYMS_TypeNode *result = &syms_type_graph_nil;
  
  SYMS_U64 content[2];
  content[0] = ptr_kind;
  content[1] = (SYMS_U64)type;
  SYMS_String8 data = {(SYMS_U8*)content, sizeof(content)};
  result = syms_type_from_content(&graph->type_content_buckets, data);
  
  if (result == 0){
    result = syms_push_array_zero(graph->arena, SYMS_TypeNode, 1);
    syms_type_content_buckets_insert(graph->arena, &graph->type_content_buckets, data, result);
    
    result->kind = ptr_kind;
    result->byte_size = graph->address_size;
    result->direct_type = type;
    result->this_type = &syms_type_graph_nil;
  }
  
  SYMS_ProfEnd();
  return(result);
}

SYMS_API SYMS_TypeNode*
syms_type_array_from_type(SYMS_TypeGraph *graph, SYMS_TypeNode *type, SYMS_U64 count){
  SYMS_ProfBegin("syms_type_array_from_type");
  
  SYMS_TypeNode *result = &syms_type_graph_nil;
  
  SYMS_U64 content[3];
  content[0] = SYMS_TypeKind_Array;
  content[1] = count;
  content[2] = (SYMS_U64)type;
  SYMS_String8 data = {(SYMS_U8*)content, sizeof(content)};
  result = syms_type_from_content(&graph->type_content_buckets, data);
  
  if (result == 0){
    result = syms_push_array_zero(graph->arena, SYMS_TypeNode, 1);
    syms_type_content_buckets_insert(graph->arena, &graph->type_content_buckets, data, result);
    
    result->kind = SYMS_TypeKind_Array;
    result->byte_size = count*type->byte_size;
    result->direct_type = type;
    result->this_type = &syms_type_graph_nil;
    result->array_count = count;
  }
  
  SYMS_ProfEnd();
  return(result);
}

SYMS_API SYMS_TypeNode*
syms_type_proc_from_type(SYMS_TypeGraph *graph, SYMS_TypeNode *ret_type, SYMS_TypeNode *this_type,
                         SYMS_TypeNode **param_types, SYMS_U64 param_count){
  SYMS_ProfBegin("syms_type_proc_from_type");
  
  SYMS_TypeNode *result = &syms_type_graph_nil;
  
  SYMS_String8 params_as_string = {(SYMS_U8*)param_types, sizeof(*param_types)*param_count};
  SYMS_String8 params_cons = syms_string_cons(graph->arena, graph->string_cons, params_as_string);
  
  SYMS_U64 content[4];
  content[0] = SYMS_TypeKind_Proc;
  content[1] = (SYMS_U64)ret_type;
  content[2] = (SYMS_U64)this_type;
  content[3] = (SYMS_U64)params_cons.str;
  SYMS_String8 data = {(SYMS_U8*)content, sizeof(content)};
  result = syms_type_from_content(&graph->type_content_buckets, data);
  
  if (result == 0){
    result = syms_push_array_zero(graph->arena, SYMS_TypeNode, 1);
    syms_type_content_buckets_insert(graph->arena, &graph->type_content_buckets, data, result);
    
    result->kind = SYMS_TypeKind_Proc;
    result->byte_size = 0;
    result->direct_type = ret_type;
    result->this_type = this_type;
    result->proc.params = (SYMS_TypeNode**)(params_cons.str);
    result->proc.param_count = param_count;
  }
  
  SYMS_ProfEnd();
  return(result);
}

SYMS_API SYMS_TypeNode*
syms_type_member_ptr_from_type(SYMS_TypeGraph *graph, SYMS_TypeNode *container, SYMS_TypeNode *type){
  SYMS_ProfBegin("syms_type_member_ptr_from_type");
  
  SYMS_TypeNode *result = &syms_type_graph_nil;
  
  SYMS_U64 content[3];
  content[0] = SYMS_TypeKind_MemberPtr;
  content[1] = (SYMS_U64)container;
  content[2] = (SYMS_U64)type;
  SYMS_String8 data = {(SYMS_U8*)content, sizeof(content)};
  result = syms_type_from_content(&graph->type_content_buckets, data);
  
  if (result == 0){
    result = syms_push_array_zero(graph->arena, SYMS_TypeNode, 1);
    syms_type_content_buckets_insert(graph->arena, &graph->type_content_buckets, data, result);
    
    SYMS_U64 byte_size = graph->address_size;
    if (type->kind == SYMS_TypeKind_Proc){
      byte_size *= 2;
    }
    
    result->kind = SYMS_TypeKind_MemberPtr;
    result->byte_size = byte_size;
    result->direct_type = type;
    result->this_type = container;
  }
  
  SYMS_ProfEnd();
  return(result);
}

////////////////////////////////
// NOTE(allen): Type Info Operators

SYMS_API SYMS_TypeNode*
syms_type_resolved(SYMS_TypeNode *type){
  SYMS_TypeNode *result = type;
  for (;syms_type_kind_is_forward(result->kind) ||
       result->kind == SYMS_TypeKind_Modifier ||
       result->kind == SYMS_TypeKind_Typedef;){
    result = result->direct_type;
  }
  return(result);
}

SYMS_API SYMS_B32
syms_type_node_match(SYMS_TypeNode *l, SYMS_TypeNode *r){
  SYMS_B32 result = syms_false;
  
  // resolve forward references
  SYMS_TypeNode *lres = syms_type_resolved(l);
  SYMS_TypeNode *rres = syms_type_resolved(r);
  
  if (lres == rres){
    result = syms_true;
  }
  else{
    if (lres->kind == rres->kind){
      switch (lres->kind){
        default:
        {
          result = syms_true;
        }break;
        
        case SYMS_TypeKind_Struct:
        case SYMS_TypeKind_Union:
        case SYMS_TypeKind_Enum:
        case SYMS_TypeKind_Typedef:
        {
          if (lres->src_coord != 0 && rres->src_coord != 0){
            SYMS_USID lusid = lres->src_coord->usid;
            SYMS_USID rusid = rres->src_coord->usid;
            if (lusid.uid == rusid.uid && lusid.sid == rusid.sid){
              result = syms_true;
            }
          }
        }break;
        
        case SYMS_TypeKind_Ptr:
        case SYMS_TypeKind_LValueReference:
        case SYMS_TypeKind_RValueReference:
        {
          if (syms_type_node_match(lres->direct_type, rres->direct_type)){
            result = syms_true;
          }
        }break;
        
        case SYMS_TypeKind_MemberPtr:
        {
          if (syms_type_node_match(lres->direct_type, rres->direct_type) &&
              syms_type_node_match(lres->this_type, rres->this_type)){
            result = syms_true;
          }
        }break;
        
        case SYMS_TypeKind_Array:
        {
          if (lres->array_count == rres->array_count &&
              syms_type_node_match(lres->direct_type, rres->direct_type)){
            result = syms_true;
          }
        }break;
        
        case SYMS_TypeKind_Proc:
        {
          if (lres->proc.param_count == rres->proc.param_count &&
              syms_type_node_match(lres->direct_type, rres->direct_type) &&
              syms_type_node_match(lres->this_type, rres->this_type)){
            SYMS_B32 params_match = syms_true;
            SYMS_TypeNode **lp = lres->proc.params;
            SYMS_TypeNode **rp = rres->proc.params;
            SYMS_U64 count = lres->proc.param_count;
            for (SYMS_U64 i = 0; i < count; i += 1, lp += 1, rp += 1){
              if (!syms_type_node_match(*lp, *rp)){
                params_match = syms_false;
                break;
              }
            }
            result = params_match;
          }
        }break;
      }
    }
  }
  return(result);
}

SYMS_API SYMS_TypeNode*
syms_type_resolve_enum_to_basic(SYMS_TypeGraph *graph, SYMS_TypeNode *t){
  SYMS_TypeNode *result = t;
  if (t->kind == SYMS_TypeKind_Enum){
    result = t->direct_type;
  }
  return(result);
}

SYMS_API SYMS_TypeNode*
syms_type_promoted_from_type_node(SYMS_TypeGraph *graph, SYMS_TypeNode *c){
  SYMS_TypeNode *result = c;
  SYMS_TypeKind kind = c->kind;
  if (kind == SYMS_TypeKind_Bool ||
      kind == SYMS_TypeKind_Int8  ||
      kind == SYMS_TypeKind_Int16 ||
      kind == SYMS_TypeKind_UInt8 ||
      kind == SYMS_TypeKind_UInt16){
    result = syms_type_basic(graph, SYMS_TypeKind_Int32, 4, syms_str8_lit("int32"));
  }
  return(result);
}

SYMS_API SYMS_TypeNode*
syms_type_auto_casted_from_type_nodes(SYMS_TypeGraph *graph, SYMS_TypeNode *l, SYMS_TypeNode *r){
  SYMS_ASSERT(syms_type_kind_is_basic_or_enum(l->kind) &&
              syms_type_kind_is_basic_or_enum(r->kind));
  
  // replace enums with corresponding ints
  SYMS_TypeNode *lt = syms_type_resolve_enum_to_basic(graph, l);
  SYMS_TypeNode *rt = syms_type_resolve_enum_to_basic(graph, r);
  
  // first promote each
  SYMS_TypeNode *lp = syms_type_promoted_from_type_node(graph, lt);
  SYMS_TypeNode *rp = syms_type_promoted_from_type_node(graph, rt);
  SYMS_TypeKind lk = lp->kind;
  SYMS_TypeKind rk = rp->kind;
  
  // pick the type with higher rank
  static SYMS_TypeKind type_table[] = {
    SYMS_TypeKind_Float64,
    SYMS_TypeKind_Float32,
    SYMS_TypeKind_UInt64,
    SYMS_TypeKind_Int64,
    SYMS_TypeKind_UInt32,
  };
  SYMS_TypeKind k = SYMS_TypeKind_Int32;
  for (SYMS_U64 i = 0; i < SYMS_ARRAY_SIZE(type_table); i += 1){
    SYMS_TypeKind check = type_table[i];
    if (check == lk || check == rk){
      k = check;
      break;
    }
  }
  
  // construct the output type
  SYMS_TypeNode *result = &syms_type_graph_nil;
  if (k == lk){
    result = l;
  }
  else if (k == rk){
    result = r;
  }
  else{
    SYMS_String8 name = syms_string_from_enum_value(SYMS_TypeKind, k);
    SYMS_U64 bit_size = syms_bit_size_from_type_kind(k);
    SYMS_U64 byte_size = bit_size >> 3;
    result = syms_type_basic(graph, k, byte_size, name);
  }
  
  return(result);
}

////////////////////////////////
// NOTE(allen): Type Stringizing

SYMS_API SYMS_String8
syms_string_from_type(SYMS_Arena *arena, SYMS_TypeNode *type){
  SYMS_ProfBegin("syms_string_from_type");
  SYMS_ArenaTemp scratch = syms_get_scratch(&arena, 1);
  SYMS_String8List list = {0};
  syms_lhs_string_from_type(scratch.arena, type, &list);
  syms_rhs_string_from_type(scratch.arena, type, &list);
  SYMS_String8 result = syms_string_list_join(arena, &list, 0);
  syms_release_scratch(scratch);
  SYMS_ProfEnd();
  return(result);
}

SYMS_API void
syms_lhs_string_from_type(SYMS_Arena *arena, SYMS_TypeNode *type, SYMS_String8List *out){
  SYMS_ProfBegin("syms_lhs_string_from_type");
  syms_lhs_string_from_type__internal(arena, type, out, 0, syms_false);
  SYMS_ProfEnd();
}

SYMS_API void
syms_rhs_string_from_type(SYMS_Arena *arena, SYMS_TypeNode *type, SYMS_String8List *out){
  SYMS_ProfBegin("syms_rhs_string_from_type");
  syms_rhs_string_from_type__internal(arena, type, out, 0);
  SYMS_ProfEnd();
}

SYMS_API void
syms_lhs_string_from_type_skip_return(SYMS_Arena *arena, SYMS_TypeNode *type, SYMS_String8List *out){
  SYMS_ProfBegin("syms_lhs_string_from_type_skip_return");
  syms_lhs_string_from_type__internal(arena, type, out, 0, syms_true);
  SYMS_ProfEnd();
}

SYMS_API void
syms_lhs_string_from_type__internal(SYMS_Arena *arena, SYMS_TypeNode *type,
                                    SYMS_String8List *out,
                                    SYMS_U32 precedence_level, SYMS_B32 skip_return){
  switch (type->kind){
    default:
    {
      syms_string_list_push(arena, out, type->name);
      syms_string_list_push(arena, out, syms_str8_lit(" "));
    }break;
    
    case SYMS_TypeKind_Bitfield:
    {
      syms_lhs_string_from_type__internal(arena, type->direct_type, out, precedence_level, skip_return);
    }break;
    
    case SYMS_TypeKind_Modifier:
    {
      syms_lhs_string_from_type__internal(arena, type->direct_type, out, 1, skip_return);
      if (type->mods & SYMS_TypeModifier_Const){
        syms_string_list_push(arena, out, syms_str8_lit("const "));
      }
      if (type->mods & SYMS_TypeModifier_Restrict){
        syms_string_list_push(arena, out, syms_str8_lit("restrict "));
      }
      if (type->mods & SYMS_TypeModifier_Volatile){
        syms_string_list_push(arena, out, syms_str8_lit("volatile "));
      }
    }break;
    
    case SYMS_TypeKind_Variadic:
    {
      syms_string_list_push(arena, out, syms_str8_lit("..."));
    }break;
    
    case SYMS_TypeKind_Label:
    {
      // TODO(allen): ???
    }break;
    
    case SYMS_TypeKind_Struct:
    case SYMS_TypeKind_Union:
    case SYMS_TypeKind_Enum:
    case SYMS_TypeKind_Class:
    case SYMS_TypeKind_Typedef:
    {
      syms_string_list_push(arena, out, type->name);
      syms_string_list_push(arena, out, syms_str8_lit(" "));
    }break;
    
    case SYMS_TypeKind_ForwardStruct:
    case SYMS_TypeKind_ForwardUnion:
    case SYMS_TypeKind_ForwardEnum:
    case SYMS_TypeKind_ForwardClass:
    {
      SYMS_String8 keyword = syms_string_from_enum_value(SYMS_TypeKind, type->kind);
      syms_string_list_push(arena, out, keyword);
      syms_string_list_push(arena, out, syms_str8_lit(" "));
      syms_string_list_push(arena, out, type->name);
      syms_string_list_push(arena, out, syms_str8_lit(" "));
    }break;
    
    case SYMS_TypeKind_Array:
    {
      syms_lhs_string_from_type__internal(arena, type->direct_type, out, 2, skip_return);
      if (precedence_level == 1){
        syms_string_list_push(arena, out, syms_str8_lit("("));
      }
    }break;
    
    case SYMS_TypeKind_Proc:
    {
      if (!skip_return){
        syms_lhs_string_from_type__internal(arena, type->direct_type, out, 2, syms_false);
      }
      if (precedence_level == 1){
        syms_string_list_push(arena, out, syms_str8_lit("("));
      }
    }break;
    
    case SYMS_TypeKind_Ptr:
    {
      syms_lhs_string_from_type__internal(arena, type->direct_type, out, 1, skip_return);
      syms_string_list_push(arena, out, syms_str8_lit("*"));
    }break;
    
    case SYMS_TypeKind_LValueReference:
    {
      syms_lhs_string_from_type__internal(arena, type->direct_type, out, 1, skip_return);
      syms_string_list_push(arena, out, syms_str8_lit("&"));
    }break;
    
    case SYMS_TypeKind_RValueReference:
    {
      syms_lhs_string_from_type__internal(arena, type->direct_type, out, 1, skip_return);
      syms_string_list_push(arena, out, syms_str8_lit("&&"));
    }break;
    
    case SYMS_TypeKind_MemberPtr:
    {
      syms_lhs_string_from_type__internal(arena, type->direct_type, out, 1, skip_return);
      SYMS_TypeNode *container = type->this_type;
      if (container != 0){
        syms_string_list_push(arena, out, container->name);
      }
      else{
        syms_string_list_push(arena, out, syms_str8_lit("<unknown-class>"));
      }
      syms_string_list_push(arena, out, syms_str8_lit("::*"));
    }break;
  }
}

SYMS_API void
syms_rhs_string_from_type__internal(SYMS_Arena *arena, SYMS_TypeNode *type,
                                    SYMS_String8List *out,
                                    SYMS_U32 precedence_level){
  switch (type->kind){
    default:break;
    
    case SYMS_TypeKind_Bitfield:
    {
      syms_rhs_string_from_type__internal(arena, type->direct_type, out, precedence_level);
    }break;
    
    case SYMS_TypeKind_Modifier:
    case SYMS_TypeKind_Ptr:
    case SYMS_TypeKind_LValueReference:
    case SYMS_TypeKind_RValueReference:
    case SYMS_TypeKind_MemberPtr:
    {
      syms_rhs_string_from_type__internal(arena, type->direct_type, out, 1);
    }break;
    
    case SYMS_TypeKind_Array:
    {
      if (precedence_level == 1){
        syms_string_list_push(arena, out, syms_str8_lit(")"));
      }
      
      SYMS_String8 count_str = syms_string_from_u64(arena, type->array_count);
      syms_string_list_push(arena, out, syms_str8_lit("["));
      syms_string_list_push(arena, out, count_str);
      syms_string_list_push(arena, out, syms_str8_lit("]"));
      
      syms_rhs_string_from_type__internal(arena, type->direct_type, out, 2);
    }break;
    
    case SYMS_TypeKind_Proc:
    {
      if (precedence_level == 1){
        syms_string_list_push(arena, out, syms_str8_lit(")"));
      }
      
      // parameters
      if (type->proc.param_count == 0){
        syms_string_list_push(arena, out, syms_str8_lit("(void)"));
      }
      else{
        syms_string_list_push(arena, out, syms_str8_lit("("));
        SYMS_U64 param_count = type->proc.param_count;
        SYMS_TypeNode **param = type->proc.params;
        for (SYMS_U64 i = 0; i < param_count; i += 1, param += 1){
          SYMS_String8 param_str = syms_string_from_type(arena, *param);
          SYMS_String8 param_str_trimmed = syms_str8_skip_chop_whitespace(param_str);
          syms_string_list_push(arena, out, param_str_trimmed);
          if (i + 1 < param_count){
            syms_string_list_push(arena, out, syms_str8_lit(", "));
          }
        }
        syms_string_list_push(arena, out, syms_str8_lit(")"));
      }
      
      syms_rhs_string_from_type__internal(arena, type->direct_type, out, 2);
    }break;
    
    case SYMS_TypeKind_Label:
    {
      // TODO(allen): ???
    }break;
  }
}

#endif //SYMS_TYPE_GRAPH_C
