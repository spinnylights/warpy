# frozen_string_literal: true

require 'erb'
require 'rake/clean'
require 'rake/loaders/makefile'

require_relative 'rake/orc_file'

COMPILER = 'gcc'

FLAGS = %w(
  -Wall
  -Werror
).join(' ')

PROD_FLAGS = %w(
  -ggdb
  -Ofast
  -march=native
).join(' ')

TEST_FLAGS = %w(
  -ggdb
).join(' ')

LIBS_A = %w(
  -lm
  -lcsound64
  -lsox
  -lfftw3
)

LIBS = LIBS_A.join(' ')

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

CLOBBER.include('**/*.so')

CLEAN.include('**/*.o')
CLEAN.include('*.mf')

def compile_opcode(t)
  sh "#{COMPILER} #{FLAGS} #{TEST_FLAGS} -shared -fPIC #{t.prerequisites[0]} #{LIBS} -o opcodes/#{t.name}"
end

FileList['opcodes/*.c'].each do |opcode|
  so = File.basename(opcode, '.c') + '.so'
  file so => opcode do |t|
    compile_opcode(t)
  end
  file ORC_OUTFILE => so
end

file 'warpy.o' => ['warpy.c', ORC_OUTFILE] do |t|
  sh "#{COMPILER} #{FLAGS} #{TEST_FLAGS} -c -o #{t.name} #{t.prerequisites[0]}"
end

file 'test_warpy.o' => 'test_warpy.c' do |t|
  sh "#{COMPILER} #{FLAGS} #{TEST_FLAGS} -c -o #{t.name} #{t.prerequisites[0]}"
end

task 'test_warpy' => [:clean, 'test_warpy.o', 'warpy.o'] do |t|
  sh "#{COMPILER} #{FLAGS} #{TEST_FLAGS} #{t.prerequisites[1]} #{t.prerequisites[2]} #{LIBS} #{TEST_LIBS} -o #{t.name}"
end

LD_LIB_PATH = 'LD_LIBRARY_PATH=$HOME/build/csound-6.13.0/build/:$HOME/code/c/warpy/opcodes/:$HOME/build/fftw-3.3.8/.libs/:$LD_LIBRARY_PATH'
LD_PRE='LD_PRELOAD="libvocparam.so libchorusig.so"'

task 'prof_warpy' => [:clean, 'test_warpy'] do |t|
  sh "valgrind --trace-children=yes -v --tool=callgrind env #{LD_LIB_PATH} #{LD_PRE} ./#{t.prerequisites[1]}"
end

task 'debug' => 'test_warpy' do |t|
  sh "#{LD_LIB_PATH} gdb ./test_warpy"
end

task 'test_warpy_pgo' => [:clean, ORC_OUTFILE, 'warpy.c', 'test_warpy.c'] do |t|
  sh "#{COMPILER} #{FLAGS} #{PROD_FLAGS} -fprofile-generate #{t.prerequisites[2]} #{t.prerequisites[3]} #{LIBS} #{TEST_LIBS} -o instrumented"
  sh "./instrumented"
  sh "#{COMPILER} #{FLAGS} #{PROD_FLAGS} -fprofile-use #{t.prerequisites[2]} #{t.prerequisites[3]} #{LIBS} #{TEST_LIBS} -o test_warpy_profiled"
end

file 'warpy.so' => [:clean, ORC_OUTFILE, 'warpy.c', 'warpy_lv2.c', 'warpy.ttl', 'opcodes/libvocparam.c', 'opcodes/libvochorus.c'] do |t|
  sh "#{COMPILER} #{FLAGS} #{PROD_FLAGS} -c -fPIC #{t.prerequisites[2]} #{t.prerequisites[3]}"
  objs = [t.prerequisites[2], t.prerequisites[3]].join(' ')
  sh "#{COMPILER} #{FLAGS} #{PROD_FLAGS} -fPIC -shared -o #{t.name} #{objs} #{LIBS}"
end

task default: :build
task :build => [ORC_OUTFILE] do
end

