/********************************************************************** 

 	  subpixel.c -- sub pixel rendering

	  (c) Copyright 1999-2000  Microsoft Corp.  All rights reserved. 

				 8/24/00  BeatS		Add support for switchable SP filter 
 
**********************************************************************/
 
/*********************************************************************/

/*        Imports                                                    */
 
/*********************************************************************/
 
#include "precomp.hxx" 

#define FSCFG_INTERNAL 

#include    "fscdefs.h"             /* shared data types  */
#include    "scentry.h"             /* for own function prototypes */
 
#ifdef FSCFG_SUBPIXEL
 
// this should be considered a temporary solution. currently, the rasterizer is plumbed for 8 bpp output in SubPixel, 
// and appears to have John Platt's filter hard-wired in. In the future, we would rather have the overscaled b/w bitmap
// as output, such that we can do any color filtering and gamma correction outside and independent of the rasterizer 

#define CHAR_BIT      8         /* number of bits in a char */

unsigned const char ajRGBToWeight222[64] = { 
    0,1,1,2,4,5,5,6,4,5,5,6,8,9,9,10,
    16,17,17,18,20,21,21,22,20,21,21,22,24,25,25,26, 
    16,17,17,18,20,21,21,22,20,21,21,22,24,25,25,26, 
    32,33,33,34,36,37,37,38,36,37,37,38,40,41,41,42};
 
unsigned const char ajRGBToWeightMask[CHAR_BIT] = {
        0xFC, 0x7E, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01};

FS_PUBLIC void fsc_OverscaleToSubPixel (GlyphBitMap  * OverscaledBitmap, GlyphBitMap  * SubPixelBitMap) 
{
	char * pbyInputRowData, *pbyOutputRowData;			// Pointer to one scanline of data 
 	uint32 ulOverscaledBitmapWidth; // Fscaled bitmap widths 
	uint32 ulHeightIndex, ulWidthIndex, ulMaxWidthIndex;	// Scanline index and byte pixel index
    int16 sBitmapSubPixelStart; 
    uint16 usColorIndex;

    uint16 usSubPixelIndex;
    int16 uSubPixelRightShift, uSubPixelLeftShift; 
    char * pbyInputEndRowData;
 
 
 	// Start processing the bitmap
 	// This is the heart of the RGB striping algorithm 
	sBitmapSubPixelStart = OverscaledBitmap->rectBounds.left % RGB_OVERSCALE;
    if (sBitmapSubPixelStart < 0)
    {
        sBitmapSubPixelStart += RGB_OVERSCALE; 
    }
 
    ulOverscaledBitmapWidth = OverscaledBitmap->rectBounds.right - OverscaledBitmap->rectBounds.left; 
    ulMaxWidthIndex = (uint32)(SubPixelBitMap->rectBounds.right - SubPixelBitMap->rectBounds.left);
 
 	for ( ulHeightIndex = 0; ulHeightIndex < (uint32)(SubPixelBitMap->sHiBand - SubPixelBitMap->sLoBand); ulHeightIndex++ )
	{
		// Initialize Input to start of current row
 
		pbyInputRowData = OverscaledBitmap->pchBitMap + (OverscaledBitmap->sRowBytes * ulHeightIndex);
        pbyInputEndRowData = pbyInputRowData + OverscaledBitmap->sRowBytes; 
 		pbyOutputRowData = SubPixelBitMap->pchBitMap + (SubPixelBitMap->sRowBytes * ulHeightIndex); 
				
 
        /* do the first partial byte : */

        usColorIndex = (unsigned char)(*pbyInputRowData) >> (sBitmapSubPixelStart + (8 - RGB_OVERSCALE));
        usColorIndex = ajRGBToWeight222[usColorIndex]; 

        *pbyOutputRowData = (char) usColorIndex; 
 
        pbyOutputRowData++;
 
        usSubPixelIndex = (CHAR_BIT + RGB_OVERSCALE - sBitmapSubPixelStart) % CHAR_BIT;

 		// Walk the scanline from the first subpixel, calculating R,G, & B values
 
 		for( ulWidthIndex = 1; ulWidthIndex < ulMaxWidthIndex; ulWidthIndex++)
		{ 
            uSubPixelLeftShift = 0; 
            uSubPixelRightShift = CHAR_BIT - RGB_OVERSCALE - usSubPixelIndex;
            if (uSubPixelRightShift < 0) 
            {
                uSubPixelLeftShift = - uSubPixelRightShift;
                uSubPixelRightShift = 0;
            } 

            usColorIndex = ((unsigned char)(*pbyInputRowData) & ajRGBToWeightMask[usSubPixelIndex]) >> uSubPixelRightShift << uSubPixelLeftShift; 
 
            if (pbyInputRowData+1 < pbyInputEndRowData)
            { 
                /* avoid reading too far for the partial pixel at the end */
                uSubPixelRightShift = CHAR_BIT + CHAR_BIT - RGB_OVERSCALE - usSubPixelIndex;

                usColorIndex += (unsigned char)(*(pbyInputRowData+1)) >> uSubPixelRightShift; 
            }
            usColorIndex = ajRGBToWeight222[usColorIndex]; 
 
            *pbyOutputRowData = (char) usColorIndex;
 
            pbyOutputRowData++;

            usSubPixelIndex = (usSubPixelIndex + RGB_OVERSCALE);
            if (usSubPixelIndex >= CHAR_BIT) 
            {
                usSubPixelIndex = usSubPixelIndex % CHAR_BIT; 
                pbyInputRowData ++; 
            }
 		} 
	}
} // fsc_OverscaleToSubPixel_Platt

