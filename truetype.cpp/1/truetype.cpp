// This is the main DLL file. 

#include "precomp.hxx"

#define FSCFG_INTERNAL 

#include "fserror.h" 
#include "fscdefs.h" 
#include "fontmath.h"        /* For numeric conversion macros    */
#include "fnt.h" 
#include "scentry.h"
#include "sfntaccs.h"
#include "fsglue.h"
#include "sbit.h" 
#include "fscaler.h"         // moved this to be the last include file (key moved in dot h)
 
#include "truetype.h" 

// TrueType subsetter from TtfDelta 

namespace MS { namespace Internal { namespace TtfDelta {

// Disable subsetter include of assert.h (and other system headers in future) 
// as they are broken by the __cdecl redefinition below.
#define NO_CRT_ASSERT 
 
#define __cdecl
#pragma warning(disable : 4100 4127 4245 4702 4706) 
#include "TtfDelta\ttmem.cpp"
#include "TtfDelta\ttfcntrl.cpp"
#include "TtfDelta\ttfacc.cpp"
#include "TtfDelta\ttftabl1.cpp" 
#include "TtfDelta\ttftable.cpp"
#include "TtfDelta\modcmap.cpp" 
#include "TtfDelta\modglyf.cpp" 
#include "TtfDelta\modsbit.cpp"
#include "TtfDelta\modtable.cpp" 
#include "TtfDelta\makeglst.cpp"
#include "TtfDelta\mtxcalc.cpp"
#include "TtfDelta\automap.cpp"
#include "TtfDelta\util.cpp" 
#include "TtfDelta\ttfdelta.cpp"
#pragma warning(default : 4127 4245 4189 4100 4706 4701 4702 4505) 
#undef __cdecl 

}}} // namespace MS::Internal::TtfDelta 

[module:CLSCompliant(true)];

using namespace System::IO; 

struct ErrorMessage 
{ 
    unsigned short errorID;
    const wchar_t * message; 
};

using MS::Internal::TtfDelta::Mem_Free;
using MS::Internal::TtfDelta::Mem_Alloc; 
using MS::Internal::TtfDelta::Mem_ReAlloc;
using MS::Internal::TtfDelta::CreateDeltaTTF; 
 
namespace MS { namespace Internal {
 
#define NOT_IMPLEMENTED                 0x2001
#define TTC_FACE_INDEX_OUT_OF_RANGE     0x2002
#define OTF_ADOBE_NOT_IMPLEMENTED       0x2003
 
// sin of 20 degrees in 16.16 notation, computed with 8.8 precision to get same value as GDI
#define FX_SIN20 0x5700 
 
long TrueTypeRasterizer::Round26Dot6ToInt(long value)
{ 
    /* doing a rounding from 26.6 to int that is symmetric around zero */
    if (value >= 0)
    {
        return ((value + (1L << 5)) >> 6); 
    }
    else 
    { 
        return ( -((-value + (1L << 5)) >> 6));
    } 
}

uint32 TrueTypeRasterizer::ReadUINT32(BYTE * bytes)
{ 
    return (uint32)((((((bytes[0] << 8) + bytes[1]) << 8) + bytes[2]) << 8)+ bytes[3]);
} 
 

TrueTypeRasterizer::TrueTypeRasterizer() 
{
    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct;
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct;
    fs_OpenFonts(PinnedOutputStruct); 
    fs_Initialize(PinnedInputStruct);
    designSpaceTransform = false; 
} // TrueTypeRasterizer::TrueTypeRasterizer 

TrueTypeRasterizer::~TrueTypeRasterizer() 
{
    InternalDispose(true);
}
 
TrueTypeRasterizer::!TrueTypeRasterizer()
{ 
    InternalDispose(false); 
}
 
void TrueTypeRasterizer::InternalDispose(bool)
{
    Mem_Free(InputStruct.memoryBaseWorkSpace);
    InputStruct.memoryBaseWorkSpace = NULL; 

    Mem_Free(InputStruct.memoryBasePrivateFontSpace); 
    InputStruct.memoryBasePrivateFontSpace = NULL; 

    Mem_Free(InputStruct.memoryBaseBitmap); 
    InputStruct.memoryBaseBitmap = NULL;

    Mem_Free(InputStruct.memoryBaseBitmap2);
    InputStruct.memoryBaseBitmap2 = NULL; 

    Mem_Free(InputStruct.memoryBaseDropOut); 
    InputStruct.memoryBaseDropOut = NULL; 

    Mem_Free(InputStruct.memoryBaseGrayOverscale); 
    InputStruct.memoryBaseGrayOverscale = NULL;
}

Uri ^ TrueTypeRasterizer::SourceUri::get() 
{
    return InputStruct.clientID.sourceUri; 
} 

ushort TrueTypeRasterizer::NewFont(UnmanagedMemoryStream ^ fileStream, Uri ^ sourceUri, int faceIndex) 
{
    InputStruct.clientID.fileStream = fileStream;
    InputStruct.clientID.sourceUri = sourceUri;
    InputStruct.clientID.offsetToHeader = 0; // if simple ttf 

    if (fileStream->Length < 4) 
        throw gcnew FileFormatException(SourceUri); 

    BYTE * file = fileStream->PositionPointer; 
    uint32 uintTemp = ReadUINT32(file);

    if (uintTemp == 0x74746366) // 'ttcf'
    { 
        // this is a TTC file, we need to decode the ttc header
        if (fileStream->Length < 12) 
            throw gcnew FileFormatException(SourceUri); 

        file = fileStream->PositionPointer + 8; 

        // skip version
        uint32 numFaces = ReadUINT32(file);
 
        if (faceIndex >= (int)numFaces)
            throw gcnew FileFormatException(SourceUri); 
 
        if (fileStream->Length < 12 + 4 * faceIndex + 4)
            throw gcnew FileFormatException(SourceUri); 

        file = fileStream->PositionPointer + 12 + 4 * faceIndex;;
        InputStruct.clientID.offsetToHeader = ReadUINT32(file);
    } 
    else if (uintTemp == 0x4f54544f) // Adobe OpenType 'OTTO'
    { 
        // We should've used CFFRasterizer instead. 
        throw gcnew FileFormatException(SourceUri);
    } 
    else if (faceIndex != 0)
    {
        throw gcnew FileFormatException(SourceUri);
    } 

    InputStruct.param.newsfnt.platformID = 0xFFFF; // not used since this assembly is only giving glyphIndex access 
    InputStruct.param.newsfnt.specificID = 0xFFFF; 

    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct; 
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct;
    usLastError = fs_NewSfnt(PinnedInputStruct, PinnedOutputStruct);

    if (usLastError != NO_ERR) 
    {
        throw gcnew FileFormatException(SourceUri); 
    } 

    Mem_Free(InputStruct.memoryBaseWorkSpace); 
    if (OutputStruct.memorySizes[WORK_SPACE_BASE] != 0)
    {
        InputStruct.memoryBaseWorkSpace = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[WORK_SPACE_BASE]);
        if (InputStruct.memoryBaseWorkSpace == NULL) 
            throw gcnew OutOfMemoryException();
    } 
    else 
    {
        InputStruct.memoryBaseWorkSpace = NULL; 
    }

