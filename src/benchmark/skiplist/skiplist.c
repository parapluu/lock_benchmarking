#include "skiplist.h"
#include "stdio.h"


/*
 * ========================
 * Internal data structures
 * ========================
 */


#define SKIPLIST_NUM_OF_LEVELS 30

#define SKIPLIST_NORMAL_NODE 1
#define SKIPLIST_LEFT_BORDER_NODE 1 << 2
#define SKIPLIST_RIGHT_BORDER_NODE 1 << 1


typedef struct skiplist_node {
    //contains information about if it is a boarder point
    int info;
    void *  element;
    int  num_of_levels;
    struct skiplist_node * lower_lists[];    
} SkiplistNode;

typedef struct skiplist {
    int (*compare)(void *, void *);
    void (*free)(void *);
    void *(*malloc)(size_t size);
    SkiplistNode head_node;
} Skiplist;

struct one_level_find_result {
    SkiplistNode * neigbour_before;
    SkiplistNode * element_skiplist;
    SkiplistNode * neigbour_after;
};

struct find_result {
    SkiplistNode * element_skiplist;
    SkiplistNode * neigbours_before[SKIPLIST_NUM_OF_LEVELS];
    SkiplistNode * neigbours_after[SKIPLIST_NUM_OF_LEVELS];    
};

/*
 * ==================
 * Internal functions
 * ==================
 */

static inline 
SkiplistNode* create_skiplist_node(int num_of_levels, 
                                   void * element,
                                   void *(*malloc_fun)(size_t)){
    SkiplistNode* skiplist = 
        (SkiplistNode*)malloc_fun(sizeof(SkiplistNode) + 
                                  sizeof(SkiplistNode*) * (num_of_levels));
    skiplist->element = element;
    skiplist->num_of_levels = num_of_levels;
    skiplist->info = SKIPLIST_NORMAL_NODE;
    return skiplist;
}

static inline 
int random_level(int num_of_levels){
    int i;
    long random_number = rand();
    int num = 2;
    for(i = num_of_levels -1 ; i > 0 ; i--){
        if(random_number > (RAND_MAX / num)){
            return i;
        }
        num = num * 2;
    }
    return 0;
}

static inline 
int compare(SkiplistNode* skiplist,
            void * key, 
            int (*compare_function)(void *, void *), 
            unsigned int key_offset){
    if(skiplist->info & SKIPLIST_NORMAL_NODE){
        return compare_function(skiplist->element + key_offset, key);
    } else if (skiplist->info & SKIPLIST_LEFT_BORDER_NODE){
        return -1;
    } else {
        return 1;
    }
}

static inline 
struct one_level_find_result find_neigbours_1_level(SkiplistNode* skiplist,
                                                    void * element,
                                                    int level,
                                                    int (*compare_function)(void *, void *),
                                                    unsigned int key_offset){

    int cmp_result;
    SkiplistNode* skiplist_prev = skiplist;
    int level_pos = level - (SKIPLIST_NUM_OF_LEVELS - skiplist_prev->num_of_levels);
    SkiplistNode* skiplist_next = skiplist_prev->lower_lists[level_pos];
    struct one_level_find_result result;

    do{
        cmp_result = compare(skiplist_next, element, compare_function, key_offset);
        if(0 < cmp_result){
            result.neigbour_before = skiplist_prev;
            result.element_skiplist = NULL;
            result.neigbour_after = skiplist_next;
        } else if(0 == cmp_result){
            level_pos = level - (SKIPLIST_NUM_OF_LEVELS - skiplist_next->num_of_levels);
            result.neigbour_before = skiplist_prev;
            result.element_skiplist = skiplist_next;
            result.neigbour_after = skiplist_next->lower_lists[level_pos];
        } else {
            level_pos = level - (SKIPLIST_NUM_OF_LEVELS - skiplist_next->num_of_levels);
            skiplist_prev = skiplist_next;
            skiplist_next = skiplist_next->lower_lists[level_pos];
        }
    } while(0 > cmp_result);

    return result;

}

