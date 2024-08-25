// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AvaType.h"
#include "IAvaObjectHandle.h"

class UMaterialInstance;
class UMaterialInterface;

class IAvaMaterialHandle
	: public IAvaObjectHandle
{
public:
	UE_AVA_INHERITS(IAvaMaterialHandle, IAvaObjectHandle);
	
	virtual ~IAvaMaterialHandle() override = default;

	virtual FString GetMaterialName() = 0;
	virtual UMaterialInterface* GetMaterial() = 0;

	virtual void CopyParametersFrom(UMaterialInstance* InSourceMaterial) = 0;
};
