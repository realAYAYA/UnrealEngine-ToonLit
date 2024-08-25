// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handling/AvaMaterialInstanceHandle.h"

#include "Engine/Texture.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "UObject/Package.h"

FAvaMaterialInstanceHandle::FAvaMaterialInstanceHandle(
	const TWeakObjectPtr<UMaterialInterface>& InWeakParentMaterial)
	: WeakParentMaterial(InWeakParentMaterial)
	, WeakMaterialInstance(Cast<UMaterialInstanceDynamic>(InWeakParentMaterial))
{
}

FString FAvaMaterialInstanceHandle::GetMaterialName()
{
	if (const UMaterialInterface* MaterialInstance = GetMaterial())
	{
		return MaterialInstance->GetName();
	}

	return { };
}

UMaterialInterface* FAvaMaterialInstanceHandle::GetMaterial()
{
	if (UMaterialInterface* MaterialInstance = GetMaterialInstance())
	{
		return MaterialInstance;
	}

	return WeakParentMaterial.Get();
}

void FAvaMaterialInstanceHandle::CopyParametersFrom(UMaterialInstance* InSourceMaterial)
{
	if (UMaterialInstanceDynamic* MaterialInstance = GetMaterialInstance())
	{
		MaterialInstance->CopyInterpParameters(InSourceMaterial);
	}
}

bool FAvaMaterialInstanceHandle::IsValid() const
{
	// @note: MaterialInstance doesn't have to be valid - this handle should deal with MID creation
	return WeakParentMaterial.IsValid();
}

bool FAvaMaterialInstanceHandle::IsSupported(
	const UStruct* InStruct
	, const TVariant<UObject*, FStructView>& InInstance
    , FName InTag)
{
	return InStruct
		&& ::IsValid(InStruct)
		&& InStruct->IsChildOf(UMaterialInterface::StaticClass())
		&& InInstance.TryGet<UObject*>() != nullptr;
}

UMaterialInterface* FAvaMaterialInstanceHandle::GetParentMaterial() const
{
	if (UMaterialInterface* ParentMaterial = WeakParentMaterial.Get())
	{
		return ParentMaterial;
	}

	return nullptr;
}

UMaterialInstanceDynamic* FAvaMaterialInstanceHandle::GetMaterialInstance()
{
	if (UMaterialInstanceDynamic* MaterialInstance = WeakMaterialInstance.Get())
	{
		return MaterialInstance;
	}

	return nullptr;
}

UMaterialInstanceDynamic* FAvaMaterialInstanceHandle::GetOrCreateMaterialInstance()
{
	if (UMaterialInstanceDynamic* MaterialInstance = GetMaterialInstance())
	{
		return MaterialInstance;
	}
	
	if (UMaterialInterface* ParentMaterial = GetParentMaterial())
	{
		UObject* Outer = GetTransientPackage();
		UMaterialInstanceDynamic* MaterialInstance = UMaterialInstanceDynamic::Create(ParentMaterial, Outer);
		WeakMaterialInstance = MaterialInstance;
		return MaterialInstance;
	}

	return nullptr;
}
