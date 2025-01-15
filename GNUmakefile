# Optimization flags.  Use -g for debug.
OFLAGS := -O3

BOOST_DIR = /tools_vendor/FOSS/boost/1.82
LDFLAGS = -L$(BOOST_DIR)/lib

# Command to compile .cpp files.
override CXXFLAGS += -MMD -MP -std=c++20 $(OFLAGS) -pedantic -Wall -Wextra

# Rule to make a .o from a .cpp file.
%.o:  %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

SRC_FILES := Domain.cpp Aplic.cpp example.cpp
OBJ_FILES := $(SRC_FILES:.cpp=.o)
DEP_FILES := $(SRC_FILES:.cpp=.d)
example: example.o Domain.o Aplic.o
	$(CXX) $(LDFLAGS) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ -l:libboost_program_options.a -lz

# Include Generated Dependency files if available.
-include $(DEP_FILES)

clean:
	$(RM) example $(OBJ_FILES) $(DEP_FILES)

.PHONY: clean
