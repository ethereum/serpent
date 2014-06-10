PYTHON = /usr/include/python2.7
BOOST_INC = /usr/include
BOOST_LIB = /usr/lib
TARGET = pyserpent

serpent : bignum.o util.o tokenize.o lllparser.o parser.o rewriter.o compiler.o cmdline.cpp
	g++ -Wall bignum.o util.o tokenize.o lllparser.o parser.o rewriter.o compiler.o cmdline.cpp -o serpent

bignum.o : bignum.cpp bignum.h

util.o : util.cpp util.h bignum.o

tokenize.o : tokenize.cpp tokenize.h util.o

lllparser.o : lllparser.cpp lllparser.h tokenize.o util.o

parser.o : parser.cpp parser.h tokenize.o util.o

rewriter.o : rewriter.cpp rewriter.h lllparser.o util.o

compiler.o : compiler.cpp compiler.h util.o

clean:
	rm -f serpent *\.o

libserpent.a: $(COMMON_OBJS)
	ar rcs $@ $^

$(TARGET).so: $(TARGET).o
	g++ -shared -Wl,-soname,$(TARGET).so \
	$(TARGET).o -L$(BOOST_LIB) -lboost_python -fPIC \
	-L/usr/lib/python2.7/config -lpython2.7 \
	-o $(TARGET).so -I. -L. -lserpent $(ARCH)

$(TARGET).o: $(TARGET).cpp libserpent.a
	g++ -I$(PYTHON) -I$(BOOST_INC) -c $(TARGET).cpp -fPIC -L. -lserpent $(ARCH)


install:
	cp serpent /usr/local/bin
