#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "common.h"
#include "point.h"
#include "sorted_points.h"

struct sorted_points {
    /* you can define this struct to have whatever fields you want. */
    struct point* point;
    struct sorted_points* next;
};

struct sorted_points *
sp_init() {
    struct sorted_points *sp;

    sp = (struct sorted_points *) malloc(sizeof (struct sorted_points));
    assert(sp);

    sp->next = NULL;

    return sp;
}

//testing function for debugging

void print_list(struct sorted_points *sp) {
    printf("____________________________\n");
    struct sorted_points *temp = sp->next;
    while (temp != NULL) {
        printf("(%f", temp->point->x);
        printf(",%f) -> ", temp->point->y);
        temp = temp->next;
    }
    printf("____________________________\n");
}

void
sp_destroy(struct sorted_points *sp) {
    struct sorted_points * pre = sp;
    struct sorted_points * cur = sp->next;
    if (cur == NULL) {
        //if no element
        free(pre);
        return;
    }

    //if there are elements, loop through the whole list with two pointers
    do {
        free(pre->point);
        free(pre);
        pre = cur;
        cur = cur->next;
    } while (cur != NULL);
    free(pre->point);
    free(pre);
}

int
sp_add_point(struct sorted_points *sp, double x, double y) {
    struct sorted_points * new_sp;
    new_sp = sp_init();
    new_sp->point = (struct point *) malloc(sizeof (struct point));
    point_set(new_sp->point, x, y);
    if (sp->next == NULL) {
        //this is the first one
        sp->next = new_sp;
        return 1;
    } else {
        //this is not the first one, find the correct position
        struct sorted_points *current_pointer = sp->next;
        struct sorted_points *pre_pointer = sp;
        int compare_result;
        compare_result = point_compare(new_sp->point, current_pointer->point);
        while (compare_result != -1) {
            //if this is not the last node, indexing into the next node
            if (current_pointer -> next != NULL) {
                pre_pointer = current_pointer;
                current_pointer = current_pointer ->next;
                compare_result = point_compare(new_sp->point, current_pointer->point);
            } else break;
        }

        if (compare_result == 0) {
            //same distance
            if ((new_sp->point->x == current_pointer->point->x)&&(new_sp->point->y == current_pointer->point->y)) {
                pre_pointer->next = new_sp;
                new_sp->next = current_pointer;
                return 1;
            }
            if (new_sp->point->x < current_pointer->point->x) {
                pre_pointer->next = new_sp;
                new_sp->next = current_pointer;
                return 1;
            } else if (new_sp->point->x > current_pointer->point->x) {
                new_sp->next = current_pointer->next;
                current_pointer->next = new_sp;
                return 1;
            } else if (new_sp->point->y < current_pointer->point->y) {
                pre_pointer->next = new_sp;
                new_sp->next = current_pointer;
                return 1;
            } else {
                new_sp->next = current_pointer->next;
                current_pointer->next = new_sp;
                return 1;
            }
        } else if (compare_result == 1) {
            //larger then current_pointer
            new_sp->next = current_pointer->next;
            current_pointer->next = new_sp;
            return 1;
        } else if (compare_result == -1) {
            //smaller than the current pointer
            pre_pointer->next = new_sp;
            new_sp->next = current_pointer;
            return 1;
        }
    }
    return 0;
}

int
sp_remove_first(struct sorted_points *sp, struct point *ret) {
    //if there is no element
    if (sp->next == NULL)
        return 0;
    ret->x = sp->next->point->x;
    ret->y = sp->next->point->y;
    struct sorted_points * temp;
    temp = sp->next;
    sp->next = sp->next->next;
    free(temp->point);
    free(temp);
    return 1;
}

int
sp_remove_last(struct sorted_points *sp, struct point *ret) {
    //if there is no element
    if (sp->next == NULL)
        return 0;
    struct sorted_points *pre = sp;
    struct sorted_points *cur = sp->next;
    while (cur->next != NULL) {
        pre = cur;
        cur = cur->next;
    }
    //very important
    ret->x = cur->point->x;
    ret->y = cur->point->y;
    pre->next = NULL;
    free(cur->point);
    free(cur);
    return 1;
}

int
sp_remove_by_index(struct sorted_points *sp, int index, struct point *ret) {
    //the first index 0 is the first valid point
    int current_index = 0;
    if (sp->next == NULL)
        return 0;
    struct sorted_points *pre = sp;
    struct sorted_points *cur = sp->next;
    while ((cur->next != NULL) && (current_index != index)) {
        if (current_index != index) {
            pre = cur;
            cur = cur->next;
            current_index++;
        }
    }
    if (current_index != index)
        return 0;
    else {
        if (cur->next == NULL) {
            //is deleting the last one
            ret->x = cur->point->x;
            ret->y = cur->point->y;
            pre->next = NULL;
            free(cur->point);
            free(cur);
        } else {
            //is deleting a middle one
            ret->x = cur->point->x;
            ret->y = cur->point->y;
            pre->next = cur->next;
            free(cur->point);
            free(cur);
        }
    }
    return 1;
}

int
sp_delete_duplicates(struct sorted_points *sp) {
    //keep track of ther number of duplicate
    int number_records = 0;

    if (sp->next == NULL)
        return 0;
    if (sp->next->next == NULL)
        //only one element
        return 0;
    struct sorted_points *pre = sp->next;
    struct sorted_points *cur = sp->next->next;
    while (cur != NULL) {
        if ((pre->point->x) != (cur->point->x) || (pre->point->y) != (cur->point->y)) {
            //not the same point
            pre = cur;
            cur = cur->next;
        } else if ((pre->point->x == cur->point->x) & (pre->point->y == cur->point->y)) {
            //the same point
            cur = cur->next;
            free(pre->next->point);
            free(pre->next);
            pre->next = cur;
            number_records++;
        }
    }
    return number_records;
}
