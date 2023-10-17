CFLAGS = -std=c++17 -O2
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
VulkanRenderer: main.cpp
	g++ $(CFLAGS) -o VulkanRenderer main.cpp $(LDFLAGS)

.PHONY: test clean

test: VulkanRenderer
	./VulkanRenderer

clean:
	rm -f VulkanRenderer