// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "MuR/Image.h"

namespace mu
{
    //---------------------------------------------------------------------------------------------
    //! Convert an image to another pixel format.
    //! \warning Not all format conversions are implemented.
    //! \param onlyLOD If different than -1, only the specified lod level will be converted in the
    //! returned image.
    //! \return false if the conversion failed, usually because not enough memory was allocated in
    //!     the result. This is only checked for RLE compression.
    //---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API extern bool ImagePixelFormatInPlace( int quality, Image* pResult, const Image* pBase,
                                         int onlyLOD = -1 );

    //---------------------------------------------------------------------------------------------
    //! Wrapper for the above method that allocates the destination image.
    //---------------------------------------------------------------------------------------------
	MUTABLERUNTIME_API extern ImagePtr ImagePixelFormat( int quality, const Image* pBase, EImageFormat targetFormat,
                                      int onlyLOD = -1 );

}
