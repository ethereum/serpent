PLATFORM_OPTS = 
PYTHON = /usr/include/python2.7
CXXFLAGS = -fPIC -Wno-sign-compare
# -g3 -O0
BOOST_INC = /usr/include
BOOST_LIB = /usr/lib
TARGET = pyserpent
COMMON_OBJS = keccak-tiny.o bignum.o util.o tokenize.o lllparser.o parser.o opcodes.o optimize.o functions.o rewriteutils.o preprocess.o rewriter.o compiler.o funcs.o
HEADERS = bignum.h util.h keccak-tiny-wrapper.h tokenize.h lllparser.h parser.h opcodes.h functions.h optimize.h rewriteutils.h preprocess.h rewriter.h compiler.h funcs.h
PYTHON_VERSION = 2.7

serpent : serpentc lib

lib: $(COMMON_OBJS)
	ar rvs libserpent.a $(COMMON_OBJS) 
	g++ $(CXXFLAGS) -shared $(COMMON_OBJS) -o libserpent.so

serpentc: $(COMMON_OBJS) cmdline.o
	rm -rf serpent
	g++ -Wall $(COMMON_OBJS) cmdline.o -o serpent

keccak-tiny.o : keccak-tiny.cpp

bignum.o : bignum.cpp bignum.h

opcodes.o : opcodes.cpp opcodes.h

util.o : util.cpp util.h bignum.o

tokenize.o : tokenize.cpp tokenize.h util.o

lllparser.o : lllparser.cpp lllparser.h tokenize.o util.o

parser.o : parser.cpp parser.h tokenize.o util.o

rewriter.o : rewriter.cpp rewriter.h lllparser.o util.o rewriteutils.o preprocess.o opcodes.o functions.o

preprocessor.o: rewriteutils.o functions.o

compiler.o : compiler.cpp compiler.h util.o

funcs.o : funcs.cpp funcs.h

cmdline.o: cmdline.cpp

pyext.o: pyext.cpp

clean:
	rm -f serpent *\.o libserpent.a libserpent.so

install:
	cp serpent /usr/local/bin
	cp libserpent.a /usr/local/lib
	cp libserpent.so /usr/local/lib
	rm -rf /usr/local/include/libserpent
	mkdir -p /usr/local/include/libserpent
	cp $(HEADERS) /usr/local/include/libserpent
