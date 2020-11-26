
all: sspgame sspd



servermain.o: servermain.cpp
	$(CXX) -Wall -pthread -c servermain.cpp  -I. 

clientmain.o: clientmain.cpp
	$(CXX) -Wall -pthread -c clientmain.cpp -I .


sspgame: clientmain.o 
	$(CXX) -L./ -Wall -pthread -o sspgame clientmain.o 

sspd: servermain.o 
	$(CXX) -L./ -Wall -pthread -o sspd servermain.o 


clean:
	rm *.o sspd sspgame
