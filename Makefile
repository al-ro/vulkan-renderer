CFLAGS = -std=c++20 -DDEBUG
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
VulkanRenderer: main.cpp
	g++ $(CFLAGS) -o VulkanRenderer main.cpp $(LDFLAGS)

.PHONY: test clean

test: VulkanRenderer
	./VulkanRenderer

clean:
	rm -f VulkanRenderer