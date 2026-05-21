#include <stdint.h>

// Константа, размещённая в секции .config
__attribute__((section(".config")))
const uint8_t slave_default_address = 1;