CXX = g++
CXXFLAGS = -Wall -std=c++11
SRC = src

all: papa

papa: $(SRC)/main.o $(SRC)/buzon.o
	$(CXX) $(CXXFLAGS) -o papa $(SRC)/main.o $(SRC)/buzon.o

$(SRC)/main.o: $(SRC)/main.cc $(SRC)/buzon.h
	$(CXX) $(CXXFLAGS) -c $(SRC)/main.cc -o $(SRC)/main.o

$(SRC)/buzon.o: $(SRC)/buzon.cc $(SRC)/buzon.h
	$(CXX) $(CXXFLAGS) -c $(SRC)/buzon.cc -o $(SRC)/buzon.o

clean:
	rm -f $(SRC)/*.o papa