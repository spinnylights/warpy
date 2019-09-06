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

massign 0,1

gistereo init 0
gisampleready init 0
gkfirstrun init 1
gSpath init ""
gisampledur init 0

instr PathGetter
    gSpath chnget "path"

    knewfile changed2 gSpath
    ipathempty strcmp gSpath, ""
    if ((knewfile == 1) || ((gkfirstrun == 1) && (ipathempty != 0))) then
        event "i", "FileLoader", 0, 0
        if gkfirstrun == 1 then
            gkfirstrun = 0
        endif
    endif
endin

instr +FileLoader
    ichannels filenchnls gSpath
    gistereo = ichannels - 1
    gileftchan ftgen 1, 0, 0, 1, gSpath, 0, 0, 1
    if gistereo == 1 then
        girightchan ftgen 2, 0, 0, 1, gSpath, 0, 0, 2
    endif
    gisampledur chnget "sample_dur"
    gisampleready = 1
endin

#define NOTE_DIFF(freq'center) #$freq / cpsmidinn($center)#
#define SCALED(diff'scale) #1 + (($diff - 1) * $scale)#
#define VOC_MIN #0.0001#
#define MINCER_FFT_SIZE #8184#
#define MINCER_DECIM #8#
#define MINCER_OUT(chan) #mincer asamplepos, iamp*kgain, kpitchfinal,\
                                 $chan, 1, $MINCER_FFT_SIZE, $MINCER_DECIM#

instr 1
    if gisampleready == 1 then
        kgain     chnget "gain"
        ; speed
        kspeedadjust     chnget "speed_adjust"
        kspeedcenter     chnget "speed_center"
        kspeedlowerscale chnget "speed_lower_scale"
        kspeedupperscale chnget "speed_upper_scale"
        ispeedadjust     chnget "speed_adjust"
        ispeedcenter     chnget "speed_center"
        ispeedlowerscale chnget "speed_lower_scale"
        ispeedupperscale chnget "speed_upper_scale"
        ; pitch
        kpitchadjust     chnget "pitch_adjust"
        kpitchcenter     chnget "pitch_center"
        kpitchlowerscale chnget "pitch_lower_scale"
        kpitchupperscale chnget "pitch_upper_scale"
        ; envelope
        ienvatt   chnget "env_attack_time"
        ienvattsh chnget "env_attack_shape"
        ienvdec   chnget "env_decay_time"
        ienvdecsh chnget "env_decay_shape"
        ienvsus   chnget "env_sustain_level"
        ienvrel   chnget "env_release_time"
        ienvrelsh chnget "env_release_shape"
        ; reverse
        kreverse chnget "reverse"
        ; loop times
        ilooptimes chnget "loop_times"

        iamp  ampmidi 1

        imfreq cpsmidi
        kspeeddiff = $NOTE_DIFF(imfreq'kspeedcenter)
        ispeeddiff = $NOTE_DIFF(imfreq'ispeedcenter)
        kpitchdiff = $NOTE_DIFF(imfreq'kpitchcenter)

        if kspeeddiff == 1 then
            kspeedscaled = 1
        else
            if kspeeddiff > 1 then
                if kspeedupperscale < 0 then
                    kspeedupperscale = abs(kspeedupperscale)
                    kspeeddiff = 1 / kspeeddiff
                endif
                kspeedscaled = $SCALED(kspeeddiff'kspeedupperscale)
            else
                if kspeedlowerscale < 0 then
                    kspeedlowerscale = abs(kspeedlowerscale)
                    kspeeddiff = 1 / kspeeddiff
                endif
                kspeedscaled = $SCALED(kspeeddiff'kspeedlowerscale)
            endif
        endif

        if ispeeddiff == 1 then
            ispeedscaled = 1
        else
            if ispeeddiff > 1 then
                if ispeedupperscale < 0 then
                    ispeedupperscale = abs(ispeedupperscale)
                    ispeeddiff = 1 / ispeeddiff
                endif
                ispeedscaled = $SCALED(ispeeddiff'ispeedupperscale)
            else
                if ispeedlowerscale < 0 then
                    ispeedlowerscale = abs(ispeedlowerscale)
                    ispeeddiff = 1 / ispeeddiff
                endif
                ispeedscaled = $SCALED(ispeeddiff'ispeedlowerscale)
            endif
        endif

        if kpitchdiff == 1 then
            kpitchscaled = 1
        else
            if kpitchdiff > 1 then
                if kpitchupperscale < 0 then
                    kpitchupperscale = abs(kpitchupperscale)
                    kpitchdiff = 1 / kpitchdiff
                endif
                kpitchscaled = $SCALED(kpitchdiff'kpitchupperscale)
            else
                if kpitchlowerscale < 0 then
                    kpitchlowerscale = abs(kpitchlowerscale)
                    kpitchdiff = 1 / kpitchdiff
                endif
                kpitchscaled = $SCALED(kpitchdiff'kpitchlowerscale)
            endif
        endif

        kspeedfinal = kspeedscaled * kspeedadjust
        ispeedfinal = ispeedscaled * ispeedadjust

        kpitchfinal = kpitchscaled * kpitchadjust

        if kspeedfinal < $VOC_MIN then
            kspeedfinal = $VOC_MIN
        endif

        if kpitchfinal < $VOC_MIN then
            kpitchfinal = $VOC_MIN
        endif

        aenv transegr 0,       ienvatt, ienvattsh, \
                      1,       ienvdec, ienvdecsh, \
                      ienvsus, ienvrel, ienvrelsh, \
                      0

        if ispeedfinal == 0 then
            ispeedfinal = 1
        endif

        if (ilooptimes > 0) then
            klimit line 0, gisampledur / ispeedfinal, 1
        else
            klimit = 0
        endif

        apointer phasor kspeedfinal / gisampledur

        if kreverse == 1 then
            asamplepos = abs(apointer - 0.9)*gisampledur
        else
            asamplepos = apointer*gisampledur
        endif

        if gistereo == 1 then
            apresigl $MINCER_OUT(gileftchan)
            apresigr $MINCER_OUT(girightchan)
        else
            apresigl $MINCER_OUT(gileftchan)
            apresigr = apresigl
        endif

        asigl = apresigl * aenv
        asigr = apresigr * aenv

        kdialdown init 1
        kloopover init 0

        if (ilooptimes > 0) && (klimit >= ilooptimes - 0.1) then
            kloopover = 1
        endif

        if kloopover == 1 then
            kdialdown = kdialdown - 0.3
            if kdialdown < 0 then
                kdialdown = 0
            endif
            asigl = asigl * kdialdown
            asigr = asigr * kdialdown
        endif

        outs asigl, asigr
    endif
endin

