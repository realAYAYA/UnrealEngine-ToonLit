// Copyright Epic Games, Inc. All Rights Reserved.

#include "DataprepEditor.h"

#include "Widgets/DataprepAssetView.h"
#include "DataprepContentConsumer.h"
#include "DataprepContentProducer.h"
#include "DataprepEditorActions.h"
#include "DataprepEditorModule.h"
#include "DataprepEditorStyle.h"
#include "Widgets/SAssetsPreviewWidget.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "DesktopPlatformModule.h"
#include "Dialogs/DlgPickPath.h"
#include "EditorDirectories.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Engine/Blueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "K2Node_AddComponent.h"
#include "K2Node_CustomEvent.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Materials/MaterialInstance.h"
#include "Misc/Attribute.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "SceneOutlinerModule.h"
#include "ScopedTransaction.h"
#include "Toolkits/IToolkit.h"
#include "UnrealEdGlobals.h"

#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/STextComboBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "DataprepEditor"

TSharedRef<SDockTab> FDataprepEditor::SpawnTabDetails(const FSpawnTabArgs & Args)
{
	check(Args.GetTabId() == DetailsTabId);

	return SAssignNew(DetailsTabPtr, SDockTab)
		.Label(LOCTEXT("DataprepEditor_DetailsTab_Title", "Details"))
		[
			SNullWidget::NullWidget
		];
}

void FDataprepEditor::CreateDetailsViews()
{
	DetailsView = SNew( SGraphNodeDetailsWidget );
}

void FDataprepEditor::OnPipelineEditorSelectionChanged(const TSet<UObject*>& SelectedNodes)
{
	if (DetailsView.IsValid())
	{
		SetDetailsObjects(SelectedNodes, true);
	}
}

void FDataprepEditor::SetDetailsObjects(const TSet<UObject*>& Objects, bool bCanEditProperties)
{
	if (DetailsView.IsValid())
	{
		DetailsView->ShowDetailsObjects(Objects.Array());
		DetailsView->SetCanEditProperties(bCanEditProperties);

		if ( DetailsTabPtr.IsValid() && DetailsTabPtr.Pin()->GetContent() != DetailsView )
		{
			DetailsTabPtr.Pin()->SetContent( DetailsView.ToSharedRef() );
		}
	}
}

void FDataprepEditor::TryInvokingDetailsTab(bool bFlash)
{
	if (TabManager->HasTabSpawner(FDataprepEditor::DetailsTabId))
	{
		TSharedPtr<SDockTab> DataprepTab = FGlobalTabmanager::Get()->GetMajorTabForTabManager(TabManager.ToSharedRef());

		// We don't want to force this tab into existence when the data prep editor isn't in the foreground and actively
		// being interacted with.  So we make sure the window it's in is focused and the tab is in the foreground.
		if (DataprepTab.IsValid() && DataprepTab->IsForeground())
		{
			TSharedPtr<SWindow> ParentWindow = DataprepTab->GetParentWindow();
			if (ParentWindow.IsValid() && ParentWindow->HasFocusedDescendants())
			{
				if (!DetailsTabPtr.IsValid())
				{
					// Show the details panel if it doesn't exist.
					TabManager->TryInvokeTab(FDataprepEditor::DetailsTabId);
				}
			}
		}
	}

	if (bFlash)
	{
		if (DetailsTabPtr.IsValid())
		{
			TSharedPtr<SDockTab> DetailsTab = DetailsTabPtr.Pin();
			DetailsTab->FlashTab();
		}
	}
}

#undef LOCTEXT_NAMESPACE
