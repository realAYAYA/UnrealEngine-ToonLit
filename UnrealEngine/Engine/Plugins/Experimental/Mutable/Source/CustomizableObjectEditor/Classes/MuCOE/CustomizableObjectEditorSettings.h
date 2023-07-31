// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "CustomizableObjectEditorSettings.generated.h"

UCLASS(config = Engine)
class UCustomizableObjectEditorSettings : public UObject
{
	GENERATED_BODY()

public:

	UCustomizableObjectEditorSettings();

	/** If true, Mutable won't compile any COs in the Editor. */
	UPROPERTY(config, EditAnywhere, Category = Compilation)
	bool bDisableMutableCompileInEditor;

	/**	If true, Mutable will automatically compile, if needed, COs being used by Actors. */
	UPROPERTY(config, EditAnywhere, Category = AutomaticCompilation)
	bool bEnableAutomaticCompilation = true;

	/**	If true, AutomaticCompilation will happen synchronously. */
	UPROPERTY(config, EditAnywhere, Category = AutomaticCompilation)
	bool bCompileObjectsSynchronously = false;

	/** If true, Root Customizable Objects in memory will be compiled, if needed, before starting a PIE session. */
	UPROPERTY(config, EditAnywhere, Category = AutomaticCompilation)
	bool bCompileRootObjectsOnStartPIE = false;

};
