# frozen_string_literal: true

require 'rake/clean'
require 'rake/loaders/makefile'

require_relative 'rake/orc_file'

COMPILER = 'gcc'
FLAGS = '-std=c18 -pedantic-errors -Wall -Werror -x c -isystem /usr/include'

ORC_INFILE = 'warpy.orc'
ORC_OUTFILE = 'warpy.orc.c'

file ORC_OUTFILE => ORC_INFILE do
  orc = OrcFile.new(ORC_INFILE, ORC_OUTFILE)
  orc.write_commentless
  orc.conv_commentless_to_hexdump
  orc.clean_tmpstrings_from_hexdump
  orc.remove_len
  orc.change_unsigned_to_const
end
CLEAN.include(ORC_OUTFILE)

CLOBBER.include('*.so')

source_files = Rake::FileList['*.c']
object_files = source_files.ext('.o')
depend_files = source_files.ext('.mf')

CLEAN.include('*.o')
CLEAN.include('*.mf')

rule '.mf' => '.c' do |t|
  sh "#{COMPILER} -MM #{t.source} -MF #{t.name}"
end

depend_files.each do |mf|
  import mf
  file mf
end

rule '.o' => '.c' do |t|
  sh "#{COMPILER} #{FLAGS} -c #{t.source}"
end

TEST_OUTFILE = 'test_warpy'

file TEST_OUTFILE => object_files do |t|
  sh "#{COMPILER} #{FLAGS} #{object_files} -o #{task_name}"
end
CLOBBER.include(TEST_OUTFILE)

task default: :build
task :build => [ORC_OUTFILE] do
end

