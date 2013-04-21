#ifndef __KVSET_H__
#define __KVSET_H__

struct kv_set;

typedef struct kv_set_functions
{   
    void (*delete_table)(struct kv_set * kv_set,
                         void (*element_free_function)(void *context, void* element),
                         void * context);
    void * (*put)(struct kv_set * kv_set, void * key_value);
    int (*put_new)(struct kv_set * kv_set, void * key_value);
    void * (*remove)(struct kv_set * kv_set, void * key);
    void * (*lookup)(struct kv_set * kv_set, void * key);
    int (*member)(struct kv_set * kv_set, void * key);
    void * (*first)(struct kv_set * kv_set);
    void * (*last)(struct kv_set * kv_set);    
    void * (*next)(struct kv_set * kv_set, void * key);
    void * (*previous)(struct kv_set * kv_set, void * key);
} KVSetFunctions;


typedef struct kv_set
{
    KVSetFunctions funs;
    unsigned int key_offset;
    void * type_specific_data;
} KVSet;

#endif
