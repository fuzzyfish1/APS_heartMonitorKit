//
// Created by fuzzyfish on 6/17/26.
//
#include <biquad.h>

#ifndef BIQUAD_CPP
#define BIQUAD_CPP

float biquadStep(Biquad& bqStr, const float& x) {
	const float y = bqStr.b0 * x + bqStr.z1;
	bqStr.z1 = bqStr.b1 * x - bqStr.a1 * y + bqStr.z2;
	bqStr.z2 = bqStr.b2 * x - bqStr.a2 * y;
	return y;
}

#endif //BIQUAD_CPP