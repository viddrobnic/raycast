# Raycast

I thought it would be fun to implement my own raycast engine. This repository
is the result of that idea. On launch the program generates a random maze
which the player can explore.

## Building

The program depends on [raylib](https://www.raylib.com/) to handle input events
and render the pixel buffer to the screen. If you are using macOS, you can
build and run the program with:

```sh
brew install raylib
make run
```

For other platforms the `INC_FLAGS` and `LDFLAGS` in the `Makefile` have to
be adjusted before running `make run`.

## License

[MIT License](LICENSE)

