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
#include <csound/csound.h>
#include <csound/csdl.h>

struct chorus_sig {
	OPDS h;
	MYFLT *out;
	MYFLT *center;
	MYFLT *voices;
	MYFLT *above_1, *below_1,
	      *above_2, *below_2,
	      *above_3, *below_3;
	MYFLT *spread_above_1, *spread_below_1,
	      *spread_above_2, *spread_below_2,
	      *spread_above_3, *spread_below_3;
	MYFLT *mix_center, *mix_sides;
	MYFLT *env;
};

#define SIDE_SPREAD(N) (p->above_##N[i] * (*p->spread_above_##N) + \
                        p->below_##N[i] * (*p->spread_below_##N))

#define A_RATE_LOOP uint32_t offset = p->h.insdshead->ksmps_offset; \
                    uint32_t n = sample_accurate_check(p, offset); \
                    for (int32_t i = offset; i < n; i++)

static inline MYFLT mix(struct chorus_sig* p, MYFLT sides, int32_t i)
{
	MYFLT mix = (sides*(*p->mix_sides) + p->center[i]*(*p->mix_center)) * p->env[i];
	//fprintf(stdout, "    mix: %0.8f\n", mix);
	//fprintf(stdout, "        mix_sides: %0.8f\n", *p->mix_sides);
	//fprintf(stdout, "        sides: %0.8f\n", sides);
	//fprintf(stdout, "        center: %0.8f\n", p->center[i]);
	//fprintf(stdout, "        env: %0.8f\n", p->env[i]);
	return mix;
}

static inline int32_t sample_accurate_check(struct chorus_sig* p, uint32_t offset)
{
	uint32_t early  = p->h.insdshead->ksmps_no_end;
	int32_t n = CS_KSMPS;

	/* sample-accurate mode mechanism */
	if(offset) memset(p->out, '\0', offset*sizeof(MYFLT));
	if(early) {
		n -= early;
		memset(&p->out[n], '\0', early*sizeof(MYFLT));
	}
	return n;
}

int get_chorus_sig_0(CSOUND* csound, struct chorus_sig* p)
{
	A_RATE_LOOP
		p->out[i] = p->center[i] * p->env[i];
	return OK;
}

int get_chorus_sig_1(CSOUND* csound, struct chorus_sig* p)
{
	A_RATE_LOOP {
		//fprintf(stdout, "time: %ld\n",
		//                csoundGetCurrentTimeSamples(csound));
		//fprintf(stdout, "        above_1: %0.8f\n",
		//                p->above_1[i]);
		//fprintf(stdout, "        below_1: %0.8f\n",
		//              p->below_1[i]);
		//fprintf(stdout, "        spread_above_1: %0.8f\n",
		//                p->spread_above_1[i]);
		//fprintf(stdout, "        spread_below_1: %0.8f\n",
		//                p->spread_below_1[i]);
		p->out[i] = mix(p, SIDE_SPREAD(1), i);
	}

	return OK;
}

int get_chorus_sig_2(CSOUND* csound, struct chorus_sig* p)
{
	A_RATE_LOOP
		p->out[i] = mix(p,
		                (SIDE_SPREAD(1) + SIDE_SPREAD(2)),
		                i);
	return OK;
}

int get_chorus_sig_3(CSOUND* csound, struct chorus_sig* p)
{
	A_RATE_LOOP
		p->out[i] = mix(p,
		                (SIDE_SPREAD(1) +
		                 SIDE_SPREAD(2) +
		                 SIDE_SPREAD(3)),
		                i);
	return OK;
}

static OENTRY localops[] = {
	{ "chorusig.akiiiiiiiiiiiiiia",
	  sizeof(struct chorus_sig),
	  0, 4, "a", "akiiiiiiiiiiiiiia",
	  NULL, (SUBR)get_chorus_sig_0    },
	{ "chorusig.akaaiiiikkiiiikka",
	  sizeof(struct chorus_sig),
	  0, 4, "a", "akaaiiiikkiiiikka",
	  NULL, (SUBR)get_chorus_sig_1    },
	{ "chorusig.akaaaaiikkkkiikka",
	  sizeof(struct chorus_sig),
	  0, 4, "a", "akaaaaiikkkkiikka",
	  NULL, (SUBR)get_chorus_sig_2    },
	{ "chorusig.akaaaaaakkkkkkkka",
	  sizeof(struct chorus_sig),
	  0, 4, "a", "akaaaaaakkkkkkkka",
	  NULL, (SUBR)get_chorus_sig_3    },
};

LINKAGE
