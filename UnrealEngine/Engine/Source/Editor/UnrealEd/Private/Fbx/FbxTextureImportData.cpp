// Copyright Epic Games, Inc. All Rights Reserved.

#include "Factories/FbxTextureImportData.h"

#include "UObject/Object.h"

class FProperty;

UFbxTextureImportData::UFbxTextureImportData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	
}

bool UFbxTextureImportData::CanEditChange(const FProperty* InProperty) const
{
	bool bMutable = Super::CanEditChange(InProperty);
	UObject* Outer = GetOuter();
	if(Outer && bMutable)
	{
		// Let the parent object handle the editability of our properties
		bMutable = Outer->CanEditChange(InProperty);
	}
	return bMutable;
}
