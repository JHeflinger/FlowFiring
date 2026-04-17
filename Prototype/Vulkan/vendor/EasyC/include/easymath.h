#ifndef EASYMATH_H
#define EASYMATH_H

#define EZ_CLAMP(x, min, max) ((x) < min ? min : ((x) > max ? max : (x)))
#define EZ_DISTANCE(x1, y1, x2, y2) ez_distance((float)(x1), (float)(y1), (float)(x2), (float)(y2))

float ez_distance(float x1, float y1, float x2, float y2);

#endif
