// clang++ -std=c++17 -o bin/main src/main.cpp -I/opt/homebrew/Cellar/sdl3/3.2.24/include -L/opt/homebrew/Cellar/sdl3/3.2.24/lib -I/opt/homebrew/Cellar/sdl3_image/3.2.4/include -L/opt/homebrew/Cellar/sdl3_image/3.2.4/lib -lSDL3 -lSDL3_image
// ./bin/main

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef float_t f32;
typedef double_t f64;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

struct SDLWindowDeleter
{
  void operator()(SDL_Window *window) const
  {
    if (window)
      SDL_DestroyWindow(window);
  }
};

struct SDLRendererDeleter
{
  void operator()(SDL_Renderer *renderer) const
  {
    if (renderer)
      SDL_DestroyRenderer(renderer);
  }
};

struct SDLTextureDeleter
{
  void operator()(SDL_Texture *texture) const
  {
    if (texture)
      SDL_DestroyTexture(texture);
  }
};

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

std::string readFile(const std::string &path)
{
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file)
    return "";
  std::streamsize fileSize = file.tellg();
  if (fileSize == -1)
    return "";
  file.seekg(0, std::ios::beg);
  std::vector<char> buffer(fileSize);
  if (!file.read(buffer.data(), fileSize))
    return "";
  return std::string(buffer.begin(), buffer.end());
}

std::vector<u32> loadImage(const char *file)
{
  SDL_Surface *surface = IMG_Load(file);
  if (!surface)
    return {};
  SDL_Surface *rgbaSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA8888);
  SDL_DestroySurface(surface);
  if (!rgbaSurface)
    return {};
  std::vector<u32> pixels(rgbaSurface->w * rgbaSurface->h);
  SDL_LockSurface(rgbaSurface);
  memcpy(pixels.data(), rgbaSurface->pixels, rgbaSurface->w * rgbaSurface->h * sizeof(u32));
  SDL_UnlockSurface(rgbaSurface);
  SDL_DestroySurface(rgbaSurface);
  return pixels;
}

typedef struct Input
{
  bool keyW;
  bool keyA;
  bool keyS;
  bool keyD;
  bool keySpace;
  bool keySpaceLast;
  bool keyShift;
  bool keyShiftLast;
  f32 mouseMovementX;
  f32 mouseMovementY;
} Input;

typedef struct Player
{
  f32 velocityZ;
} Player;

typedef struct Camera
{
  f32 positionX;
  f32 positionY;
  f32 positionZ;
  f32 directionX;
  f32 directionY;
  f32 planeX;
  f32 planeY;
  f32 pitch;

  Camera(const f32 &positionX, const f32 &positionY, const f32 &positionZ, const f32 &directionX, const f32 &directionY, const f32 &planeX, const f32 &planeY)
      : positionX(positionX), positionY(positionY), positionZ(positionZ), directionX(directionX), directionY(directionY), planeX(planeX), planeY(planeY)
  {
  }
} Camera;

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

