// Copyright Epic Games Tools, LLC. All Rights Reserved.

#ifndef RADAUDIO_ENCODER_NEON_H
#define RADAUDIO_ENCODER_NEON_H

#include "radaudio_common.h"

#ifdef RADAUDIO_WRAP
#define WRAPPED_NAME(name) RR_STRING_JOIN(RADAUDIO_WRAP, name##_)

#define radaudio_neon_compute_best_quantized_coeff16_loop	WRAPPED_NAME(radaudio_neon_compute_best_quantized_coeff16_loop)
#define radaudio_neon_find_median_4th_of_8                  WRAPPED_NAME(radaudio_neon_find_median_4th_of_8)
#define radaudio_neon_find_median_8th_of_16                 WRAPPED_NAME(radaudio_neon_find_median_8th_of_16)
#endif


extern void radaudio_neon_compute_best_quantized_coeff16_loop(S16 best_quantized[], F32 best_so_far, F32 ncoeff[], F32 quantizer, F32 step_quantizer, int num_quantizers);

extern int radaudio_neon_find_median_4th_of_8(S16 *data);
extern int radaudio_neon_find_median_8th_of_16(S16 *data);

#endif
