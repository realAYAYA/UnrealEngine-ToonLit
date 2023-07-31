// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "SubobjectEditorExtensionContext.generated.h"

class SSubobjectBlueprintEditor;
class SSubobjectEditor;
class SSubobjectInstanceEditor;

UCLASS()
class KISMET_API USubobjectEditorExtensionContext : public UObject
{
	GENERATED_BODY()

public:
	
	const TWeakPtr<SSubobjectEditor>& GetSubobjectEditor() const { return SubobjectEditor; }

private:
	friend SSubobjectEditor;
	friend SSubobjectInstanceEditor;
	friend SSubobjectBlueprintEditor;

	TWeakPtr<SSubobjectEditor> SubobjectEditor;
};
