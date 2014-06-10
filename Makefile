PYTHON = /usr/include/python2.7
BOOST_INC = /usr/include
BOOST_LIB = /usr/lib
TARGET = pyserpent
COMMON_OBJS = bignum.o util.o tokenize.o lllparser.o parser.o rewriter.o compiler.o
PYTHON_VERSION = 2.7

all: serpent $(TARGET).so

serpent : $(COMMON_OBJS) cmdline.cpp
	g++ -Wall $(COMMON_OBJS) cmdline.cpp -o serpent

bignum.o : bignum.cpp bignum.h

util.o : util.cpp util.h bignum.o

tokenize.o : tokenize.cpp tokenize.h util.o

lllparser.o : lllparser.cpp lllparser.h tokenize.o util.o

parser.o : parser.cpp parser.h tokenize.o util.o

rewriter.o : rewriter.cpp rewriter.h lllparser.o util.o

compiler.o : compiler.cpp compiler.h util.o

clean:
	rm -f serpent *\.o $(TARGET).so

$(TARGET).so: $(TARGET).o
	g++ -shared -Wl,--export-dynamic $(TARGET).o -L$(BOOST_LIB) -lboost_python -L/usr/lib/python$(PYTHON_VERSION)/config -lpython$(PYTHON_VERSION) $(COMMON_OBJS) -o $(TARGET).so
 
$(TARGET).o: $(TARGET).cpp $(COMMON_OBJS)
	g++ -I$(PYTHON) -I$(BOOST_INC) -fPIC -c $(TARGET).cpp



install:
	cp serpent /usr/local/bin
	cp pyserpent.so /usr/lib/python2.7/lib-dynload/
