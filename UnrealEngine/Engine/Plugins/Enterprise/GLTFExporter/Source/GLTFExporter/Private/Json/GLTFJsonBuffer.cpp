// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonBuffer.h"

void FGLTFJsonBuffer::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!Name.IsEmpty())
	{
		Writer.Write(TEXT("name"), Name);
	}

	if (!URI.IsEmpty())
	{
		Writer.Write(TEXT("uri"), URI);
	}

	Writer.Write(TEXT("byteLength"), ByteLength);
}
