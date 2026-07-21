CXX := g++
CXXFLAGS :=-I/usr/local/opt/capnp/include/ -I/usr/local/opt/yaml-cpp/include/ -I/Users/di75gix/codes/seanergys/seanerbus/cpp_examples/install/include -I/usr/local/Cellar/googletest/1.17.0/include -std=c++20 -Wall -Wextra -Wpedantic -O2 -g

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
	LIB_EXT := dylib
	LIBFLAGS := -dynamiclib
	LDLIBS := -L/usr/local/opt/yaml-cpp/lib -lyaml-cpp -L/usr/local/opt/capnp/lib/ -lcapnp -lkj -L/Users/di75gix/codes/seanergys/seanerbus/cpp_examples/install/lib -lseanerbus
	GTEST_LIBS := -L/usr/local/Cellar/googletest/1.17.0/lib -lgtest -lgtest_main -pthread
else
	LIB_EXT := so
	LIBFLAGS := -shared -fPIC
	LDLIBS := -ldl -lyaml-cpp
	GTEST_LIBS := -L/usr/local/Cellar/googletest/1.17.0/lib -lgtest -lgtest_main -pthread
endif

COLLECTOR := telemetry-collector

COMMON_OBJS := plugin.o pluginManager.o config.o

PLUGIN_SRCS := $(wildcard plugins/*_plugin.cpp)

PLUGIN_LIBS := $(patsubst plugins/%_plugin.cpp,plugins/%.$(LIB_EXT),$(PLUGIN_SRCS))

TEST_PLUGIN_SRCS := $(wildcard tests/plugins/*_plugin.cpp)

TEST_PLUGIN_LIBS := $(patsubst tests/plugins/%_plugin.cpp,	tests/plugins/%.$(LIB_EXT),	$(TEST_PLUGIN_SRCS))

all: $(COLLECTOR) $(PLUGIN_LIBS)

tests: test_plugin_manager $(TEST_PLUGIN_LIBS)

$(COLLECTOR): telemetry-collector.o $(COMMON_OBJS)
	$(CXX) $^ -o $@ $(LDLIBS)

plugins/%.$(LIB_EXT): plugins/%_plugin.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(LIBFLAGS) $(LDLIBS) $^ -o $@

tests/plugins/%.$(LIB_EXT): tests/plugins/%_plugin.cpp $(COMMON_OBJS)
	$(CXX) $(CXXFLAGS) $(LIBFLAGS) $(LDLIBS) $^ -o $@

test_plugin_manager: tests/unit_tests/test_plugin_manager.o $(COMMON_OBJS)
	$(CXX) $^ -o tests/unit_tests/test_plugin_manager $(LDLIBS) $(GTEST_LIBS)
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f *.o plugins/*.so plugins/*.dylib $(COLLECTOR) test_plugin_manager
	rm -f tests/*.o tests/unit_tests/*.o tests/plugins/*.o tests/plugins/*.so tests/plugins/*.dylib tests/unit_tests/test_plugin_manager

.PHONY: all clean tests

