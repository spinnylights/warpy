<CsoundSynthesizer>
<CsOptions>
-o vochorus_test.wav
</CsOptions>
; ==============================================
<CsInstruments>

sr      =  48000
ksmps   =  64
nchnls  =  2
0dbfs   =  1

instr 1

isampledur = 125474
krate = 1 / isampledur
apointer phasor krate
asamplepos = apointer * isampledur

kpitch = 1
kchorusvoices = 0
kchorusmix = 0
kchorusdetune = 0
kchorusspread = 0

asigll, asiglr vochorus asamplepos,    kpitch,  1,
                        kchorusvoices, kchorusmix, kchorusdetune,
                        kchorusspread, 0
asigrl, asigrr vochorus asamplepos,    kpitch,  1,
                        kchorusvoices, kchorusmix, kchorusdetune,
                        kchorusspread, 1
asigl = asigll + asigrl
asigr = asiglr + asigrr

    outs asigl, asigr
endin

</CsInstruments>
; ==============================================
<CsScore>
f1 0 125474 1 "fox.wav" 0 1 0

i1 0 5
</CsScore>
</CsoundSynthesizer>

