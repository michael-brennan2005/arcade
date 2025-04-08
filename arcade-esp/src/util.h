#ifndef UTIL_H
#define UTIL_H

#include <math.h>

typedef struct rgb_t {
	int r;
	int g;
	int b;
} rgb_t;

rgb_t hsv2rgb(float H, float S, float V) {
	float r = 0.0;
    float g = 0.0;
    float b = 0.0;
	
	float h = H / 360;
	float s = S / 100;
	float v = V / 100;
	
	int i = floor(h * 6);
	float f = h * 6 - i;
	float p = v * (1 - s);
	float q = v * (1 - f * s);
	float t = v * (1 - (1 - f) * s);
	
	switch (i % 6) {
		case 0: r = v, g = t, b = p; break;
		case 1: r = q, g = v, b = p; break;
		case 2: r = p, g = v, b = t; break;
		case 3: r = p, g = q, b = v; break;
		case 4: r = t, g = p, b = v; break;
		case 5: r = v, g = p, b = q; break;
	}
	
	rgb_t color;
	color.r = r * 255;
	color.g = g * 255;
	color.b = b * 255;
	
	return color;
}

#endif