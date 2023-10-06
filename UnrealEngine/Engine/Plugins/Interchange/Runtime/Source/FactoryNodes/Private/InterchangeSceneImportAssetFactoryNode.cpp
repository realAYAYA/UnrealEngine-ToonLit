// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeSceneImportAssetFactoryNode.h"

#include "InterchangeSceneImportAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeSceneImportAssetFactoryNode)

UClass* UInterchangeSceneImportAssetFactoryNode::GetObjectClass() const
{
	return UInterchangeSceneImportAsset::StaticClass();
}
