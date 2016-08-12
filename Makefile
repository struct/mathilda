## Copyright 2015/2016 Yahoo Inc.
## Licensed under the BSD license, see LICENSE file for terms.
## Written by Chris Rohlf

CXX = g++
CXXFLAGS = -std=c++11 -Wall -pedantic
LDFLAGS = -lcurl -luv
MATHILDA_LDFLAGS = build/libmathilda.so
INCLUDE_DIR = -I utils/ -I lib/
DEBUG_FLAGS = -DDEBUG -ggdb
UNIT_TEST = -DMATHILDA_TESTING
ASAN = -fsanitize=address -lasan

all: library spider dirbuster test

## Builds libmathilda library shared object
## This includes everything in utils/
library:
	mkdir build/ ; $(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -fPIC lib/mathilda.cpp lib/mathilda_fork.cpp lib/mathilda_c.cpp \
	utils/mathilda_utils.cpp utils/mathilda_dns.cpp \
	$(LDFLAGS) $(INCLUDE_DIR) -shared -o build/libmathilda.so && chmod 644 build/libmathilda.so

## Builds a stand alone spider for testing
spider: library
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDE_DIR) -DSPIDER utils/spider.cpp -o build/spider $(LDFLAGS) $(MATHILDA_LDFLAGS) -lgumbo

## Builds a stand alone dirbuster for testing
dirbuster: library
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) $(INCLUDE_DIR) -DDIRBUSTER utils/dirbuster.cpp -o build/dirbuster $(LDFLAGS) $(MATHILDA_LDFLAGS)

## Builds the standard unit tests found
## in mathilda.cpp
test: library
	$(CXX) $(CXXFLAGS) $(UNIT_TEST) $(INCLUDE_DIR) $(DEBUG_FLAGS) lib/mathilda.cpp lib/mathilda_fork.cpp \
	utils/mathilda_utils.cpp -o build/mathilda_test $(LDFLAGS) $(MATHILDA_LDFLAGS)

## Cleans up all build artificats by
## deleting everything in build/
clean:
	rm build/*