#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "request.h"
#include "server_thread.h"
#include "common.h"
#include <ctype.h>

struct wc * my_wc;
struct sorted_data_entry* my_list;
struct sorted_data_entry* head = NULL;
struct sorted_data_entry* tail = NULL;

#define rot(x,k) (((x)<<(k)) | ((x)>>(32-(k))))

#define mix(a,b,c) \
{ \
  a -= c;  a ^= rot(c, 4);  c += b; \
  b -= a;  b ^= rot(a, 6);  a += c; \
  c -= b;  c ^= rot(b, 8);  b += a; \
  a -= c;  a ^= rot(c,16);  c += b; \
  b -= a;  b ^= rot(a,19);  a += c; \
  c -= b;  c ^= rot(b, 4);  b += a; \
}

#define final(a,b,c) \
{ \
  c ^= b; c -= rot(b,14); \
  a ^= c; a -= rot(c,11); \
  b ^= a; b -= rot(a,25); \
  c ^= b; c -= rot(b,16); \
  a ^= c; a -= rot(c,4);  \
  b ^= a; b -= rot(a,14); \
  c ^= b; c -= rot(b,24); \
}

//main body of hash function

uint32_t lookup3(
        const void *key,
        size_t length,
        uint32_t initval
        ) {
    uint32_t a, b, c;
    const uint8_t *k;
    const uint32_t *data32Bit;

    data32Bit = key;
    a = b = c = 0xdeadbeef + (((uint32_t) length) << 2) + initval;

    while (length > 12) {
        a += *(data32Bit++);
        b += *(data32Bit++);
        c += *(data32Bit++);
        mix(a, b, c);
        length -= 12;
    }

    k = (const uint8_t *) data32Bit;
    switch (length) {
        case 12: c += ((uint32_t) k[11]) << 24;
        case 11: c += ((uint32_t) k[10]) << 16;
        case 10: c += ((uint32_t) k[9]) << 8;
        case 9: c += k[8];
        case 8: b += ((uint32_t) k[7]) << 24;
        case 7: b += ((uint32_t) k[6]) << 16;
        case 6: b += ((uint32_t) k[5]) << 8;
        case 5: b += k[4];
        case 4: a += ((uint32_t) k[3]) << 24;
        case 3: a += ((uint32_t) k[2]) << 16;
        case 2: a += ((uint32_t) k[1]) << 8;
        case 1: a += k[0];
            break;
        case 0: return c;
    }
    final(a, b, c);
    return c;
}


int update = 1;
int no_update = 0;


//hash value generator

unsigned int stringToHash(char *word, unsigned int hashTableSize) {
    unsigned int initval;
    unsigned int hashAddress;

    initval = 12345;
    hashAddress = lookup3(word, strlen(word), initval);
    return (hashAddress % hashTableSize);
    // If hashtable is guaranteed to always have a size that is a power of 2,
    // replace the line above with the following more effective line:
    // return (hashAddress & (hashTableSize - 1));
}

struct sorted_data_entry {
    /* you can define this struct to have whatever fields you want. */
    struct file_data *data;
    int reference_count;
    struct sorted_data_entry* next;
};

struct wc {
    /* you can define this struct to have whatever fields you want. */
    struct component *hashtable;
    unsigned int hash_size;
    int max_cache_size;
    int occupied_cache_size;
    pthread_mutex_t cache_lock;
    //int *used_index_array;
    //int current_index_in_used_array;
};

struct component {
    struct file_data *data;
    int count;
    struct component *next; //default is not NULL initialize
};

struct server {
    int nr_threads;
    int max_requests;
    int max_cache_size;
    /* add any other parameters you need */
    pthread_t * thread_array;
    int * buf;
    int in;
    int out;
    pthread_mutex_t lock;
    pthread_cond_t full;
    pthread_cond_t empty;
};

/* static functions */

/* initialize file data */
static struct file_data *
file_data_init(void) {
    struct file_data *data;

    data = Malloc(sizeof (struct file_data));
    data->file_name = NULL;
    data->file_buf = NULL;
    data->file_size = 0;
    return data;
}

/* free all file data */
static void
file_data_free(struct file_data *data) {
    free(data->file_name);
    free(data->file_buf);
    free(data);
}

/* entry point functions */





void set_LRU(struct file_data * data) {
    //if the program reaches this point, data must exist in LRU
    if (head == tail) {
        //no op
    } else {
        struct sorted_data_entry * pre = head;
        struct sorted_data_entry * cur = head;
        while ((cur->next != NULL) && (strcmp(cur->data->file_name, data->file_name) != 0)) {
            pre = cur;
            cur = cur->next;
        }
        if (strcmp(cur->data->file_name, data->file_name) == 0) {
            //found
            if (cur == head) {
                head = cur->next;
                cur->next = NULL;
                tail ->next = cur;
                tail = cur;
            } else if (cur == tail) {
                //no op
            } else {
                //it is not at the tail
                pre->next = cur->next;
                cur->next = NULL;
                tail->next = cur;
                tail = cur;
            }
        }
    }
}

