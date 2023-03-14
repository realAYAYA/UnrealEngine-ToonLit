// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonScene.h"
#include "Json/GLTFJsonNode.h"
#include "Json/GLTFJsonEpicLevelVariantSets.h"

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

	if (EpicLevelVariantSets.Num() > 0)
	{
		Writer.StartExtensions();

		Writer.StartExtension(EGLTFJsonExtension::EPIC_LevelVariantSets);
		Writer.Write(TEXT("levelVariantSets"), EpicLevelVariantSets);
		Writer.EndExtension();

		Writer.EndExtensions();
	}
}
