// Copyright Epic Games, Inc. All Rights Reserved.

#include "Handling/AvaParametricMaterialHandle.h"

#include "AvaShapeParametricMaterial.h"

FAvaParametricMaterialHandle::FAvaParametricMaterialHandle(const FStructView& InParametricMaterial)
	: FAvaMaterialInstanceHandle(InParametricMaterial.GetPtr<FAvaShapeParametricMaterial>()->GetMaterial())
	, ParametricMaterial(InParametricMaterial)
{
}

void FAvaParametricMaterialHandle::CopyParametersFrom(UMaterialInstance* InSourceMaterial)
{
	Super::CopyParametersFrom(InSourceMaterial);
	
	if (FAvaShapeParametricMaterial* ParametricMtl = GetParametricMaterial())
	{
		ParametricMtl->CopyFromMaterialParameters(InSourceMaterial);
	}
}

bool FAvaParametricMaterialHandle::IsValid() const
{
	return this && ParametricMaterial.IsValid() && WeakMaterialInstance.IsValid();
}

bool FAvaParametricMaterialHandle::IsSupported(
	const UStruct* InStruct
	, const TVariant<UObject*, FStructView>& InInstance
	, FName InTag)
{
	return InStruct
		&& ::IsValid(InStruct)
		&& InStruct->IsChildOf(FAvaShapeParametricMaterial::StaticStruct())
		&& InInstance.TryGet<FStructView>() != nullptr;
}

FAvaShapeParametricMaterial* FAvaParametricMaterialHandle::GetParametricMaterial()
{
	if (ParametricMaterial.IsValid())
	{
		return ParametricMaterial.GetPtr<FAvaShapeParametricMaterial>();
	}

	return nullptr;
}

UMaterialInstanceDynamic* FAvaParametricMaterialHandle::GetMaterialInstance()
{
	if (const FAvaShapeParametricMaterial* ParametricMtl = GetParametricMaterial())
	{
		return ParametricMtl->GetMaterial();
	}
	
	return FAvaMaterialInstanceHandle::GetMaterialInstance();
}
