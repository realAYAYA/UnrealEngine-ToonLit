//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "PhononSourceComponentDetails.h"

#include "PhononCommon.h"
#include "PhononSourceComponent.h"
#include "SteamAudioEditorModule.h"
#include "IndirectBaker.h"

#include "Components/AudioComponent.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailsView.h"
#include "IDetailCustomization.h"
#include "IDetailChildrenBuilder.h"
#include "LevelEditorViewport.h"
#include "EngineUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"

namespace SteamAudio
{
	//==============================================================================================================================================
	// FPhononSourceComponentDetails
	//==============================================================================================================================================

	TSharedRef<IDetailCustomization> FPhononSourceComponentDetails::MakeInstance()
	{
		return MakeShareable(new FPhononSourceComponentDetails());
	}

	void FPhononSourceComponentDetails::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
	{
		const TArray<TWeakObjectPtr<UObject>>& SelectedObjects = DetailLayout.GetSelectedObjects();

		for (int32 ObjectIndex = 0; ObjectIndex < SelectedObjects.Num(); ++ObjectIndex)
		{
			const TWeakObjectPtr<UObject>& CurrentObject = SelectedObjects[ObjectIndex];
			if (CurrentObject.IsValid())
			{
				auto SelectedPhononSourceComponent = Cast<UPhononSourceComponent>(CurrentObject.Get());
				if (SelectedPhononSourceComponent)
				{
					PhononSourceComponent = SelectedPhononSourceComponent;
					break;
				}
			}
		}

		DetailLayout.EditCategory("Baking").AddProperty(GET_MEMBER_NAME_CHECKED(UPhononSourceComponent, UniqueIdentifier));
		DetailLayout.EditCategory("Baking").AddProperty(GET_MEMBER_NAME_CHECKED(UPhononSourceComponent, BakingRadius));
		DetailLayout.EditCategory("Baking").AddCustomRow(NSLOCTEXT("SteamAudio", "Bake Propagation", "Bake Propagation"))
			.NameContent()
			[
				SNullWidget::NullWidget
			]
			.ValueContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.ContentPadding(2)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					.IsEnabled(this, &FPhononSourceComponentDetails::IsBakeEnabled)
					.OnClicked(this, &FPhononSourceComponentDetails::OnBakePropagation)
					[
						SNew(STextBlock)
						.Text(NSLOCTEXT("SteamAudio", "BakePropagation", "Bake Propagation"))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			];
	}

	FReply FPhononSourceComponentDetails::OnBakePropagation()
	{
		TArray<UPhononSourceComponent*> SourceComponents;
		
		// Retrieve all phonon source components and ensure it is the correct component instance in the persistent level
		for (TObjectIterator<UPhononSourceComponent> PhononSourceObj; PhononSourceObj; ++PhononSourceObj)
		{
			UPhononSourceComponent *PhononComponent = *PhononSourceObj;
			if ((PhononComponent && PhononComponent->IsValidLowLevelFast())
				&& (PhononComponent->UniqueIdentifier == PhononSourceComponent.Get()->UniqueIdentifier))
			{
				if (PhononComponent->GetOwner())
				{
					// Debug only - get all audio components and print out their ids
					//for (auto ActorComp : PhononComponent->GetOwner()->GetComponentsByClass(UAudioComponent::StaticClass()))
					//{
					//	UAudioComponent* AudioComponent = Cast<UAudioComponent>(ActorComp);

					//	if (AudioComponent)
					//	{
					//		UE_LOG(LogTemp, Warning, TEXT("Found Audio Component [%s] with UserID [%s]"), *AudioComponent->GetFullName(), *AudioComponent->AudioComponentUserID.ToString());
					//	}
					//}
					//
					
					if (PhononComponent->GetOwner()->GetFullName().Contains(TEXT("Transient.World")))
					{
						// Go through all audio components that the owner of this phonon source actor has
						TArray<UActorComponent*> Components;
						PhononComponent->GetOwner()->GetComponents(UAudioComponent::StaticClass(), Components);

						for (auto ActorComp : Components)
						{
							if (ActorComp && ActorComp->IsValidLowLevel())
							{
								UAudioComponent* AudioComponent = Cast<UAudioComponent>(ActorComp);

								if (AudioComponent 
									&& AudioComponent->IsValidLowLevelFast()
									&& AudioComponent->AudioComponentUserID != PhononSourceComponent->UniqueIdentifier
									)
								{
									// Apply the phonon source id to this audio component
									AudioComponent->AudioComponentUserID = PhononSourceComponent->UniqueIdentifier;
									//UE_LOG(LogTemp, Warning, TEXT("Audio Component [%s] UserID is now set to [%s]"), *AudioComponent->GetFullName(), *AudioComponent->AudioComponentUserID.ToString());

									// Check if there's a phonon source child for this audio component
									for (auto ChildComp : AudioComponent->GetAttachChildren())
									{
										auto* ChildPhononComponent = Cast<UPhononSourceComponent>(ChildComp);

										// If there is one, then apply it's child phonon source id, overriding and phonon source id already applied to it
										if (ChildPhononComponent && ChildPhononComponent->IsValidLowLevelFast())
										{
											AudioComponent->AudioComponentUserID = ChildPhononComponent->UniqueIdentifier;
											//UE_LOG(LogTemp, Warning, TEXT("Audio Component [%s] UserID is now set to [%s]"), *AudioComponent->GetFullName(), *AudioComponent->AudioComponentUserID.ToString());
											break;
										}
									}
								}
							}
						}
					}
					else
					{
						SourceComponents.Add(PhononComponent);
					}
				}
			}
		}
		
		Bake(SourceComponents, false, nullptr);
		
		return FReply::Handled();
	}

	bool FPhononSourceComponentDetails::IsBakeEnabled() const
	{
		const bool bIsBaking = GIsBaking.Load();
		return !(PhononSourceComponent->UniqueIdentifier.IsNone() || bIsBaking);
	}
}
