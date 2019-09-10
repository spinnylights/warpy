# frozen_string_literal: true

require_relative 'orc_file_erb'

class OrcFile
  attr_reader :infile, :outfile
  def initialize(infile, outfile)
    @infile = infile
    @outfile = outfile
  end

  def preprocess
    orc_source = File.open(infile) {|f| f.read}
    erb_result = OrcFileERB.new(orc_source).result
    minified   = Orc.new(erb_result).minify!
    File.open(preproced_file, 'w') {|f| f.puts minified}
  end

  def conv_to_hexdump
    `xxd -i < #{preproced_file} > #{outfile}`
    `echo ', 0' >> #{outfile}`
    #`rm #{preproced_file}`
  end

  private

  def preproced_file
     infile + '.tmp'
  end
end

class Orc
  attr_reader :source
  def initialize(source)
    @source = source
  end

  def minify!
    remove_comments
    minimize_newlines
    strip
  end

  private

  def remove_comments
    @source = source.gsub(/^(\/\/|;).*/, '').gsub(/\/\*.*\*\//m, '').strip
  end

  def minimize_newlines
    @source = source.gsub(/\n{2,}/m, "\n")
  end

  def strip
    @source = source.lines.map {|l| l.strip}.join("\n")
  end
end
