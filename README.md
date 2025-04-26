# eigenmath port for micropython

![PicoCalc screenshoot](./imgs/screenshot.jpg)
## Build Instructions
eigenmath port for micropython, for example, here is the cmake commond for rp2 port.

```sh
cd micropython/ports/rp2
git submodule update --init --recursive
mkdir build && cd build
cmake .. -G "Unix Makefiles" \
  -DUSER_C_MODULES="Path/To/eigenmath_micropython/micropython.cmake;\
  -DMICROPY_BOARD=[TARGET_BOARD] -DPICO_STACK_SIZE=0x6000
```
RPI_PICO2 or RPI_PICO2_W recommonded!


## Credits
Eigenmath https://github.com/georgeweigt/eigenmath