    Mem_Free(InputStruct.memoryBasePrivateFontSpace);
    if (OutputStruct.memorySizes[PRIVATE_FONT_SPACE_BASE] != 0) 
    {
        InputStruct.memoryBasePrivateFontSpace = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[PRIVATE_FONT_SPACE_BASE]); 
        if (InputStruct.memoryBasePrivateFontSpace == NULL) 
            throw gcnew OutOfMemoryException();
    } 
    else
    {
        InputStruct.memoryBasePrivateFontSpace = NULL;
    } 

    return InputStruct.key.maxProfile.numGlyphs; 
} 

void TrueTypeRasterizer::NewTransform(ushort pointSize, Transform transform, OverscaleMode overscaleMode, RenderingFlags renderingFlags) 
{
    transMatrix transformMatrix;

    /* dealing with a matrix that have the EmResolution precomposed would cause a big change is scl_InitializeScaling, 
        for now I multiply back EmResolution in the matrix here */
    transformMatrix.transform00 = transform.a00; 
    transformMatrix.transform01 = transform.a01; 
    transformMatrix.transform02 = 0; // subpixelOffsetX
    transformMatrix.transform10 = transform.a10; 
    transformMatrix.transform11 = transform.a11;
    transformMatrix.transform12 = 0; // subpixelOffsetY
    transformMatrix.transform20 = 0x0;
    transformMatrix.transform21 = 0x0; 
    transformMatrix.transform22 = 1L << 30;
 
    InputStruct.param.newtrans.transformMatrix= &transformMatrix; 

    InputStruct.param.newtrans.xResolution = 96; 
    InputStruct.param.newtrans.yResolution = 96;
    InputStruct.param.newtrans.pointSize = (long)pointSize << 16;
    InputStruct.param.newtrans.pixelDiameter = (Fixed) FIXEDSQRT2;
 
    InputStruct.param.newtrans.usOverScale = 0;
 
    InputStruct.param.newtrans.nonSubPixelOverscale = 1; 

    const unsigned int NonCTOverscale = 5; 
    const unsigned int RgbOverscale = 6;
    switch (overscaleMode)
    {
    case OverscaleMode::None: 
        InputStruct.param.newtrans.flSubPixel = 0;
        InputStruct.param.newtrans.subPixelOverscale = 1; 
        break; 

    case OverscaleMode::OverscaleXandY: 
        InputStruct.param.newtrans.nonSubPixelOverscale = NonCTOverscale;
    case OverscaleMode::OverscaleX:
        InputStruct.param.newtrans.flSubPixel = (SP_SUB_PIXEL | SP_FRACTIONAL_WIDTH);
        //if ((int)renderingFlags & (int)RenderingFlags::BGRStripes) 
        //    InputStruct.param.newtrans.flSubPixel |= SP_BGR_ORDER;
        //if ((int)renderingFlags & (int)RenderingFlags::HorizontalStripes) 
        //    InputStruct.param.newtrans.flSubPixel  |= SP_VERTICAL_DIRECTION; 
        InputStruct.param.newtrans.subPixelOverscale = RgbOverscale;
        break; 
    }

    if ((int)renderingFlags & (int)RenderingFlags::BoldSimulation)
    { 
    /* 2% + 1 pixel along baseline, 2% along descender line */
        InputStruct.param.newtrans.usEmboldWeightx = 20; 
        InputStruct.param.newtrans.usEmboldWeighty = 20; 
    }
    else 
    {
        InputStruct.param.newtrans.usEmboldWeightx = 0;
        InputStruct.param.newtrans.usEmboldWeighty = 0;
    } 

    if ((int)renderingFlags & (int)RenderingFlags::ItalicSimulation) 
    { 
        if ((int)renderingFlags & (int)RenderingFlags::SidewaysItalicSimulation)
        { 
            // the result of multiplying arbitrary matrix with italicization matrix
            // We are multiplying from the left because the italicization matrix
            // acts first on the notional space vectors on the left
            // 
            // |1  -sin20|   |m00    m01|   |m00 - m10 * sin20   m01 - m11 * sin20|
            // |         | * |          | = |                                     | 
            // |0       1|   |m10    m11|   |m10                 m11              | 
            //
 
            transformMatrix.transform00 -= FixMul(transformMatrix.transform10, FX_SIN20);
            transformMatrix.transform01 -= FixMul(transformMatrix.transform11, FX_SIN20);
        }
        else 
        {
 
            // the result of multiplying arbitrary matrix with italicization matrix 
            // We are multiplying from the left because the italicization matrix
            // acts first on the notional space vectors on the left 
            //
            // |1      0|   |m00    m01|   |m00                 m01              |
            // |        | * |          | = |                                     |
            // |sin20  1|   |m10    m11|   |m10 + m00 * sin20   m11 + m01 * sin20| 
            //
 
            transformMatrix.transform10 += FixMul(transformMatrix.transform00, FX_SIN20); 
            transformMatrix.transform11 += FixMul(transformMatrix.transform01, FX_SIN20);
        } 
    }

    InputStruct.param.newtrans.bBitmapEmboldening = FALSE;
 
    if ((int)renderingFlags & (int)RenderingFlags::Hinting)
    { 
        InputStruct.param.newtrans.bHintAtEmSquare = FALSE; 
    }
    else 
    {
        InputStruct.param.newtrans.bHintAtEmSquare = TRUE;
    }
 
    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct;
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct; 
    usLastError = fs_NewTransformation(PinnedInputStruct, PinnedOutputStruct); 

    if (usLastError != NO_ERR) 
    {
        throw gcnew FileFormatException(SourceUri);
    }
} 

void TrueTypeRasterizer::NewGlyph(ushort glyphIndex) 
{ 
    InputStruct.param.newglyph.characterCode = 0xFFFF;
    InputStruct.param.newglyph.glyphIndex = glyphIndex; 

    InputStruct.param.newglyph.bNoEmbeddedBitmap = true;
    InputStruct.param.newglyph.bMatchBBox = false;
 
    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct;
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct; 
    usLastError = fs_NewGlyph(PinnedInputStruct, PinnedOutputStruct); 

    if (usLastError != NO_ERR) 
    {
        throw gcnew FileFormatException(SourceUri);
    }
} 