static inline 
struct find_result find_neigbours(SkiplistNode* skiplist,
                                  void * element,
                                  int (*compare_function)(void *, void *),
                                  unsigned int key_offset){
    int level;
    struct find_result result;
    struct one_level_find_result level_result;
    SkiplistNode* neigbour_before_iter = skiplist;

    for(level = 0; level < SKIPLIST_NUM_OF_LEVELS; level++){
        level_result = 
            find_neigbours_1_level(neigbour_before_iter, 
                                   element, 
                                   level, 
                                   compare_function, 
                                   key_offset);
        result.neigbours_before[level] = level_result.neigbour_before;
        result.neigbours_after[level] = level_result.neigbour_after;
        neigbour_before_iter = level_result.neigbour_before;
    }
    result.element_skiplist = level_result.element_skiplist;

    return result;

}

static inline
void set_next_at_level(SkiplistNode* skiplist,
                       SkiplistNode* next_skiplist,
                       int level){
    int level_pos = level - (SKIPLIST_NUM_OF_LEVELS - skiplist->num_of_levels);
    skiplist->lower_lists[level_pos] = next_skiplist;
}

static inline 
void insert_sublist(SkiplistNode* skiplist, 
                    struct find_result neigbours, 
                    SkiplistNode* sublist, 
                    int level_to_insert_from,
                    int level_to_remove_from){
    int link_level;
    for(link_level = level_to_remove_from; link_level < level_to_insert_from; link_level++){
        set_next_at_level(neigbours.neigbours_before[link_level],
                          neigbours.neigbours_after[link_level],
                          link_level);
    }
    for(link_level = level_to_insert_from; link_level < SKIPLIST_NUM_OF_LEVELS; link_level++){
        set_next_at_level(neigbours.neigbours_before[link_level],
                          sublist,
                          link_level);
        set_next_at_level(sublist,
                          neigbours.neigbours_after[link_level],
                          link_level);
    }
}

static
int default_compare_function(void * e1, void * e2){
    return (e1 > e2 ? 1 : (e1 < e2 ? -1 : 0));
}

/*
 * Internal function used in the external data structure
 */

static
void skiplist_delete(KVSet* kv_set,
                     void (*element_free_function)(void *context, void* element),
                     void * context){
    
    Skiplist * skiplist = (Skiplist *) &(kv_set->type_specific_data);
    SkiplistNode * head_node = &(skiplist->head_node);

    SkiplistNode* node_temp = head_node->lower_lists[head_node->num_of_levels -1];
    SkiplistNode* node_iter = node_temp;

    while(node_iter->info &  SKIPLIST_NORMAL_NODE){
        node_temp = node_iter;
        node_iter = node_iter->lower_lists[node_iter->num_of_levels -1];
        if(NULL != element_free_function){
            element_free_function(context, node_temp->element);
        }
        skiplist->free(node_temp);
    }
    skiplist->free(node_iter);
    skiplist->free(kv_set);

    return;
}

static
void * skiplist_put(KVSet* kv_set, void * key_value){

    Skiplist * skiplist = (Skiplist *) &(kv_set->type_specific_data);
    SkiplistNode * head_node = &(skiplist->head_node);
    void * key = key_value + kv_set->key_offset;
    void * returnValue;
    struct find_result neigbours = find_neigbours(head_node, 
                                                  key, 
                                                  skiplist->compare,
                                                  kv_set->key_offset);
    int level = random_level(head_node->num_of_levels);
    int num_of_elements_in_insert_level = head_node->num_of_levels - level;
    SkiplistNode* new_skiplist_node =
        create_skiplist_node(num_of_elements_in_insert_level, key_value, skiplist->malloc);
    
    if(neigbours.element_skiplist == NULL){
        insert_sublist(head_node, neigbours, new_skiplist_node, level, level);
        return NULL;
    } else {
        insert_sublist(head_node, 
                       neigbours, 
                       new_skiplist_node, 
                       level, 
                       head_node->num_of_levels - neigbours.element_skiplist->num_of_levels);
        returnValue = neigbours.element_skiplist->element;
        skiplist->free(neigbours.element_skiplist);
        return returnValue;
    }
}

