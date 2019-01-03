# A6lib 
[![Build Status](https://travis-ci.com/IMAN4K/A6lib.svg?branch=master)](https://travis-ci.com/IMAN4K/A6lib)
[![release](https://img.shields.io/badge/release-v1.0.1-blue.svg)](https://github.com/IMAN4K/A6lib/tree/v1.0.1)
[![APMLicense](https://img.shields.io/apm/l/vim-mode.svg)](https://github.com/IMAN4K/A6lib/blob/master/LICENSE.md)
[![docs](https://img.shields.io/badge/docs-Doxygen-red.svg)](https://github.com/IMAN4K/A6lib/tree/master/docs)
[![platform](https://img.shields.io/badge/platform-espressif8266%20%7C%20atmelavr-orange.svg)](https://github.com/IMAN4K/A6lib)

An `Arduino` library for controlling `Ai-Thinker A6` GSM modem, It currently supports `ESP8266` and `AVR` architectures and also works with other GSM modems supporting standard AT command set(e.g `SIM800`, `SIM900`,...)

# Getting Started
* Clone or download [latest ](https://github.com/IMAN4K/A6lib/releases)`git` version to your `Arduino/libraries` directory
* Before include it to your project, you may need to modify `A6lib.h`:
```c++
/* comment the following to disable them */
//#define DEBUG
#define SIM800_T
//#define A6_T
```
* Then include it and use the public APIs to control your modem or check out one of the examples

## Related Information
  * [API Reference & Documentation](https://github.com/IMAN4K/A6lib/tree/master/docs)
  * [Examples](https://github.com/IMAN4K/A6lib/tree/master/examples)
  * [Hardware Consideration](https://github.com/IMAN4K/A6lib/wiki)