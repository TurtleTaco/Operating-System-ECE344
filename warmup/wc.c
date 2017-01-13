#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "common.h"
#include <ctype.h>
#include "wc.h"

//define global variables and struct for hash function
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

struct wc {
    /* you can define this struct to have whatever fields you want. */
    struct component *hashtable;
    unsigned int hash_size;
    //int *used_index_array;
    //int current_index_in_used_array;
};

struct component {
    char *key;
    int count;
    struct component * next; //default is not NULL initialize
};

char * parse_string(char *string_array, int* current_index, long whole_size) {
    //one extra space for NULL character
    int word_size = 1;
    int starting_index = *current_index;
    char *dest;

    //condition for proceeding: current char is space; current char is new line; current char is the last char in the list
    while (isspace(string_array [*current_index]) == 0 && string_array [*current_index] != '\0' && (*current_index != whole_size)) {
        (*current_index)++;
        word_size++;
    }

    //the current character is space, deal with consecutive spaces
    if (*current_index == starting_index)
        return NULL;

    //allocate new space for extracted string
    dest = (char *) malloc(sizeof (char)*word_size);
    strncpy(dest, &string_array[starting_index], word_size - 1);
    //add NULL char at the end, string ends with '\0'
    dest[word_size - 1] = '\0';
    return dest;
}

struct wc * wc_init(char *word_array, long size) {
    struct wc *my_wc;
    char *processed_string;
    my_wc = (struct wc *) malloc(sizeof (struct wc));
    assert(my_wc);

    //hash table initialization, choose the hash table size to be power of 2
    my_wc->hashtable = (struct component *) malloc(sizeof (struct component)*16777216);
    my_wc->hash_size = 16777216;

    //indexing through the whole string
    int current_index = 0;
    while (current_index != size) {
        processed_string = parse_string(word_array, &current_index, size);
        if (processed_string == NULL) {
            current_index++;
            continue;
        }

        //compute the hash value of the string, return value is a pointer, space already allocated
        unsigned int hash_value;
        hash_value = stringToHash(processed_string, my_wc->hash_size);
        struct component * pre_node = &((my_wc->hashtable)[hash_value]);
        struct component * current_node = pre_node->next;
        bool found = false;

        //link list insertion
        if (pre_node -> key == NULL) {
            //the first one is empty
            pre_node->key = processed_string;
            pre_node->count = 1;
        } else if (strcmp(pre_node->key, processed_string) == 0) {
            pre_node->count++;
            free(processed_string);
        } else {
            while (current_node != NULL && !found) {
                if (strcmp(current_node->key, processed_string) == 0) {
                    current_node->count++;
                    free(processed_string);
                    found = true;
                } else {
                    pre_node = current_node;
                    current_node = current_node->next;
                }
            }
            //if not found create new node at the end
            if (found == false) {
                pre_node->next = (struct component*) malloc(sizeof (struct component));
                pre_node->next->key = processed_string;
                pre_node->next->next = NULL;
                pre_node->next->count = 1;
            }
        }
        current_index++;
    }
    return my_wc;
}

void wc_output(struct wc *my_wc) {
    //indexing through the whole table 
    int table_index = 0;
    for (table_index = 0; table_index < 16777216; table_index++) {
        struct component* current = &(my_wc->hashtable)[table_index];
        if ((my_wc->hashtable)[table_index].key != NULL) {
            //output the first entry of the list
            printf("%s:%d\n", current->key, current->count);
            current = current->next;
            while (current != NULL) {
                //indexing through the whole link list
                printf("%s:%d\n", current->key, current->count);
                current = current->next;
            }
        }
    }
}

void wc_destroy(struct wc *my_wc) {
    int i = 0;

    //free memory of all linked list including keys
    for (i = 0; i < 16777216; i++) {
        if (my_wc->hashtable[i].next != NULL) {
            struct component * current_pointer = my_wc->hashtable[i].next;
            struct component * next_pointer;
            while (current_pointer != NULL) {
                next_pointer = current_pointer->next;
                free(current_pointer->key);
                free(current_pointer);
                current_pointer = next_pointer;
            }
        }
    }

    //delete the keys in the first row
    for (i = 0; i < 16777216; i++) {
        free(my_wc->hashtable[i].key);
    }

    //the table is being allocated as a whole chunk of memory, free all at once
    free(my_wc->hashtable);
    free(my_wc);
    my_wc = NULL;
    //check for free error and memory leak
    assert(my_wc == NULL);
}


