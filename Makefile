## Copyright 2015/2016 Yahoo Inc.
## Licensed under the BSD license, see LICENSE file for terms.
## Written by Chris Rohlf

CXX = g++
CXXFLAGS = -std=c++11 -Wall -pedantic
LDFLAGS = -lcurl -luv
TEST_FLAGS = -DDEBUG -ggdb -DMATHILDA_TESTING

all: library

## Builds the standard unit tests found
## in mathilda.cpp
test:
	$(CXX) $(CXXFLAGS) $(TEST_FLAGS) mathilda.cpp mathilda_fork.cpp mathilda_utils.cpp -o mathilda_test $(LDFLAGS)

## Builds a stand alone spider for testing
spider:
	$(CXX) $(CXXFLAGS) $(TEST_FLAGS) -DSPIDER mathilda.cpp mathilda_fork.cpp mathilda_utils.cpp spider.cpp -o spider $(LDFLAGS) -lgumbo

## Builds a stand alone dirbuster for testing
dirbuster:
	$(CXX) $(CXXFLAGS) $(TEST_FLAGS) -DDIRBUSTER mathilda.cpp mathilda_fork.cpp mathilda_utils.cpp dirbuster.cpp -o dirbuster $(LDFLAGS)

## Builds libmathilda DSO. This is the default
library:
	$(CXX) $(CXXFLAGS) $(TEST_FLAGS) -fPIC -DSPIDER mathilda.cpp mathilda_fork.cpp mathilda_utils.cpp spider.cpp $(LDFLAGS) -shared -o libmathilda.so

## Cleans up all build artificats
clean:
	rm *.o mathilda_test spider dirbuster libmathilda.so