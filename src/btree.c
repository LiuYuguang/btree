#include "btree.h"
#include <stdlib.h>
#include <string.h>

#define BTREE_NON_LEAF 0
#define BTREE_LEAF 1

typedef struct{
    btree_key_t key;
    intptr_t value;
    intptr_t child;
}btree_key_value;

typedef struct btree_node_s{
    uint32_t num:31;
    uint32_t leaf:1;
    btree_key_value kvs[0];  
}btree_node;

typedef struct btree_s{
    size_t block_size;
    size_t count;
    btree_node root[0];
}btree;

#define M(block_size) (((block_size)-sizeof(btree_node))/sizeof(btree_key_value) - 1) // 子树为M, 但合并时候会出现M+1, 需要预留足够位置
#define ceil(M) (((M)-1)/2)

inline static btree_node* btree_node_create(size_t block_size,int leaf){
    btree_node *node = calloc(block_size,sizeof(char));
    node->leaf = leaf;
    return node;
}

inline static void btree_node_destroy(btree_node *node){
    free(node);
}

size_t btree_size(size_t block_size){
    return sizeof(btree) + block_size;
}

int btree_create(size_t block_size,btree **T){
    if((block_size & (block_size-1)) != 0){
        // block size must be pow of 2!
        return -1;
    }
    if(block_size < sizeof(btree_node) + sizeof(btree_key_value)){
        return -1;
    }
    size_t M = M(block_size);
    if(M <= 2){
        return -1;
    }

    if(M >= (1UL<<31)){
        return -1;
    }

    if(*T == NULL){
        *T = malloc(sizeof(btree) + block_size);
    }
    memset(*T,0,sizeof(btree) + block_size);
    (*T)->block_size = block_size;
    (*T)->root->leaf = BTREE_LEAF;
    return 0;
}

inline static int key_binary_search(btree_key_value *key,int num, btree_key_t target)
{
	int low = -1;
	int high = num;
	int mid;
	while (low + 1 < high) {
		mid = low + (high - low) / 2;
		if (target > key[mid].key) {
			low = mid;
		} else {
			high = mid;
		}
	}

	return high;
}

inline static void nodecpy(btree_key_value *dest,btree_key_value *src,size_t n){
    intptr_t child = src[n].child;
    memmove(dest,src,sizeof(btree_key_value)*n);
    dest[n].child = child;
}

inline static void btree_split_child(btree* T,btree_node *node,int idx){
    // 新建右结点, 分走x的一半，x的中间结点升到父节点
    btree_node *x = (btree_node *)node->kvs[idx].child;
    btree_node *y = btree_node_create(T->block_size,x->leaf);

    size_t n = ceil(M(T->block_size));
    nodecpy(y->kvs,&x->kvs[n+1],x->num - n - 1);

    y->num = x->num - n - 1;
    x->num = n;
    
    nodecpy(&node->kvs[idx+1],&node->kvs[idx],node->num - idx);// 父节点右移1个空位
    node->kvs[idx] = x->kvs[n];
    node->kvs[idx].child = (intptr_t)x;
    node->kvs[idx+1].child = (intptr_t)y;
    node->num++;
}

int btree_insert(btree* T,btree_key_t key,intptr_t value, int overwrite){
    btree_node *node;
    int i;

    // root结点已满, 在merge的时候关键词有机会出现==M的情况
    if(T->root->num >= M(T->block_size)-1){
        node = btree_node_create(T->block_size,T->root->leaf);
        memcpy(node,T->root,T->block_size);
        memset(T->root,0,T->block_size);
        T->root->leaf = BTREE_NON_LEAF;
        T->root->kvs[0].child = (intptr_t)node;
        btree_split_child(T,T->root,0);
    }

    node = T->root;
    
    while(node->leaf == BTREE_NON_LEAF){
        i = key_binary_search(node->kvs,node->num,key);
        if(i<node->num && key == node->kvs[i].key){
            if(overwrite){
                node->kvs[i].value = value;
                return 1;
            }else{
                return 0;
            }
        }
        // 子节点已满, 在merge的时候关键词有机会出现==M的情况
        if(((btree_node *)node->kvs[i].child)->num >= M(T->block_size)-1){
            btree_split_child(T,node,i);
            if(key == node->kvs[i].key){// 上升关键词的正好相等
                if(overwrite){
                    node->kvs[i].value = value;
                    return 1;
                }else{
                    return 0;
                }
            }else if(key > node->kvs[i].key){
                i++;
            }
        }
        node = (btree_node *)node->kvs[i].child;
    }

    i = key_binary_search(node->kvs,node->num,key);
    if(i<node->num && key == node->kvs[i].key){
        if(overwrite){
            node->kvs[i].value = value;
            return 1;
        }else{
            return 0;
        }
    }

    nodecpy(&node->kvs[i+1],&node->kvs[i],node->num-i);// 右移1个空位
    node->kvs[i].key = key;
    node->kvs[i].value = value;
    node->num++;
    T->count++;
    return 1;
}

inline static void btree_merge(btree* T,btree_node *node,int idx){
    // 父的一个节点下降，并与两个子节点合并
    btree_node *x = (btree_node *)node->kvs[idx].child;
    btree_node *y = (btree_node *)node->kvs[idx+1].child;

    x->kvs[x->num].key = node->kvs[idx].key;
    x->kvs[x->num].value = node->kvs[idx].value;
    nodecpy(&x->kvs[x->num+1],y->kvs,y->num);    
    x->num += ( 1 + y->num );
    btree_node_destroy(y);

    nodecpy(&node->kvs[idx],&node->kvs[idx+1],node->num - idx - 1);// 左移一位
    node->kvs[idx].child = (intptr_t)x;
    node->num--;

    if(node->num == 0){// must be root
        memcpy(T->root,x,T->block_size);
        btree_node_destroy(x);
    }
}