#endif // FSCFG_SUBPIXEL 

// File provided for Reference Use Only by Microsoft Corporation (c) 2007.
// Copyright (c) Microsoft Corporation. All rights reserved.
/********************************************************************** 

 	  subpixel.c -- sub pixel rendering

	  (c) Copyright 1999-2000  Microsoft Corp.  All rights reserved. 

				 8/24/00  BeatS		Add support for switchable SP filter 
 
**********************************************************************/
 
/*********************************************************************/

/*        Imports                                                    */
 
/*********************************************************************/
 
#include "precomp.hxx" 

#define FSCFG_INTERNAL 

#include    "fscdefs.h"             /* shared data types  */
#include    "scentry.h"             /* for own function prototypes */
 
#ifdef FSCFG_SUBPIXEL
 
// this should be considered a temporary solution. currently, the rasterizer is plumbed for 8 bpp output in SubPixel, 
// and appears to have John Platt's filter hard-wired in. In the future, we would rather have the overscaled b/w bitmap
// as output, such that we can do any color filtering and gamma correction outside and independent of the rasterizer 

#define CHAR_BIT      8         /* number of bits in a char */

unsigned const char ajRGBToWeight222[64] = { 
    0,1,1,2,4,5,5,6,4,5,5,6,8,9,9,10,
    16,17,17,18,20,21,21,22,20,21,21,22,24,25,25,26, 
    16,17,17,18,20,21,21,22,20,21,21,22,24,25,25,26, 
    32,33,33,34,36,37,37,38,36,37,37,38,40,41,41,42};
 
unsigned const char ajRGBToWeightMask[CHAR_BIT] = {
        0xFC, 0x7E, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01};

