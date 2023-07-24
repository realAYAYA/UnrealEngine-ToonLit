// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "CineSplineComponentVisualizer.h"
#include "CineSplineComponent.h"
#include "CineCameraRigRail.h"
#include "CineCameraRigRailDetails.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "PropertyEditorModule.h"
#include "PropertyEditorDelegates.h"

#define LOCTEXT_NAMESPACE "CineCameraRigsEditor"

class FCineCameraRigsEditorModule : public IModuleInterface
{
public:

	virtual void StartupModule() override
	{
		CineSplineComponentName = UCineSplineComponent::StaticClass()->GetFName();

		if (GUnrealEd)
		{
			TSharedPtr<FCineSplineComponentVisualizer> Visualizer = MakeShared<FCineSplineComponentVisualizer>();
			GUnrealEd->RegisterComponentVisualizer(CineSplineComponentName, Visualizer);
			Visualizer->OnRegister();
			RegisterCustomizations();
		}
	}

	virtual void ShutdownModule() override
	{
		if (GUnrealEd)
		{
			GUnrealEd->UnregisterComponentVisualizer(CineSplineComponentName);
			UnregisterCustomizations();
		}
	}

private:
	FName CineSplineComponentName;


	void RegisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.RegisterCustomClassLayout(ACineCameraRigRail::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FCineCameraRigRailDetails::MakeInstance));
	}
	void UnregisterCustomizations()
	{
		FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
		PropertyModule.UnregisterCustomPropertyTypeLayout(ACineCameraRigRail::StaticClass()->GetFName());
	}
};


#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FCineCameraRigsEditorModule, CineCameraRigsEditor)
