CFLAGS = -std=c++20 -DDEBUG -O3
LDFLAGS = -lglfw -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
STBPATH = /usr/lib/stb-image/
OBJ_LOADER_PATH = /usr/lib/tiny-obj-loader/
VulkanRenderer: main.cpp
	cd shaders && ./compileShaders.sh && cd .. &&\
	g++ $(CFLAGS) -o VulkanRenderer *.cpp $(LDFLAGS) -I$(STBPATH) -I$(OBJ_LOADER_PATH)

.PHONY: test clean

test: VulkanRenderer
	./VulkanRenderer

clean:
	rm -f VulkanRenderer