FS_PUBLIC void fsc_OverscaleToSubPixel (GlyphBitMap  * OverscaledBitmap, GlyphBitMap  * SubPixelBitMap) 
{
	char * pbyInputRowData, *pbyOutputRowData;			// Pointer to one scanline of data 
 	uint32 ulOverscaledBitmapWidth; // Fscaled bitmap widths 
	uint32 ulHeightIndex, ulWidthIndex, ulMaxWidthIndex;	// Scanline index and byte pixel index
    int16 sBitmapSubPixelStart; 
    uint16 usColorIndex;

    uint16 usSubPixelIndex;
    int16 uSubPixelRightShift, uSubPixelLeftShift; 
    char * pbyInputEndRowData;
 
 
 	// Start processing the bitmap
 	// This is the heart of the RGB striping algorithm 
	sBitmapSubPixelStart = OverscaledBitmap->rectBounds.left % RGB_OVERSCALE;
    if (sBitmapSubPixelStart < 0)
    {
        sBitmapSubPixelStart += RGB_OVERSCALE; 
    }
 
    ulOverscaledBitmapWidth = OverscaledBitmap->rectBounds.right - OverscaledBitmap->rectBounds.left; 
    ulMaxWidthIndex = (uint32)(SubPixelBitMap->rectBounds.right - SubPixelBitMap->rectBounds.left);
 
 	for ( ulHeightIndex = 0; ulHeightIndex < (uint32)(SubPixelBitMap->sHiBand - SubPixelBitMap->sLoBand); ulHeightIndex++ )
	{
		// Initialize Input to start of current row
 
		pbyInputRowData = OverscaledBitmap->pchBitMap + (OverscaledBitmap->sRowBytes * ulHeightIndex);
        pbyInputEndRowData = pbyInputRowData + OverscaledBitmap->sRowBytes; 
 		pbyOutputRowData = SubPixelBitMap->pchBitMap + (SubPixelBitMap->sRowBytes * ulHeightIndex); 
				
 
        /* do the first partial byte : */

        usColorIndex = (unsigned char)(*pbyInputRowData) >> (sBitmapSubPixelStart + (8 - RGB_OVERSCALE));
        usColorIndex = ajRGBToWeight222[usColorIndex]; 

        *pbyOutputRowData = (char) usColorIndex; 
 
        pbyOutputRowData++;
 
        usSubPixelIndex = (CHAR_BIT + RGB_OVERSCALE - sBitmapSubPixelStart) % CHAR_BIT;

 		// Walk the scanline from the first subpixel, calculating R,G, & B values
 
 		for( ulWidthIndex = 1; ulWidthIndex < ulMaxWidthIndex; ulWidthIndex++)
		{ 
            uSubPixelLeftShift = 0; 
            uSubPixelRightShift = CHAR_BIT - RGB_OVERSCALE - usSubPixelIndex;
            if (uSubPixelRightShift < 0) 
            {
                uSubPixelLeftShift = - uSubPixelRightShift;
                uSubPixelRightShift = 0;
            } 

            usColorIndex = ((unsigned char)(*pbyInputRowData) & ajRGBToWeightMask[usSubPixelIndex]) >> uSubPixelRightShift << uSubPixelLeftShift; 
 
            if (pbyInputRowData+1 < pbyInputEndRowData)
            { 
                /* avoid reading too far for the partial pixel at the end */
                uSubPixelRightShift = CHAR_BIT + CHAR_BIT - RGB_OVERSCALE - usSubPixelIndex;

                usColorIndex += (unsigned char)(*(pbyInputRowData+1)) >> uSubPixelRightShift; 
            }
            usColorIndex = ajRGBToWeight222[usColorIndex]; 
 
            *pbyOutputRowData = (char) usColorIndex;
 
            pbyOutputRowData++;

            usSubPixelIndex = (usSubPixelIndex + RGB_OVERSCALE);
            if (usSubPixelIndex >= CHAR_BIT) 
            {
                usSubPixelIndex = usSubPixelIndex % CHAR_BIT; 
                pbyInputRowData ++; 
            }
 		} 
	}
} // fsc_OverscaleToSubPixel_Platt

#endif // FSCFG_SUBPIXEL 

// File provided for Reference Use Only by Microsoft Corporation (c) 2007.
// Copyright (c) Microsoft Corporation. All rights reserved.
