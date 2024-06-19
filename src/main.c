#include <SDL2/SDL.h>
#include <vulkan/vulkan.h>
#include <SDL2/SDL_vulkan.h>
#include <cglm/cglm.h>
#include <stdio.h>


int main(void) {
  SDL_Init(SDL_INIT_EVERYTHING);
  auto window = SDL_CreateWindow("my test window", SDL_WINDOWPOS_CENTERED,
                                 SDL_WINDOWPOS_CENTERED, 800, 600,
                                 SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN);
uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
printf("extensions supported %d\n",extensionCount);
   
mat4 matrix;
glm_mat4_identity(matrix);
glm_mat4_print(matrix, stderr);

  SDL_Event event;
  bool running = true;
  while (running) {
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_QUIT) {
        running = false;
      }
    }
  }

  SDL_DestroyWindow(window);
  SDL_Quit();

  return 0;
}
