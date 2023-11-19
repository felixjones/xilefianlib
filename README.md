# What the heck is this?

Just a collection of random, sometimes useful, code that I write.

# FAQ

## Do you accept pull requests?

Absolutely. Some of this stuff has undefined behaviour and bugs.

## What's up with the name?

I used to have a library called "xiLib", but that seems like a poor name these days, so `xilefianlib` it is.
Don't think too hard as to what "Xilefian" means.

# What's in the box

## C++20

### bvec

An implementation of `std::vector<bool>` that provides something akin to small-string-optimisation.

### btree

Unbalanced binary tree container.

## Arm GBA

### Mode 4 Column Unpack/Pack

Pixel buffer column pack/unpack utilities for Mode 4 video memory (240 pixels wide, 8 bits per pixel).

#### 4x Columns

Packs/unpacks 4 columns of pixel data at a time.

```c
#include <xilefian/m4column.h>

#define VIDEO4_VRAM (*(char*) 0x6000000)

int main() {
    char columnBuffer[160][4];
    xilefian_m4col_unpack4(columnBuffer, VIDEO4_VRAM + 4, 160); /* Read 4x160 pixels into columnBuffer, starting at column 4 */
    xilefian_m4col_pack4(VIDEO4_VRAM + 8, columnBuffer, 160); /* Write 4x160 pixels from columnBuffer, starting at column 8 */
}
```

| Signature                                                            | Description                                                                                            |
|:---------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------|
| `void xilefian_m4col_pack4(void* dest, const void* src, size_t n)`   | Packs n rows of 4 columns of pixels from src into dest<br/>Assumes n is non-zero and a multiple of 4   |
| `void xilefian_m4col_unpack4(void* dest, const void* src, size_t n)` | Unpacks n rows of 4 columns of pixels from src into dest<br/>Assumes n is non-zero and a multiple of 4 |

#### 2x Columns

Packs/unpacks 2 columns of pixel data at a time.

```c
#include <xilefian/m4column.h>

#define VIDEO4_VRAM (*(char*) 0x6000000)

int main() {
    char columnBuffer[160][2];
    xilefian_m4col_unpack2(columnBuffer, VIDEO4_VRAM + 2, 160); /* Read 2x160 pixels into columnBuffer, starting at column 2 */
    xilefian_m4col_pack2(VIDEO4_VRAM + 4, columnBuffer, 160); /* Write 2x160 pixels from columnBuffer, starting at column 4 */
}
```

| Signature                                                            | Description                                                                                            |
|:---------------------------------------------------------------------|:-------------------------------------------------------------------------------------------------------|
| `void xilefian_m4col_pack2(void* dest, const void* src, size_t n)`   | Packs n rows of 2 columns of pixels from src into dest<br/>Assumes n is non-zero and a multiple of 4   |
| `void xilefian_m4col_unpack2(void* dest, const void* src, size_t n)` | Unpacks n rows of 2 columns of pixels from src into dest<br/>Assumes n is non-zero and a multiple of 4 |
