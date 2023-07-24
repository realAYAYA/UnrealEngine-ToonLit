// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonImage.h"
#include "Json/GLTFJsonBufferView.h"

void FGLTFJsonImage::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!Name.IsEmpty())
	{
		Writer.Write(TEXT("name"), Name);
	}

	if (!URI.IsEmpty())
	{
		Writer.Write(TEXT("uri"), URI);
	}

	if (MimeType != EGLTFJsonMimeType::None)
	{
		Writer.Write(TEXT("mimeType"), MimeType);
	}

	if (BufferView != nullptr)
	{
		Writer.Write(TEXT("bufferView"), BufferView);
	}
}