static
int skiplist_put_new(KVSet* kv_set, void * key_value){

    Skiplist * skiplist = (Skiplist *) &(kv_set->type_specific_data);
    SkiplistNode * head_node = &(skiplist->head_node);
    void * key = key_value + kv_set->key_offset;
    struct find_result neigbours = find_neigbours(head_node, key, skiplist->compare, kv_set->key_offset);
    int level = random_level(head_node->num_of_levels);
    int num_of_elements_in_insert_level = head_node->num_of_levels - level;
    SkiplistNode* new_skiplist_node;
    if(neigbours.element_skiplist == NULL){
        new_skiplist_node =
            create_skiplist_node(num_of_elements_in_insert_level, key_value, skiplist->malloc);
        insert_sublist(head_node, neigbours, new_skiplist_node, level, level);
        return 1;
    } else {
        return 0;
    }
}

static
void * skiplist_remove(KVSet* kv_set, void * key){

    Skiplist * skiplist = (Skiplist *) &(kv_set->type_specific_data);
    SkiplistNode * head_node = &(skiplist->head_node);

    struct find_result neigbours = find_neigbours(head_node,
                                                  key, skiplist->compare,
                                                  kv_set->key_offset);
    void * returnValue;
    int remove_level;
    int remove_from_level;
    if(neigbours.element_skiplist == NULL){
        return NULL;
    } else {
        remove_from_level = 
            SKIPLIST_NUM_OF_LEVELS - neigbours.element_skiplist->num_of_levels;
        for(remove_level = remove_from_level; remove_level < head_node->num_of_levels; remove_level++){
            set_next_at_level(neigbours.neigbours_before[remove_level],
                              neigbours.neigbours_after[remove_level],
                              remove_level);
        }
        returnValue = neigbours.element_skiplist->element;
        skiplist->free(neigbours.element_skiplist);
        return returnValue;
    }
}

static
void * skiplist_lookup(KVSet* kv_set, void * key){

    Skiplist * skiplist = (Skiplist *) &(kv_set->type_specific_data);
    SkiplistNode * head_node = &(skiplist->head_node);

    struct find_result neigbours = find_neigbours(head_node, 
                                                  key, 
                                                  skiplist->compare, 
                                                  kv_set->key_offset);
    void * returnValue;
    if(neigbours.element_skiplist == NULL){
        return NULL;
    } else {
        returnValue = neigbours.element_skiplist->element;
        return returnValue;
    }
}

static
int member(KVSet * kv_set, void * key){
    Skiplist * skiplist = (Skiplist *) &(kv_set->type_specific_data);
    SkiplistNode * head_node = &(skiplist->head_node);

    struct find_result neigbours = find_neigbours(head_node, 
                                                  key, 
                                                  skiplist->compare, 
                                                  kv_set->key_offset);

    return neigbours.element_skiplist != NULL;
}


static
void * skiplist_first(KVSet* kv_set){

    Skiplist * skiplist = (Skiplist *) &(kv_set->type_specific_data);
    SkiplistNode * head_node = &(skiplist->head_node);

    SkiplistNode * firstCandidate = 
        head_node->lower_lists[head_node->num_of_levels - 1];
    if(firstCandidate->info & SKIPLIST_RIGHT_BORDER_NODE){
        return NULL;
    } else {
        return firstCandidate->element;
    }
}

