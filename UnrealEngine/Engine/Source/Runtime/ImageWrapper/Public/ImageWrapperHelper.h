// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Containers/StringFwd.h"
#include "CoreMinimal.h"
#include "IImageWrapper.h"

class ImageWrapperHelper
{
public:
	 static IMAGEWRAPPER_API FStringView GetFormatExtension(EImageFormat InImageFormat, bool bIncludeDot=false);
	 static IMAGEWRAPPER_API EImageFormat GetImageFormat(FStringView StringExtention);
	 static IMAGEWRAPPER_API const FStringView GetImageFilesFilterString(bool bIncludeAllFiles);
};
