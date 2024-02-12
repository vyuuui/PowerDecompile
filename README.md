# Power Decompiler
Disassembly and decompilation suite tuned for working with GameCube/Wii binaries. Work in progress!

## Building
### Linux
Clone, cmake, and make.
```
git clone https://git.shiiion.me/vyuuui/ppc-decomp.git --recurse-submodules
mkdir ppc-decomp/build
cd ppc-decomp/build
cmake ..
make -j
```
Writes the product to `<repository>/build/decomp`

### Windows
Same steps as linux, just use Visual Studio/MSBuild.

## Plans
Here's what I can hope to see accomplished by this
#### Basic feature set
- Subroutine decompilation targeting C
- Subroutine detection

Here's what I can hope to see accomplished by this if what I can hope to see accomplished by this is accomplished
#### It'd-be-nice feature set
 - In house graph view listing
 - Disassembly/Decompilation side by side
 - Function pointer in data detection
 - Symbol map integration
 - Integration with dynamic linker
