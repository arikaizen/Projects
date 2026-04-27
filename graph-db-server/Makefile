CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2

SRCS = main.cpp \
       graph_types.cpp \
       graph_store.cpp \
       persistence.cpp \
       protocol.cpp \
       server.cpp \
       request_handler.cpp

OBJS   = $(SRCS:.cpp=.o)
TARGET = graph_db_server

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
