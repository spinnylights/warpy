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
    puts gSpath, 1
    ichannels filenchnls gSpath
    gistereo = ichannels - 1
    gileftchan ftgen 1, 0, 0, 1, gSpath, 0, 0, 1
    if gistereo == 1 then
        girightchan ftgen 2, 0, 0, 1, gSpath, 0, 0, 2
    endif
    gisampleready = 1
endin

#define NOTE_DIFF(freq'center) #$freq / cpsmidinn($center)#
#define SCALED(diff'scale) #1 + (($diff - 1) * $scale)#
#define VOC_MIN #0.0001#

instr 1
    if gisampleready == 1 then
        kgain     chnget "gain"
        ; speed
        kspeedadjust     chnget "speed_adjust"
        kspeedcenter     chnget "speed_center"
        kspeedlowerscale chnget "speed_lower_scale"
        kspeedlowerscale = kspeedlowerscale * -1
        kspeedupperscale chnget "speed_upper_scale"
        ; pitch
        kpitchadjust     chnget "pitch_adjust"
        kpitchcenter     chnget "pitch_center"
        kpitchlowerscale chnget "pitch_lower_scale"
        kpitchlowerscale = kpitchlowerscale * -1
        kpitchupperscale chnget "pitch_upper_scale"
        ; envelope
        ienvatt   chnget "env_attack_time"
        ienvattsh chnget "env_attack_shape"
        ienvdec   chnget "env_decay_time"
        ienvdecsh chnget "env_decay_shape"
        ienvsus   chnget "env_sustain_level"
        ienvrel   chnget "env_release_time"
        ienvrelsh chnget "env_release_shape"

        iamp  ampmidi 1

        imfreq cpsmidi
        kspeeddiff = $NOTE_DIFF(imfreq'kspeedcenter)
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
        kpitchfinal = kpitchscaled * kpitchadjust

        if kspeedfinal < $VOC_MIN then
            kspeedfinal = $VOC_MIN
        endif

        if kpitchfinal < $VOC_MIN then
            kpitchfinal = $VOC_MIN
        endif

        printks "speed_adjust: %f\n",  1, kspeedadjust
        printks "speed_scaled: %f\n",  1, kspeedscaled
        printks "speed_final: %f\n",   1, kspeedfinal
        printks "pitch_adjust: %f\n",  1, kpitchadjust
        printks "pitch_scaled: %f\n",  1, kpitchscaled
        printks "pitch_final: %f\n\n", 1, kpitchfinal

        aenv transegr 0,       ienvatt, ienvattsh, \
                      1,       ienvdec, ienvdecsh, \
                      ienvsus, ienvrel, ienvrelsh, \
                      0

        if gistereo == 1 then
            asigl temposcal kspeedfinal, iamp*kgain, kpitchfinal, gileftchan, 1
            asigr temposcal kspeedfinal, iamp*kgain, kpitchfinal, girightchan, 1
                outs asigl*aenv, asigr*aenv
        else
            asig temposcal kspeedfinal, iamp*kgain, kpitchfinal, gileftchan, 1
                outs asig*aenv, asig*aenv
        endif
    endif
endin

