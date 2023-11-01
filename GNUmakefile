# Optimization flags.  Use -g for debug.
OFLAGS := -O3

# Command to compile .cpp files.
override CXXFLAGS += -MMD -MP -std=c++20 $(OFLAGS) -pedantic -Wall -Wextra

# Rule to make a .o from a .cpp file.
%.o:  %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<


SRC_FILES := Domain.cpp Aplic.cpp aplic-test.cpp
OBJ_FILES := $(SRC_FILES:.cpp=.o)
DEP_FILES := $(SRC_FILES:.cpp=.d)
aplic-test: aplic-test.o Domain.o Aplic.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -o $@ $^ -l:libboost_program_options.a -lz

# Include Generated Dependency files if available.
-include $(DEP_FILES)

clean:
	$(RM) aplic-test $(OBJ_FILES) $(DEP_FILES)

.PHONY: clean
