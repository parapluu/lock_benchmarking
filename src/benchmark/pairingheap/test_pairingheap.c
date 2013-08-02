#include "pairingheap.h"

/* For verification purpose only. */
#include <assert.h> 

#define BIG_RAND()  (rand() % 10000) 
/* End of verification purpose only part */

void heap_sort(int* xs, int n){
  int i;
  struct node* h = NULL;
  for(i=0; i<n; ++i)
    h = insert(h, xs[i]);
  for(i=0; i<n; ++i){
    xs[i] = top(h);
    h = pop(h);
  }
  destroy_heap(h);
}

/* Testing */
int sorted(const int* xs, int n){
  int i;
  for(i=0; i<n-1; ++i)
    if(xs[i+1] < xs[i])
      return 0;
  return 1;
}

int check_sum(const int* xs, int n){
  int x = 0;
  while(n>0)
    x ^= xs[--n];
  return x;
}

void test_heap_sort(){
  int m = 1000;
  int i, n, c, *xs;
  while(m--){
    n = 1 + BIG_RAND();
    xs = (int*)malloc(sizeof(int)*n);
    for(i=0; i<n; ++i)
      xs[i] = BIG_RAND();
    c = check_sum(xs, n);
    heap_sort(xs, n);
    assert(sorted(xs, n));
    assert(c == check_sum(xs, n));
    free(xs);
  }
}

void test_decrease_key(){
  int m = 100;
  int i, n, c, *xs;
  struct node* h;
  struct node** ns;
  while(m--){
    h = NULL;
    n = 1 + BIG_RAND();
    xs = (int*)malloc(sizeof(int)*n);
    ns = (struct node**)malloc(sizeof(struct node*)*n);
    for(i=0; i<n; ++i){
      xs[i] = BIG_RAND();
      ns[i] = singleton(xs[i]);
      h = insert_node(h, ns[i]);
    }
    for(i=0; i<n; ++i){
      xs[i] -= BIG_RAND();
      h = decrease_key(h, ns[i], xs[i]);
    }
    c = check_sum(xs, n);
    for(i=0; i<n; ++i){
      xs[i] = top(h);
      h = pop(h);
    }
    assert(sorted(xs, n));
    assert(c == check_sum(xs, n));
    free(xs);
    free(ns);
    destroy_heap(h);
  }
}

int main(){
  test_heap_sort();
  test_decrease_key();
  return 0;
}
