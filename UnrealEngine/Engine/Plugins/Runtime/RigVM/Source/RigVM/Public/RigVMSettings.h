// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RigVMSettings.h: Declares the RigVMEditorSettings class.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"

#include "RigVMSettings.generated.h"

class UStaticMesh;

/**
 * Customize RigVM Rig Editor.
 */
UCLASS(config = EditorPerProjectUserSettings, meta=(DisplayName="RigVM Editor"))
class RIGVM_API URigVMEditorSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

public:
	/** Gets the category for the settings, some high level grouping like, Editor, Engine, Game...etc. */
	virtual FName GetCategoryName() const override { return TEXT("ContentEditors"); }
	
#if WITH_EDITORONLY_DATA

	// When this is checked mutable nodes (nodes with an execute pin)
	// will be hooked up automatically.
	UPROPERTY(EditAnywhere, config, Category = Interaction)
	bool bAutoLinkMutableNodes;

#endif
};