int btree_delete(btree* T,btree_key_t key, intptr_t * value){
    btree_node *node = T->root;
    int i;
    
    while(node->leaf == BTREE_NON_LEAF){
        i = key_binary_search(node->kvs,node->num,key);

        // 匹配到, 需要将关键词下降至叶子节点
        if(i<node->num && key == node->kvs[i].key){
            if(((btree_node *)node->kvs[i].child)->num > ceil(M(T->block_size))){// 左子节点有多
                btree_node *x = (btree_node *)node->kvs[i].child;
                btree_node *y = (btree_node *)node->kvs[i+1].child;
                
                nodecpy(&y->kvs[1],y->kvs,y->num);// 右子节点右移一位
                
                y->kvs[0].key = node->kvs[i].key;// 下降
                y->kvs[0].value = node->kvs[i].value;
                y->kvs[0].child = x->kvs[x->num].child;// 向左子结点借
                y->num++;

                node->kvs[i].key = x->kvs[x->num-1].key;// 左子节点上升
                node->kvs[i].value = x->kvs[x->num-1].value;
                x->num--;
                
                node = y;// 下次查右子节点
            }else if(((btree_node *)node->kvs[i+1].child)->num > ceil(M(T->block_size))){// 右子节点有多
                btree_node *x = (btree_node *)node->kvs[i].child;
                btree_node *y = (btree_node *)node->kvs[i+1].child;
                
                x->kvs[x->num].key = node->kvs[i].key;// 下降
                x->kvs[x->num].value = node->kvs[i].value;
                x->kvs[x->num+1].child = y->kvs[0].child;// 向右子结点借
                x->num++;

                node->kvs[i].key = y->kvs[0].key;// 右子节点上升
                node->kvs[i].value = y->kvs[0].value;

                nodecpy(y->kvs,&y->kvs[1],y->num-1);// 右子节点左移一位
                y->num--;

                node = x;// 下次查左子节点
            }else{
                char flag = (node->num == 1);// must be root
                btree_merge(T,node,i);// 都不足, 则合并
                if(flag){
                    node = T->root;
                }else{
                    node = (btree_node *)node->kvs[i].child;
                }
            }
            continue;
        }
        
        // 没匹配到, 但是要为下一个子节点准备好充足结点, 万一下次就匹配上呢
        btree_node *next = (btree_node *)node->kvs[i].child;

        if(next->num <= ceil(M(T->block_size))){// 不足
            btree_node *x=NULL,*y=NULL;
            if(i-1>=0){
                x = (btree_node *)node->kvs[i-1].child;
            }
            if(i+1<=node->num){
                y = (btree_node *)node->kvs[i+1].child;
            }
            int richR = 0;
            if(y!=NULL){
                richR = 1;
            }
            if(x!=NULL&&y!=NULL){
                richR = y->num > x->num ? 1 : 0;
            }
            if(y!=NULL&&y->num>ceil(M(T->block_size))&&richR){//borrow from right
                next->kvs[next->num].key = node->kvs[i].key;// 下降
                next->kvs[next->num].value = node->kvs[i].value;
                next->kvs[next->num+1].child = y->kvs[0].child;// 向右子结点借
                next->num++;

                node->kvs[i].key = y->kvs[0].key;// 右子节点上升
                node->kvs[i].value = y->kvs[0].value;
                nodecpy(y->kvs,&y->kvs[1],y->num-1);// 右子节点左移一位
                y->num--;
            }else if(x!=NULL&&x->num>ceil(M(T->block_size))){//borrow from left
                nodecpy(&next->kvs[1],next->kvs,next->num);// 右移一位
                next->kvs[0].key = node->kvs[i-1].key;// 下降
                next->kvs[0].value = node->kvs[i-1].value;
                next->kvs[0].child = x->kvs[x->num].child;// 向左子结点借
                next->num++;
                
                node->kvs[i-1].key = x->kvs[x->num-1].key;// 左子节点上升
                node->kvs[i-1].value = x->kvs[x->num-1].value;
                x->num--;
            }else{
                if(x!=NULL)// merge with left
                    i--;
                char flag = (node->num == 1);// must be root
                btree_merge(T,node,i);
                if(flag){// must be root
                    next = T->root;
                }else{
                    next = (btree_node *)node->kvs[i].child;
                }
            }
        }
        node = next;
    }

    i = key_binary_search(node->kvs,node->num,key);
    if(i<node->num && key == node->kvs[i].key){
        if(value)
            *value = node->kvs[i].value;
        nodecpy(&node->kvs[i],&node->kvs[i+1],node->num - i - 1);// 左移一位
        node->num--;
        T->count--;
        return 1;
    }
    return 0;
}

int btree_search(btree* T,btree_key_t key, intptr_t * value){
    btree_node *node = T->root;
    int i;
    while(node){
        i = key_binary_search(node->kvs,node->num,key);
        if(i<node->num && key == node->kvs[i].key){
            if(value)
                *value = node->kvs[i].value;
            return 1;
        }
        
        node = (btree_node *)node->kvs[i].child;
    }

    return 0;
}

static void btree_node_free(size_t M, btree_node *node){
    if(node == 0){
        return ;
    }
    int i;
    for(i=0;i<=node->num;i++){
        btree_node_free(M,(btree_node *)node->kvs[i].child);
    }
    free(node);
}

void btree_free(btree *T,int notroot){
    int i;
    for(i=0;i<=T->root->num;i++){
        btree_node_free(M(T->block_size),(btree_node *)T->root->kvs[i].child);
    }
    if(notroot){
        return;
    }
    free(T);
}

size_t btree_count(btree* T){
    return T->count;
}