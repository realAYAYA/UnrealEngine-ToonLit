// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonObject.h"
#include "Json/GLTFJsonWriter.h"

void IGLTFJsonObject::WriteValue(IGLTFJsonWriter& Writer) const
{
	Writer.StartObject();
	WriteObject(Writer);
	Writer.EndObject();
}
