#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <time.h>
#include "btree.h"

#define COUNT 1000000L

int main(){
    btree *tree = NULL;
    int *nums,i,j,k,rc;
    intptr_t value;
    clock_t t;

    nums = malloc(sizeof(int) * COUNT);
    assert(nums != NULL);

    for(i=0;i<COUNT;i++){
        nums[i] = i;
    }

    srand(getpid()^time(NULL));
    for(i=COUNT;i>0;i--){
        j = rand()%i;
        k = nums[j];
        nums[j] = nums[i-1];
        nums[i-1] = k;
    }

    assert(btree_create(4096,&tree)==0);

    t = clock();
    for(i=0;i<COUNT;i++){
        rc = btree_insert(tree,nums[i],nums[i],1);
        assert(rc);
    }
    t = clock() - t;
    printf("insert %ld use %ldus, per %lfus\n",COUNT, t, t/(double)COUNT);

    assert(btree_count(tree) == COUNT);

    t = clock();
    for(i=0;i<COUNT;i++){
        rc = btree_search(tree,nums[i],&value);
        assert(rc && nums[i] == value);
    }
    t = clock() - t;
    printf("search %ld use %ldus, per %lfus\n",COUNT,t,t/(double)COUNT);

    t  = clock();
    for(i=0;i<COUNT;i++){
        rc = btree_delete(tree,nums[i],&value);
        assert(rc && nums[i] == value);
    }
    printf("delete %ld use %ldus, per %lfus\n", COUNT, t, t/(double)COUNT);

    assert(btree_count(tree) == 0);

    btree_free(tree,0);

    free(nums);
    return 0;
}