void TrueTypeRasterizer::PrepareMemoryBases() 
{ 
    for (int i = 2; i <= MEMORYFRAGMENTS-1; i++)
    { 
        OutputStruct.memorySizes[i] = 0;
    }

    Mem_Free(InputStruct.memoryBaseBitmap); 
    InputStruct.memoryBaseBitmap = NULL;
 
    Mem_Free(InputStruct.memoryBaseBitmap2); 
    InputStruct.memoryBaseBitmap2 = NULL;
 
    Mem_Free(InputStruct.memoryBaseDropOut);
    InputStruct.memoryBaseDropOut = NULL;

    Mem_Free(InputStruct.memoryBaseGrayOverscale); 
    InputStruct.memoryBaseGrayOverscale = NULL;
} 
 
void TrueTypeRasterizer::GetBitmap(
        GetMemoryCallback ^ getMemoryCallback, 
        IntPtr              currentPointer,
        int                 currentSize,
        [Out] GlyphBitmap ^% glyphBitmap,
        [Out] GlyphMetrics ^% glyphMetrics 
        )
{ 
    glyphBitmap = gcnew GlyphBitmap; 
    glyphMetrics = gcnew GlyphMetrics;
 
    PrepareMemoryBases();

    InputStruct.param.gridfit.bSkipIfBitmap = FALSE;
 
    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct;
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct; 
 
    usLastError = fs_ContourGridFit(PinnedInputStruct, PinnedOutputStruct);
 
    if (usLastError != NO_ERR)
    {
        throw gcnew FileFormatException(SourceUri);
    } 

    Point topOrigin; 
    topOrigin.x = OutputStruct.xPtr[OutputStruct.endPtr[OutputStruct.numberOfContours-1] + 1 + TOPSIDEBEARING]; 
    topOrigin.y = OutputStruct.yPtr[OutputStruct.endPtr[OutputStruct.numberOfContours-1] + 1 + TOPSIDEBEARING];
 
    usLastError = fs_FindBitMapSize(PinnedInputStruct, PinnedOutputStruct);

    if (usLastError != NO_ERR)
    { 
        throw gcnew FileFormatException(SourceUri);
    } 
 
    if (OutputStruct.memorySizes[BITMAP_PTR_1] != 0)
    { 
        InputStruct.memoryBaseBitmap = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[BITMAP_PTR_1]);
        if (InputStruct.memoryBaseBitmap == NULL)
            throw gcnew OutOfMemoryException();
    } 
    else
    { 
        InputStruct.memoryBaseBitmap = NULL; 
    }
 
    if (OutputStruct.memorySizes[BITMAP_PTR_2] != 0)
    {
        InputStruct.memoryBaseBitmap2 = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[BITMAP_PTR_2]);
        if (InputStruct.memoryBaseBitmap2 == NULL) 
            throw gcnew OutOfMemoryException();
    } 
    else 
    {
        InputStruct.memoryBaseBitmap2 = NULL; 
    }

    if (OutputStruct.memorySizes[BITMAP_PTR_3] != 0)
    { 
        InputStruct.memoryBaseDropOut = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[BITMAP_PTR_3]);
        if (InputStruct.memoryBaseDropOut == NULL) 
            throw gcnew OutOfMemoryException(); 
    }
    else 
    {
        InputStruct.memoryBaseDropOut = NULL;
    }
 
    if (OutputStruct.memorySizes[BITMAP_PTR_4] != 0)
    { 
        InputStruct.memoryBaseGrayOverscale = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[BITMAP_PTR_4]); 
        if (InputStruct.memoryBaseGrayOverscale == NULL)
            throw gcnew OutOfMemoryException(); 
    }
    else
    {
        InputStruct.memoryBaseGrayOverscale = NULL; 
    }
 
    InputStruct.param.scan.bottomClip = 0; 
    InputStruct.param.scan.topClip = -1;
    InputStruct.param.scan.outlineCache = 0; 

    usLastError = fs_ContourScan(PinnedInputStruct, PinnedOutputStruct);

    if (usLastError != NO_ERR) 
    {
        throw gcnew FileFormatException(SourceUri); 
    } 

    BYTE * pixels; 

    if (InputStruct.param.newtrans.flSubPixel & SP_SUB_PIXEL)
    {
        glyphMetrics->height = OutputStruct.overscaledBitmapInfo.bounds.bottom - OutputStruct.overscaledBitmapInfo.bounds.top; // bottom < top 
        glyphMetrics->width = OutputStruct.overscaledBitmapInfo.bounds.right - OutputStruct.overscaledBitmapInfo.bounds.left;
        glyphBitmap->stride = OutputStruct.overscaledBitmapInfo.rowBytes; 
        pixels = InputStruct.memoryBaseGrayOverscale; 

        glyphMetrics->horizontalOrigin.x = OutputStruct.overscaledBitmapInfo.bounds.left; 
        glyphMetrics->horizontalOrigin.y = OutputStruct.overscaledBitmapInfo.bounds.bottom; // internally the rasterizer has top and bottom inverted
    }
    else
    { 
        glyphMetrics->height = OutputStruct.bitMapInfo.bounds.bottom - OutputStruct.bitMapInfo.bounds.top; // bottom < top
        glyphMetrics->width = OutputStruct.bitMapInfo.bounds.right - OutputStruct.bitMapInfo.bounds.left; 
        glyphBitmap->stride = OutputStruct.bitMapInfo.rowBytes; 
        pixels = InputStruct.memoryBaseBitmap;
 
        glyphMetrics->horizontalOrigin.x = OutputStruct.bitMapInfo.bounds.left;
        glyphMetrics->horizontalOrigin.y = OutputStruct.bitMapInfo.bounds.bottom; // internally the rasterizer has top and bottom inverted
    }
 
    glyphMetrics->verticalOrigin.x = -glyphMetrics->horizontalOrigin.y + Round26Dot6ToInt(topOrigin.y);
    glyphMetrics->verticalOrigin.y = -glyphMetrics->horizontalOrigin.x + Round26Dot6ToInt(topOrigin.x); 
 
    // convert from 16.16 to integer
    glyphMetrics->horizontalAdvance = ROUNDFIXTOINT(OutputStruct.metricInfo.devAdvanceWidth.x); 

    //

 
    glyphMetrics->verticalAdvance = ROUNDFIXTOINT(OutputStruct.verticalMetricInfo.devAdvanceHeight.y);
 
    int gbdataLength = glyphBitmap->stride * glyphMetrics->height; 
    if (currentSize >= gbdataLength)
        glyphBitmap->pixels = currentPointer; 
    else
    {
        glyphBitmap->pixels = getMemoryCallback(gbdataLength);
        if (glyphBitmap->pixels == IntPtr::Zero) 
        {
            OutOfMemoryException ^ e = gcnew OutOfMemoryException(); 
            e->Data->Add("GetMemoryDelegateError", (System::Boolean)true); 
            throw e;
        } 
    }

    MEMCPY(glyphBitmap->pixels.ToPointer(), pixels, gbdataLength);
} 

