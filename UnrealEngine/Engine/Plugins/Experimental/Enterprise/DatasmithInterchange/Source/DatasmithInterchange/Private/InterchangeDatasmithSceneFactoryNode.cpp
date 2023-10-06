// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeDatasmithSceneFactoryNode.h"

#include "DatasmithScene.h"

FString UInterchangeDatasmithSceneFactoryNode::GetTypeName() const
{
	const FString TypeName = TEXT("DatasmithSceneFactoryNode");
	return TypeName;
}

UClass* UInterchangeDatasmithSceneFactoryNode::GetObjectClass() const
{
	return UDatasmithScene::StaticClass();
}