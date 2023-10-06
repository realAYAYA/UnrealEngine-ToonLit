// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SEditorViewportViewMenuContext.generated.h"

class SEditorViewportViewMenu;

UCLASS(MinimalAPI)
class UEditorViewportViewMenuContext : public UObject
{
	GENERATED_BODY()
public:

	TWeakPtr<const SEditorViewportViewMenu> EditorViewportViewMenu;
};
