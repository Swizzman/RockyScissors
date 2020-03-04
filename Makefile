
all: cchat cserverd



servermain.o: servermain.cpp
	$(CXX) -Wall -c servermain.cpp -I.

clientmain.o: clientmain.cpp
	$(CXX) -Wall -c clientmain.cpp -I.

main.o: main.cpp
	$(CXX) -Wall -c main.cpp -I.



cchat: clientmain.o 
	$(CXX) -L./ -Wall -o cchat clientmain.o 

cserverd: servermain.o 
	$(CXX) -L./ -Wall -o cserverd servermain.o 




clean:
	rm *.o *.a cserverd cchat
