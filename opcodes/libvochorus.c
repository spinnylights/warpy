/*
 * This file is part of Warpy.
 *
 * Warpy is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Warpy is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Warpy.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <math.h>
#include <stdbool.h>
#include <pthread.h>

#include <fftw3.h>
#include <csound/csdl.h>

#include "hann_window.h"
#include "chorus_scales.h"

#define MAX_OUTS 2

#define HALF_N 2048
#define MAX_POLY 30
#define MAX_CHORUS_VOICES 6

#define LEFT_ONLY 0
#define RIGHT_ONLY 1
#define BOTH_CHANNELS 2

static const unsigned N                    = 4096;
static const unsigned half_N               = 2048;
static const unsigned decim                = 8;
static const unsigned hop_size             = N / decim;
static const size_t   out_frames_size      = decim * sizeof(double) * N;
static const size_t   fft_win_size         = sizeof(double) * N;
static const size_t   max_chorus_scale_val = CHORUS_SCALES_LEN - 1;
static const double   threeqtr_pi          = M_PI_4 * 3;

struct warpy_chorus_voice {
	double*             fwin;
	double*             bwin;
	double*             pwin;
	struct fftw_plan_s* fft_fwin_forw;
	struct fftw_plan_s* fft_fwin_back;
	struct fftw_plan_s* fft_bwin_forw;
	double              max_detune;
	double              max_pan;
};

struct warpy_fft_machinery {
	bool    in_use;
	double* fwin;
	double* bwin;
	double* pwin;
	struct  fftw_plan_s*  fft_fwin_forw;
	struct  fftw_plan_s*  fft_fwin_back;
	struct  fftw_plan_s*  fft_bwin_forw;
	struct  warpy_chorus_voice* chor_voices;
};

void init_warpy_fft(struct warpy_fft_machinery* fft_mach)
{
	fft_mach->in_use = false;

	fft_mach->fwin          = fftw_malloc(fft_win_size);
	fft_mach->bwin          = fftw_malloc(fft_win_size);
	fft_mach->pwin          = fftw_malloc(fft_win_size);
	fft_mach->fft_fwin_forw = fftw_plan_r2r_1d(N,
	                                           fft_mach->fwin,
	                                           fft_mach->fwin,
	                                           FFTW_R2HC,
	                                           FFTW_PATIENT);
	fft_mach->fft_fwin_back = fftw_plan_r2r_1d(N,
	                                           fft_mach->fwin,
	                                           fft_mach->fwin,
	                                           FFTW_HC2R,
	                                           FFTW_PATIENT);
	fft_mach->fft_bwin_forw = fftw_plan_r2r_1d(N,
	                                           fft_mach->bwin,
	                                           fft_mach->bwin,
	                                           FFTW_R2HC,
	                                           FFTW_PATIENT);

	const double max_detunes[] = { 0.1191221,  -0.11952356,
	                               0.16216538, -0.16288439,
	                               0.21045242, -0.20702313 };

	const double max_pans[] =    { 0.75,        0.25,
	                               1.0/3.0,     2.0/3.0,
	                               0.5,         0.5        };

	fft_mach->chor_voices = malloc(sizeof(struct warpy_chorus_voice) *
	                               MAX_CHORUS_VOICES);

	for (size_t i = 0; i < MAX_CHORUS_VOICES; i++) {
		struct warpy_chorus_voice* voice =
		        (struct warpy_chorus_voice*)&fft_mach->chor_voices[i];

		voice->fwin          = fftw_malloc(fft_win_size);
		voice->bwin          = fftw_malloc(fft_win_size);
		voice->pwin          = fftw_malloc(fft_win_size);
		voice->fft_fwin_forw = fftw_plan_r2r_1d(N,
		                                           voice->fwin,
		                                           voice->fwin,
		                                           FFTW_R2HC,
		                                           FFTW_PATIENT);
		voice->fft_fwin_back = fftw_plan_r2r_1d(N,
		                                           voice->fwin,
		                                           voice->fwin,
		                                           FFTW_HC2R,
		                                           FFTW_PATIENT);
		voice->fft_bwin_forw = fftw_plan_r2r_1d(N,
		                                           voice->bwin,
		                                           voice->bwin,
		                                           FFTW_R2HC,
		                                           FFTW_PATIENT);

		voice->max_detune = max_detunes[i];
		voice->max_pan    = max_pans[i];
	}
}

struct voc_chorus {
	struct opds          h;
	double*              out[MAX_OUTS];
	double*              seek_point;
	double*              pitch_arg;
	double*              table_no;
	double*              no_of_c_voices_arg;
	double*              mix;
	double*              detune;
	double*              spread;
	double*              main_channel_pan;

	bool                 first_run;
	uint32_t             output_arg_cnt;
	size_t               out_frames_index_seek;
	size_t               up_to_hop_size;
	struct auxch         out_frames_index;
	struct auxch         out_frames_center;
	struct auxch         out_frames_chor_l;
	struct auxch         out_frames_chor_r;

	struct warpy_fft_machinery* fft_mach;

	uint64_t             env_samp_rate;
	double*              sample;
	size_t               sample_len;
	int64_t              sample_seek;
	double               rate_adjust;
	double               pitch;
	size_t               no_of_c_voices;
};

static void init_out_frame(struct auxch* out_field,
                           struct CSOUND_* const csound)
{
	const size_t out_field_size = out_field->size;
	if (out_field->auxp == NULL || out_field_size < out_frames_size)
		csound->AuxAlloc(csound, out_frames_size, out_field);
}

static inline void init_out_frames(struct voc_chorus* p,
                                   struct CSOUND_* const csound)
{
	struct auxch* center = &p->out_frames_center;
	struct auxch* chor_l = &p->out_frames_chor_l;
	struct auxch* chor_r = &p->out_frames_chor_r;
	init_out_frame(center, csound);
	init_out_frame(chor_l, csound);
	init_out_frame(chor_r, csound);
}

static void init_out_frames_indices(struct voc_chorus* p,
                                    struct CSOUND_* const csound)
{
	// used to add N samples to an out_frame array and to take N hop-sized
	// sections of samples separated by decim from an out_frame array
	struct auxch* out_frames_index = &p->out_frames_index;
	const size_t out_frames_index_size = out_frames_index->size;
	const size_t out_frames_indices = decim * sizeof(size_t);
	if (out_frames_index->auxp == NULL ||
	    out_frames_index_size < out_frames_indices)
		csound->AuxAlloc(csound,
		                 out_frames_indices,
		                 out_frames_index);

	size_t* const init_out_frames_index_contents =
		(size_t*)out_frames_index->auxp;
	for (size_t i = 0; i < decim; i++) {
		size_t initial_index = i * N;
		init_out_frames_index_contents[i] = initial_index;
	}
}

static int32_t deinit_voc_chorus(struct CSOUND_* const csound, void* op)
{
	const void* const safe_op = op;
	struct voc_chorus* p = (struct voc_chorus*)safe_op;

	p->fft_mach->in_use = false;

	//_exit(0);
	return OK;
}

static int32_t init_voc_chorus(struct CSOUND_* const csound,
                               struct voc_chorus* p)
{
	//printf("start clock: %2.8f\n", (double)clock() / CLOCKS_PER_SEC);
	uint32_t output_arg_cnt = csound->GetOutputArgCnt(p);
	p->output_arg_cnt = output_arg_cnt;

	struct warpy_fft_machinery* fft_machs =
	        (struct warpy_fft_machinery*)
	        csound->QueryGlobalVariable(csound, "warpfft");
	struct warpy_fft_machinery* fft_mach;
	bool mach_in_use = true;
	for (size_t i = 0; i < MAX_POLY; i++) {
		fft_mach = &fft_machs[i];
		mach_in_use = fft_mach->in_use;
		if (!mach_in_use)
			break;
	}
	if (mach_in_use) {
		fprintf(stderr, "WARPY WARN: polyphony limit exceeded\n");
		p->fft_mach = NULL;
	}
	else {
		fft_mach->in_use = true;
		p->fft_mach = fft_mach;
	}

	init_out_frames(p, csound);
	init_out_frames_indices(p, csound);

	p->out_frames_index_seek = 0;
	p->up_to_hop_size = 0;
	p->first_run = true;
	p->sample = NULL;
	p->no_of_c_voices = 0;

	csound->RegisterDeinitCallback(csound, p, &deinit_voc_chorus);

	//printf("end clock: %2.8f\n", (double)clock() / CLOCKS_PER_SEC);
	return OK;
}

static inline int32_t sample_accurate_check(struct voc_chorus* p,
                                            const uint32_t offset)
{
	const uint32_t early = p->h.insdshead->ksmps_no_end;
	int32_t n = CS_KSMPS;
	const uint32_t outputs = p->output_arg_cnt;
	double* out_chn;

	if (UNLIKELY(early)) {
		n -= early;
		for (int i = 0; i < outputs; i++) {
			out_chn = p->out[i];
			memset(&out_chn[n], '\0', early*sizeof(double));
		}
	}
	if (UNLIKELY(offset)) {
		for (int i = 0; i < outputs; i++) {
			out_chn = p->out[i];
			memset(out_chn, '\0', offset*sizeof(double));
		}
	}

	return n;
}

static size_t chorus_scales_index(const double scale_val_arg)
{
	double scale_val;
	if (scale_val_arg < 0)
		scale_val = 0;
	else if (scale_val_arg > 1)
		scale_val = 1;
	else
		scale_val = scale_val_arg;

	size_t scale_index = scale_val * max_chorus_scale_val;
	return scale_index;
}

static double get_chorus_detune(const double detune)
{
	size_t index = chorus_scales_index(detune);
	double scaled_detune = chorus_detune_scale[index];
	return scaled_detune;
}

static double get_chorus_mix_center(const double mix)
{
	size_t index = chorus_scales_index(mix);
	double scaled_mix = chorus_mix_center_scale[index];
	return scaled_mix;
}

static double get_chorus_mix_sides(const double mix)
{
	size_t index = chorus_scales_index(mix);
	double scaled_mix = chorus_mix_side_scale[index];
	return scaled_mix;
}

static inline void run_forward_ffts(struct voc_chorus* const p)
{
	fftw_execute(p->fft_mach->fft_fwin_forw);
	fftw_execute(p->fft_mach->fft_bwin_forw);

	for (size_t i = 0; i < MAX_CHORUS_VOICES; i++) {
		if (p->no_of_c_voices > i) {
			struct warpy_chorus_voice* voice =
			        &p->fft_mach->chor_voices[i];
			fftw_execute(voice->fft_fwin_forw);
			fftw_execute(voice->fft_bwin_forw);
		}
	}
}

static inline int64_t check_samp_seek_bounds(const int64_t sample_seek,
                                             const int64_t sample_len)
{
	int64_t adj_sample_seek = sample_seek;
	while (adj_sample_seek > sample_len)
		adj_sample_seek -= sample_len;
	while (adj_sample_seek <= 0)
		adj_sample_seek += sample_len;

	return adj_sample_seek;
}

static inline void check_win_seek_bounds(int64_t* seek_pos,
                                         const int64_t sample_len)
{
	while (*seek_pos < 0)
		*seek_pos += sample_len;
	while (*seek_pos >= sample_len)
		*seek_pos -= sample_len;
}

static void fill_win_bins(double* fwin,
                          double* bwin,
                          double sample_seek,
                          double pitch,
                          struct voc_chorus* p)
{
	for (size_t i = 0; i < N; i++) {
		int64_t fwin_read_pos = round(sample_seek);
		const double interpolation = fabs(sample_seek - fwin_read_pos);
		check_win_seek_bounds(&fwin_read_pos, p->sample_len);
		const int64_t round_pitch = round(pitch);
		int64_t next_pos = fwin_read_pos + round_pitch;
		check_win_seek_bounds(&next_pos, p->sample_len);
		const double this_sample = p->sample[fwin_read_pos];
		fwin[i] = (this_sample + interpolation *
		          (this_sample - p->sample[next_pos])) *
		          hann_window[i];

		int64_t bwin_read_pos = fwin_read_pos - hop_size * pitch;
		check_win_seek_bounds(&bwin_read_pos, p->sample_len);
		int64_t next_bpos = bwin_read_pos + round_pitch;
		check_win_seek_bounds(&next_bpos, p->sample_len);
		const double this_bsample = p->sample[bwin_read_pos];
		bwin[i] = (this_bsample + interpolation *
		          (this_bsample - p->sample[next_bpos])) *
		          hann_window[i];

		sample_seek += pitch;
	}
}

static void fill_bins(struct voc_chorus* const p, const size_t n)
{
	const double seek_point = p->seek_point[n];
	const double rate_adjust = p->rate_adjust;
	const double seek_time = seek_point * rate_adjust;
	const double env_samp_rate = p->env_samp_rate;
	const int64_t sample_seek_in_hops =
	        (int64_t)(seek_time * env_samp_rate / hop_size);
	const double sample_seek_raw = hop_size * sample_seek_in_hops;

	double sample_seek =
	        check_samp_seek_bounds(sample_seek_raw, p->sample_len);
	fill_win_bins(p->fft_mach->fwin,
	              p->fft_mach->bwin,
		      sample_seek,
		      p->pitch,
		      p);
	for (size_t i = 0; i < MAX_CHORUS_VOICES; i++) {
		if (p->no_of_c_voices > i) {
			struct warpy_chorus_voice* voice =
				&p->fft_mach->chor_voices[i];
			fill_win_bins(voice->fwin,
			              voice->bwin,
			              sample_seek,
			              (voice->max_detune) *
			                      get_chorus_detune(*p->detune) +
			                      p->pitch,
			              p);

		}
	}
}

static inline double atan2_approx(double y, double x)
{
	//http://pubs.opengroup.org/onlinepubs/009695399/functions/atan2.html
	//Volkan SALMA
	//https://gist.github.com/volkansalma/2972237

	double r, angle;
	double abs_y = fabs(y) + 1e-10f;      // kludge to prevent 0/0 condition
	if ( x < 0.0f )
	{
		r = (x + abs_y) / (abs_y - x);
		angle = threeqtr_pi;
	}
	else
	{
		r = (x - abs_y) / (x + abs_y);
		angle = M_PI_4;
	}
	angle += (0.1963f * r * r - 0.9817f) * r;
	if ( y < 0.0f )
		return( -angle );     // negate if in quad III or IV
	else
		return( angle );
}

static void smoothe_phase(const double* const pwin_fft, double* const bwin)
{
	for (size_t i = 0; i <= half_N; i++) {
		const bool at_edges = i == 0 || i == half_N;
		const size_t imag_index = N - i;
		double pwin_fft_comp[2];
		pwin_fft_comp[0] = pwin_fft[i];
		if (at_edges)
			pwin_fft_comp[1] = pwin_fft[imag_index];
		else
			pwin_fft_comp[1] = 0;

		if (pwin_fft_comp[0] == 0 && pwin_fft_comp[1] == 0)
			continue;

		const double pwin_pangle = atan2_approx(pwin_fft_comp[0],
		                                        pwin_fft_comp[1]);
		const double pwin_angle_sin = sin(pwin_pangle);
		const double pwin_angle_cos = cos(pwin_pangle);

		bwin[i] = bwin[i] * pwin_angle_sin +
		          bwin[i] * pwin_angle_cos;
		if (!at_edges)
			bwin[imag_index] = bwin[imag_index] * pwin_angle_sin +
			                   bwin[imag_index] * pwin_angle_cos;

	}
}

static void vocode_voice(double* const fwin,
                         double* const bwin,
                         double* const pwin,
                         const uint32_t sample_n)
{
	smoothe_phase(pwin, bwin);
	for (size_t i = 0; i <= half_N; i++) {
		const size_t imag_index = N - i;
		double plocked_bcomp[] = {0,0};
		switch (i) {
			case 0:
				plocked_bcomp[0] = bwin[i] +
				                   bwin[i + 1];
				break;
			case HALF_N:
				plocked_bcomp[0] = bwin[i] +
				                   bwin[i - 1];
				break;
			default:
				plocked_bcomp[0] = bwin[i] +
				                   bwin[i + 1] +
				                   bwin[i - 1];
				plocked_bcomp[1] = bwin[imag_index] +
				                   bwin[imag_index + 1] +
				                   bwin[imag_index - 1];
		}
		const bool at_edges = i == 0 || i == half_N;
		const double bwin_phase_ang = atan2_approx(plocked_bcomp[0],
		                                           plocked_bcomp[1]);
		const double bwin_phase_ang_sin = sin(bwin_phase_ang);
		const double bwin_phase_ang_cos = cos(bwin_phase_ang);
		fwin[i] = bwin_phase_ang_sin * fwin[i] +
			      bwin_phase_ang_cos * fwin[i];
		pwin[i] = fwin[i];
		if (!at_edges) {
			fwin[N-i] = bwin_phase_ang_sin *
			                fwin[imag_index] +
			                bwin_phase_ang_cos *
			                fwin[imag_index];
			pwin[imag_index] = fwin[imag_index];
		}
	}
}

static void vocode(struct voc_chorus* p,
                   const uint32 sample_n)
{
	vocode_voice(p->fft_mach->fwin,
	             p->fft_mach->bwin,
	             p->fft_mach->pwin,
	             sample_n);
	for (size_t i = 0; i < MAX_CHORUS_VOICES; i++)
	{
		if (p->no_of_c_voices > i) {
			struct warpy_chorus_voice* voice =
			        &p->fft_mach->chor_voices[i];
			vocode_voice(voice->fwin,
			             voice->bwin,
			             voice->pwin,
			             sample_n);
		}
	}
}

static inline void run_backwards_fft(double* const win,
                                     struct fftw_plan_s* const plan)
{
	fftw_execute(plan);
	// FFTW backwards FFT output is scaled up by N
	for (size_t i = 0; i < N; i++)
		win[i] /= N;
}

static void run_backwards_ffts(struct voc_chorus* p)
{
	run_backwards_fft(p->fft_mach->fwin,
	                  p->fft_mach->fft_fwin_back);
	for (size_t i = 0; i < MAX_CHORUS_VOICES; i++) {
		if (p->no_of_c_voices > i) {
			struct warpy_chorus_voice* voice =
			        &p->fft_mach->chor_voices[i];
			run_backwards_fft(voice->fwin,
			                  voice->fft_fwin_back);
		}
	}
}

static void write_to_out_frames(struct voc_chorus* const p,
                                const size_t output_start_pos)
{
	double* const center_out_frames = (double*)(p->out_frames_center.auxp);
	for (size_t i = 0; i < N; i++)
		center_out_frames[output_start_pos + i] =
		        p->fft_mach->fwin[i] * hann_window[i];

	double* const side_out_frames_l = (double*)p->out_frames_chor_l.auxp;
	double* const side_out_frames_r = (double*)p->out_frames_chor_r.auxp;
	for (size_t i = 0; i < MAX_CHORUS_VOICES; i++) {
		if (p->no_of_c_voices > i) {
			struct warpy_chorus_voice* voice =
			        &p->fft_mach->chor_voices[i];
			if (p->output_arg_cnt == 1) {
				for (size_t j = 0; j < N; j++) {
					side_out_frames_l[output_start_pos+j] =
					        voice->fwin[j] * hann_window[j];
				}
			}
			else {
				const double max_pan = voice->max_pan;
				const double spread = *p->spread;
				const double pan =
					(spread * (max_pan - 0.5) + 0.5) *
					M_PI_2;
				const double left_pan = (double)cos(pan);
				const double right_pan = (double)sin(pan);
				for (size_t j = 0; j < N; j++) {
					const double sample =
					        voice->fwin[j] * hann_window[j];
					const double l_sample = sample * left_pan;
					const double r_sample = sample * right_pan;
					side_out_frames_l[output_start_pos+j] =
					        l_sample;
					side_out_frames_r[output_start_pos+j] =
					        r_sample;
				}
			}
		}
	}
}


static size_t set_output_start_pos(struct voc_chorus* const p)
{
	const size_t pos = p->out_frames_index_seek * N;
	size_t* const out_frames_index = (size_t*)p->out_frames_index.auxp;
	out_frames_index[p->out_frames_index_seek] = pos;
	return pos;
}

static void reset_counters(struct voc_chorus* const p)
{
	p->up_to_hop_size = 0;
	size_t out_frames_index_seek = p->out_frames_index_seek + 1;
	if (out_frames_index_seek == decim)
		out_frames_index_seek = 0;
	p->out_frames_index_seek = out_frames_index_seek;
}

static void write_to_output(struct voc_chorus* const p,
                            const size_t n)
{
	const uint32_t output_arg_cnt = p->output_arg_cnt;
	const double mix_arg = *p->mix;
	const double center_mix = get_chorus_mix_center(mix_arg);
	const double sides_mix = get_chorus_mix_sides(mix_arg);
	size_t* const out_frames_index =
	        (size_t*)p->out_frames_index.auxp;

	for (size_t channel = 0; channel < output_arg_cnt; channel++) {
		double* const out_channel = p->out[channel];
		out_channel[n] = 0;
		for (size_t hop = 0; hop < decim; hop++) {
			const double* const to_out_center =
			        (double*)p->out_frames_center.auxp;
			double sample = to_out_center[out_frames_index[hop]];
			if (p->no_of_c_voices > 0 && *p->mix > 0)
				sample *= center_mix;
			if (*p->main_channel_pan == BOTH_CHANNELS)
				out_channel[n] += sample;
			else {
				if (channel == 0) {
					const double* const to_out_sides_l =
					     (double*)p->out_frames_chor_l.auxp;
					double l_sample =
					 to_out_sides_l[out_frames_index[hop]] *
					 sides_mix;
					if (*p->main_channel_pan != BOTH_CHANNELS)
						l_sample /= 2;
					out_channel[n] += l_sample;
					if (*p->main_channel_pan == LEFT_ONLY)
						out_channel[n] += sample;
				}
				else {
					const double* const to_out_sides_r =
					     (double*)p->out_frames_chor_r.auxp;
					double r_sample =
					 to_out_sides_r[out_frames_index[hop]] *
					 sides_mix;
					if (*p->main_channel_pan != BOTH_CHANNELS)
						r_sample /= 2;
					out_channel[n] += r_sample;
					if (*p->main_channel_pan == RIGHT_ONLY)
						out_channel[n] += sample;
				}
			}
		}
		const double amp_scaling = 0.3;
		const double scaled_out = out_channel[n] * amp_scaling;
		out_channel[n] = scaled_out;
	}
	for (size_t hop = 0; hop < decim; hop++)
		out_frames_index[hop]++;
}

static int32_t run_voc_chorus(struct CSOUND_* csound, struct voc_chorus* const p)
{
	if (p->fft_mach == NULL)
		return OK;

	const double env_samp_rate = csound->GetSr(csound);
	p->env_samp_rate = env_samp_rate;

	const FUNC* const cs_table = csound->FTnp2Find(csound, p->table_no);
	double* const sample = cs_table->ftable;
	const uint64_t sample_len = cs_table->flen;
	const double rate_adjust =
	        cs_table->gen01args.sample_rate/env_samp_rate;
	const double pitch = *p->pitch_arg * rate_adjust;
	const size_t no_of_c_voices = (size_t)*p->no_of_c_voices_arg;
	p->sample = sample;
	p->sample_len = sample_len;
	p->rate_adjust = rate_adjust;
	p->pitch = pitch;
	p->no_of_c_voices = no_of_c_voices;

	const uint32_t offset = p->h.insdshead->ksmps_offset;
	const uint64_t nsmps = sample_accurate_check(p, offset);
	for (size_t n = offset; n < nsmps; n++) {
		const bool first_run = p->first_run;
		const unsigned up_to_hop_size = p->up_to_hop_size;
		if (first_run || up_to_hop_size == hop_size) {
			p->first_run = false;
			fill_bins(p, n);
			run_forward_ffts(p);
			vocode(p, n);
			run_backwards_ffts(p);
			write_to_out_frames(p, set_output_start_pos(p));
			reset_counters(p);
		}
		write_to_output(p, n);
		p->up_to_hop_size++;
	}
	return OK;
}

static OENTRY localops[] = {
	{ "vochorus.akkkkkki",
	  sizeof(struct voc_chorus),
	  0, 3, "mm", "akkkkkki",
	  (SUBR)init_voc_chorus, (SUBR)run_voc_chorus },
};

PUBLIC int32_t csoundModuleCreate(CSOUND *csound)
{
    return 0;
}

PUBLIC int32_t csoundModuleInit(struct CSOUND_ *csound)
{
	fftw_import_wisdom_from_filename("$HOME/.config/warpy/warpy.wis");

	csound->CreateGlobalVariable(csound,
	                             "warpfft",
	                             sizeof(struct warpy_fft_machinery) *
	                             MAX_POLY);
	struct warpy_fft_machinery* fft_machs =
	        (struct warpy_fft_machinery*)
	        csound->QueryGlobalVariable(csound, "warpfft");
	for (size_t i = 0; i < MAX_POLY; i++)
		init_warpy_fft(&fft_machs[i]);

	OENTRY *ep = (OENTRY *)&(localops[0]);
	int err = 0;
	while (ep->opname != NULL) {
		err |= csound->AppendOpcode(csound,
		                            ep->opname,  ep->dsblksiz,
		                            ep->flags,   ep->thread,
		                            ep->outypes, ep->intypes,
		                            (int (*)(CSOUND *, void *))
		                            ep->iopadr,
		                            (int (*)(CSOUND *, void *))
		                            ep->kopadr,
		                            (int (*)(CSOUND *, void *))
		                            ep->aopadr);
		ep++;
	}

	return err;
}

PUBLIC int32_t csoundModuleDestroy(struct CSOUND_ *csound)
{
	struct warpy_fft_machinery* fft_machs =
	        (struct warpy_fft_machinery*)
	        csound->QueryGlobalVariable(csound, "warpfft");
	for (size_t i = 0; i < MAX_POLY; i++) {
		struct warpy_fft_machinery* fft_mach = &fft_machs[i];
		fftw_destroy_plan(fft_mach->fft_fwin_forw);
		fftw_destroy_plan(fft_mach->fft_fwin_back);
		fftw_destroy_plan(fft_mach->fft_bwin_forw);
		fftw_free(fft_mach->fwin);
		fftw_free(fft_mach->bwin);
		fftw_free(fft_mach->pwin);
		for (size_t j = 0; j < MAX_CHORUS_VOICES; j++) {
			struct warpy_chorus_voice* voice =
			        &fft_mach->chor_voices[j];
			fftw_destroy_plan(voice->fft_fwin_forw);
			fftw_destroy_plan(voice->fft_fwin_back);
			fftw_destroy_plan(voice->fft_bwin_forw);
			fftw_free(voice->fwin);
			fftw_free(voice->bwin);
			fftw_free(voice->pwin);
		}
		free(fft_mach->chor_voices);
	}

	csound->DestroyGlobalVariable(csound, "warpfft");
	fftw_export_wisdom_to_filename("$HOME/.config/warpy/warpy.wis");
	fftw_cleanup();

	return 0;
}

LINKAGE
