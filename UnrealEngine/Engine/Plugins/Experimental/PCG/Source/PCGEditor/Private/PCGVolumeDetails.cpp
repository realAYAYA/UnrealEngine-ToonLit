// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGVolumeDetails.h"

#include "PCGComponent.h"
#include "PCGVolume.h"

TSharedRef<IDetailCustomization> FPCGVolumeDetails::MakeInstance()
{
	return MakeShareable(new FPCGVolumeDetails());
}

void FPCGVolumeDetails::GatherPCGComponentsFromSelection(const TArray<TWeakObjectPtr<UObject>>& InObjectSelected)
{
	for (const TWeakObjectPtr<UObject>& Object : InObjectSelected)
	{
		if (APCGVolume* Volume = Cast<APCGVolume>(Object))
		{
			TInlineComponentArray<UPCGComponent*, 1> Components;
			Volume->GetComponents<UPCGComponent>(Components);
			for (UPCGComponent* Component : Components)
			{
				if (Component)
				{
					SelectedComponents.Add(Component);
				}
			}
		}
	}
}