// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Delegates/Delegate.h"
#include "EditorConfigBase.h"
#include "Engine/EngineTypes.h"
#include "HAL/Platform.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "NaniteToolsArguments.generated.h"

class FProperty;
class UObject;

DECLARE_MULTICAST_DELEGATE_TwoParams(FNaniteAuditErrorArgumentsPropertySetModifiedSignature, UObject*, FProperty*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FNaniteAuditOptimizeArgumentsPropertySetModifiedSignature, UObject*, FProperty*);

UCLASS(EditorConfig = "NaniteAuditError")
class UNaniteAuditErrorArguments : public UEditorConfigBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = MaterialErrors, meta = (EditorConfig, DisplayName = "Non-Opaque Blend Mode", Tooltip = "Prohibit non-opaque blend mode in the assigned materials."))
	bool ProhibitUnsupportedBlendMode = true;

	UPROPERTY(EditAnywhere, Category = MaterialErrors, meta = (EditorConfig, DisplayName = "Vertex Interpolator Usage", Tooltip = "Prohibit vertex interpolator usage in the assigned materials."))
	bool ProhibitVertexInterpolator = true;

	UPROPERTY(EditAnywhere, Category = MaterialErrors, meta = (EditorConfig, DisplayName = "Pixel Depth Offset Usage", Tooltip = "Prohibit pixel depth offset usage in the assigned materials."))
	bool ProhibitPixelDepthOffset = true;

	UPROPERTY(EditAnywhere, Category = MaterialErrors, meta = (EditorConfig, DisplayName = "World Position Offset Usage", Tooltip = "Prohibit world position offset usage in the assigned materials."))
	bool ProhibitWorldPositionOffset = true;

	/** @return the multicast delegate that is called when properties are modified */
	FNaniteAuditErrorArgumentsPropertySetModifiedSignature& GetOnModified()
	{
		return OnModified;
	}

private:
	FNaniteAuditErrorArgumentsPropertySetModifiedSignature OnModified;
};

UCLASS(EditorConfig = "NaniteAuditOptimize")
class UNaniteAuditOptimizeArguments : public UEditorConfigBase
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(EditAnywhere, Category = GeometryFilters, meta = (EditorConfig, DisplayName = "Triangle Threshold", Tooltip = "Ignore non-Nanite meshes with fewer triangles than this threshold."))
	uint32 TriangleThreshold = 64000;

	UPROPERTY(EditAnywhere, Category = UnsupportedFeatures, meta = (EditorConfig, DisplayName = "Exclude Unsupported Blend Mode", Tooltip = "Ignore non-Nanite meshes using an unsupported blend mode in the assigned materials."))
	bool DisallowUnsupportedBlendMode = true;

	UPROPERTY(EditAnywhere, Category = UnsupportedFeatures, meta = (EditorConfig, DisplayName = "Exclude Vertex Interpolator Usage", Tooltip = "Ignore non-Nanite meshes using vertex interpolator in the assigned materials."))
	bool DisallowVertexInterpolator = true;

	UPROPERTY(EditAnywhere, Category = UnsupportedFeatures, meta = (EditorConfig, DisplayName = "Exclude Pixel Depth Offset Usage", Tooltip = "Ignore non-Nanite meshes using pixel depth offset in the assigned materials."))
	bool DisallowPixelDepthOffset = true;

	UPROPERTY(EditAnywhere, Category = UnsupportedFeatures, meta = (EditorConfig, DisplayName = "Exclude World Position Offset Usage", Tooltip = "Ignore non-Nanite meshes using world position offset in the assigned materials."))
	bool DisallowWorldPositionOffset = true;

	/** @return the multicast delegate that is called when properties are modified */
	FNaniteAuditOptimizeArgumentsPropertySetModifiedSignature& GetOnModified()
	{
		return OnModified;
	}

private:
	FNaniteAuditOptimizeArgumentsPropertySetModifiedSignature OnModified;
};