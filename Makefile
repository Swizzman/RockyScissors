
all: client server



servermain.o: servermain.cpp
	$(CXX) -Wall -c servermain.cpp -I.

clientmain.o: clientmain.cpp
	$(CXX) -Wall -c clientmain.cpp -I.

main.o: main.cpp
	$(CXX) -Wall -c main.cpp -I.



client: clientmain.o 
	$(CXX) -L./ -Wall -o client clientmain.o 

server: servermain.o 
	$(CXX) -L./ -Wall -o server servermain.o 




clean:
	rm *.o *.a server client
