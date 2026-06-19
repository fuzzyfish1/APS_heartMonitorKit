//
// Created by fuzzyfish on 6/17/26.
//

#ifndef BIQUAD_H
#define BIQUAD_H

struct Biquad {
	float b0, b1, b2, a1, a2, z1, z2;
};

/**
 * Biquad Filter removes and amplifies certain frequencies in the Sample-Time Domain (before FFT)
 * So they don't show up in the frequency-Amplitude domain (after FFT)
 * works internally with math so study hard
 * @param bqStr - a place to store persistent information
 * @param x - the sampling imput
 * @return - new sampling input, with certain frequencies amplified and other frequencies damped
 */
float biquadStep(Biquad& bqStr, const float& x);

#endif //BIQUAD_H