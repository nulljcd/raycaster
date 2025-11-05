# raycaster
A simple low-level raycaster engine made like the originals. It uses SDL3 for input and displaying the screen, and it uses SDL3_image for loading images.
### How to play (Linux and MacOS)
build (with homebrew)
```
clang++ -std=c++17 -o bin/main src/main.cpp -I/opt/homebrew/Cellar/sdl3/3.2.24/include -L/opt/homebrew/Cellar/sdl3/3.2.24/lib -I/opt/homebrew/Cellar/sdl3_image/3.2.4/include -L/opt/homebrew/Cellar/sdl3_image/3.2.4/lib -lSDL3 -lSDL3_image
```
run
```
./bin/main
```
<img width="1648" height="1100" alt="Screenshot 2025-10-26 at 17 22 21" src="https://github.com/user-attachments/assets/2ecfda62-0c23-4462-81f6-09f876c21230" />
Art inspired by the game POST VOID. https://store.steampowered.com/app/1285670/Post_Void/
