// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Tools/LegacyEdModeWidgetHelpers.h"

#include "SequencerToolsEditMode.generated.h"


UCLASS()
class USequencerToolsEditMode : public UBaseLegacyWidgetEdMode
{
	GENERATED_BODY()
public:
	static FEditorModeID ModeName;

	USequencerToolsEditMode();
	virtual ~USequencerToolsEditMode();

	// UEdMode interface
	virtual void Enter();
	virtual void Exit();

	// UBaseLegacyWidgetEdMode interface
	
	virtual bool UsesToolkits() const  override { return false; }
	virtual bool InputKey(FEditorViewportClient* InViewportClient, FViewport* InViewport, FKey InKey, EInputEvent InEvent) override;
	virtual bool UsesPropertyWidgets() const override { return true; }
	virtual bool UsesTransformWidget() const { return false; }
	virtual bool IsCompatibleWith(FEditorModeID OtherModeID) const override;

};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#endif
