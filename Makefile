
CXX = g++
CXXFLAGS = -ggdb 
TARGET = FritzShell.cc

all: FritzShell

FritzShell: $(TARGET)
	$(CXX) $(CXXFLAGS) $(TARGET) -o $@

clean:
	rm *.o FritzShell 
