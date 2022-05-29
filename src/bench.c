#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include "btree.h"

int main(){
    btree *T = NULL;
    srand(getpid()^time(NULL));
    int nums[1000000],i,j,k;
    int n = 1,rc;
    intptr_t value;
    while(n-- > 0){
        for(i=0;i<sizeof(nums)/sizeof(int);i++){
            nums[i] = i;
        }

        for(i=sizeof(nums)/sizeof(int);i>0;i--){
            j = rand()%i;
            k = nums[j];
            nums[j] = nums[i-1];
            nums[i-1] = k;
        }
        
        T = NULL;
        assert(btree_create(4096,&T)==0);

        for(i=0;i<sizeof(nums)/sizeof(int)/2;i++){
            rc = btree_insert(T,nums[i],nums[i],1);
            assert(rc);
        }

        assert(btree_count(T) == sizeof(nums)/sizeof(int)/2);

        for(i=0;i<sizeof(nums)/sizeof(int)/2;i++){
            rc = btree_search(T,nums[i],&value);
            assert(rc && nums[i] == value);
        }

        for(i=0;i<sizeof(nums)/sizeof(int)/2;i++){
            rc = btree_search(T,i,&value);
            assert(!rc || (rc && i == value));
        }

        for(i=0;i<sizeof(nums)/sizeof(int);i++){
            rc = btree_delete(T,i,&value);
            assert(!rc || (rc && i == value));
        }

        for(i=sizeof(nums)/sizeof(int)/2;i<sizeof(nums)/sizeof(int);i++){
            rc = btree_insert(T,nums[i],nums[i],1);
            assert(rc);
        }

        for(i=sizeof(nums)/sizeof(int)/2;i<sizeof(nums)/sizeof(int);i++){
            rc = btree_search(T,nums[i],&value);
            assert(rc && nums[i] == value);
        }

        for(i=0;i<sizeof(nums)/sizeof(int);i++){
            rc = btree_delete(T,i,&value);
            assert(!rc || (rc && i == value));
        }

        assert(btree_count(T) == 0);
        btree_free(T,0);
    }
    return 0;
}
