/*
 * Platform-agnostic DSP primitives ported from the original PSDR
 * Source/src/main.c (Michael R. Colton). The math is unchanged; only
 * the buffer layout and globals were lifted into explicit arguments.
 *
 * Complex buffers are CMSIS-DSP convention: interleaved float32
 * pairs [re, im, re, im, ...], length 2 * FFT_SIZE.
 */

#ifndef PSDR_DSP_H_
#define PSDR_DSP_H_

#include <stdint.h>

#define PSDR_FFT_SIZE     256u
#define PSDR_FFT_BUFLEN   (2u * PSDR_FFT_SIZE)
#define PSDR_OVERLAP_MAX  200u

enum psdr_sideband {
	PSDR_SIDEBAND_LSB = 0,
	PSDR_SIDEBAND_USB = 1,
	PSDR_SIDEBAND_AM  = 2,
};

void psdr_dsp_init(void);

/* Build a brick-wall filter mask in the frequency domain. */
void psdr_dsp_set_filter(int bandwidth, enum psdr_sideband sb, int offset);

/* In-place complex multiply samples[] by the current filter mask. */
void psdr_dsp_apply_filter(float *samples, int shift);

/* Sum-of-squares RMS over `length` floats. */
float psdr_dsp_rms(const float *samples, int length);

/* Zero a sample bank and copy the saved overlap region back in. */
void psdr_dsp_zero_with_overlap(float *bank, const float *overlap,
				int filter_kernel_len);

/* Inject a synthetic frequency-domain impulse — bin set to (0.9, 0.9). */
void psdr_dsp_fft_impulse(float *samples, int bin);

/* Run forward FFT → filter → inverse FFT → optional AM magnitude. */
void psdr_dsp_process(float *bank, int if_shift, enum psdr_sideband sb,
		      int am_demod);

/* Convenience: roundtrip an impulse and return the time-domain RMS.
 * Used as a boot-time self-test. */
float psdr_dsp_self_test(void);

#endif /* PSDR_DSP_H_ */
