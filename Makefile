test_prg: test_prg.cpp libmap.a goldchase.h
	g++ test_prg.cpp serverdaemon.cpp -g -o test_prg -L. -lmap -lpanel -lncurses -lpthread -lrt -std=c++11

libmap.a: Screen.o Map.o
	ar -r libmap.a Screen.o Map.o

Screen.o: Screen.cpp
	g++ -c Screen.cpp

Map.o: Map.cpp
	g++ -c Map.cpp

clean:
	rm -f Screen.o Map.o libmap.a test_prg
