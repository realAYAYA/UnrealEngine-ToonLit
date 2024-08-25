// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "SlateMaterialBrush.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class UDMMaterialComponent;
class UDMMaterialStage;

class SDMStagePreview : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMStagePreview)
		: _PreviewSize(FVector2D(48.f, 48.f))
	{}
		SLATE_ATTRIBUTE(FVector2D, PreviewSize)
	SLATE_END_ARGS()

	SDMStagePreview();
	virtual ~SDMStagePreview() override;

	void Construct(const FArguments& InArgs, UDMMaterialStage* InStage);

	//~ Begin SWidget
	virtual void Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	//~ End SWidget

protected:
	TWeakObjectPtr<UDMMaterialStage> StageWeak;
	TWeakObjectPtr<UMaterialInterface> PreviewMaterialWeak;
	FSlateMaterialBrush Brush;
	TAttribute<FVector2D> PreviewSize;

	void OnStageUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);

	TOptional<FVector2D> GetPreviewSize() const;
};
