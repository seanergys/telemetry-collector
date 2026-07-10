CXX := g++
CXXFLAGS := -I/Users/di75gix/codes/seanergys/seanerbus/cpp_examples/install/include -std=c++20 -Wall -Wextra -Wpedantic -O2

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
	LIB_EXT := dylib
	LIBFLAGS := -dynamiclib
	LDLIBS := -lyaml-cpp -L/usr/local/opt/capnp/lib/ -lcapnp -lkj -L/Users/di75gix/codes/seanergys/seanerbus/cpp_examples/install/lib -lseanerbus
else
	LIB_EXT := so
	LIBFLAGS := -shared -fPIC
	LDLIBS := -ldl -lyaml-cpp
endif

COLLECTOR := telemetry-collector

COMMON_OBJS := plugin.o pluginManager.o config.o

PLUGIN_SRCS := $(wildcard plugins/*_plugin.cpp)

PLUGIN_LIBS := $(patsubst plugins/%_plugin.cpp,\
	plugins/%.$(LIB_EXT),\
	$(PLUGIN_SRCS))

all: $(COLLECTOR) $(PLUGIN_LIBS)

$(COLLECTOR): telemetry-collector.o $(COMMON_OBJS)
	$(CXX) $^ -o $@ $(LDLIBS)

plugins/%.$(LIB_EXT): plugins/%_plugin.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(LIBFLAGS) $(LDLIBS) $^ -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o plugins/*.so plugins/*.dylib $(COLLECTOR)

.PHONY: all clean