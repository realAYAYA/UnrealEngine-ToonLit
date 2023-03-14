// Copyright Epic Games, Inc. All Rights Reserved.

#include "VCamOutputComposure.h"

void UVCamOutputComposure::CreateUMG()
{
	DisplayType = EVPWidgetDisplayType::Composure;

	// Super must be here so that the UMGWidget is already created
	Super::CreateUMG();

	if (UMGWidget)
	{
		UMGWidget->PostProcessDisplayType.ComposureLayerTargets.Empty();

		for (TSoftObjectPtr<ACompositingElement> Layer : LayerTargets)
		{
			if (Layer.IsValid())
			{
				UMGWidget->PostProcessDisplayType.ComposureLayerTargets.Emplace(Layer.Get());
			}
		}
	}
	else
	{
		UE_LOG(LogVCamOutputProvider, Error, TEXT("Composure mode - Either UMG Class is None or Super::CreateUMG isn't being called first!"));
	}
}

#if WITH_EDITOR
void UVCamOutputComposure::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	FProperty* Property = PropertyChangedEvent.MemberProperty;

	if (Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive)
	{
		static FName NAME_LayerTargets = GET_MEMBER_NAME_CHECKED(UVCamOutputComposure, LayerTargets);

		if (Property->GetFName() == NAME_LayerTargets)
		{
			if (bIsActive)
			{
				SetActive(false);
				SetActive(true);
			}
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif
