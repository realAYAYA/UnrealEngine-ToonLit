// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonScene.h"
#include "Json/GLTFJsonNode.h"

void FGLTFJsonScene::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!Name.IsEmpty())
	{
		Writer.Write(TEXT("name"), Name);
	}

	if (Nodes.Num() > 0)
	{
		Writer.Write(TEXT("nodes"), Nodes);
	}
}