static
void * skiplist_last(KVSet* kv_set){

    Skiplist * skiplist = (Skiplist *) &(kv_set->type_specific_data);
    SkiplistNode * head_node = &(skiplist->head_node);

    int level;
    int level_pos;
    SkiplistNode* skiplist_iter_prev = head_node;
    SkiplistNode* skiplist_iter;
    
    for(level = 0; level < SKIPLIST_NUM_OF_LEVELS; level++){
        level_pos = level - (SKIPLIST_NUM_OF_LEVELS - skiplist_iter_prev->num_of_levels);
        skiplist_iter = skiplist_iter_prev->lower_lists[level_pos];
        while(skiplist_iter->info & 
              (SKIPLIST_LEFT_BORDER_NODE | SKIPLIST_NORMAL_NODE)) {
            skiplist_iter_prev = skiplist_iter;
            level_pos = level - ( head_node->num_of_levels - skiplist_iter->num_of_levels);
            skiplist_iter = skiplist_iter->lower_lists[level_pos];
        }
    }

    if(skiplist_iter_prev->info & SKIPLIST_LEFT_BORDER_NODE){
        return NULL;
    } else {
        return skiplist_iter_prev->element;;
    }  
}

static
void * skiplist_next(KVSet* kv_set, void * key){

    Skiplist * skiplist = (Skiplist *) &(kv_set->type_specific_data);
    SkiplistNode * head_node = &(skiplist->head_node);

    struct find_result neigbours = find_neigbours(head_node, 
                                                  key, 
                                                  skiplist->compare, 
                                                  kv_set->key_offset);
    return neigbours.neigbours_after[SKIPLIST_NUM_OF_LEVELS-1]->element;
}

static
void * skiplist_previous(KVSet* kv_set, void * key){

    Skiplist * skiplist = (Skiplist *) &(kv_set->type_specific_data);
    SkiplistNode * head_node = &(skiplist->head_node);

    struct find_result neigbours = find_neigbours(head_node, 
                                                  key, 
                                                  skiplist->compare, 
                                                  kv_set->key_offset);
    return neigbours.neigbours_before[SKIPLIST_NUM_OF_LEVELS-1]->element;
}



/*
 *=================
 * Public interface
 *=================
 */
KVSet * new_skiplist(int (*compare_function)(void *, void *), 
                     void (*free_function)(void *),
                     void *(*malloc_function)(size_t),
                     unsigned int key_offset){
    int i;

    KVSet* kv_set = 
        (KVSet*)malloc_function(sizeof(KVSet) +
                                sizeof(Skiplist) + 
                                sizeof(SkiplistNode*) * (SKIPLIST_NUM_OF_LEVELS));

    KVSetFunctions funs =
        {
            skiplist_delete,
            skiplist_put,
            skiplist_put_new,
            skiplist_remove,
            skiplist_lookup,
            member,
            skiplist_first,
            skiplist_last,    
            skiplist_next,
            skiplist_previous
        };

    Skiplist * skiplist = (Skiplist *) &(kv_set->type_specific_data);

    SkiplistNode* rightmost_skiplist =
        create_skiplist_node(SKIPLIST_NUM_OF_LEVELS, NULL, malloc_function);

    SkiplistNode* leftmost_skiplist = (SkiplistNode*)&(skiplist->head_node);

    kv_set->funs = funs;
    kv_set->key_offset = key_offset;
    
    skiplist->compare = compare_function;
    skiplist->free = free_function;
    skiplist->malloc = malloc_function;

    leftmost_skiplist->element = NULL;
    leftmost_skiplist->num_of_levels = SKIPLIST_NUM_OF_LEVELS;
    
    for(i = 0 ; i < SKIPLIST_NUM_OF_LEVELS ; i++){
        leftmost_skiplist->lower_lists[i] =
            rightmost_skiplist;
    }

    leftmost_skiplist->info = SKIPLIST_LEFT_BORDER_NODE;

    rightmost_skiplist->info = SKIPLIST_RIGHT_BORDER_NODE;
   
    return kv_set;
}

KVSet * new_skiplist_default(){
    return new_skiplist(default_compare_function, 
                        free,
                        malloc,
                        0);
}
