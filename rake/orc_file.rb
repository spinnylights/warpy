# frozen_string_literal: true

class OrcFile
  attr_reader :infile, :outfile
  def initialize(infile, outfile)
    @infile = infile
    @outfile = outfile
  end

  def write_commentless
    orc = File.open(infile) {|f| f.read}
    commentless = orc.gsub(/\/\*.*\*\//m, '').strip
    File.open(comless_file, 'w') {|f| f.puts commentless}
  end

  def conv_commentless_to_hexdump
    `xxd -i < #{comless_file} > #{outfile}`
    `echo ', 0' >> #{outfile}`
    `rm #{comless_file}`
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

  def comless_file
     infile + '.tmp'
  end
end