void TrueTypeRasterizer::GetMetrics([Out] GlyphMetrics ^%) 
{ 
    // This method is not used by Avalon because font driver parses 'glyf' table directly.
    throw gcnew NotSupportedException(); 
}

void TrueTypeRasterizer::GetOutline(
    GetMemoryCallback ^  , 
    IntPtr               ,
    int                  , 
    [Out] GlyphOutline ^%, 
    [Out] GlyphMetrics ^%
    ) 
{
    // TrueType outlines are not cubic, so the client will call GetPath instead.
    throw gcnew NotSupportedException();
} 

GlyphPathData * TrueTypeRasterizer::GetPath( 
    Allocator ^ allocator, 
    RenderingFlags renderingFlags,
    unsigned short glyphIndex) 
{
    if (!designSpaceTransform)
    {
        Transform tform; 
        tform.a00 = InputStruct.key.TransformInfo.usEmResolution*1024; // design Em * 0x10000 would give 26.6, by dividing by 64 we get integer design values
        tform.a01 = 0; 
        tform.a10 = 0; 
        tform.a11 = InputStruct.key.TransformInfo.usEmResolution*1024;
        NewTransform( 
            12,
            tform,
            OverscaleMode::None,
            (RenderingFlags)((int)renderingFlags & ~(int)RenderingFlags::Hinting)); 
        designSpaceTransform = true;
    } 
 
    NewGlyph(glyphIndex);
 
    PrepareMemoryBases();

    InputStruct.param.gridfit.bSkipIfBitmap = FALSE;
 
    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct;
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct; 
    usLastError = fs_ContourGridFit(PinnedInputStruct, PinnedOutputStruct); 

    if (usLastError != NO_ERR) 
    {
        throw gcnew FileFormatException(SourceUri);
    }
 
    System::UInt16 numberOfContours, numberOfPoints;
 
    numberOfContours = OutputStruct.numberOfContours; 

    if (OutputStruct.numberOfContours > 0) 
        numberOfPoints = OutputStruct.endPtr[OutputStruct.numberOfContours-1] + 1;
    else
        numberOfPoints = 0;
 
    UINT allocSize = GlyphPathData::baseSize +
        numberOfContours * sizeof(USHORT) + 
        numberOfPoints * (2 * sizeof(USHORT) + sizeof(BYTE)); 

    GlyphPathData  * glyphPathData = (GlyphPathData  *)allocator(allocSize); 

    glyphPathData->numberOfContours = numberOfContours;

    System::UInt16  *endPointNumbers = GlyphPathData::EndPointNumbers(glyphPathData); 

    for (int i = 0; i < glyphPathData->numberOfContours; i++) 
        endPointNumbers[i] = OutputStruct.endPtr[i]; 

    short *x = GlyphPathData::X(glyphPathData); 
    short *y = GlyphPathData::Y(glyphPathData);
    unsigned char *flags = GlyphPathData::Flags(glyphPathData);

    for (int i = 0; i < GlyphPathData::NumPoints(glyphPathData); i++) 
    {
        x[i] = (short)OutputStruct.xPtr[i]; 
        y[i] = (short)OutputStruct.yPtr[i]; 
        flags[i] = OutputStruct.onCurve[i];
 
        // It is possible for a malformed font to produce glyph points outside the 16 bit integer range.
        // We reject such glyphs.
        if (x[i] != OutputStruct.xPtr[i] || y[i] != OutputStruct.yPtr[i])
            throw gcnew FileFormatException(SourceUri); 
    }
 
    F26Dot6 verOriginX = OutputStruct.xPtr[OutputStruct.endPtr[OutputStruct.numberOfContours-1] + 1 + TOPSIDEBEARING]; 
    glyphPathData->verOriginX = (short)verOriginX;
    F26Dot6 verOriginY = OutputStruct.yPtr[OutputStruct.endPtr[OutputStruct.numberOfContours-1] + 1 + TOPSIDEBEARING]; 
    glyphPathData->verOriginY = (short)verOriginY;

    // It is possible for a malformed font to produce glyph points outside the 16 bit integer range.
    // We reject such glyphs. 
    if (glyphPathData->verOriginX != verOriginX || glyphPathData->verOriginY != verOriginY)
        throw gcnew FileFormatException(SourceUri); 
 
    return glyphPathData;
} 

array<System::Byte> ^ TrueTypeSubsetter::ComputeSubset(void * fontData, int fileSize, Uri ^ sourceUri, int directoryOffset, array<System::UInt16> ^ glyphArray)
{
    uint8 * puchDestBuffer = NULL; 
    unsigned long ulDestBufferSize = 0, ulBytesWritten = 0;
 
    assert(glyphArray != nullptr && glyphArray->Length > 0 && glyphArray->Length <= USHRT_MAX); 

    pin_ptr<const System::UInt16> pinnedGlyphArray = &glyphArray[0]; 
    int16 errCode = CreateDeltaTTF(
        static_cast<CONST uint8 *>(fontData),
        fileSize,
        &puchDestBuffer, 
        &ulDestBufferSize,
        &ulBytesWritten, 
        0, // format of the subset font to create. 0 = Subset 
        0, // all languages in the Name table should be retained
        0, // Ignored for usListType = 1 
        0, // Ignored for usListType = 1
        1, // usListType, 1 means the KeepCharCodeList represents raw Glyph indices from the font
        pinnedGlyphArray, // glyph indices array
        static_cast<USHORT>(glyphArray->Length), // number of glyph indices 
        Mem_ReAlloc,   // call back function to reallocate temp and output buffers
        Mem_Free,      // call back function to output buffers on error 
        directoryOffset, 
        NULL // Reserved
        ); 

    array<System::Byte> ^ retArray = nullptr;

    try 
    {
        if (errCode == NO_ERROR) 
        { 
            retArray = gcnew array<System::Byte>(ulBytesWritten);
            Marshal::Copy((IntPtr)puchDestBuffer, retArray, 0, ulBytesWritten); 
        }
    }
    finally
    { 
        Mem_Free(puchDestBuffer);
    } 
 
    if (errCode != NO_ERROR)
        throw gcnew FileFormatException(sourceUri); 

    return retArray;
}
 
} } // MS.Internal
 
#include "..\\Shared\\MS\\Internal\\FriendAccessAllowedAttribute.cpp" 

// Cross process library from wcp\IPCUtil 

#include "..\IPCUtil\main.cpp"


// File provided for Reference Use Only by Microsoft Corporation (c) 2007.
// Copyright (c) Microsoft Corporation. All rights reserved.
// This is the main DLL file. 

#include "precomp.hxx"

#define FSCFG_INTERNAL 