i32 main()
{
  const u32 resolutionX = 512;
  const u32 resolutionY = 320;
  const u32 windowScale = 3;

  bool useVSync = 0;

  if (!SDL_Init(SDL_INIT_VIDEO))
  {
    std::cerr << "error: failed to initialize SDL: " << SDL_GetError() << '\n';
    return 1;
  }
  std::unique_ptr<SDL_Window, SDLWindowDeleter> window(
      SDL_CreateWindow("title", resolutionX * windowScale, resolutionY * windowScale, SDL_WINDOW_RESIZABLE),
      SDLWindowDeleter());
  if (!window)
  {
    std::cerr << "error: failed to create window: " << SDL_GetError() << '\n';
    SDL_Quit();
    return 1;
  }
  std::unique_ptr<SDL_Renderer, SDLRendererDeleter> renderer(
      SDL_CreateRenderer(window.get(), NULL),
      SDLRendererDeleter());
  if (!renderer)
  {
    std::cerr << "error: failed to create renderer: " << SDL_GetError() << '\n';
    SDL_Quit();
    return 1;
  }
  if (!SDL_SetRenderVSync(renderer.get(), useVSync))
  {
    std::cerr << "error: failed to set vsync" << '\n';
    SDL_Quit();
    return 1;
  }
  std::unique_ptr<SDL_Texture, SDLTextureDeleter> windowTexture(
      SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, resolutionX, resolutionY),
      SDLTextureDeleter());
  if (!windowTexture)
  {
    std::cerr << "error: failed to create texture: " << SDL_GetError() << '\n';
    SDL_Quit();
    return 1;
  }
  SDL_SetTextureScaleMode(windowTexture.get(), SDL_SCALEMODE_NEAREST);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  bool running = 1;
  SDL_Event event;

  u64 lastTime = SDL_GetPerformanceCounter();
  u64 frequency = SDL_GetPerformanceFrequency();

  SDL_FRect windowDestRect;
  windowDestRect.w = resolutionX * windowScale;
  windowDestRect.h = resolutionY * windowScale;
  windowDestRect.x = 0;
  windowDestRect.y = 0;

  SDL_SetWindowRelativeMouseMode(window.get(), 1);

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<Input> input = std::make_unique<Input>();
  std::unique_ptr<Player> player = std::make_unique<Player>();
  std::unique_ptr<Camera> camera = std::make_unique<Camera>(1.5, 1.5, 0.6, 1, 0, 0, (f32)resolutionX / resolutionY * 0.5);

  f32 mouseLookSpeedHorizontal = 0.0015;
  f32 mouseLookSpeedVertical = 0.0005;

  u8 textureSize = 32;
  std::vector<u32> floorTexture = loadImage("res/floor.png");
  std::vector<u32> ceilingTexture = loadImage("res/ceiling.png");
  std::vector<u32> wallTexture = loadImage("res/wall.png");

  if (wallTexture.size() == 0 || floorTexture.size() == 0 || ceilingTexture.size() == 0) {
    std::cerr << "error: failed to load images" << '\n';
    SDL_Quit();
    return 1;
  }

  std::vector<u8> map = std::vector<u8>({1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1});
  u8 mapScaleX = 12;
  u8 mapScaleY = 12;

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  while (running)
  {
    input->keySpaceLast = input->keySpace;
    input->keyShiftLast = input->keyShift;

    while (SDL_PollEvent(&event))
    {
      if (event.type == SDL_EVENT_QUIT)
      {
        running = 0;
      }
      else if (event.type == SDL_EVENT_KEY_DOWN)
      {
        if (event.key.key == SDLK_W)
        {
          input->keyW = 1;
        }
        else if (event.key.key == SDLK_A)
        {
          input->keyA = 1;
        }
        else if (event.key.key == SDLK_S)
        {
          input->keyS = 1;
        }
        else if (event.key.key == SDLK_D)
        {
          input->keyD = 1;
        }
        else if (event.key.key == SDLK_SPACE)
        {
          input->keySpace = 1;
        }
        else if (event.key.key == SDLK_LSHIFT)
        {
          input->keyShift = 1;
        }
      }
      else if (event.type == SDL_EVENT_KEY_UP)
      {
        if (event.key.key == SDLK_W)
        {
          input->keyW = 0;
        }
        else if (event.key.key == SDLK_A)
        {
          input->keyA = 0;
        }
        else if (event.key.key == SDLK_S)
        {
          input->keyS = 0;
        }
        else if (event.key.key == SDLK_D)
        {
          input->keyD = 0;
        }
        else if (event.key.key == SDLK_SPACE)
        {
          input->keySpace = 0;
        }
        else if (event.key.key == SDLK_LSHIFT)
        {
          input->keyShift = 0;
        }
      }
    }
    SDL_GetRelativeMouseState(&input->mouseMovementX, &input->mouseMovementY);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    u64 currentTime = SDL_GetPerformanceCounter();
    f32 deltaTime = (f32)(currentTime - lastTime) / frequency;
    lastTime = currentTime;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    void *uncastedPixels;
    i32 pitch;
    if (!SDL_LockTexture(windowTexture.get(), NULL, &uncastedPixels, &pitch))
    {
      running = 0;
      break;
    }
    u32 *pixels = static_cast<u32 *>(uncastedPixels);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    f32 movementX = 0;
    f32 movementY = 0;

    if (input->keyW && !input->keyS)
    {
      movementX += 7 * camera->directionX;
      movementY += 7 * camera->directionY;
    }
    if (input->keyS && !input->keyW)
    {
      movementX -= 7 * camera->directionX;
      movementY -= 7 * camera->directionY;
    }
    if (input->keyA && !input->keyD)
    {
      movementX += 3 * camera->directionY;
      movementY -= 3 * camera->directionX;
    }
    if (input->keyD && !input->keyA)
    {
      movementX -= 3 * camera->directionY;
      movementY += 3 * camera->directionX;
    }

    if (map[((i32)(camera->positionX + SDL_copysignf(1, movementX) * 0.2)) + ((i32)(camera->positionY)) * mapScaleX] != 0)
    {
      movementX = -movementX * 0.5;
    }
    if (map[((i32)(camera->positionX)) + ((i32)(camera->positionY + SDL_copysignf(1, movementY) * 0.2)) * mapScaleX] != 0)
    {
      movementY = -movementY * 0.5;
    }

    camera->positionX += movementX * deltaTime;
    camera->positionY += movementY * deltaTime;

    f32 oldDirectionX = camera->directionX;
    camera->directionX = camera->directionX * SDL_cosf(input->mouseMovementX * mouseLookSpeedHorizontal) - camera->directionY * SDL_sinf(input->mouseMovementX * mouseLookSpeedHorizontal);
    camera->directionY = oldDirectionX * SDL_sinf(input->mouseMovementX * mouseLookSpeedHorizontal) + camera->directionY * SDL_cosf(input->mouseMovementX * mouseLookSpeedHorizontal);
    f32 oldPlaneX = camera->planeX;
    camera->planeX = camera->planeX * SDL_cosf(input->mouseMovementX * mouseLookSpeedHorizontal) - camera->planeY * SDL_sinf(input->mouseMovementX * mouseLookSpeedHorizontal);
    camera->planeY = oldPlaneX * SDL_sinf(input->mouseMovementX * mouseLookSpeedHorizontal) + camera->planeY * SDL_cosf(input->mouseMovementX * mouseLookSpeedHorizontal);
    camera->pitch -= input->mouseMovementY * mouseLookSpeedVertical;
    camera->pitch = SDL_clamp(camera->pitch, -0.3, 0.3);

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    i32 pixelPitch = camera->pitch * resolutionY;
    i32 pixelPositionZ = (f32)resolutionY / 2 * (camera->positionZ * 2 - 1);

    for (i32 y = 0; y < resolutionY; y++)
    {
      bool isFloor = y > resolutionY / 2 + pixelPitch;
      f32 rayDirectionX0 = camera->directionX - camera->planeX;
      f32 rayDirectionY0 = camera->directionY - camera->planeY;
      f32 rayDirectionX1 = camera->directionX + camera->planeX;
      f32 rayDirectionY1 = camera->directionY + camera->planeY;
      i32 p = isFloor ? (y - resolutionY / 2 - pixelPitch) : (resolutionY / 2 - y + pixelPitch);
      f32 camZ = isFloor ? (0.5 * resolutionY + pixelPositionZ) : (0.5 * resolutionY - pixelPositionZ);
      f32 rowDistance = camZ / p;
      f32 floorStepX = rowDistance * (rayDirectionX1 - rayDirectionX0) / resolutionX;
      f32 floorStepY = rowDistance * (rayDirectionY1 - rayDirectionY0) / resolutionX;
      f32 floorX = camera->positionX + rowDistance * rayDirectionX0;
      f32 floorY = camera->positionY + rowDistance * rayDirectionY0;

      for (u32 x = 0; x < resolutionX; ++x)
      {
        u32 cellX = (u32)(floorX);
        u32 cellY = (u32)(floorY);
        u32 textureX = (u32)(textureSize * (floorX - cellX)) & (textureSize - 1);
        u32 textureY = (u32)(textureSize * (floorY - cellY)) & (textureSize - 1);
        floorX += floorStepX;
        floorY += floorStepY;

        if (isFloor)
        {
          pixels[x + y * resolutionX] = floorTexture[textureX + textureY * textureSize];
        }
        else
        {
          pixels[x + y * resolutionX] = ceilingTexture[textureX + textureY * textureSize];
        }
      }
    }

    for (u32 x = 0; x < resolutionX; x++)
    {
      f32 cameraX = (f32)x / resolutionX * 2 - 1;
      f32 rayDirectionX = camera->directionX + camera->planeX * cameraX;
      f32 rayDirectionY = camera->directionY + camera->planeY * cameraX;
      i32 mapX = camera->positionX;
      i32 mapY = camera->positionY;
      f32 deltaDistanceX = (rayDirectionX == 0) ? 1e5 : SDL_fabsf(1 / rayDirectionX);
      f32 deltaDistanceY = (rayDirectionY == 0) ? 1e5 : SDL_fabsf(1 / rayDirectionY);
      f32 sideDistanceX;
      f32 sideDistanceY;
      i32 stepX;
      i32 stepY;

      if (rayDirectionX < 0)
      {
        sideDistanceX = (camera->positionX - mapX) * deltaDistanceX;
        stepX = -1;
      }
      else
      {
        sideDistanceX = (mapX + 1 - camera->positionX) * deltaDistanceX;
        stepX = 1;
      }
      if (rayDirectionY < 0)
      {
        sideDistanceY = (camera->positionY - mapY) * deltaDistanceY;
        stepY = -1;
      }
      else
      {
        sideDistanceY = (mapY + 1 - camera->positionY) * deltaDistanceY;
        stepY = 1;
      }

      for (i32 i = 0; i < 24; i++)
      {
        bool side = 0;
        if (sideDistanceX < sideDistanceY)
        {
          sideDistanceX += deltaDistanceX;
          mapX += stepX;
        }
        else
        {
          sideDistanceY += deltaDistanceY;
          mapY += stepY;
          side = 1;
        }

        if (mapX < 0 || mapX >= mapScaleX || mapY < 0 || mapY >= mapScaleY)
          break;

        u8 mapHitIndex = map[mapX + mapY * mapScaleX];

        if (mapHitIndex != 0)
        {
          f32 perpendicularWallDistance;
          if (side == 0)
            perpendicularWallDistance = sideDistanceX - deltaDistanceX;
          else
            perpendicularWallDistance = sideDistanceY - deltaDistanceY;
          f32 lineHeight = (f32)resolutionY / perpendicularWallDistance;
          f32 zOffset = pixelPositionZ / perpendicularWallDistance;
          zOffset += pixelPitch;
          u32 drawStart = SDL_clamp(SDL_roundf(-lineHeight * 0.5 + resolutionY * 0.5 + zOffset), 0, resolutionY);
          u32 drawEnd = SDL_clamp(SDL_roundf(lineHeight * 0.5 + resolutionY * 0.5 + zOffset), 0, resolutionY);

          f32 wallX;
          if (side == 0)
            wallX = camera->positionY + perpendicularWallDistance * rayDirectionY;
          else
            wallX = camera->positionX + perpendicularWallDistance * rayDirectionX;
          wallX -= SDL_floorf(wallX);
          i32 textureX = (i32)(wallX * (f32)textureSize);
          if (side == 0 && rayDirectionX > 0)
            textureX = textureSize - textureX - 1;
          if (side == 1 && rayDirectionY < 0)
            textureX = textureSize - textureX - 1;
          f32 step = 1 * textureSize / lineHeight;
          f32 texPos = ((drawStart - resolutionY * 0.5 + lineHeight * 0.5) - zOffset) * step;

          for (u32 y = drawStart; y < drawEnd; y++)
          {
            i32 textureY = (i32)(texPos + 0.001) & (textureSize - 1);
            texPos += step;

            pixels[x + y * resolutionX] = wallTexture[textureX + textureY * textureSize];
          }

          break;
        }
      }
    }

    pixels[resolutionX / 2 - 1 + (resolutionY / 2 - 1) * resolutionX] = 0xffffffff;
    pixels[resolutionX / 2 - 1 + resolutionY / 2 * resolutionX] = 0xffffffff;
    pixels[resolutionX / 2 + (resolutionY / 2 - 1) * resolutionX] = 0xffffffff;
    pixels[resolutionX / 2 + resolutionY / 2 * resolutionX] = 0xffffffff;

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    SDL_UnlockTexture(windowTexture.get());
    SDL_RenderTexture(renderer.get(), windowTexture.get(), NULL, &windowDestRect);
    SDL_RenderPresent(renderer.get());
  }

  //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

  SDL_Quit();
  return 0;
}
