# frozen_string_literal: true

module OrcFileERB
  MINCER_FFT_SIZE = '8184'
  MINCER_DECIM = '8'

  def mincer_out(file_channel)
    <<~MINCER
      mincer asamplepos, iamp*kgain, kpitchfinal, #{file_channel}, 1,\
             #{MINCER_FFT_SIZE}, #{MINCER_DECIM}
    MINCER
  end

  class VocoderParams
    attr_reader :vartype, :param
    def initialize(param, vartype)
      @param = param
      @vartype = vartype
    end

    VOC_MIN = '0.0001'

    def params
      <<~VOC
        #{prefix}adjust     chnget "#{param}_adjust"
        #{prefix}center     chnget "#{param}_center"
        #{prefix}lowerscale chnget "#{param}_lower_scale"
        #{prefix}upperscale chnget "#{param}_upper_scale"
      VOC
    end

    def param_process
      <<~VOC
        #{prefix}diff = imfreq / cpsmidinn(#{prefix}center)

        if #{prefix}diff == 1 then
            #{prefix}scaled = 1
        else
            if #{prefix}diff > 1 then
                if #{prefix}upperscale < 0 then
                    #{prefix}upperscale = abs(#{prefix}upperscale)
                    #{prefix}diff = 1 / #{prefix}diff
                endif
                #{prefix}scaled = 1 + ((#{prefix}diff - 1) * #{prefix}upperscale)
            else
                if #{prefix}lowerscale < 0 then
                    #{prefix}lowerscale = abs(#{prefix}lowerscale)
                    #{prefix}diff = 1 / #{prefix}diff
                endif
                #{prefix}scaled = 1 + ((#{prefix}diff - 1) * #{prefix}lowerscale)
            endif
        endif

        #{prefix}final = #{prefix}scaled * #{prefix}adjust

        if #{prefix}final < #{VOC_MIN} then
            #{prefix}final = #{VOC_MIN}
        endif
      VOC
    end

    private

    def prefix
      "#{vartype}#{param}"
    end
  end
end
