// Copyright Epic Games, Inc. All Rights Reserved.

#include "UI/SRenderGrid.h"
#include "IRenderGridEditor.h"
#include "RenderGrid/RenderGrid.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "SlateOptMacros.h"

#define LOCTEXT_NAMESPACE "SRenderGrid"


void UE::RenderGrid::Private::SRenderGrid::Tick(const FGeometry&, const double, const float)
{
	if (DetailsView.IsValid())
	{
		if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
		{
			if (URenderGrid* Grid = BlueprintEditor->GetInstance())
			{
				if (!IsValid(Grid) || BlueprintEditor->IsBatchRendering())
				{
					Grid = nullptr;
				}
				if (DetailsViewRenderGridWeakPtr != Grid)
				{
					DetailsViewRenderGridWeakPtr = Grid;
					DetailsView->SetObject(Grid);
				}
			}
		}
	}
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void UE::RenderGrid::Private::SRenderGrid::Construct(const FArguments& InArgs, TSharedPtr<IRenderGridEditor> InBlueprintEditor)
{
	BlueprintEditorWeakPtr = InBlueprintEditor;
	DetailsViewRenderGridWeakPtr = InBlueprintEditor->GetInstance();

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	DetailsViewArgs.NotifyHook = this;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
	DetailsView->SetObject(DetailsViewRenderGridWeakPtr.Get());

	ChildSlot
	[
		DetailsView->AsShared()
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void UE::RenderGrid::Private::SRenderGrid::NotifyPreChange(FEditPropertyChain* PropertyAboutToChange) {}

void UE::RenderGrid::Private::SRenderGrid::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FEditPropertyChain* PropertyThatChanged)
{
	if (const TSharedPtr<IRenderGridEditor> BlueprintEditor = BlueprintEditorWeakPtr.Pin())
	{
		BlueprintEditor->MarkAsModified();
		BlueprintEditor->OnRenderGridChanged().Broadcast();
	}
}


#undef LOCTEXT_NAMESPACE
