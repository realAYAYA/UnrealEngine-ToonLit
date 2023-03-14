// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Blueprints/TextureShareBlueprintContainers.h"
#include "Misc/TextureShareCoreStrings.h"

//////////////////////////////////////////////////////////////////////////////////////////////
namespace TextureShareBlueprintContainersHelpers
{
	static FString GetValidTextureShareObjectName(const FString& InShareName)
	{
		return InShareName.IsEmpty() ? TextureShareCoreStrings::Default::ShareName : InShareName;
	}
};

using namespace TextureShareBlueprintContainersHelpers;

//////////////////////////////////////////////////////////////////////////////////////////////
// FTextureShareObjectDesc
//////////////////////////////////////////////////////////////////////////////////////////////
FString FTextureShareObjectDesc::GetTextureShareObjectName() const
{
	return GetValidTextureShareObjectName(ShareName);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// UTextureShareObject
//////////////////////////////////////////////////////////////////////////////////////////////
UTextureShareObject::UTextureShareObject()
{

}
UTextureShareObject::~UTextureShareObject()
{

}

void UTextureShareObject::SendCustomData(const TMap<FString, FString>& InSendParameters)
{
	CustomData.SendParameters.Empty();
	CustomData.SendParameters.Append(InSendParameters);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// UTextureShare
//////////////////////////////////////////////////////////////////////////////////////////////
UTextureShare::UTextureShare()
{
	// Create default textureshare
	GetOrCreateTextureShareObject(TextureShareCoreStrings::Default::ShareName);
}

UTextureShare::~UTextureShare()
{
	TextureShareObjects.Empty();
}

TSet<FString> UTextureShare::GetTextureShareObjectNames() const
{
	TSet<FString> Result;
	for (const UTextureShareObject* TextureShareObjectIt : TextureShareObjects)
	{
		if (TextureShareObjectIt && TextureShareObjectIt->bEnable)
		{
			Result.Add(TextureShareObjectIt->Desc.GetTextureShareObjectName().ToLower());
		}
	}

	return Result;
}

UTextureShareObject* UTextureShare::GetTextureShareObject(const FString& InShareName) const
{
	TObjectPtr<UTextureShareObject> const* ExistObject = TextureShareObjects.FindByPredicate([InShareName](const UTextureShareObject* TextureShareObjectIt)
	{
		if (TextureShareObjectIt && TextureShareObjectIt->bEnable)
		{
			const FString ShareName = TextureShareObjectIt->Desc.GetTextureShareObjectName().ToLower();
			return ShareName == InShareName;
		}
		return false;
	});

	return ExistObject ? *ExistObject : nullptr;
}

const TArray<UTextureShareObject*> UTextureShare::GetTextureShareObjects() const
{
	return TextureShareObjects;
}

bool UTextureShare::RemoveTextureShareObject(const FString& InShareName)
{
	const FString ShareName = GetValidTextureShareObjectName(InShareName);

	const int32 Index = TextureShareObjects.IndexOfByPredicate([ShareName](UTextureShareObject* ObjectIt) {
		return ObjectIt && ObjectIt->Desc.GetTextureShareObjectName() == ShareName;
	});

	if (Index != INDEX_NONE)
	{
		TextureShareObjects[Index]->ConditionalBeginDestroy();
		TextureShareObjects.RemoveAt(Index);
		return true;
	}

	return false;
}

UTextureShareObject* UTextureShare::GetOrCreateTextureShareObject(const FString& InShareName)
{
	const FString ShareName = GetValidTextureShareObjectName(InShareName);

	for(UTextureShareObject* ObjectIt : TextureShareObjects)
	{
		if (ObjectIt && ObjectIt->Desc.GetTextureShareObjectName() == ShareName)
		{
			return ObjectIt;
		}
	}

	// Create default
	UTextureShareObject* NewTextureShareObject = NewObject<UTextureShareObject>(GetTransientPackage(), NAME_None, RF_Transient | RF_ArchetypeObject | RF_Public | RF_Transactional);
	if (NewTextureShareObject)
	{
		NewTextureShareObject->Desc.ShareName = InShareName;

		TextureShareObjects.Add(NewTextureShareObject);
	}

	return NewTextureShareObject;
}

