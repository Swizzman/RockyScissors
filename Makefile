
all: sspgame sspd



servermain.o: servermain.cpp
	$(CXX) -Wall -lpthread -c servermain.cpp  -I. 

clientmain.o: clientmain.cpp
	$(CXX) -Wall -lpthread -c clientmain.cpp -I .


sspgame: clientmain.o 
	$(CXX) -L./ -Wall -lpthread -o sspgame clientmain.o 

sspd: servermain.o 
	$(CXX) -L./ -Wall -lpthread -o sspd servermain.o 


clean:
	rm *.o sspd sspgame
