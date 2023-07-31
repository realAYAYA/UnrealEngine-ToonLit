// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SEditorViewportViewMenuContext.generated.h"

class SEditorViewportViewMenu;

UCLASS()
class UNREALED_API UEditorViewportViewMenuContext : public UObject
{
	GENERATED_BODY()
public:

	TWeakPtr<const SEditorViewportViewMenu> EditorViewportViewMenu;
};
