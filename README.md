# eigenmath port for micropython

![PicoCalc screenshoot](./imgs/screenshot.jpg)

## Firmware for pico2
-- micropython_ulab_eigenmath_pico2.uf2
No file system included. Also added ulab from :https://github.com/v923z/micropython-ulab


## Firmware for PicoCalc system
### With filesystem
The uf2 file already included file system and main.py, boot.py. Just flash it and remove the usb link to the pico module, tune on the picocalc. 
- **NO FILE COPY NEEDED!! The old file system will be destroyed!**
- picocalc_micropython_ulab_eigenmath_withfilesystem_pico2.uf2 (you could use it with your pico 2 or pico 2w module)
Included ulab, eigenmath port (https://github.com/zenodante/eigenmath_micropython), make picocalc a full function advanced calculator!

```python

import eigenmath
em = eigenmath.EigenMath(300*1024) #apply 300kB heap from micropython GC
em("d(sin(x),x)")#run commonds
em.run("d(sin(x),x)",True) #optinal True means not print result, return result as a bytearray with \n\x0
with open("filename",'r') as f:
  em.runfile(f) #run script file
em.status() #get memory status
em.reset()#reset the heap and everything

#if you are using the uf2 for picocalc, you don't need to init em, it has been init in the boot.py

```
The optinal par for run() and runfile() config the output behavior, if set to True, the system won't print the result in REPL, but return the result as a bytearray. 
## Build Instructions
eigenmath port for micropython, for example, here is the cmake command for rp2 port.

```sh
cd micropython/ports/rp2
git submodule update --init --recursive
mkdir build && cd build
cmake .. -G "Unix Makefiles" \
  -DUSER_C_MODULES="Path/To/eigenmath_micropython/micropython.cmake;\
  -DMICROPY_BOARD=[TARGET_BOARD] -DPICO_STACK_SIZE=0x6000
```
RPI_PICO2 or RPI_PICO2_W recommended!


## Credits
Eigenmath https://github.com/georgeweigt/eigenmath
