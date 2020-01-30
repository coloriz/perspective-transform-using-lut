CXXFLAGS=-c -Wall -std=c++17

CXXFLAGS+=`pkg-config --cflags opencv4`
LDFLAGS+=`pkg-config --libs opencv4`

SOURCES=app.cpp common.cpp
OBJS=$(SOURCES:.cpp=.o)

TARGET=app.out

all: $(TARGET) getBuildInformation.out

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

getBuildInformation.out: getBuildInformation.cpp
	$(CXX) `pkg-config --cflags --libs opencv4` -std=c++17 -o $@ $^

.cpp.o:
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)