#include "fserror.h" 
#include "fscdefs.h" 
#include "fontmath.h"        /* For numeric conversion macros    */
#include "fnt.h" 
#include "scentry.h"
#include "sfntaccs.h"
#include "fsglue.h"
#include "sbit.h" 
#include "fscaler.h"         // moved this to be the last include file (key moved in dot h)
 
#include "truetype.h" 

// TrueType subsetter from TtfDelta 

namespace MS { namespace Internal { namespace TtfDelta {

// Disable subsetter include of assert.h (and other system headers in future) 
// as they are broken by the __cdecl redefinition below.
#define NO_CRT_ASSERT 
 
#define __cdecl
#pragma warning(disable : 4100 4127 4245 4702 4706) 
#include "TtfDelta\ttmem.cpp"
#include "TtfDelta\ttfcntrl.cpp"
#include "TtfDelta\ttfacc.cpp"
#include "TtfDelta\ttftabl1.cpp" 
#include "TtfDelta\ttftable.cpp"
#include "TtfDelta\modcmap.cpp" 
#include "TtfDelta\modglyf.cpp" 
#include "TtfDelta\modsbit.cpp"
#include "TtfDelta\modtable.cpp" 
#include "TtfDelta\makeglst.cpp"
#include "TtfDelta\mtxcalc.cpp"
#include "TtfDelta\automap.cpp"
#include "TtfDelta\util.cpp" 
#include "TtfDelta\ttfdelta.cpp"
#pragma warning(default : 4127 4245 4189 4100 4706 4701 4702 4505) 
#undef __cdecl 

}}} // namespace MS::Internal::TtfDelta 

[module:CLSCompliant(true)];

using namespace System::IO; 

struct ErrorMessage 
{ 
    unsigned short errorID;
    const wchar_t * message; 
};

using MS::Internal::TtfDelta::Mem_Free;
using MS::Internal::TtfDelta::Mem_Alloc; 
using MS::Internal::TtfDelta::Mem_ReAlloc;
using MS::Internal::TtfDelta::CreateDeltaTTF; 
 
namespace MS { namespace Internal {
 
#define NOT_IMPLEMENTED                 0x2001
#define TTC_FACE_INDEX_OUT_OF_RANGE     0x2002
#define OTF_ADOBE_NOT_IMPLEMENTED       0x2003
 
// sin of 20 degrees in 16.16 notation, computed with 8.8 precision to get same value as GDI
#define FX_SIN20 0x5700 
 
long TrueTypeRasterizer::Round26Dot6ToInt(long value)
{ 
    /* doing a rounding from 26.6 to int that is symmetric around zero */
    if (value >= 0)
    {
        return ((value + (1L << 5)) >> 6); 
    }
    else 
    { 
        return ( -((-value + (1L << 5)) >> 6));
    } 
}

uint32 TrueTypeRasterizer::ReadUINT32(BYTE * bytes)
{ 
    return (uint32)((((((bytes[0] << 8) + bytes[1]) << 8) + bytes[2]) << 8)+ bytes[3]);
} 
 

TrueTypeRasterizer::TrueTypeRasterizer() 
{
    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct;
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct;
    fs_OpenFonts(PinnedOutputStruct); 
    fs_Initialize(PinnedInputStruct);
    designSpaceTransform = false; 
} // TrueTypeRasterizer::TrueTypeRasterizer 

TrueTypeRasterizer::~TrueTypeRasterizer() 
{
    InternalDispose(true);
}
 
TrueTypeRasterizer::!TrueTypeRasterizer()
{ 
    InternalDispose(false); 
}
 
void TrueTypeRasterizer::InternalDispose(bool)
{
    Mem_Free(InputStruct.memoryBaseWorkSpace);
    InputStruct.memoryBaseWorkSpace = NULL; 

    Mem_Free(InputStruct.memoryBasePrivateFontSpace); 
    InputStruct.memoryBasePrivateFontSpace = NULL; 

    Mem_Free(InputStruct.memoryBaseBitmap); 
    InputStruct.memoryBaseBitmap = NULL;

    Mem_Free(InputStruct.memoryBaseBitmap2);
    InputStruct.memoryBaseBitmap2 = NULL; 

    Mem_Free(InputStruct.memoryBaseDropOut); 
    InputStruct.memoryBaseDropOut = NULL; 

    Mem_Free(InputStruct.memoryBaseGrayOverscale); 
    InputStruct.memoryBaseGrayOverscale = NULL;
}

Uri ^ TrueTypeRasterizer::SourceUri::get() 
{
    return InputStruct.clientID.sourceUri; 
} 

ushort TrueTypeRasterizer::NewFont(UnmanagedMemoryStream ^ fileStream, Uri ^ sourceUri, int faceIndex) 
{
    InputStruct.clientID.fileStream = fileStream;
    InputStruct.clientID.sourceUri = sourceUri;
    InputStruct.clientID.offsetToHeader = 0; // if simple ttf 

    if (fileStream->Length < 4) 
        throw gcnew FileFormatException(SourceUri); 

    BYTE * file = fileStream->PositionPointer; 
    uint32 uintTemp = ReadUINT32(file);

    if (uintTemp == 0x74746366) // 'ttcf'
    { 
        // this is a TTC file, we need to decode the ttc header
        if (fileStream->Length < 12) 
            throw gcnew FileFormatException(SourceUri); 

        file = fileStream->PositionPointer + 8; 

        // skip version
        uint32 numFaces = ReadUINT32(file);
 
        if (faceIndex >= (int)numFaces)
            throw gcnew FileFormatException(SourceUri); 
 
        if (fileStream->Length < 12 + 4 * faceIndex + 4)
            throw gcnew FileFormatException(SourceUri); 

        file = fileStream->PositionPointer + 12 + 4 * faceIndex;;
        InputStruct.clientID.offsetToHeader = ReadUINT32(file);
    } 
    else if (uintTemp == 0x4f54544f) // Adobe OpenType 'OTTO'
    { 
        // We should've used CFFRasterizer instead. 
        throw gcnew FileFormatException(SourceUri);
    } 
    else if (faceIndex != 0)
    {
        throw gcnew FileFormatException(SourceUri);
    } 

    InputStruct.param.newsfnt.platformID = 0xFFFF; // not used since this assembly is only giving glyphIndex access 
    InputStruct.param.newsfnt.specificID = 0xFFFF; 

    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct; 
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct;
    usLastError = fs_NewSfnt(PinnedInputStruct, PinnedOutputStruct);

    if (usLastError != NO_ERR) 
    {
        throw gcnew FileFormatException(SourceUri); 
    } 

    Mem_Free(InputStruct.memoryBaseWorkSpace); 
    if (OutputStruct.memorySizes[WORK_SPACE_BASE] != 0)
    {
        InputStruct.memoryBaseWorkSpace = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[WORK_SPACE_BASE]);
        if (InputStruct.memoryBaseWorkSpace == NULL) 
            throw gcnew OutOfMemoryException();
    } 
    else 
    {
        InputStruct.memoryBaseWorkSpace = NULL; 
    }

