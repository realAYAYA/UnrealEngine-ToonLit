// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosVDParticleActorCustomization.h"

#include "ChaosVDModule.h"
#include "ChaosVDParticleActor.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IStructureDetailsView.h"
#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "DetailsCustomizations/ChaosVDDetailsCustomizationUtils.h"

#define LOCTEXT_NAMESPACE "ChaosVisualDebugger"

FChaosVDParticleActorCustomization::FChaosVDParticleActorCustomization()
{
	AllowedCategories.Add(FChaosVDParticleActorCustomization::ChaosVDCategoryName);
	AllowedCategories.Add(FChaosVDParticleActorCustomization::ChaosVDVisualizationCategoryName);
}

FChaosVDParticleActorCustomization::~FChaosVDParticleActorCustomization()
{
	if (CurrentObservedActor.Get())
	{
		CurrentObservedActor->OnParticleDataUpdated().Unbind();
	}
}

TSharedRef<IDetailCustomization> FChaosVDParticleActorCustomization::MakeInstance()
{
	return MakeShareable( new FChaosVDParticleActorCustomization );
}

void FChaosVDParticleActorCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.EditCategory(ChaosVDVisualizationCategoryName, FText::GetEmpty(), ECategoryPriority::Important);

	FChaosVDDetailsCustomizationUtils::HideAllCategories(DetailBuilder, AllowedCategories);

	// We keep the particle data we need to visualize as a shared ptr because copying it each frame we advance/rewind to to an struct that lives in the particle actor it is not cheap.
	// Having a struct details view to which we set that pointer data each time the data in the particle is updated (meaning we assigned another ptr from the recording)
	// seems to be more expensive because it has to rebuild the entire layout from scratch.
	// So a middle ground I found is to have a Particle Data struct in this customization instance, which we add as external property. Then each time the particle data is updated we copy the data over.
	// This allow us to only perform the copy just for the particle that is being inspected and not every particle updated in that frame.

	IDetailCategoryBuilder& CVDMainCategoryBuilder = DetailBuilder.EditCategory(ChaosVDCategoryName).InitiallyCollapsed(false);

	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	if (SelectedObjects.Num() > 0)
	{
		//TODO: Add support for multi-selection.
		if (!ensure(SelectedObjects.Num() == 1))
		{
			UE_LOG(LogChaosVDEditor, Warning, TEXT("[%s] [%d] objects were selectioned but this customization panel only support single object selection."), ANSI_TO_TCHAR(__FUNCTION__), SelectedObjects.Num())
		}

		if (AChaosVDParticleActor* CurrentActor = CurrentObservedActor.Get())
		{
			CurrentParticleDataCopy = FChaosVDParticleDataWrapper();
			CurrentActor->OnParticleDataUpdated().Unbind();
			CurrentActor = nullptr;
		}
		
		if (AChaosVDParticleActor* ParticleActor = Cast<AChaosVDParticleActor>(SelectedObjects[0]))
		{
			CurrentObservedActor = ParticleActor;
			CurrentObservedActor->OnParticleDataUpdated().BindRaw(this, &FChaosVDParticleActorCustomization::HandleParticleDataUpdated);

			HandleParticleDataUpdated();

			const TSharedPtr<FStructOnScope> ParticleDataView = MakeShared<FStructOnScope>(FChaosVDParticleDataWrapper::StaticStruct(), reinterpret_cast<uint8*>(&CurrentParticleDataCopy));
			TArray<TSharedPtr<IPropertyHandle>> Handles = CVDMainCategoryBuilder.AddAllExternalStructureProperties(ParticleDataView.ToSharedRef(), EPropertyLocation::Default, nullptr);

			FChaosVDDetailsCustomizationUtils::HideInvalidCVDDataWrapperProperties(Handles);
		}
	}
}

void FChaosVDParticleActorCustomization::HandleParticleDataUpdated()
{	
	if (const FChaosVDParticleDataWrapper* ParticleDataPtr = CurrentObservedActor.Get() ? CurrentObservedActor->GetParticleData() : nullptr)
	{
		CurrentParticleDataCopy = *ParticleDataPtr;	
	}
	else
	{
		CurrentParticleDataCopy = FChaosVDParticleDataWrapper();
	}
}

#undef LOCTEXT_NAMESPACE
