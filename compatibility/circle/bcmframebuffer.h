#ifndef CIRCLE_BCMFRAMEBUFFER_H
#define CIRCLE_BCMFRAMEBUFFER_H


#include "types.h"
#include <QtGlobal>
#include <QtDebug>

class CBcmFrameBuffer {

private:
	unsigned nWidth = 0;
	unsigned nHeight = 0;
	unsigned nDepth = 0;

	uint8_t *buffer = nullptr;

public:

	uint32_t palette[256] = { 0 };

	CBcmFrameBuffer(unsigned nWidth, unsigned nHeight, unsigned nDepth, unsigned nVirtualWidth = 0, unsigned nVirtualHeight = 0) {
		this->nWidth = nWidth;
		this->nHeight = nHeight;
		this->nDepth = nDepth;

		// TODO: change data type according to the colour depth (min 8 bits)
		buffer = new uint8_t[nWidth * nHeight]; 
	};

	~CBcmFrameBuffer(void) {};

	void SetPalette(u8 nIndex, u16 nRGB565) {
		if ((nIndex >= 0) && (nIndex <= 255)) {
			/* Z80 spectrum palette.  Plase note that the bit order in attribute memory
			 * is GRB in attribute whilst QImage is expecting RGB.  In this method, we are
			 * re-ordereding, i.e. swapping around the R and B components, to avoid having
			 * to swap attribute bits around when decoding colors.
     		 * 
			 * - http://www.overtakenbyevents.com/lets-talk-about-the-zx-specrum-screen-layout/
			 * - https://forum.arduino.cc/index.php?topic=285303.0
			 *
			 * The correct conversion, without colour component swapping is:
			 *
			 * uint32_t nRGBA;
			 * nRGBA  = (uint32_t) (nRGB565 >> 11 & 0x1F) << (0+3);   // red
			 * nRGBA |= (uint32_t) (nRGB565 >> 5  & 0x3F) << (8+2);   // green
			 * nRGBA |= (uint32_t) (nRGB565       & 0x1F) << (16+3);  // blue
			 * nRGBA |=                             0xFF  << 24;      // alpha
			 */
			uint32_t nRGBA;
			nRGBA  = (uint32_t) (nRGB565       & 0x1F) << (0+3);  // blue
			nRGBA |= (uint32_t) (nRGB565 >> 5  & 0x3F) << (8+2);  // green
			nRGBA |= (uint32_t) (nRGB565 >> 11 & 0x1F) << (16+3); // red
			nRGBA |=                             0xFF  << 24;     // alpha

			qDebug("Palette[0x%02X] = nRGBA: 0x%08X (RGB565: 0x%04X)", nIndex, nRGBA, nRGB565);	

			palette[nIndex] = nRGBA;
		} else {
			qCritical("Palette index 0x%02X is out of bounds", nIndex);
		}
	};

	// void SetPalette32(u8 nIndex, u32 nRGBA);	// with Depth <= 8 only
	boolean Initialize(void) {
		return true;
	};

	// u32 GetWidth(void) const;
	// u32 GetHeight(void) const;
	// u32 GetVirtWidth(void) const;
	// u32 GetVirtHeight(void) const;
	// u32 GetPitch(void) const;
	// u32 GetDepth(void) const;
	// u32 * GetBuffer(void) const { return reinterpret_cast<u32 *>(buffer); };
	u32 *GetBuffer(void) const { return reinterpret_cast<u32 *>(buffer); };
	u32 GetSize(void) const { return nWidth * nHeight; };

};


#endif // CIRCLE_BCMFRAMEBUFFER_H
