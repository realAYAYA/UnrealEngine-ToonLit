// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Handling/AvaMaterialInstanceHandle.h"
#include "IAvaObjectHandle.h"

class UDynamicMaterialInstance;

class FAvaDesignedMaterialHandle
	: public FAvaMaterialInstanceHandle
{
public:
	UE_AVA_INHERITS_WITH_SUPER(FAvaDesignedMaterialHandle, FAvaMaterialInstanceHandle);
	
	explicit FAvaDesignedMaterialHandle(const TWeakObjectPtr<UDynamicMaterialInstance>& InWeakDesignedMaterial);

	// ~Begin IAvaMaterialHandle
	virtual void CopyParametersFrom(UMaterialInstance* InSourceMaterial) override;
	// ~End IAvaMaterialHandle

	// ~Begin IAvaObjectHandle
	virtual bool IsValid() const override;
	// ~End IAvaObjectHandle

	static bool IsSupported(const UStruct* InStruct, const TVariant<UObject*, FStructView>& InInstance, FName InTag = NAME_None);
	
protected:
	TWeakObjectPtr<UDynamicMaterialInstance> WeakDesignedMaterial;
};
