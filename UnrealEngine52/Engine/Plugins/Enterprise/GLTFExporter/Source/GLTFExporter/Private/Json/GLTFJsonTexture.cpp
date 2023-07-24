// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonTexture.h"
#include "Json/GLTFJsonSampler.h"
#include "Json/GLTFJsonImage.h"

void FGLTFJsonTexture::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!Name.IsEmpty())
	{
		Writer.Write(TEXT("name"), Name);
	}

	if (Sampler != nullptr)
	{
		Writer.Write(TEXT("sampler"), Sampler);
	}

	if (Source != nullptr)
	{
		Writer.Write(TEXT("source"), Source);
	}
}
