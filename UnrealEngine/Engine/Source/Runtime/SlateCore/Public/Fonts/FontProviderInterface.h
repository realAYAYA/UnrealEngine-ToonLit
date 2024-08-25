// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "FontSdfSettings.h"

#include "FontProviderInterface.generated.h"

class UObject;
struct FCompositeFont;

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UFontProviderInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IFontProviderInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual const FCompositeFont* GetCompositeFont() const = 0;

	virtual bool IsSdfFont() const = 0;

	virtual EFontRasterizationMode GetFontRasterizationMode() const = 0;

	virtual const FFontSdfSettings& GetSdfSettings() const = 0;

};
