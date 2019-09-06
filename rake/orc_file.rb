# frozen_string_literal: true

require_relative 'orc_file_erb'

class OrcFile
  include OrcFileERB

  attr_reader :infile, :outfile
  def initialize(infile, outfile)
    @infile = infile
    @outfile = outfile
  end

  def preprocess
    orc_source = File.open(infile) {|f| f.read}
    orc = ERB.new(orc_source).result(binding)
    min_newlines = orc.gsub(/\n{2,}/m, "\n")
    stripped = min_newlines.lines.map {|l| l.strip}.join("\n")
    commentless = stripped.gsub(/\/\*.*\*\//m, '').strip
    File.open(preproced_file, 'w') {|f| f.puts commentless}
  end

  def conv_to_hexdump
    `xxd -i < #{preproced_file} > #{outfile}`
    `echo ', 0' >> #{outfile}`
    `rm #{preproced_file}`
  end

  def clean_tmpstrings_from_hexdump
    outfile_contents = File.open(outfile) {|f| f.read}
    outfile_tmpless = outfile_contents.gsub('_TMP', '')
    File.open(outfile, 'w') {|f| f.print outfile_tmpless}
  end

  def remove_len
    outfile_contents = File.open(outfile) {|f| f.read}
    outfile_lines = outfile_contents.lines
    outfile_lines.pop
    File.open(outfile, 'w') {|f| f.print outfile_lines.join('')}
  end

  def change_unsigned_to_const
    outfile_contents = File.open(outfile) {|f| f.read}
    outfile_const = outfile_contents.gsub('unsigned', 'const')
    File.open(outfile, 'w') {|f| f.print outfile_const}
  end

  private

  def preproced_file
     infile + '.tmp'
  end
end
