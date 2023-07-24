// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorView.h"

#include "DMXControlConsole.h"
#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorManager.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXEditorStyle.h"
#include "Commands/DMXControlConsoleEditorCommands.h"
#include "Customizations/DMXControlConsoleDataDetails.h"
#include "Customizations/DMXControlConsoleFaderGroupDetails.h"
#include "Library/DMXEntityReference.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Views/SDMXControlConsoleEditorFaderGroupRowView.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"
#include "Widgets/SDMXControlConsoleEditorAssetPicker.h"
#include "Widgets/SDMXControlConsoleEditorFixturePatchVerticalBox.h"

#include "Editor.h"
#include "IDetailsView.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "ScopedTransaction.h"
#include "TimerManager.h"
#include "Application/ThrottleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Commands/UICommandList.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Modules/ModuleManager.h"
#include "Layout/Visibility.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Text/STextBlock.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorView"

SDMXControlConsoleEditorView::~SDMXControlConsoleEditorView()
{
	FGlobalTabmanager::Get()->OnActiveTabChanged_Unsubscribe(OnActiveTabChangedDelegateHandle);
}

void SDMXControlConsoleEditorView::Construct(const FArguments& InArgs)
{
	RegisterCommands();

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	EditorConsoleModel->GetOnConsoleLoaded().AddSP(this, &SDMXControlConsoleEditorView::OnConsoleLoaded);
	EditorConsoleModel->GetOnConsoleSaved().AddSP(this, &SDMXControlConsoleEditorView::OnConsoleSaved);

	FDMXControlConsoleEditorManager& ControlConsoleManager = FDMXControlConsoleEditorManager::Get();
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = ControlConsoleManager.GetSelectionHandler();
	SelectionHandler->GetOnSelectionChanged().AddSP(this, &SDMXControlConsoleEditorView::RequestUpdateDetailsViews);

	OnActiveTabChangedDelegateHandle = FGlobalTabmanager::Get()->OnActiveTabChanged_Subscribe(FOnActiveTabChanged::FDelegate::CreateSP(this, &SDMXControlConsoleEditorView::OnActiveTabChanged));

	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.bHideSelectionTip = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Automatic;

	ControlConsoleDataDetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);
	FaderGroupsDetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);
	FadersDetailsView = PropertyEditor.CreateDetailView(DetailsViewArgs);

	FOnGetDetailCustomizationInstance ControlConsoleCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXControlConsoleDataDetails::MakeInstance);
	ControlConsoleDataDetailsView->RegisterInstancedCustomPropertyLayout(UDMXControlConsoleData::StaticClass(), ControlConsoleCustomizationInstance);
	ControlConsoleDataDetailsView->GetOnDisplayedPropertiesChanged().BindSP(this, &SDMXControlConsoleEditorView::UpdateFixturePatchRows);

	FOnGetDetailCustomizationInstance FaderGroupsCustomizationInstance = FOnGetDetailCustomizationInstance::CreateStatic(&FDMXControlConsoleFaderGroupDetails::MakeInstance);
	FaderGroupsDetailsView->RegisterInstancedCustomPropertyLayout(UDMXControlConsoleFaderGroup::StaticClass(), FaderGroupsCustomizationInstance);

	const TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Vertical);

	const TSharedRef<SScrollBar> HorizontalScrollBar = SNew(SScrollBar)
		.Orientation(Orient_Horizontal);

	ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar Section
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				GenerateToolbar()
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSeparator)
			]

			// Panel Section
			+ SVerticalBox::Slot()
			[
				SNew(SSplitter)
				.Orientation(Orient_Horizontal)
				.ResizeMode(ESplitterResizeMode::FixedSize)

				// DMX Control Console Section
				+ SSplitter::Slot()
				.Value(.62f)
				.MinSize(10.f)
				[

					SNew(SVerticalBox)

					+ SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						[
							SNew(SScrollBox)
							.ExternalScrollbar(HorizontalScrollBar)
							.Orientation(Orient_Horizontal)

							+ SScrollBox::Slot()
							[
								SNew(SBorder)
								.BorderImage(FAppStyle::GetBrush("NoBorder"))
								.Padding(10.f)
								[
									SNew(SScrollBox)
									.ExternalScrollbar(VerticalScrollBar)
									.Orientation(Orient_Vertical)

									+ SScrollBox::Slot()
									.HAlign(HAlign_Left)
									.VAlign(VAlign_Center)
									[
										SNew(SBox)
										.WidthOverride(50.f)
										.HeightOverride(50.f)
										.HAlign(HAlign_Center)
										.VAlign(VAlign_Center)
										[
											SNew(SDMXControlConsoleEditorAddButton)
											.OnClicked(this, &SDMXControlConsoleEditorView::OnAddFirstFaderGroup)
											.Visibility(TAttribute<EVisibility>(this, &SDMXControlConsoleEditorView::GetAddButtonVisibility))
										]
									]

									+ SScrollBox::Slot()
									[
										SAssignNew(FaderGroupRowsVerticalBox, SVerticalBox)
									]
								]
							]
						]
						
						// Horizontal ScrollBar slot
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							VerticalScrollBar
						]
					]
					
					// Vertical Scrollbar slot
					+SVerticalBox::Slot()
					.AutoHeight()
					[
						HorizontalScrollBar
					]
				]


				// Details View Section
				+ SSplitter::Slot()
				.Value(.38)
				.MinSize(10.f)
				[
					SNew(SScrollBox)
					.Orientation(Orient_Vertical)
					+ SScrollBox::Slot()
					[
						SNew(SVerticalBox)

						+SVerticalBox::Slot()
						.AutoHeight()
						[
							FadersDetailsView.ToSharedRef()
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SSeparator)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							FaderGroupsDetailsView.ToSharedRef()
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SSeparator)
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							ControlConsoleDataDetailsView.ToSharedRef()
						]

						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(FixturePatchVerticalBox, SDMXControlConsoleEditorFixturePatchVerticalBox)
						]
					]
				]
			]
		];
	
	ForceUpdateDetailsViews();

	FSlateApplication::Get().SetKeyboardFocus(AsShared());
}

