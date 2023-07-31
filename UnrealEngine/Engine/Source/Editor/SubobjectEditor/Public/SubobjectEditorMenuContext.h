// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "SubobjectEditorMenuContext.generated.h"

class SSubobjectEditor;
struct FFrame;

UCLASS()
class SUBOBJECTEDITOR_API USubobjectEditorMenuContext : public UObject
{
	GENERATED_BODY()
public:

	UFUNCTION(BlueprintCallable, Category="Tool Menus")
	TArray<UObject*> GetSelectedObjects() const;

	TWeakPtr<SSubobjectEditor> SubobjectEditor;
	
	bool bOnlyShowPasteOption;
};
