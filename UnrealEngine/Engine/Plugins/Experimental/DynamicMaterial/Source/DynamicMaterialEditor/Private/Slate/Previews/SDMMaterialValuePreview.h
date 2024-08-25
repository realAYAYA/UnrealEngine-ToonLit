// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DMEDefs.h"
#include "SlateMaterialBrush.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class UDMMaterialComponent;
class UDMMaterialValue;

class SDMMaterialValuePreview : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SDMMaterialValuePreview)
		: _DesiredSize(FVector2D(48.f, 48.f))
		{}
		SLATE_ARGUMENT(FVector2D, DesiredSize)
	SLATE_END_ARGS()

public:
	SDMMaterialValuePreview();
	virtual ~SDMMaterialValuePreview() override;

	void Construct(const FArguments& InArgs, UDMMaterialValue* InValue);

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:
	TWeakObjectPtr<UDMMaterialValue> ValueWeak;
	TWeakObjectPtr<UMaterialInterface> PreviewMaterialWeak;
	FSlateMaterialBrush Brush;

	void OnValueUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType);
};
