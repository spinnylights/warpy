# frozen_string_literal: true

require 'erb'

module ERBUtils
  MINCER_FFT_SIZE = '8184'
  MINCER_DECIM = '8'

  def mincer_out(file_channel, pitch_mod=0)
    "mincer #{mincer_args(file_channel, pitch_mod)}"
  end

  def mincer_out_func(file_channel, pitch_mod=0)
    "mincer:a(#{mincer_args(file_channel, pitch_mod)})"
  end

  def sustain_on
    "ksustainsection == 1"
  end

  def release_on
    "kreleasesection == 1"
  end

  def in_sustain_phase
    "(#{sustain_on} && kmainloops >= isusmainlooplimit && kreleased == 0)"
  end

  def phase_over(phase, early: 0)
    "((i#{phase}looptimes > 0) && (k#{phase}loops >= " +
      "i#{phase}looptimes - (#{early})))"
  end

  def scaled_pointer(phase: '', vartype: 'i')
    <<~POINTER
      apointer = (phasor:a(krate * (1 / (#{vartype}#{phase}end - \
                 #{vartype}#{phase}start))) * (#{vartype}#{phase}end - \
                 #{vartype}#{phase}start)) + #{vartype}#{phase}start
    POINTER
  end

  def kline(_start, _end)
    "(1 / (gisampledur * (#{_end} - #{_start}) / kspeedfinal)) / kr"
  end

  def chorus_var_name(type, position, id, channel='')
    "kchorus#{type}#{position}#{id}#{channel}"
  end

  def chorus_channel_get(type, position, id, channel='')
    if !channel.empty? then channel = '_' + channel end
    "chnget \"chorus_#{type}_#{position}_#{id}#{channel}\""
  end

  def chorus_param(type, position, id, channel='')
    [
      chorus_var_name(type, position, id, channel),
      chorus_channel_get(type, position, id, channel)
    ]
  end

  def chorus_params(amount_on:)
    channel_off = " = 0"
    source = String.new
    (1..3).each do |id|
      params = [
        chorus_param('detune', 'above', id),
        chorus_param('detune', 'below', id),
        chorus_param('spread', 'above', id, 'l'),
        chorus_param('spread', 'below', id, 'l'),
        chorus_param('spread', 'above', id, 'r'),
        chorus_param('spread', 'below', id, 'r'),
      ]
      params.each do |param|
        param_var_name = param[0]
        if id > amount_on
          source << param_var_name << channel_off << "\n"
        else
          param_channel = param[1]
          source << param_var_name << ' ' << param_channel << "\n"
        end
      end
    end
    return source
  end

  def mincer_args(file_channel, pitch_mod)
      "asamplepos, iamp*kgain, kpitchfinal+kvib+(#{pitch_mod}), \ \n" +
      "#{file_channel}, 1, #{MINCER_FFT_SIZE}, #{MINCER_DECIM}"
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

    def vocparam
      <<~VOC
        #{prefix}final vocparam #{prefix}adjust, #{prefix}center, \
                                #{prefix}lowerscale, #{prefix}upperscale, \
                                imfreq
      VOC
    end

    private

    def prefix
      "#{vartype}#{param}"
    end
  end
end

class OrcFileERB
  include ERBUtils

  attr_reader :orc_source
  def initialize(orc_source)
    @orc_source = orc_source
  end

  def result
    ERB.new(orc_source).result(binding)
  end
end

