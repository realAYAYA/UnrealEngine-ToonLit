// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SubclassOf.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

#include "LyraWidgetFactory.generated.h"

class UUserWidget;
struct FFrame;

UCLASS(Abstract, Blueprintable, BlueprintType, EditInlineNew)
class LYRAGAME_API ULyraWidgetFactory : public UObject
{
	GENERATED_BODY()

public:
	ULyraWidgetFactory() { }

	UFUNCTION(BlueprintNativeEvent)
	TSubclassOf<UUserWidget> FindWidgetClassForData(const UObject* Data) const;
};