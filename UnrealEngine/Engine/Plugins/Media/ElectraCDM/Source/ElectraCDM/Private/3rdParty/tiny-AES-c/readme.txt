tiny AES c from
  https://github.com/kokke/tiny-AES-c

- Retrieved on 2020/3/24
- Modified aes.h to enable only AES 128 CBC and CTR mode; surrounded API by a namespace
- Renamed aes.c to aes.cpp; surrounded API by a namespace
- Prefixed all macros/defines with TINYAES_ to avoid conflicts with existing macros,
  methods or variables in monolithic builds.

