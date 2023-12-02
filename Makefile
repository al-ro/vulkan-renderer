CFLAGS = -std=c++20 -DDEBUG
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
STBPATH = /usr/lib/stb-image/
VulkanRenderer: main.cpp
	cd shaders && pwd && ./compileShaders.sh && cd .. &&\
	g++ $(CFLAGS) -o VulkanRenderer main.cpp $(LDFLAGS) -I$(STBPATH)

.PHONY: test clean

test: VulkanRenderer
	./VulkanRenderer

clean:
	rm -f VulkanRenderer