void print_LRU() {
    struct sorted_data_entry * cur = head;
    while (cur != NULL) {
        printf("%s ->", cur->data->file_name);
        cur = cur->next;
    }
}

bool evict_LRU_hash(int required_size) {
    struct sorted_data_entry* iter = head;
    if (head == NULL)
        return false;
    if (head == tail) {
        //only one entry
        if (head->reference_count != 0)
            return false;
        else {
            //evict the only entry
            my_wc->occupied_cache_size = 0;
            struct sorted_data_entry* temp = head;
            head = NULL;
            tail = NULL;
            //delete the component in hash table
            unsigned int hashtable_index;
            hashtable_index = stringToHash(temp->data->file_name, (unsigned int) my_wc->hash_size);
            int hash_index = (int) hashtable_index;
            //found the index in hash table, can free the LRU entry now
            free(temp);
            //because this is the only one left in the hashtable
            //it must be in next of the parent
            file_data_free(my_wc->hashtable[hash_index].next->data);
            //free component entry
            free(my_wc->hashtable[hash_index].next);
            my_wc->hashtable[hash_index].data = NULL;
            my_wc->hashtable[hash_index].next = NULL;
            return true;
        }
    } else {
        print_LRU();
        //there are multiple entries
        while ((required_size > (my_wc->max_cache_size - my_wc->occupied_cache_size)) && iter != NULL) {
            if (iter->reference_count == 0) {
                //evict this data
                //LRU
                int hash_index = -1;
                if (iter == head) {
                    my_wc->occupied_cache_size = my_wc->occupied_cache_size - head->data->file_size;
                    head = head->next;
                    unsigned int hashtable_index;
                    hashtable_index = stringToHash(iter->data->file_name, (unsigned int) my_wc->hash_size);
                    hash_index = (int) hashtable_index;
                    //delete in hash table, data being held by iter
                    goto hash_delete;
                } else if (iter == tail) {
                    my_wc->occupied_cache_size = my_wc->occupied_cache_size - iter->data->file_size;
                    struct sorted_data_entry * find_pre_tail = head;
                    while (find_pre_tail->next != tail)
                        find_pre_tail = find_pre_tail ->next;
                    tail = find_pre_tail;
                    unsigned int hashtable_index;
                    hashtable_index = stringToHash(iter->data->file_name, (unsigned int) my_wc->hash_size);
                    hash_index = (int) hashtable_index;
                    //delete in hash table, data being held by iter
                    goto hash_delete;
                } else {
                    //iter is an entry in middle
                    my_wc->occupied_cache_size = my_wc->occupied_cache_size - iter->data->file_size;
                    //printf("middle data evict name: %s\n", iter->data->file_name);
                    struct sorted_data_entry * find_pre_iter = head;
                    while (find_pre_iter->next != iter)
                        find_pre_iter = find_pre_iter ->next;
                    find_pre_iter->next = iter->next;
                    iter->next = NULL;
                    unsigned int hashtable_index;
                    hashtable_index = stringToHash(iter->data->file_name, (unsigned int) my_wc->hash_size);
                    hash_index = (int) hashtable_index;
                    //delete in hash table, data being help by iter
                    goto hash_delete;
                }
hash_delete:
                printf("evict: %d\n", hash_index);
                if (my_wc->hashtable[hash_index].next->next == NULL) {
                    //data to delete is the only one in this  row
                    file_data_free(my_wc->hashtable[hash_index].next->data);
                    my_wc->hashtable[hash_index].data = NULL;
                    my_wc->hashtable[hash_index].next = NULL;
                    free(iter);
                } else {
                    struct component * cur = my_wc->hashtable[hash_index].next;
                    struct component * pre = my_wc->hashtable[hash_index].next;
                    //must be able to find the data if it exist in LRU link list
                    bool data_found = false;
                    while (cur != NULL && data_found == false) {
                        if (strcmp(cur->data->file_name, iter->data->file_name) == 0)
                            data_found = true;
                        else {
                            pre = cur;
                            cur = cur->next;
                        }
                    }
                    if (data_found == true) {
                        pre->next = cur->next;
                        file_data_free(cur->data);
                        free(cur);
                        free(iter);
                    }
                }
                iter = iter->next;
            } else {
                iter = iter->next;
                //printf("iter next name: %s", iter->data->file_name);
            }
        }
        if (iter == NULL)
            return false;
        else
            return true;
    }

}

