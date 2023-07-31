// Copyright Epic Games, Inc. All Rights Reserved. 
#include "InterchangeGraphInspectorPipeline.h"

#include "CoreMinimal.h"

#include "Framework/Application/SlateApplication.h"
#include "InterchangeSourceData.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/Paths.h"
#include "Nodes/InterchangeBaseNodeContainer.h"
#include "SInterchangeGraphInspectorWindow.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWindow.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeGraphInspectorPipeline)

void UInterchangeGraphInspectorPipeline::ExecutePreImportPipeline(UInterchangeBaseNodeContainer* BaseNodeContainer, const TArray<UInterchangeSourceData*>& SourceDatas)
{
	//Create and show the graph inspector UI dialog
	TSharedPtr<SWindow> ParentWindow;
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}
	TSharedRef<SWindow> Window = SNew(SWindow)
		.ClientSize(FVector2D(800.f, 650.f))
		.Title(NSLOCTEXT("Interchange", "GraphInspectorTitle", "Interchange node graph inspector"));
	TSharedPtr<SInterchangeGraphInspectorWindow> InterchangeGraphInspectorWindow;

	Window->SetContent
	(
		SAssignNew(InterchangeGraphInspectorWindow, SInterchangeGraphInspectorWindow)
		.InterchangeBaseNodeContainer(BaseNodeContainer)
		.OwnerWindow(Window)
	);

	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);
}

