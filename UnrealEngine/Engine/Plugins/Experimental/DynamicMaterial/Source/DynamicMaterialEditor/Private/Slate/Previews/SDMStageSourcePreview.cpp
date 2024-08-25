// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMStageSourcePreview.h"
#include "Components/DMMaterialStageSource.h"
#include "DynamicMaterialModule.h"
#include "Materials/Material.h"
#include "Widgets/Images/SImage.h"

SDMStageSourcePreview::SDMStageSourcePreview()
	: Brush(FSlateMaterialBrush(FVector2D(1.f, 1.f)))
{

}

SDMStageSourcePreview::~SDMStageSourcePreview()
{
	if (!FDynamicMaterialModule::AreUObjectsSafe())
	{
		return;
	}

	UDMMaterialStageSource* StageSource = StageSourceWeak.Get();

	if (StageSource)
	{
		StageSource->GetOnUpdate().RemoveAll(this);
	}
}

void SDMStageSourcePreview::Construct(const FArguments& InArgs, UDMMaterialStageSource* InStageSource)
{
	StageSourceWeak = InStageSource;

	SetCanTick(true);

	Brush.SetImageSize(InArgs._DesiredSize);

	if (ensure(IsValid(InStageSource)))
	{
		InStageSource->GetOnUpdate().AddSP(this, &SDMStageSourcePreview::OnStageSourceUpdated);
		OnStageSourceUpdated(InStageSource, EDMUpdateType::Structure);
	}

	ChildSlot
	[
		SNew(SImage)
		.Image(&Brush)
		.DesiredSizeOverride(InArgs._DesiredSize)
	];
}

void SDMStageSourcePreview::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

	if (!PreviewMaterialWeak.IsValid())
	{
		Brush.SetMaterial(nullptr);
	}

	if (UDMMaterialComponent::CanClean())
	{
		if (UDMMaterialStageSource* Source = StageSourceWeak.Get())
		{
			if (Source->IsComponentValid() && Source->NeedsClean())
			{
				Source->DoClean();
				OnStageSourceUpdated(Source, EDMUpdateType::Structure);
			}
		}
	}
}

void SDMStageSourcePreview::OnStageSourceUpdated(UDMMaterialComponent* InComponent, EDMUpdateType InUpdateType)
{
	if (UDMMaterialStageSource* Source = Cast<UDMMaterialStageSource>(InComponent))
	{
		if (Source == StageSourceWeak.Get() && Source->IsComponentValid())
		{
			UMaterial* PreviewMaterial = Source->GetPreviewMaterial();

			if (Brush.GetResourceObject() != PreviewMaterial)
			{
				Brush.SetMaterial(PreviewMaterial);
				PreviewMaterialWeak = PreviewMaterial;
			}
		}
	}
}

