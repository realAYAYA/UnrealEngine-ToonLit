// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMStagePreview.h"
#include "Components/DMMaterialStage.h"
#include "DynamicMaterialModule.h"
#include "Materials/Material.h"
#include "Widgets/Images/SImage.h"

SDMStagePreview::SDMStagePreview()
	: Brush(FSlateMaterialBrush(FVector2D(1.f, 1.f)))
{
}

SDMStagePreview::~SDMStagePreview()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	if (UDMMaterialStage* Stage = StageWeak.Get())
	{
		Stage->GetOnUpdate().RemoveAll(this);
	}
}

void SDMStagePreview::Construct(const FArguments& InArgs, UDMMaterialStage* InStage)
{
	StageWeak = InStage;

	PreviewSize = InArgs._PreviewSize;

	SetCanTick(true);

	Brush.SetImageSize(PreviewSize.Get());

	if (ensure(IsValid(InStage)))
	{
		InStage->GetOnUpdate().AddSP(this, &SDMStagePreview::OnStageUpdated);
		OnStageUpdated(InStage, EDMUpdateType::Structure);
	}

	ChildSlot
	[
		SNew(SImage)
		.Image(&Brush)
		.DesiredSizeOverride(this, &SDMStagePreview::GetPreviewSize)
	];
}

void SDMStagePreview::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!PreviewMaterialWeak.IsValid())
	{
		Brush.SetMaterial(nullptr);
	}

	if (UDMMaterialComponent::CanClean())
	{
		if (UDMMaterialStage* Stage = StageWeak.Get())
		{
			if (Stage->IsComponentValid() && Stage->NeedsClean())
			{
				Stage->DoClean();
				OnStageUpdated(StageWeak.Get(), EDMUpdateType::Structure);
			}
		}
	}	
}

void SDMStagePreview::OnStageUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (UDMMaterialStage* Stage = Cast<UDMMaterialStage>(InComponent))
	{
		if (Stage == StageWeak.Get() && IsValid(Stage) && Stage->IsComponentValid())
		{
			UMaterialInterface* PreviewMaterial = Stage->GetPreviewMaterial();

			if (Brush.GetResourceObject() != PreviewMaterial)
			{
				Brush.SetMaterial(PreviewMaterial);
				PreviewMaterialWeak = PreviewMaterial;
			}
		}
	}
}

TOptional<FVector2D> SDMStagePreview::GetPreviewSize() const
{
	return PreviewSize.Get();
}