UDMXControlConsoleData* SDMXControlConsoleEditorView::GetControlConsoleData() const
{ 
	return FDMXControlConsoleEditorManager::Get().GetEditorConsoleData();
}

void SDMXControlConsoleEditorView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	const TObjectPtr<UDMXControlConsoleData> ControlConsoleData = GetControlConsoleData();
	if (!ensureMsgf(ControlConsoleData, TEXT("Invalid DMX Control Console, can't update DMX Control Console state correctly.")))
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroupRow*> FaderGroupRows = ControlConsoleData->GetFaderGroupRows();

	if (FaderGroupRows.Num() == FaderGroupRowViews.Num())
	{
		return;
	}

	if (FaderGroupRows.Num() > FaderGroupRowViews.Num())
	{
		OnFaderGroupRowAdded();
	}
	else
	{
		OnFaderGroupRowRemoved();
	}
}

FReply SDMXControlConsoleEditorView::OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent)
{
	if (CommandList->ProcessCommandBindings(InKeyEvent))
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SDMXControlConsoleEditorView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	FSlateApplication::Get().SetKeyboardFocus(AsShared());

	return FReply::Handled();
}

void SDMXControlConsoleEditorView::RegisterCommands()
{
	CommandList = MakeShared<FUICommandList>();

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().CreateNewConsole,
		FExecuteAction::CreateUObject(EditorConsoleModel, &UDMXControlConsoleEditorModel::CreateNewConsole)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().SaveConsole,
		FExecuteAction::CreateUObject(EditorConsoleModel, &UDMXControlConsoleEditorModel::SaveConsole)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().SaveConsoleAs,
		FExecuteAction::CreateUObject(EditorConsoleModel, &UDMXControlConsoleEditorModel::SaveConsoleAs)
	);

	FDMXControlConsoleEditorManager& ControlConsoleManager = FDMXControlConsoleEditorManager::Get();
	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().SendDMX,
		FExecuteAction::CreateSP(ControlConsoleManager.AsShared(), &FDMXControlConsoleEditorManager::SendDMX),
		FCanExecuteAction::CreateSP(ControlConsoleManager.AsShared(), &FDMXControlConsoleEditorManager::CanSendDMX),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(ControlConsoleManager.AsShared(), &FDMXControlConsoleEditorManager::CanSendDMX)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().StopDMX,
		FExecuteAction::CreateSP(ControlConsoleManager.AsShared(), &FDMXControlConsoleEditorManager::StopDMX),
		FCanExecuteAction::CreateSP(ControlConsoleManager.AsShared(), &FDMXControlConsoleEditorManager::CanStopDMX),
		FIsActionChecked(),
		FIsActionButtonVisible::CreateSP(ControlConsoleManager.AsShared(), &FDMXControlConsoleEditorManager::CanStopDMX)
	);

	CommandList->MapAction
	(
		FDMXControlConsoleEditorCommands::Get().ClearAll,
		FExecuteAction::CreateSP(ControlConsoleManager.AsShared(), &FDMXControlConsoleEditorManager::ClearAll)
	);
}

