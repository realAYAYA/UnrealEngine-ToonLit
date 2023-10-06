// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"

#include "BrushEditingSubsystem.generated.h"

class FLevelEditorViewportClient;
class FEditorModeTools;
class HHitProxy;
class ABrush;
struct FViewportClick;

UCLASS(abstract, MinimalAPI)
class UBrushEditingSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UNREALED_API UBrushEditingSubsystem();

	virtual bool ProcessClickOnBrushGeometry(FLevelEditorViewportClient* ViewportClient, HHitProxy* InHitProxy, const FViewportClick& Click) { return false; }


	virtual void UpdateGeometryFromSelectedBrushes() {}
	virtual void UpdateGeometryFromBrush(ABrush* Brush) {}

	virtual bool IsGeometryEditorModeActive() const { return false; }

	virtual void DeselectAllEditingGeometry() {};

	virtual bool HandleActorDelete() { return false; };
private:

};
