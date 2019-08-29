# frozen_string_literal: true

require 'rake/clean'
require 'rake/loaders/makefile'

require_relative 'rake/orc_file'

COMPILER = 'gcc'
FLAGS = %w(
  -std=gnu18
  -pedantic-errors
  -Wall
  -Werror
  -x
  c
  -isystem
  /usr/include
  -lm
  /usr/lib/libcsound64.so
).join(' ')

ORC_INFILE = 'warpy.orc'
ORC_OUTFILE = 'warpy.orc.xxd'

file ORC_OUTFILE => ORC_INFILE do
  orc = OrcFile.new(ORC_INFILE, ORC_OUTFILE)
  orc.write_commentless
  orc.conv_commentless_to_hexdump
  #orc.clean_tmpstrings_from_hexdump
  #orc.remove_len
  #orc.change_unsigned_to_const
end
CLEAN.include(ORC_OUTFILE)

CLOBBER.include('*.so')

CLEAN.include('*.o')
CLEAN.include('*.mf')

file 'warpy.orc.o' => [ORC_OUTFILE] do
  sh "#{COMPILER} #{FLAGS} -c #{ORC_OUTFILE}"
end

file 'warpy.o' => ['warpy.c', 'warpy.h'] do
  sh "#{COMPILER} #{FLAGS} -c warpy.c"
end

TEST_OUTFILE = 'test_warpy.o'

file 'test_warpy.o' => ['warpy.o', 'test_warpy.c'] do |t|
  sh "#{COMPILER} #{FLAGS} -c test_warpy.c"
end

file 'test_warpy' => ['warpy.c', 'test_warpy.c', 'warpy.orc.xxd'] do |t|
  sh "#{COMPILER} #{FLAGS} warpy.c test_warpy.c -o #{t.name}"
end

CLOBBER.include(TEST_OUTFILE)

task default: :build
task :build => [ORC_OUTFILE] do
end

