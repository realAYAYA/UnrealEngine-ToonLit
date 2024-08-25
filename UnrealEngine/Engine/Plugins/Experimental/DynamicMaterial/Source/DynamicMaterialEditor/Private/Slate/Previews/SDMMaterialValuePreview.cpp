// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMMaterialValuePreview.h"
#include "Components/DMMaterialValue.h"
#include "DynamicMaterialModule.h"
#include "Materials/Material.h"
#include "Widgets/Images/SImage.h"

SDMMaterialValuePreview::SDMMaterialValuePreview()
	: Brush(FSlateMaterialBrush(FVector2D(1.f, 1.f)))
{

}

SDMMaterialValuePreview::~SDMMaterialValuePreview()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	UDMMaterialValue* Value = ValueWeak.Get();

	if (Value)
	{
		Value->GetOnUpdate().RemoveAll(this);
	}
}

void SDMMaterialValuePreview::Construct(const FArguments& InArgs, UDMMaterialValue* InValue)
{
	ValueWeak = InValue;

	SetCanTick(true);

	Brush.SetImageSize(InArgs._DesiredSize);

	if (ensure(IsValid(InValue)))
	{
		InValue->GetOnUpdate().AddSP(this, &SDMMaterialValuePreview::OnValueUpdated);
		OnValueUpdated(InValue, EDMUpdateType::Value);
	}

	ChildSlot
	[
		SNew(SImage)
		.Image(&Brush)
		.DesiredSizeOverride(InArgs._DesiredSize)
	];
}

void SDMMaterialValuePreview::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!PreviewMaterialWeak.IsValid())
	{
		Brush.SetMaterial(nullptr);
	}

	if (UDMMaterialComponent::CanClean())
	{
		if (UDMMaterialValue* Value = ValueWeak.Get())
		{
			if (Value->IsComponentValid() && Value->NeedsClean())
			{
				Value->DoClean();
				OnValueUpdated(Value, EDMUpdateType::Value);
			}
		}
	}
}

void SDMMaterialValuePreview::OnValueUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (UDMMaterialValue* Value = Cast<UDMMaterialValue>(InComponent))
	{
		if (Value == ValueWeak.Get() && IsValid(Value) && Value->IsComponentValid())
		{
			UMaterial* PreviewMaterial = Value->GetPreviewMaterial();

			if (Brush.GetResourceObject() != PreviewMaterial)
			{
				Brush.SetMaterial(PreviewMaterial);
				PreviewMaterialWeak = PreviewMaterial;
			}
		}
	}
}

