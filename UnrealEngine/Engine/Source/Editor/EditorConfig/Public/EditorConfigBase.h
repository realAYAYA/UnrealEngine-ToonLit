// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "EditorConfigBase.generated.h"

/** Inherit from this class to simplify saving and loading properties from editor configs. */
UCLASS()
class EDITORCONFIG_API UEditorConfigBase : public UObject
{
	GENERATED_BODY()

public:

	/** Load any properties of this class into properties marked with metadata tag "EditorConfig" from the class's EditorConfig */
	bool LoadEditorConfig();

	/** Save any properties of this class in properties marked with metadata tag "EditorConfig" into the class's EditorConfig. */
	bool SaveEditorConfig() const;
};