    Mem_Free(InputStruct.memoryBasePrivateFontSpace);
    if (OutputStruct.memorySizes[PRIVATE_FONT_SPACE_BASE] != 0) 
    {
        InputStruct.memoryBasePrivateFontSpace = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[PRIVATE_FONT_SPACE_BASE]); 
        if (InputStruct.memoryBasePrivateFontSpace == NULL) 
            throw gcnew OutOfMemoryException();
    } 
    else
    {
        InputStruct.memoryBasePrivateFontSpace = NULL;
    } 

    return InputStruct.key.maxProfile.numGlyphs; 
} 

void TrueTypeRasterizer::NewTransform(ushort pointSize, Transform transform, OverscaleMode overscaleMode, RenderingFlags renderingFlags) 
{
    transMatrix transformMatrix;

    /* dealing with a matrix that have the EmResolution precomposed would cause a big change is scl_InitializeScaling, 
        for now I multiply back EmResolution in the matrix here */
    transformMatrix.transform00 = transform.a00; 
    transformMatrix.transform01 = transform.a01; 
    transformMatrix.transform02 = 0; // subpixelOffsetX
    transformMatrix.transform10 = transform.a10; 
    transformMatrix.transform11 = transform.a11;
    transformMatrix.transform12 = 0; // subpixelOffsetY
    transformMatrix.transform20 = 0x0;
    transformMatrix.transform21 = 0x0; 
    transformMatrix.transform22 = 1L << 30;
 
    InputStruct.param.newtrans.transformMatrix= &transformMatrix; 

    InputStruct.param.newtrans.xResolution = 96; 
    InputStruct.param.newtrans.yResolution = 96;
    InputStruct.param.newtrans.pointSize = (long)pointSize << 16;
    InputStruct.param.newtrans.pixelDiameter = (Fixed) FIXEDSQRT2;
 
    InputStruct.param.newtrans.usOverScale = 0;
 
    InputStruct.param.newtrans.nonSubPixelOverscale = 1; 

    const unsigned int NonCTOverscale = 5; 
    const unsigned int RgbOverscale = 6;
    switch (overscaleMode)
    {
    case OverscaleMode::None: 
        InputStruct.param.newtrans.flSubPixel = 0;
        InputStruct.param.newtrans.subPixelOverscale = 1; 
        break; 

    case OverscaleMode::OverscaleXandY: 
        InputStruct.param.newtrans.nonSubPixelOverscale = NonCTOverscale;
    case OverscaleMode::OverscaleX:
        InputStruct.param.newtrans.flSubPixel = (SP_SUB_PIXEL | SP_FRACTIONAL_WIDTH);
        //if ((int)renderingFlags & (int)RenderingFlags::BGRStripes) 
        //    InputStruct.param.newtrans.flSubPixel |= SP_BGR_ORDER;
        //if ((int)renderingFlags & (int)RenderingFlags::HorizontalStripes) 
        //    InputStruct.param.newtrans.flSubPixel  |= SP_VERTICAL_DIRECTION; 
        InputStruct.param.newtrans.subPixelOverscale = RgbOverscale;
        break; 
    }

    if ((int)renderingFlags & (int)RenderingFlags::BoldSimulation)
    { 
    /* 2% + 1 pixel along baseline, 2% along descender line */
        InputStruct.param.newtrans.usEmboldWeightx = 20; 
        InputStruct.param.newtrans.usEmboldWeighty = 20; 
    }
    else 
    {
        InputStruct.param.newtrans.usEmboldWeightx = 0;
        InputStruct.param.newtrans.usEmboldWeighty = 0;
    } 

    if ((int)renderingFlags & (int)RenderingFlags::ItalicSimulation) 
    { 
        if ((int)renderingFlags & (int)RenderingFlags::SidewaysItalicSimulation)
        { 
            // the result of multiplying arbitrary matrix with italicization matrix
            // We are multiplying from the left because the italicization matrix
            // acts first on the notional space vectors on the left
            // 
            // |1  -sin20|   |m00    m01|   |m00 - m10 * sin20   m01 - m11 * sin20|
            // |         | * |          | = |                                     | 
            // |0       1|   |m10    m11|   |m10                 m11              | 
            //
 
            transformMatrix.transform00 -= FixMul(transformMatrix.transform10, FX_SIN20);
            transformMatrix.transform01 -= FixMul(transformMatrix.transform11, FX_SIN20);
        }
        else 
        {
 
            // the result of multiplying arbitrary matrix with italicization matrix 
            // We are multiplying from the left because the italicization matrix
            // acts first on the notional space vectors on the left 
            //
            // |1      0|   |m00    m01|   |m00                 m01              |
            // |        | * |          | = |                                     |
            // |sin20  1|   |m10    m11|   |m10 + m00 * sin20   m11 + m01 * sin20| 
            //
 
            transformMatrix.transform10 += FixMul(transformMatrix.transform00, FX_SIN20); 
            transformMatrix.transform11 += FixMul(transformMatrix.transform01, FX_SIN20);
        } 
    }

    InputStruct.param.newtrans.bBitmapEmboldening = FALSE;
 
    if ((int)renderingFlags & (int)RenderingFlags::Hinting)
    { 
        InputStruct.param.newtrans.bHintAtEmSquare = FALSE; 
    }
    else 
    {
        InputStruct.param.newtrans.bHintAtEmSquare = TRUE;
    }
 
    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct;
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct; 
    usLastError = fs_NewTransformation(PinnedInputStruct, PinnedOutputStruct); 

    if (usLastError != NO_ERR) 
    {
        throw gcnew FileFormatException(SourceUri);
    }
} 

void TrueTypeRasterizer::NewGlyph(ushort glyphIndex) 
{ 
    InputStruct.param.newglyph.characterCode = 0xFFFF;
    InputStruct.param.newglyph.glyphIndex = glyphIndex; 

    InputStruct.param.newglyph.bNoEmbeddedBitmap = true;
    InputStruct.param.newglyph.bMatchBBox = false;
 
    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct;
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct; 
    usLastError = fs_NewGlyph(PinnedInputStruct, PinnedOutputStruct); 

    if (usLastError != NO_ERR) 
    {
        throw gcnew FileFormatException(SourceUri);
    }
} 

