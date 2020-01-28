#ifndef CIRCLE_UTIL_H
#define CIRCLE_UTIL_H

// Since C++ 11 this header provides all the size specific data types
#include <cstdint>


uint32_t bswap32(uint32_t ulValue) {
#if BYTE_SWAP_DISABLED
    return ulValue;
#else
    uint32_t word;
    word  = (uint32_t) ((ulValue >> 0x00) & 0xFF) << (24);  // low-low
    word |= (uint32_t) ((ulValue >> 0x08) & 0xFF) << (16);  // low-high
    word |= (uint32_t) ((ulValue >> 0x10) & 0xFF) << (8); // high-low
    word |= (uint32_t) ((ulValue >> 0x18) & 0xFF) << (0); // high-high
    return word;
#endif
};


void *memset(void *pBuffer, int nValue, size_t nLength) {     
    for (unsigned int i=0; i<nLength; i++) {
        *(reinterpret_cast<uint8_t *>(pBuffer) + i) = (uint8_t) nValue;
    }
    return pBuffer; 
};


#endif // CIRCLE_UTIL_H
