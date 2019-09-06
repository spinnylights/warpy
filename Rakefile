# frozen_string_literal: true

require 'erb'
require 'rake/clean'
require 'rake/loaders/makefile'

require_relative 'rake/orc_file'

COMPILER = 'gcc'

FLAGS = %w(
  -g
  -Wall
  -Werror
).join(' ')

LIBS = %w(
  -lm
  -lcsound64
  -lsox
).join(' ')

TEST_LIBS = 'test/tinywav/tinywav.so'

ORC_INFILE = 'warpy.orc.erb'
ORC_OUTFILE = 'warpy.orc.xxd'

file ORC_OUTFILE => ORC_INFILE do
  orc = OrcFile.new(ORC_INFILE, ORC_OUTFILE)
  orc.preprocess
  orc.conv_to_hexdump
end
CLEAN.include(ORC_OUTFILE)

TTL_INFILE = 'warpy.ttl.erb'
TTL_OUTFILE = 'warpy.ttl'

file TTL_OUTFILE => TTL_INFILE do
  ttl_f = File.open(TTL_INFILE) {|f| f.read}
  ttl = ERB.new(ttl_f)
  File.open(TTL_OUTFILE, 'w') {|f| f.print ttl.result.gsub(/\n{3,}/, "\n\n")}
end
CLEAN.include(TTL_OUTFILE)

CLOBBER.include('*.so')

CLEAN.include('*.o')
CLEAN.include('*.mf')

file 'warpy.o' => ['warpy.c', ORC_OUTFILE] do |t|
  sh "#{COMPILER} #{FLAGS} -c -o #{t.name} warpy.c"
end

file 'test_warpy.o' => 'test_warpy.c' do |t|
  sh "#{COMPILER} #{FLAGS} -c -o #{t.name} test_warpy.c"
end

task 'test_warpy' => ['test_warpy.o', 'warpy.o'] do |t|
  sh "#{COMPILER} #{FLAGS} test_warpy.o warpy.o #{LIBS} #{TEST_LIBS} -o #{t.name}"
end

file 'warpy.so' => [ORC_OUTFILE, 'warpy.c', 'warpy_lv2.c', 'warpy.ttl'] do |t|
  sh "#{COMPILER} #{FLAGS} -c -fpic warpy_lv2.c warpy.c"
  sh "#{COMPILER} #{FLAGS} -shared -o warpy.so warpy_lv2.o warpy.o #{LIBS}"
end

task default: :build
task :build => [ORC_OUTFILE] do
end

