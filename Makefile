
all: cchat cserverd



servermain.o: servermain.cpp
	$(CXX) -Wall -lpthread -c servermain.cpp -I.

clientmain.o: clientmain.cpp
	$(CXX) -Wall -lpthread -c clientmain.cpp -I.

main.o: main.cpp
	$(CXX) -Wall -lpthread -c main.cpp -I.



cchat: clientmain.o 
	$(CXX) -L./ -Wall -lpthread -o cchat clientmain.o 

cserverd: servermain.o 
	$(CXX) -L./ -Wall -lpthread -o cserverd servermain.o 




clean:
	rm *.o *.a cserverd cchat
