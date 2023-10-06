// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartitionHLODDetailsCustomization.h"

#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "ComponentRecreateRenderStateContext.h"
#include "Components/StaticMeshComponent.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "MaterialShared.h"
#include "Misc/ScopedSlowTask.h"
#include "PropertyHandle.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Input/SButton.h"
#include "WorldPartition/WorldPartition.h"
#include "WorldPartition/HLOD/HLODActor.h"

#define LOCTEXT_NAMESPACE "FWorldPartitionHLODDetailsCustomization"

TSharedRef<IDetailCustomization> FWorldPartitionHLODDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FWorldPartitionHLODDetailsCustomization);
}

void FWorldPartitionHLODDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayoutBuilder)
{
	SelectedObjects = DetailLayoutBuilder.GetSelectedObjects();

	IDetailCategoryBuilder& PluginCategory = DetailLayoutBuilder.EditCategory("Tools");
	PluginCategory.AddCustomRow(LOCTEXT("HLODTools", "HLOD Tools"), false)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			SNew(SButton)
			.Text(LOCTEXT("BuildHLODButtonText", "Build HLOD"))
			.OnClicked(this, &FWorldPartitionHLODDetailsCustomization::OnBuildHLOD)
		]
	];
}

TArray<AWorldPartitionHLOD*> FWorldPartitionHLODDetailsCustomization::GetSelectedHLODActors() const
{
	TArray<AWorldPartitionHLOD*> SelectedHLODActors;
	SelectedHLODActors.Reserve(SelectedObjects.Num());

	auto IsA_WPHLOD = [](TWeakObjectPtr<UObject> InObject) { return InObject.Get() && InObject->IsA<AWorldPartitionHLOD>(); };
	auto Cast_WPHLOD = [](TWeakObjectPtr<UObject> InObject) { return Cast<AWorldPartitionHLOD>(InObject); };
	Algo::TransformIf(SelectedObjects, SelectedHLODActors, IsA_WPHLOD, Cast_WPHLOD);

	return SelectedHLODActors;
}

FReply FWorldPartitionHLODDetailsCustomization::OnBuildHLOD()
{
	TArray<AWorldPartitionHLOD*> SelectedHLODActors = GetSelectedHLODActors();

	// Gather all components
	TArray<UActorComponent*> ActorComponents;
	Algo::ForEach(SelectedHLODActors, [&ActorComponents](AWorldPartitionHLOD* HLODActor) { ActorComponents.Append(HLODActor->GetComponents().Array()); });

	// Recreate render states for all the components we're about to process
	FGlobalComponentRecreateRenderStateContext RecreateRenderStateContext(ActorComponents);

	// Use a material update context to signal any change made to materials to the render thread... Exclude the recreate render state flag as this step is already performed
	// by the FGlobalComponentRecreateRenderStateContext above.
	FMaterialUpdateContext MaterialUpdateContext(FMaterialUpdateContext::EOptions::Default & ~FMaterialUpdateContext::EOptions::RecreateRenderStates);
	for (UActorComponent* ActorComponent : ActorComponents)
	{
		if (UStaticMeshComponent* SMComponent = Cast<UStaticMeshComponent>(ActorComponent))
		{
			TArray<UMaterialInterface*> UsedMaterials;
			SMComponent->GetUsedMaterials(UsedMaterials);

			// Add all used materials to the material update context.
			for (UMaterialInterface* UsedMaterial : UsedMaterials)
			{
				if (UsedMaterial)
				{
					MaterialUpdateContext.AddMaterialInterface(UsedMaterial);
				}
			}
		}
	}

	FScopedSlowTask Progress(SelectedHLODActors.Num(), LOCTEXT("BuildingHLODsforActors", "Building HLODs for actors..."));
	Progress.MakeDialog(true);

	// Build HLODs
	for (int32 Index = 0; Index < SelectedHLODActors.Num(); ++Index)
	{
		Progress.EnterProgressFrame(1.0f, FText::Format(LOCTEXT("BuildingHLODsforActorProgress", "Building HLODs ({0}/{1})"), Index+1, SelectedHLODActors.Num()));
		if (Progress.ShouldCancel())
		{
			break;
		}

		AWorldPartitionHLOD* HLODActor = SelectedHLODActors[Index];
		HLODActor->BuildHLOD(true);
	};
	
	// Force refresh the UI so that any change to the selected actors are properly shown to the user
	GEditor->NoteSelectionChange();

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
