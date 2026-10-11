# Host-side tests: compile the real component against the stub ESPHome headers in inc/.
COMP := ../../components/me007ys
CXXFLAGS := -std=gnu++17 -Wall -Wextra -I inc -I $(COMP)

test: harness edge
	./harness
	./edge

harness: harness.cpp $(COMP)/me007ys_sensor.cpp $(COMP)/me007ys_sensor.h
	$(CXX) $(CXXFLAGS) harness.cpp $(COMP)/me007ys_sensor.cpp -o $@

edge: edge.cpp $(COMP)/me007ys_sensor.cpp $(COMP)/me007ys_sensor.h
	$(CXX) $(CXXFLAGS) edge.cpp $(COMP)/me007ys_sensor.cpp -o $@

.PHONY: test
