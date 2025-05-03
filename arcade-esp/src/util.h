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

typedef struct hsv_t {
    float h;
    float s; 
    float v;  
} hsv_t;

hsv_t rgb2hsv(int R, int G, int B) {
    // Convert RGB values to 0-1 range
    float r = R / 255.0;
    float g = G / 255.0; 
    float b = B / 255.0;

    // Find min and max RGB values
    float cmax = fmax(r, fmax(g, b));
    float cmin = fmin(r, fmin(g, b));
    float delta = cmax - cmin;

    hsv_t hsv;
    
    // Calculate hue (in degrees)
    if (delta == 0) {
        hsv.h = 0;
    } else if (cmax == r) {
        hsv.h = 60 * fmod(((g - b) / delta), 6);
    } else if (cmax == g) {
        hsv.h = 60 * (((b - r) / delta) + 2);
    } else {
        hsv.h = 60 * (((r - g) / delta) + 4);
    }

    // Ensure hue is positive
    if (hsv.h < 0) {
        hsv.h += 360;
    }

    // Calculate saturation (in percent)
    hsv.s = (cmax == 0) ? 0 : (delta / cmax) * 100;

    // Calculate value (in percent) 
    hsv.v = cmax * 100;

    return hsv;
}

#endif