void TrueTypeRasterizer::PrepareMemoryBases() 
{ 
    for (int i = 2; i <= MEMORYFRAGMENTS-1; i++)
    { 
        OutputStruct.memorySizes[i] = 0;
    }

    Mem_Free(InputStruct.memoryBaseBitmap); 
    InputStruct.memoryBaseBitmap = NULL;
 
    Mem_Free(InputStruct.memoryBaseBitmap2); 
    InputStruct.memoryBaseBitmap2 = NULL;
 
    Mem_Free(InputStruct.memoryBaseDropOut);
    InputStruct.memoryBaseDropOut = NULL;

    Mem_Free(InputStruct.memoryBaseGrayOverscale); 
    InputStruct.memoryBaseGrayOverscale = NULL;
} 
 
void TrueTypeRasterizer::GetBitmap(
        GetMemoryCallback ^ getMemoryCallback, 
        IntPtr              currentPointer,
        int                 currentSize,
        [Out] GlyphBitmap ^% glyphBitmap,
        [Out] GlyphMetrics ^% glyphMetrics 
        )
{ 
    glyphBitmap = gcnew GlyphBitmap; 
    glyphMetrics = gcnew GlyphMetrics;
 
    PrepareMemoryBases();

    InputStruct.param.gridfit.bSkipIfBitmap = FALSE;
 
    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct;
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct; 
 
    usLastError = fs_ContourGridFit(PinnedInputStruct, PinnedOutputStruct);
 
    if (usLastError != NO_ERR)
    {
        throw gcnew FileFormatException(SourceUri);
    } 

    Point topOrigin; 
    topOrigin.x = OutputStruct.xPtr[OutputStruct.endPtr[OutputStruct.numberOfContours-1] + 1 + TOPSIDEBEARING]; 
    topOrigin.y = OutputStruct.yPtr[OutputStruct.endPtr[OutputStruct.numberOfContours-1] + 1 + TOPSIDEBEARING];
 
    usLastError = fs_FindBitMapSize(PinnedInputStruct, PinnedOutputStruct);

    if (usLastError != NO_ERR)
    { 
        throw gcnew FileFormatException(SourceUri);
    } 
 
    if (OutputStruct.memorySizes[BITMAP_PTR_1] != 0)
    { 
        InputStruct.memoryBaseBitmap = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[BITMAP_PTR_1]);
        if (InputStruct.memoryBaseBitmap == NULL)
            throw gcnew OutOfMemoryException();
    } 
    else
    { 
        InputStruct.memoryBaseBitmap = NULL; 
    }
 
    if (OutputStruct.memorySizes[BITMAP_PTR_2] != 0)
    {
        InputStruct.memoryBaseBitmap2 = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[BITMAP_PTR_2]);
        if (InputStruct.memoryBaseBitmap2 == NULL) 
            throw gcnew OutOfMemoryException();
    } 
    else 
    {
        InputStruct.memoryBaseBitmap2 = NULL; 
    }

    if (OutputStruct.memorySizes[BITMAP_PTR_3] != 0)
    { 
        InputStruct.memoryBaseDropOut = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[BITMAP_PTR_3]);
        if (InputStruct.memoryBaseDropOut == NULL) 
            throw gcnew OutOfMemoryException(); 
    }
    else 
    {
        InputStruct.memoryBaseDropOut = NULL;
    }
 
    if (OutputStruct.memorySizes[BITMAP_PTR_4] != 0)
    { 
        InputStruct.memoryBaseGrayOverscale = (unsigned char *)Mem_Alloc(OutputStruct.memorySizes[BITMAP_PTR_4]); 
        if (InputStruct.memoryBaseGrayOverscale == NULL)
            throw gcnew OutOfMemoryException(); 
    }
    else
    {
        InputStruct.memoryBaseGrayOverscale = NULL; 
    }
 
    InputStruct.param.scan.bottomClip = 0; 
    InputStruct.param.scan.topClip = -1;
    InputStruct.param.scan.outlineCache = 0; 

    usLastError = fs_ContourScan(PinnedInputStruct, PinnedOutputStruct);

    if (usLastError != NO_ERR) 
    {
        throw gcnew FileFormatException(SourceUri); 
    } 

    BYTE * pixels; 

    if (InputStruct.param.newtrans.flSubPixel & SP_SUB_PIXEL)
    {
        glyphMetrics->height = OutputStruct.overscaledBitmapInfo.bounds.bottom - OutputStruct.overscaledBitmapInfo.bounds.top; // bottom < top 
        glyphMetrics->width = OutputStruct.overscaledBitmapInfo.bounds.right - OutputStruct.overscaledBitmapInfo.bounds.left;
        glyphBitmap->stride = OutputStruct.overscaledBitmapInfo.rowBytes; 
        pixels = InputStruct.memoryBaseGrayOverscale; 

        glyphMetrics->horizontalOrigin.x = OutputStruct.overscaledBitmapInfo.bounds.left; 
        glyphMetrics->horizontalOrigin.y = OutputStruct.overscaledBitmapInfo.bounds.bottom; // internally the rasterizer has top and bottom inverted
    }
    else
    { 
        glyphMetrics->height = OutputStruct.bitMapInfo.bounds.bottom - OutputStruct.bitMapInfo.bounds.top; // bottom < top
        glyphMetrics->width = OutputStruct.bitMapInfo.bounds.right - OutputStruct.bitMapInfo.bounds.left; 
        glyphBitmap->stride = OutputStruct.bitMapInfo.rowBytes; 
        pixels = InputStruct.memoryBaseBitmap;
 
        glyphMetrics->horizontalOrigin.x = OutputStruct.bitMapInfo.bounds.left;
        glyphMetrics->horizontalOrigin.y = OutputStruct.bitMapInfo.bounds.bottom; // internally the rasterizer has top and bottom inverted
    }
 
    glyphMetrics->verticalOrigin.x = -glyphMetrics->horizontalOrigin.y + Round26Dot6ToInt(topOrigin.y);
    glyphMetrics->verticalOrigin.y = -glyphMetrics->horizontalOrigin.x + Round26Dot6ToInt(topOrigin.x); 
 
    // convert from 16.16 to integer
    glyphMetrics->horizontalAdvance = ROUNDFIXTOINT(OutputStruct.metricInfo.devAdvanceWidth.x); 

    //

 
    glyphMetrics->verticalAdvance = ROUNDFIXTOINT(OutputStruct.verticalMetricInfo.devAdvanceHeight.y);
 
    int gbdataLength = glyphBitmap->stride * glyphMetrics->height; 
    if (currentSize >= gbdataLength)
        glyphBitmap->pixels = currentPointer; 
    else
    {
        glyphBitmap->pixels = getMemoryCallback(gbdataLength);
        if (glyphBitmap->pixels == IntPtr::Zero) 
        {
            OutOfMemoryException ^ e = gcnew OutOfMemoryException(); 
            e->Data->Add("GetMemoryDelegateError", (System::Boolean)true); 
            throw e;
        } 
    }

    MEMCPY(glyphBitmap->pixels.ToPointer(), pixels, gbdataLength);
} 