struct sorted_data_entry * find_LRU(struct file_data * data) {
    struct sorted_data_entry * temp = head;
    bool finish = false;
    while (temp != NULL && finish != true) {
        if (strcmp(temp->data->file_name, data->file_name) == 0)
            finish = true;
        else
            temp = temp->next;
    }
    //assume calling on this function based on the definite existence of data
    if (finish == true) {
        //while loop terminate because find data
        printf("found in LRU\n");
        return temp;
    }//otherwise terminates because reaches the end of LRU
    else {
        printf("not found in LRU\n");
        return NULL;
    }
}

void hashtable_insert(struct file_data *data) {
    struct component* insert_entry = malloc(sizeof (struct component));
    insert_entry->data = data;
    insert_entry->next = NULL;
    bool evict_state = false;
    if (data->file_size > my_wc->max_cache_size) {
        free(insert_entry);
        return;
    } else if (data->file_size > my_wc->max_cache_size - my_wc->occupied_cache_size) {
        evict_state = evict_LRU_hash(data->file_size);
        if (evict_state == false)
            return;
    }
    //now space is available
    //compute hash key
    unsigned int hashtable_index;
    hashtable_index = stringToHash(data->file_name, (unsigned int) my_wc->hash_size);

    int hash_index = (int) hashtable_index;
    printf("insert: %d\n", hash_index);
    //check if this row is empty
    if (my_wc->hashtable[hash_index].next == NULL) {
        //insert new data at here
        my_wc->hashtable[hash_index].next = insert_entry;
        my_wc->hashtable[hash_index].next->next = NULL;
        //update used cache
        my_wc->occupied_cache_size = my_wc->occupied_cache_size + data->file_size;
    } else {
        //the first column's next is not empty, a linked list (at least one component)
        //must be present, insert at the last of the linked list
        struct component * iter;
        iter = my_wc->hashtable[hash_index].next;
        while (iter -> next != NULL) {
            iter = iter->next;
        }
        //iter get the pointer to the last component
        iter->next = insert_entry;
        //update used cache
        my_wc->occupied_cache_size = my_wc->occupied_cache_size + data->file_size;
    }
    //after insertion, need to update LRU
    //put current data pointer at the tail of the LRU linked list
    struct sorted_data_entry * new_entry = malloc(sizeof (struct sorted_data_entry));
    new_entry ->data = data;
    new_entry ->next = NULL;
    new_entry ->reference_count = 0;
    if (head == NULL) {
        //new_entry is now head
        head = new_entry;
        tail = new_entry;
    } else {
        tail->next = new_entry;
        tail = new_entry;
    }

    if (find_LRU(data) != NULL)
        printf("insert successful\n");
    else
        printf("insert unsuccessful\n");
}

//reason for using LRU_update_state is to differentiate first lookup and second
//lookup, in the second lookup LRU should not be updated because
//1. second lookup find the data, another thread must have updated LRU in insert
//2. second lookup didn't find data, current thread insert and update LRU

struct file_data * cache_lookup(struct file_data * data, int current_state) {
    unsigned int hashtable_index;
    hashtable_index = stringToHash(data->file_name, (unsigned int) my_wc->hash_size);
    int hash_index = (int) hashtable_index;
    printf("lookup: %d\n", hash_index);
    bool find = false;
    if (my_wc->hashtable[hash_index].next == NULL) {
        return NULL;
    } else {
        struct component * iter;
        iter = my_wc->hashtable[hash_index].next;
        //struct component current_component = my_wc->hashtable[hash_index];
        while (iter != NULL && find == false) {
            if (strcmp(iter->data->file_name, data->file_name) == 0) {
                //find same name
                find = true;
                if (current_state == update) {
                    //update LRU
                    //if this data exist in hash table, it must exit in LRU
                    set_LRU(data);
                } else {
                    //current_state == no_update
                    //no op
                }
                //return pointer to desired data
                return iter->data;
            } else {
                iter = iter->next;
            }
        }
    }
    return NULL;
}

