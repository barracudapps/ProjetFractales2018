CC=gcc
SRC=main.c

all: exec

exec: lib $(SRC)
	$(CC) -Wall -o main $(SRC) libfractal/libfractal.a -Ilibfractal/ -lpthread -lSDL

#main.o: $(SRC) main.h
#	$(CC) -c $(SRC) libfractal/libfractal.a -Ilibfractal/ -lpthread -lSDL

lib:
	cd libfractal && $(MAKE)
test:  
	cd tests/ && gcc -Wall -I$HOME/local/include test1.c -L$HOME/local/lib -lcunit -o test1 && export LD_LIBRARY_PATH=$HOME/local/lib:$LD_LIBRARY_PATH && ./tests/test1 

clean:
	rm  main
	cd libfractal && rm *.a *.o
