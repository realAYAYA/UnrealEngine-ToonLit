// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonAsset.h"

void FGLTFJsonAsset::WriteObject(IGLTFJsonWriter& Writer) const
{
	Writer.Write(TEXT("version"), Version);

	if (!Generator.IsEmpty())
	{
		Writer.Write(TEXT("generator"), Generator);
	}

	if (!Copyright.IsEmpty())
	{
		Writer.Write(TEXT("copyright"), Copyright);
	}
}
