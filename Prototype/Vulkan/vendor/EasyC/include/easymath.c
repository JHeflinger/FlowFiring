#include "easymath.h"
#include <math.h>

float ez_distance(float x1, float y1, float x2, float y2) {
	double a = x1 - x2;
	double b = y1 - y2;
	double csq = a*a + b*b;
	return (float)sqrt(csq);
}