TSharedRef<SWidget> SDMXControlConsoleEditorView::GenerateToolbar()
{
	ensureMsgf(CommandList.IsValid(), TEXT("Invalid command list for control console editor view."));

	FSlimHorizontalToolBarBuilder ToolbarBuilder = FSlimHorizontalToolBarBuilder(CommandList, FMultiBoxCustomization::None);

	ToolbarBuilder.BeginSection("AssetActions");
	{		
		ToolbarBuilder.AddToolBarButton(
			FDMXControlConsoleEditorCommands::Get().CreateNewConsole,
			NAME_None,
			FText::GetEmpty(),
			TAttribute<FText>(),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.PlusCircle")
		);

		ToolbarBuilder.AddWidget(SNew(SDMXControlConsoleEditorAssetPicker));
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("Clear");
	{
		ToolbarBuilder.AddToolBarButton(
			FDMXControlConsoleEditorCommands::Get().ClearAll,
			NAME_None, 
			TAttribute<FText>(),
			TAttribute<FText>(),
			FSlateIcon(),
			FName(TEXT("Clear All"))
		);
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("SendDMX");
	{
		ToolbarBuilder.AddToolBarButton(
			FDMXControlConsoleEditorCommands::Get().SendDMX,
			NAME_None, 
			TAttribute<FText>(), 
			TAttribute<FText>(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.PlayDMX"),
			FName(TEXT("Send DMX"))
		);

		ToolbarBuilder.AddToolBarButton(
			FDMXControlConsoleEditorCommands::Get().StopDMX,
			NAME_None, 
			TAttribute<FText>(), 
			TAttribute<FText>(),
			FSlateIcon(FDMXControlConsoleEditorStyle::Get().GetStyleSetName(), "DMXControlConsole.StopPlayingDMX"),
			FName(TEXT("Stop Sending DMX"))
		);
	}
	ToolbarBuilder.EndSection();

	ToolbarBuilder.BeginSection("Search");
	{
			TSharedRef<SSearchBox> GlobalSearchBox = 
				SNew(SSearchBox)
				.MinDesiredWidth(400.f)
				.OnTextChanged(this, &SDMXControlConsoleEditorView::OnSearchTextChanged)
				.ToolTipText(LOCTEXT("SearchBarTooltip", "Searches for Fader Name, Attributes, Fixture ID, Universe or Patch. Examples:\n\n* FaderName\n* Dimmer\n* Pan, Tilt\n* 1\n* 1.\n* 1.1\n* Universe 1\n* Uni 1-3\n* Uni 1, 3\n* Uni 1, 4-5'."));

		ToolbarBuilder.AddWidget(GlobalSearchBox);
	}
	ToolbarBuilder.EndSection();

	return ToolbarBuilder.MakeWidget();
}

void SDMXControlConsoleEditorView::RequestUpdateDetailsViews()
{
	if(!UpdateDetailsViewTimerHandle.IsValid())
	{
		UpdateDetailsViewTimerHandle = GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SDMXControlConsoleEditorView::ForceUpdateDetailsViews));
	}
}

void SDMXControlConsoleEditorView::ForceUpdateDetailsViews()
{
	UpdateDetailsViewTimerHandle.Invalidate();

	UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
	if (!ensureMsgf(ControlConsoleData, TEXT("Invalid DMX Control Console, can't update details view correctly.")))
	{
		return;
	}

	constexpr bool bForceRefresh = true;
	ControlConsoleDataDetailsView->SetObject(ControlConsoleData, bForceRefresh);

	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
	const TArray<TWeakObjectPtr<UObject>> SelectedFaderGroups = SelectionHandler->GetSelectedFaderGroups();
	FaderGroupsDetailsView->SetObjects(SelectedFaderGroups, bForceRefresh);

	const TArray<TWeakObjectPtr<UObject>> SelectedFaders = SelectionHandler->GetSelectedFaders();
	FadersDetailsView->SetObjects(SelectedFaders, bForceRefresh);
}

void SDMXControlConsoleEditorView::UpdateFixturePatchRows()
{
	if (!FixturePatchVerticalBox.IsValid())
	{
		return;
	}

	FixturePatchVerticalBox->UpdateFixturePatchRows();
}

void SDMXControlConsoleEditorView::OnFaderGroupRowAdded()
{
	const UDMXControlConsoleData* ControlConsole = GetControlConsoleData();
	if (!ensureMsgf(ControlConsole, TEXT("Invalid DMX Control Console, can't add new fader group row correctly.")))
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroupRow*> FaderGroupRows = ControlConsole->GetFaderGroupRows();
	for (UDMXControlConsoleFaderGroupRow* FaderGroupRow : FaderGroupRows)
	{
		if (!FaderGroupRow)
		{
			continue;
		}

		if (IsFaderGroupRowContained(FaderGroupRow))
		{
			continue;
		}

		AddFaderGroupRow(FaderGroupRow);
	}
}

void SDMXControlConsoleEditorView::AddFaderGroupRow(UDMXControlConsoleFaderGroupRow* FaderGroupRow)
{
	if (!ensureMsgf(FaderGroupRow, TEXT("Invalid fader group row, can't add new fader group row view correctly.")))
	{
		return;
	}

	if (!FaderGroupRowsVerticalBox.IsValid())
	{
		return;
	}

	const TSharedRef<SDMXControlConsoleEditorFaderGroupRowView> FaderGroupRowWidget = SNew(SDMXControlConsoleEditorFaderGroupRowView, FaderGroupRow);
	const int32 RowIndex = FaderGroupRow->GetRowIndex();
	if (ensureMsgf(RowIndex == 0 || FaderGroupRowViews.IsValidIndex(RowIndex - 1), TEXT("Unexpected, invalid row index when trying to add control console row.")))
	{
		FaderGroupRowViews.Insert(FaderGroupRowWidget, RowIndex);

		FaderGroupRowsVerticalBox->InsertSlot(RowIndex)
			.AutoHeight()
			.VAlign(VAlign_Top)
			.Padding(0.f, 8.f)
			[
				FaderGroupRowWidget
			];
	}
	else
	{
		FaderGroupRowViews.Add(FaderGroupRowWidget);

		FaderGroupRowsVerticalBox->AddSlot()
			.AutoHeight()
			.VAlign(VAlign_Top)
			.Padding(0.f, 8.f)
			[
				FaderGroupRowWidget
			];
	}
}

void SDMXControlConsoleEditorView::OnFaderGroupRowRemoved()
{
	const UDMXControlConsoleData* ControlConsole = GetControlConsoleData();
	if (!ensureMsgf(ControlConsole, TEXT("Invalid DMX Control Console, can't delete fader group row correctly.")))
	{
		return;
	}

	const TArray<UDMXControlConsoleFaderGroupRow*> FaderGroupRows = ControlConsole->GetFaderGroupRows();

	TArray<TWeakPtr<SDMXControlConsoleEditorFaderGroupRowView>>FaderGroupRowViewsToRemove;
	for (TWeakPtr<SDMXControlConsoleEditorFaderGroupRowView>& FaderGroupRowView : FaderGroupRowViews)
	{
		if (!FaderGroupRowView.IsValid())
		{
			continue;
		}

		const UDMXControlConsoleFaderGroupRow* FaderGroupRow = FaderGroupRowView.Pin()->GetFaderGropuRow();
		if (!FaderGroupRow || !FaderGroupRows.Contains(FaderGroupRow))
		{
			FaderGroupRowsVerticalBox->RemoveSlot(FaderGroupRowView.Pin().ToSharedRef());
			FaderGroupRowViewsToRemove.Add(FaderGroupRowView);
		}
	}

	FaderGroupRowViews.RemoveAll([&FaderGroupRowViewsToRemove](const TWeakPtr<SDMXControlConsoleEditorFaderGroupRowView> FaderGroupRowView)
		{
			return !FaderGroupRowView.IsValid() || FaderGroupRowViewsToRemove.Contains(FaderGroupRowView);
		});
}

bool SDMXControlConsoleEditorView::IsFaderGroupRowContained(UDMXControlConsoleFaderGroupRow* FaderGroupRow)
{
	const TWeakObjectPtr<UDMXControlConsoleFaderGroupRow> FaderGroupRowWeakPtr = FaderGroupRow;

	auto IsContainedLambda = [FaderGroupRowWeakPtr](const TWeakPtr<SDMXControlConsoleEditorFaderGroupRowView> FaderGroupRowView)
	{
		if (!FaderGroupRowView.IsValid())
		{
			return false;
		}

		const TWeakObjectPtr<UDMXControlConsoleFaderGroupRow> FaderGroupRow = FaderGroupRowView.Pin()->GetFaderGropuRow();
		if (!FaderGroupRow.IsValid())
		{
			return false;
		}

		return FaderGroupRow == FaderGroupRowWeakPtr;
	};

	return FaderGroupRowViews.ContainsByPredicate(IsContainedLambda);
}

void SDMXControlConsoleEditorView::OnSearchTextChanged(const FText& SearchText)
{
	for (TWeakPtr<SDMXControlConsoleEditorFaderGroupRowView> WeakFaderGroupRowView : FaderGroupRowViews)
	{
		if (const TSharedPtr<SDMXControlConsoleEditorFaderGroupRowView>& FaderGroupRowView = WeakFaderGroupRowView.Pin())
		{
			FaderGroupRowView->ApplyGlobalFilter(SearchText.ToString());
		}
	}
}

FReply SDMXControlConsoleEditorView::OnAddFirstFaderGroup()
{
	const TObjectPtr<UDMXControlConsoleData> ControlConsole = GetControlConsoleData();
	if (!ensureMsgf(ControlConsole, TEXT("Invalid DMX Control Console, can't add fader group correctly.")))
	{
		return FReply::Unhandled();
	}

	const FScopedTransaction AddFaderGroupTransaction(LOCTEXT("AddFaderGroupTransaction", "Add Fader Group"));
	ControlConsole->PreEditChange(nullptr);

	ControlConsole->AddFaderGroupRow(0);

	ControlConsole->PostEditChange();
	return FReply::Handled();
}

void SDMXControlConsoleEditorView::OnBrowseToAssetClicked()
{
	if (UDMXControlConsole* EditorConsole = FDMXControlConsoleEditorManager::Get().GetEditorConsole())
	{
		TArray<UObject*> BrowseToObjects{ EditorConsole };
		GEditor->SyncBrowserToObjects(BrowseToObjects);
	}
}

void SDMXControlConsoleEditorView::OnConsoleLoaded()
{
	RequestUpdateDetailsViews();
	OnFaderGroupRowAdded();
	OnFaderGroupRowRemoved();

	FSlateApplication::Get().SetKeyboardFocus(AsShared());
}

void SDMXControlConsoleEditorView::OnConsoleSaved()
{
	FSlateApplication::Get().SetKeyboardFocus(AsShared());
}

void SDMXControlConsoleEditorView::OnActiveTabChanged(TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated)
{
	if (IsWidgetInTab(PreviouslyActive, AsShared()))
	{
		const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = FDMXControlConsoleEditorManager::Get().GetSelectionHandler();
		SelectionHandler->ClearSelection();
	}

	if (!FadersDetailsView.IsValid())
	{
		return;
	}

	bool bDisableThrottle = false; 
	if (IsWidgetInTab(PreviouslyActive, FadersDetailsView))
	{
		FSlateThrottleManager::Get().DisableThrottle(bDisableThrottle);
	}

	if (IsWidgetInTab(NewlyActivated, FadersDetailsView))
	{
		bDisableThrottle = true;
		FSlateThrottleManager::Get().DisableThrottle(bDisableThrottle);
	}
}

bool SDMXControlConsoleEditorView::IsWidgetInTab(TSharedPtr<SDockTab> InDockTab, TSharedPtr<SWidget> InWidget) const
{
	if (InDockTab.IsValid())
	{
		// Tab content that should be a parent of this widget on some level
		TSharedPtr<SWidget> TabContent = InDockTab->GetContent();
		// Current parent being checked against
		TSharedPtr<SWidget> CurrentParent = InWidget;

		while (CurrentParent.IsValid())
		{
			if (CurrentParent == TabContent)
			{
				return true;
			}
			CurrentParent = CurrentParent->GetParentWidget();
		}

		// reached top widget (parent is invalid) and none was the tab
		return false;
	}

	return false;
}

EVisibility SDMXControlConsoleEditorView::GetAddButtonVisibility() const
{
	UDMXControlConsoleData* ControlConsoleData = GetControlConsoleData();
	if (!ControlConsoleData)
	{
		return EVisibility::Collapsed;
	}

	return ControlConsoleData->GetFaderGroupRows().IsEmpty() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
