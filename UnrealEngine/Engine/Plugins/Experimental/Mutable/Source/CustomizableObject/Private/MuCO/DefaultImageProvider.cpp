// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/DefaultImageProvider.h" 

#include "MuCO/CustomizableObject.h"

#include "MuR/Parameters.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(DefaultImageProvider)


namespace UDefaultImageProviderCVars
{
	void CheckImageMode(IConsoleVariable* Var);
	
	int32 ImageMode = 3;
	FAutoConsoleVariableRef CVarImageMode(
		TEXT("Mutable.DefaultImageProvider.ImageMode"),
		ImageMode,
		TEXT("0 = None: Texture is not provided by this provider.\n"
		"2 = Unreal: Data will be provided from an unreal texture, loaded in the game thread and kept in memory.\n"
		"3 = Unreal_Deferred (default): Data will be provided from an unreal texture. Will only be loaded when actually needed in the Mutable thread."),
		FConsoleVariableDelegate::CreateStatic(CheckImageMode));

	void CheckImageMode(IConsoleVariable* Var)
	{
		if ((ImageMode < 0 || ImageMode >= static_cast<int32>(UCustomizableSystemImageProvider::ValueType::Count)) &&
			ImageMode == static_cast<int32>(UCustomizableSystemImageProvider::ValueType::Raw)) // Raw not supported.
		{
			UE_LOG(LogMutable, Error, TEXT("DefaultImageProvider: Incorrect Image Mode. Setting Texture Mode to \"None\"."));
			ImageMode = static_cast<int32>(UCustomizableSystemImageProvider::ValueType::None);
		}
	}
}


UCustomizableSystemImageProvider::ValueType UDefaultImageProvider::HasTextureParameterValue(const FName& ID)
{
	const TObjectPtr<UTexture2D>* Texture = Textures.Find(ID);
	
	return Texture ?
		static_cast<ValueType>(UDefaultImageProviderCVars::ImageMode) :
		ValueType::None;
}


UTexture2D* UDefaultImageProvider::GetTextureParameterValue(const FName& ID)
{
	return Textures[ID];
}


void UDefaultImageProvider::GetTextureParameterValues(TArray<FCustomizableObjectExternalTexture>& OutValues)
{
	for (TTuple<FName, TObjectPtr<UTexture2D>>& Pair : Textures)
	{
		FCustomizableObjectExternalTexture Data;
		Data.Name = Pair.Value.GetName();
		Data.Value = Pair.Key;
		OutValues.Add(Data);
	}
}


FString UDefaultImageProvider::Add(UTexture2D* Texture)
{
	if (!Texture)
	{
		return FString();
	}

	const FName Id(Texture->GetPathName());
	Textures.Add(Id, Texture);	

	return Id.ToString();
}


void UDefaultImageProvider::Remove(UTexture2D* Texture)
{
	if (!Texture)
	{
		return;
	}
	
	Textures.Remove(FName(Texture->GetFullName()));
}