void TrueTypeRasterizer::GetMetrics([Out] GlyphMetrics ^%) 
{ 
    // This method is not used by Avalon because font driver parses 'glyf' table directly.
    throw gcnew NotSupportedException(); 
}

void TrueTypeRasterizer::GetOutline(
    GetMemoryCallback ^  , 
    IntPtr               ,
    int                  , 
    [Out] GlyphOutline ^%, 
    [Out] GlyphMetrics ^%
    ) 
{
    // TrueType outlines are not cubic, so the client will call GetPath instead.
    throw gcnew NotSupportedException();
} 

GlyphPathData * TrueTypeRasterizer::GetPath( 
    Allocator ^ allocator, 
    RenderingFlags renderingFlags,
    unsigned short glyphIndex) 
{
    if (!designSpaceTransform)
    {
        Transform tform; 
        tform.a00 = InputStruct.key.TransformInfo.usEmResolution*1024; // design Em * 0x10000 would give 26.6, by dividing by 64 we get integer design values
        tform.a01 = 0; 
        tform.a10 = 0; 
        tform.a11 = InputStruct.key.TransformInfo.usEmResolution*1024;
        NewTransform( 
            12,
            tform,
            OverscaleMode::None,
            (RenderingFlags)((int)renderingFlags & ~(int)RenderingFlags::Hinting)); 
        designSpaceTransform = true;
    } 
 
    NewGlyph(glyphIndex);
 
    PrepareMemoryBases();

    InputStruct.param.gridfit.bSkipIfBitmap = FALSE;
 
    pin_ptr<fs_GlyphInputType> PinnedInputStruct = &InputStruct;
    pin_ptr<fs_GlyphInfoType> PinnedOutputStruct = &OutputStruct; 
    usLastError = fs_ContourGridFit(PinnedInputStruct, PinnedOutputStruct); 

    if (usLastError != NO_ERR) 
    {
        throw gcnew FileFormatException(SourceUri);
    }
 
    System::UInt16 numberOfContours, numberOfPoints;
 
    numberOfContours = OutputStruct.numberOfContours; 

    if (OutputStruct.numberOfContours > 0) 
        numberOfPoints = OutputStruct.endPtr[OutputStruct.numberOfContours-1] + 1;
    else
        numberOfPoints = 0;
 
    UINT allocSize = GlyphPathData::baseSize +
        numberOfContours * sizeof(USHORT) + 
        numberOfPoints * (2 * sizeof(USHORT) + sizeof(BYTE)); 

    GlyphPathData  * glyphPathData = (GlyphPathData  *)allocator(allocSize); 

    glyphPathData->numberOfContours = numberOfContours;

    System::UInt16  *endPointNumbers = GlyphPathData::EndPointNumbers(glyphPathData); 

    for (int i = 0; i < glyphPathData->numberOfContours; i++) 
        endPointNumbers[i] = OutputStruct.endPtr[i]; 

    short *x = GlyphPathData::X(glyphPathData); 
    short *y = GlyphPathData::Y(glyphPathData);
    unsigned char *flags = GlyphPathData::Flags(glyphPathData);

    for (int i = 0; i < GlyphPathData::NumPoints(glyphPathData); i++) 
    {
        x[i] = (short)OutputStruct.xPtr[i]; 
        y[i] = (short)OutputStruct.yPtr[i]; 
        flags[i] = OutputStruct.onCurve[i];
 
        // It is possible for a malformed font to produce glyph points outside the 16 bit integer range.
        // We reject such glyphs.
        if (x[i] != OutputStruct.xPtr[i] || y[i] != OutputStruct.yPtr[i])
            throw gcnew FileFormatException(SourceUri); 
    }
 
    F26Dot6 verOriginX = OutputStruct.xPtr[OutputStruct.endPtr[OutputStruct.numberOfContours-1] + 1 + TOPSIDEBEARING]; 
    glyphPathData->verOriginX = (short)verOriginX;
    F26Dot6 verOriginY = OutputStruct.yPtr[OutputStruct.endPtr[OutputStruct.numberOfContours-1] + 1 + TOPSIDEBEARING]; 
    glyphPathData->verOriginY = (short)verOriginY;

    // It is possible for a malformed font to produce glyph points outside the 16 bit integer range.
    // We reject such glyphs. 
    if (glyphPathData->verOriginX != verOriginX || glyphPathData->verOriginY != verOriginY)
        throw gcnew FileFormatException(SourceUri); 
 
    return glyphPathData;
} 

array<System::Byte> ^ TrueTypeSubsetter::ComputeSubset(void * fontData, int fileSize, Uri ^ sourceUri, int directoryOffset, array<System::UInt16> ^ glyphArray)
{
    uint8 * puchDestBuffer = NULL; 
    unsigned long ulDestBufferSize = 0, ulBytesWritten = 0;
 
    assert(glyphArray != nullptr && glyphArray->Length > 0 && glyphArray->Length <= USHRT_MAX); 

    pin_ptr<const System::UInt16> pinnedGlyphArray = &glyphArray[0]; 
    int16 errCode = CreateDeltaTTF(
        static_cast<CONST uint8 *>(fontData),
        fileSize,
        &puchDestBuffer, 
        &ulDestBufferSize,
        &ulBytesWritten, 
        0, // format of the subset font to create. 0 = Subset 
        0, // all languages in the Name table should be retained
        0, // Ignored for usListType = 1 
        0, // Ignored for usListType = 1
        1, // usListType, 1 means the KeepCharCodeList represents raw Glyph indices from the font
        pinnedGlyphArray, // glyph indices array
        static_cast<USHORT>(glyphArray->Length), // number of glyph indices 
        Mem_ReAlloc,   // call back function to reallocate temp and output buffers
        Mem_Free,      // call back function to output buffers on error 
        directoryOffset, 
        NULL // Reserved
        ); 

    array<System::Byte> ^ retArray = nullptr;

    try 
    {
        if (errCode == NO_ERROR) 
        { 
            retArray = gcnew array<System::Byte>(ulBytesWritten);
            Marshal::Copy((IntPtr)puchDestBuffer, retArray, 0, ulBytesWritten); 
        }
    }
    finally
    { 
        Mem_Free(puchDestBuffer);
    } 
 
    if (errCode != NO_ERROR)
        throw gcnew FileFormatException(sourceUri); 

    return retArray;
}
 
} } // MS.Internal
 
#include "..\\Shared\\MS\\Internal\\FriendAccessAllowedAttribute.cpp" 

// Cross process library from wcp\IPCUtil 

#include "..\IPCUtil\main.cpp"


// File provided for Reference Use Only by Microsoft Corporation (c) 2007.
// Copyright (c) Microsoft Corporation. All rights reserved.
