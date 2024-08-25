// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/StringFwd.h"
#include "CoreMinimal.h"
#include "IImageWrapper.h"

// ImageWrapperHelper is deprecated, use IImageWrapperModule instead
class ImageWrapperHelper
{
public:

	// deprecated, use IImageWrapperModule::GetExtension() instead
	static IMAGEWRAPPER_API FStringView GetFormatExtension(EImageFormat InImageFormat, bool bIncludeDot=false);

	// deprecated, use IImageWrapperModule::GetImageFormatFromExtension instead
	static IMAGEWRAPPER_API EImageFormat GetImageFormat(FStringView StringExtention);

	static IMAGEWRAPPER_API const FStringView GetImageFilesFilterString(bool bIncludeAllFiles);
};
