// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modifier/VCamModifier.h"
#include "EditorOnlyVCamModifier.generated.h"

/**
 * A modifier that only exists in uncooked builds.
 */
UCLASS(Abstract)
class VCAMCOREEDITOR_API UEditorOnlyVCamModifier : public UVCamBlueprintModifier
{
	GENERATED_BODY()
public:

	//~ Begin UObject Interface
	virtual bool IsEditorOnly() const override { return true; }
	//~ End UObject Interface
};