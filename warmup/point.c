#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "common.h"
#include "point.h"

void
point_translate(struct point *p, double x, double y)
{
	p->x = p->x + x;
        p->y = p->y + y;
}

double
point_distance(const struct point *p1, const struct point *p2)
{
	double delta_x, delta_y;
        double distance;
        delta_x = abs(p1->x - p2->x);
        delta_y = abs(p1->y - p2->y);
        distance = sqrt(delta_x*delta_x + delta_y*delta_y);
	return distance;
}

int
point_compare(const struct point *p1, const struct point *p2)
{
	double distance_1, distance_2;
        distance_1 = sqrt((p1->x)*(p1->x) + (p1->y)*(p1->y));
        distance_2 = sqrt((p2->x)*(p2->x) + (p2->y)*(p2->y));
        if (distance_1 < distance_2)
           return -1;
        if (distance_1 == distance_2)
           return 0;
        if (distance_1 > distance_2)
           return 1;
        return 0;
}