static void
do_server_request(struct server *sv, int connfd) {
    //int ret;
    struct request *rq;
    struct file_data *data;

    data = file_data_init();

    /* fills data->file_name with name of the file being requested */
    rq = request_init(connfd, data);
    if (!rq) {
        file_data_free(data);
        return;
    }
    /* reads file, 
     * fills data->file_buf with the file contents,
     * data->file_size with file size. */

    //ret = request_readfile(rq); //this has conditions to load
    struct file_data * lookup_address = NULL;
    if (sv->max_cache_size > 0) {
        pthread_mutex_lock(&my_wc->cache_lock);
        lookup_address = cache_lookup(data, 1);
        pthread_mutex_unlock(&my_wc->cache_lock);
    }
    if (lookup_address != NULL) {
        //found data in cache
        pthread_mutex_lock(&my_wc->cache_lock);
       // data=lookup_address;
      //  printf("data file name : %s\n", data->file_name);
        request_set_data(rq, lookup_address);
        printf("found in cache, ");
        struct sorted_data_entry * LRU_address = find_LRU(lookup_address);
        LRU_address->reference_count++;
        pthread_mutex_unlock(&my_wc->cache_lock);
        request_sendfile(rq);
        pthread_mutex_lock(&my_wc->cache_lock);
        LRU_address->reference_count--;
        pthread_mutex_unlock(&my_wc->cache_lock);
    } else {
        //not found
        request_readfile(rq);
        request_sendfile(rq);
        if (sv->max_cache_size > 0) {
            pthread_mutex_lock(&my_wc->cache_lock);
            lookup_address = cache_lookup(data, 0);
            if (lookup_address == NULL) {
                hashtable_insert(data);
            } else {
                file_data_free(data);
            }
            pthread_mutex_unlock(&my_wc->cache_lock);
        }
    }

    //    if (!ret)
    //        goto out;
    //    /* sends file to client */
    //    request_sendfile(rq);
    //out:
    request_destroy(rq);
    //    file_data_free(data);
}

void
server_request(struct server *sv, int connfd) {
    if (sv->nr_threads == 0) { /* no worker threads */
        do_server_request(sv, connfd);
    } else {
        /*  Save the relevant info in a buffer and have one of the
         *  worker threads do the work. */
        pthread_mutex_lock(&sv->lock);
        while ((sv->in - sv->out + sv->max_requests + 1) % (sv->max_requests + 1) == sv->max_requests) {
            pthread_cond_wait(&sv->full, &sv->lock);
        }
        sv->buf[sv->in] = connfd;
        if (sv->in == sv->out) {
            pthread_cond_signal(&sv->empty);
        }
        sv->in = (sv->in + 1) % (sv->max_requests + 1);
        pthread_mutex_unlock(&sv->lock);
    }
}

void thread_init(void * arg) {
    //cast server structure
    struct server * sv = (struct server *) arg;
    while (1) {
        pthread_mutex_lock(&sv->lock);
        while (sv->in == sv-> out) {
            pthread_cond_wait(&sv->empty, &sv->lock);
        }
        int connfd = sv->buf[sv->out];
        if ((sv->in - sv->out + sv->max_requests + 1) %
                (sv->max_requests + 1) == sv->max_requests)
            pthread_cond_signal(&sv->full);
        sv->out = (sv->out + 1) % (sv->max_requests + 1);
        pthread_mutex_unlock(&sv->lock);
        do_server_request(sv, connfd);
    }
}

struct server *
server_init(int nr_threads, int max_requests, int max_cache_size) {
    struct server *sv;

    sv = (struct server *) malloc(sizeof (struct server));
    sv->nr_threads = nr_threads;
    sv->max_requests = max_requests;
    sv->max_cache_size = max_cache_size;
    pthread_mutex_init(&sv->lock, NULL);
    pthread_cond_init(&sv->full, NULL);
    pthread_cond_init(&sv->empty, NULL);

    if (nr_threads > 0 || max_requests > 0 || max_cache_size > 0) {
        if (max_requests > 0) {
            sv -> buf = (int *) malloc(sizeof (int) * (max_requests + 1));
            sv -> in = 0;
            sv -> out = 0;
        }
        if (nr_threads > 0) {
            sv->thread_array = (pthread_t *) malloc(sizeof (pthread_t) * nr_threads);
            int i = 0;
            for (i = 0; i < nr_threads; i++) {
                pthread_create(&sv->thread_array[i], NULL, (void *) &thread_init, (void *) sv);
            }
        }
    }

    /* Lab 4: create queue of max_request size when max_requests > 0 */

    /* Lab 5: init server cache and limit its size to max_cache_size */

    //initialize hash table

    my_wc = (struct wc *) malloc(sizeof (struct wc));
    my_wc->hash_size = 16777216;
    my_wc->hashtable = (struct component*) malloc(sizeof (struct component) * 16777216);
    my_wc->max_cache_size = max_cache_size;
    my_wc->occupied_cache_size = 0;
    pthread_mutex_init(&my_wc->cache_lock, NULL);

    //initialize linked list for LRU
    //my_list = (struct sorted_data_entry *) malloc(sizeof (struct sorted_data_entry));
    //my_list->next = NULL;
    //head and tail are already pointing to NULL 
    //when insert entries into the linked list, create at that time

    /* Lab 4: create worker threads when nr_threads > 0 */

    return sv;
}