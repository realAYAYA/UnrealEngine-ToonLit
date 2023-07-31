// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

class UTexture2D;

#if WITH_EDITORONLY_DATA
class IWaterEditorServices
{
public:
	virtual ~IWaterEditorServices() {}

	virtual void RegisterWaterActorSprite(UClass* InClass, UTexture2D* Texture) = 0;
	virtual UTexture2D* GetWaterActorSprite(UClass* InClass) const = 0;
	virtual UTexture2D* GetErrorSprite() const = 0;
};
#endif // WITH_EDITORONLY_DATA