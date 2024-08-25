// Copyright Epic Games, Inc. All Rights Reserved.


#include "MuCO/EditorImageProvider.h" 

#include "MuCO/CustomizableObjectSystem.h"
#include "Engine/Texture2D.h"


UCustomizableSystemImageProvider::ValueType UEditorImageProvider::HasTextureParameterValue(const FName& ID)
{
	const UTexture2D* Texture = Cast<UTexture2D>(FSoftObjectPath(ID.ToString()).TryLoad());
	
	return Texture ? ValueType::Unreal_Deferred : ValueType::None;
}


UTexture2D* UEditorImageProvider::GetTextureParameterValue(const FName& ID)
{
	return Cast<UTexture2D>(FSoftObjectPath(ID.ToString()).TryLoad());
}

