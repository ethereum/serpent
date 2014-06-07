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
