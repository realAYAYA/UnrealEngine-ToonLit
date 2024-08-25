// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "SlateMaterialBrush.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class UDMMaterialComponent;
class UDMMaterialStageSource;

class SDMStageSourcePreview : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDMStageSourcePreview)
		: _DesiredSize(FVector2D(48.f, 48.f))
		{}
		SLATE_ARGUMENT(FVector2D, DesiredSize)
	SLATE_END_ARGS()

public:

	SDMStageSourcePreview();
	virtual ~SDMStageSourcePreview() override;

	void Construct(const FArguments& InArgs, UDMMaterialStageSource* InStageSource);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:
	TWeakObjectPtr<UDMMaterialStageSource> StageSourceWeak;
	TWeakObjectPtr<UMaterialInterface> PreviewMaterialWeak;
	FSlateMaterialBrush Brush;

	void OnStageSourceUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);
};
