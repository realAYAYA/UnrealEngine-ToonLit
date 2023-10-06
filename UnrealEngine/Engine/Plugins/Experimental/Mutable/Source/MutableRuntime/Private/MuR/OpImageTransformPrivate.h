// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/ImagePrivate.h"

#include "Math/Vector.h"
#include "Math/IntVector.h"

namespace mu
{

    //---------------------------------------------------------------------------------------------
    //! Image transform, translate, scale, rotate 
    //---------------------------------------------------------------------------------------------
	template<int32, EAddressMode> 
	void ImageTransformImpl(Image*, const Image*, FVector2f, FVector2f, float);

	// extern template could be used if for some reason we need to provide the full declaration here. 
	//extern template FORCENOINLINE void ImageTransformImpl<1, EAddressMode::ClampToBlack>(Image*, const Image*, FVector2f, FVector2f, float);
	//extern template FORCENOINLINE void ImageTransformImpl<1, EAddressMode::ClampToEdge>(Image*, const Image*, FVector2f, FVector2f, float);
	//extern template FORCENOINLINE void ImageTransformImpl<1, EAddressMode::Wrap>(Image*, const Image*, FVector2f, FVector2f, float);
	//extern template FORCENOINLINE void ImageTransformImpl<3, EAddressMode::ClampToBlack>(Image*, const Image*, FVector2f, FVector2f, float);
	//extern template FORCENOINLINE void ImageTransformImpl<3, EAddressMode::ClampToEdge>(Image*, const Image*, FVector2f, FVector2f, float);
	//extern template FORCENOINLINE void ImageTransformImpl<3, EAddressMode::Wrap>(Image*, const Image*, FVector2f, FVector2f, float);
	//extern template FORCENOINLINE void ImageTransformImpl<4, EAddressMode::ClampToBlack>(Image*, const Image*, FVector2f, FVector2f, float);
	//extern template FORCENOINLINE void ImageTransformImpl<4, EAddressMode::ClampToEdge>(Image*, const Image*, FVector2f, FVector2f, float);
	//extern template FORCENOINLINE void ImageTransformImpl<4, EAddressMode::Wrap>(Image*, const Image*, FVector2f, FVector2f, float);
}
