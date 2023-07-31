// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXFixturePatcher.h"

#include "DMXEditor.h"
#include "DMXEditorTabNames.h"
#include "DMXFixturePatchSharedData.h"
#include "DMXFixturePatchNode.h"
#include "SDMXPatchedUniverse.h"
#include "DragDrop/DMXEntityDragDropOp.h"
#include "DragDrop/DMXEntityFixturePatchDragDropOp.h"
#include "IO/DMXPortManager.h"
#include "Library/DMXEntityFixturePatch.h"
#include "Library/DMXLibrary.h"
#include "Widgets/SNullWidget.h"

#include "Styling/AppStyle.h"
#include "ScopedTransaction.h"
#include "SlateOptMacros.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"


#define LOCTEXT_NAMESPACE "SDMXFixturePatcher"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SDMXFixturePatcher::Construct(const FArguments& InArgs)
{

	DMXEditorPtr = InArgs._DMXEditor;
	if (!DMXEditorPtr.IsValid())
	{
		return;
	}

	SharedData = DMXEditorPtr.Pin()->GetFixturePatchSharedData();
		
	const FLinearColor BackgroundTint(0.6f, 0.6f, 0.6f, 1.0f);
	ChildSlot			
		[
			SNew(SBox)
			.HAlign(HAlign_Left)
			.ToolTipText(this, &SDMXFixturePatcher::GetTooltipText)
			[
				SNew(SVerticalBox)				

				// Settings area

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Fill)
				.AutoHeight()
				[

					SNew(SBorder)					
					.HAlign(HAlign_Fill)
					.BorderBackgroundColor(BackgroundTint)
					.BorderImage(FAppStyle::GetBrush("DetailsView.CategoryTop"))
					[
						SNew(SHorizontalBox)			

						+ SHorizontalBox::Slot()						
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(4.0f, 4.0f, 15.0f, 4.0f))
						[
							SNew(STextBlock)							
							.MinDesiredWidth(75.0f)
							.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
							.TextStyle(FAppStyle::Get(), "DetailsView.CategoryTextStyle")
							.IsEnabled(this, &SDMXFixturePatcher::IsUniverseSelectionEnabled)
							.Text(LOCTEXT("UniverseSelectorLabel", "Universe"))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(4.0f, 4.0f, 15.0f, 4.0f))
						[
							SNew(SBox)
							.MinDesiredWidth(210.0f)
							.MaxDesiredWidth(420.0f)
							[
								SNew(SSpinBox<int32>)								
								.SliderExponent(1000.0f)								
								.MinSliderValue(0)
								.MaxSliderValue(DMX_MAX_UNIVERSE - 1)
								.MinValue(0)
								.MaxValue(DMX_MAX_UNIVERSE - 1)
								.IsEnabled(this, &SDMXFixturePatcher::IsUniverseSelectionEnabled)
								.Value(this, &SDMXFixturePatcher::GetSelectedUniverse)
								.OnValueChanged(this, &SDMXFixturePatcher::SelectUniverse)
							]
						]
						
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(4.0f, 4.0f, 15.0f, 4.0f))						
						[
							SNew(SSeparator)
							.Orientation(EOrientation::Orient_Vertical)
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(4.0f, 4.0f, 15.0f, 4.0f))
						[
							SNew(STextBlock)
							.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
							.Text(LOCTEXT("UniverseDisplayAllText", "Show all patched Universes"))
						]

						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(4.0f, 4.0f, 15.0f, 4.0f))
						[
							SAssignNew(ShowAllUniversesCheckBox, SCheckBox)
							.IsChecked(false)
							.OnCheckStateChanged(this, &SDMXFixturePatcher::OnToggleDisplayAllUniverses)
						]
					]
				]

				// Patched Universes

				+ SVerticalBox::Slot()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Fill)
				[	
					SAssignNew(PatchedUniverseScrollBox, SScrollBox)					
					.Orientation(EOrientation::Orient_Vertical)	
					.ScrollBarAlwaysVisible(true)
					.ScrollBarThickness(FVector2D(5.f, 0.f))
				]
			]
		];

	// Bind to selection changes
	SharedData->OnFixturePatchSelectionChanged.AddSP(this, &SDMXFixturePatcher::OnFixturePatchSelectionChanged);
	SharedData->OnUniverseSelectionChanged.AddSP(this, &SDMXFixturePatcher::OnUniverseSelectionChanged);

	// If the selected universe has no patches, try to find one with patches instead
	UDMXLibrary* Library = GetDMXLibrary();
	check(Library);

	// Bind to object changes
	UDMXEntityFixtureType::GetOnFixtureTypeChanged().AddSP(this, &SDMXFixturePatcher::OnFixtureTypeChanged);
	Library->GetOnEntitiesAdded().AddSP(this, &SDMXFixturePatcher::OnEntitiesAddedOrRemoved);
	Library->GetOnEntitiesRemoved().AddSP(this, &SDMXFixturePatcher::OnEntitiesAddedOrRemoved);

	TArray<UDMXEntityFixturePatch*> Patches = Library->GetEntitiesTypeCast<UDMXEntityFixturePatch>();
	UDMXEntityFixturePatch** ExistingPatchPtr = Patches.FindByPredicate([&](UDMXEntityFixturePatch* Patch) {
		return Patch->GetUniverseID() == SharedData->GetSelectedUniverse();
		});
	if (!ExistingPatchPtr && Patches.Num() > 0)
	{
		SharedData->SelectUniverse(Patches[0]->GetUniverseID());
	}		

	ShowSelectedUniverse();

}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SDMXFixturePatcher::RefreshFromProperties()
{
	RefreshFixturePatchState = EDMXRefreshFixturePatcherState::RefreshFromProperties;
}

void SDMXFixturePatcher::RefreshFromLibrary()
{
	RefreshFixturePatchState = EDMXRefreshFixturePatcherState::RefreshFromLibrary;
}

void SDMXFixturePatcher::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Refresh if requested
	if (RefreshFixturePatchState != EDMXRefreshFixturePatcherState::NoRefreshRequested)
	{
		if (IsUniverseSelectionEnabled())
		{
			ShowSelectedUniverse();
		}
		else
		{
			ShowAllPatchedUniverses();
		}

		RefreshFixturePatchState = EDMXRefreshFixturePatcherState::NoRefreshRequested;
	}

	// Set Universe if requested
	if (UniverseToSetNextTick != INDEX_NONE)
	{
		SharedData->SelectUniverse(UniverseToSetNextTick);
		UniverseToSetNextTick = INDEX_NONE;
	}
}

FReply SDMXFixturePatcher::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return FReply::Unhandled();
	}

	if (TSharedPtr<FDMXEntityDragDropOperation> EntityDragDropOp = DragDropEvent.GetOperationAs<FDMXEntityDragDropOperation>())
	{
		return FReply::Handled().EndDragDrop();
	}

	return FReply::Unhandled();
}

void SDMXFixturePatcher::OnDragEnterChannel(int32 UniverseID, int32 ChannelID, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return;
	}

	if (TSharedPtr<FDMXEntityFixturePatchDragDropOperation> FixturePatchDragDropOp = DragDropEvent.GetOperationAs<FDMXEntityFixturePatchDragDropOperation>())
	{
		const TArray<TWeakObjectPtr<UDMXEntity>>& DraggedEntities = FixturePatchDragDropOp->GetDraggedEntities();

		if (DraggedEntities.Num() > 1)
		{
			FixturePatchDragDropOp->SetFeedbackMessageError(LOCTEXT("CannotDragDropMoreThanOnePatch", "Multi asset drag drop is not supported."));
		}

		TSharedPtr<FDMXFixturePatchNode> DraggedNode = GetDraggedNode(DraggedEntities);

		if (DraggedNode.IsValid())
		{
			if (UDMXEntityFixturePatch* FixturePatch = Cast<UDMXEntityFixturePatch>(DraggedEntities[0]))
			{
				const int32 ChannelSpan = FixturePatch->GetChannelSpan();
				const int32 NewStartingChannel = [this, ChannelID, &FixturePatchDragDropOp, ChannelSpan]()
				{
					int32 StartingChannel = ChannelID - FixturePatchDragDropOp->GetChannelOffset();
					return ClampStartingChannel(StartingChannel, ChannelSpan);
				}();


				// Patch the node but do not transact it (transact on drop instead)
				const TSharedPtr<SDMXPatchedUniverse>& Universe = PatchedUniversesByID.FindChecked(UniverseID);

				const bool bCreateTransaction = false;
				bool bPatchSuccess = Universe->Patch(DraggedNode, NewStartingChannel, bCreateTransaction);
			
				// If patching wasn't successful, try to move as close to the hovered node as possible
				if (!bPatchSuccess)
				{
					const bool bIsHoveredChannelInFrontOfPatch = [UniverseID, NewStartingChannel, FixturePatch]()
					{
						bool bUniverseInFront = UniverseID < FixturePatch->GetUniverseID();
						bool ChannelInFront = UniverseID == FixturePatch->GetUniverseID() && NewStartingChannel < FixturePatch->GetStartingChannel();

						return bUniverseInFront || ChannelInFront;
					}();

					if (bIsHoveredChannelInFrontOfPatch)
					{
						for (int32 Channel = FixturePatch->GetStartingChannel() - 1; Channel >= NewStartingChannel; Channel--)
						{
							// Try to patch as close as possible, but consider any approximation a success
							bool bCloserApproximation = Universe->Patch(DraggedNode, Channel, bCreateTransaction);
							bPatchSuccess = bPatchSuccess ? true : bCloserApproximation;
						}
					}
					else
					{
						for (int32 Channel = FixturePatch->GetStartingChannel() + 1; Channel <= NewStartingChannel; Channel++)
						{
							// Try to patch as close as possible, but consider any approximation a success
							bool bCloserApproximation = Universe->Patch(DraggedNode, Channel, bCreateTransaction);
							bPatchSuccess = bPatchSuccess ? true : bCloserApproximation;
						}
					}
				}

				if (bPatchSuccess)
				{
					TSharedRef<SWidget> DragDropDecorator = CreateDragDropDecorator(FixturePatch, NewStartingChannel);
					FixturePatchDragDropOp->SetCustomFeedbackWidget(DragDropDecorator);
				}
				else if (!DraggedNode->IsPatched())
				{
					if (NewStartingChannel + ChannelSpan > DMX_UNIVERSE_SIZE)
					{
						FixturePatchDragDropOp->SetFeedbackMessageError(LOCTEXT("CannotDragDropOnOccupiedChannels", "Channels range overflows max channels address (512)"));
					}
				}
			}
		}
	}
}

FReply SDMXFixturePatcher::OnDropOntoChannel(int32 UniverseID, int32 ChannelID, const FDragDropEvent& DragDropEvent)
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (!Operation.IsValid())
	{
		return FReply::Unhandled();
	}

	if (TSharedPtr<FDMXEntityFixturePatchDragDropOperation> FixturePatchDragDropOp = DragDropEvent.GetOperationAs<FDMXEntityFixturePatchDragDropOperation>())
	{
		const TArray<TWeakObjectPtr<UDMXEntity>>& DraggedEntities = FixturePatchDragDropOp->GetDraggedEntities();

		TSharedPtr<FDMXFixturePatchNode> DraggedNode = GetDraggedNode(DraggedEntities);

		if (DraggedNode.IsValid())
		{
			if (UDMXEntityFixturePatch* FixturePatch = Cast<UDMXEntityFixturePatch>(DraggedEntities[0]))
			{
				const int32 ChannelSpan = FixturePatch->GetChannelSpan();
				const int32 NewStartingChannel = [this, ChannelID, &FixturePatchDragDropOp, ChannelSpan]()
				{
					int32 StartingChannel = ChannelID - FixturePatchDragDropOp->GetChannelOffset();
					return ClampStartingChannel(StartingChannel, ChannelSpan);
				}();

				const TSharedPtr<SDMXPatchedUniverse>& Universe = PatchedUniversesByID.FindChecked(UniverseID);

				bool bCreateTransaction = true;
				if (Universe->Patch(DraggedNode, NewStartingChannel, bCreateTransaction))
				{
					return FReply::Handled().EndDragDrop();
				}
			}
		}
	}

	return FReply::Unhandled();
}

TSharedPtr<FDMXFixturePatchNode> SDMXFixturePatcher::GetDraggedNode(const TArray<TWeakObjectPtr<UDMXEntity>>& DraggedEntities)
{
	if (DraggedEntities.Num() == 1)
	{
		if (UDMXEntityFixturePatch* FixturePatch = Cast<UDMXEntityFixturePatch>(DraggedEntities[0]))
		{
			const TArray<TWeakObjectPtr<UDMXEntityFixturePatch>> FixturePatchArray = TArray<TWeakObjectPtr<UDMXEntityFixturePatch>>({ FixturePatch });
			TSharedPtr<FDMXFixturePatchNode> DraggedNode = FindPatchNode(FixturePatch);

			if (!DraggedNode.IsValid())
			{
				DraggedNode = FDMXFixturePatchNode::Create(DMXEditorPtr, FixturePatch);
			}

			return DraggedNode;
		}
	}

	return nullptr;
}

TSharedRef<SWidget> SDMXFixturePatcher::CreateDragDropDecorator(TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch, int32 ChannelID) const
{
	if (FixturePatch.IsValid())
	{
		int32 StartingChannel = ChannelID;
		int32 ChannelSpan = FixturePatch->GetChannelSpan();
		int32 EndingChannel = StartingChannel + FixturePatch->GetChannelSpan() - 1;

		FText PatchName = FText::Format(LOCTEXT("PatchName", "{0}"), FText::FromString(FixturePatch->GetDisplayName()));;								
		FText ChannelRangeName = FText::Format(LOCTEXT("ChannelRangeName", "Channel {0} - {1}"), StartingChannel, EndingChannel);

		return SNew(SBorder)
			.BorderImage(FAppStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			[
				SNew(SVerticalBox)				
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Fill)
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.Text(ChannelRangeName)
				]
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Bottom)
				[
					SNew(STextBlock)
					.Text(PatchName)
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.ColorAndOpacity(FLinearColor(0.9f, 0.9f, 0.9f, 1.0f))
				]
			];
	}

	return SNullWidget::NullWidget;
}

void SDMXFixturePatcher::PostUndo(bool bSuccess)
{
	UDMXLibrary* DMXLibrary = GetDMXLibrary();
	if(DMXLibrary)
	{
		DMXLibrary->Modify();
	}

	RefreshFromProperties();
}

void SDMXFixturePatcher::PostRedo(bool bSuccess)
{
	RefreshFromProperties();
}

TSharedPtr<FDMXFixturePatchNode> SDMXFixturePatcher::FindPatchNode(const TWeakObjectPtr<UDMXEntityFixturePatch> FixturePatch) const
{
	TSharedPtr<FDMXFixturePatchNode> ExistingNode;
	for (const TPair<int32, TSharedPtr<SDMXPatchedUniverse>>& UniverseByID : PatchedUniversesByID)
	{
		ExistingNode = UniverseByID.Value->FindPatchNode(FixturePatch);

		if (ExistingNode.IsValid())
		{
			return ExistingNode;
		}
	}

	return nullptr;
}

TSharedPtr<FDMXFixturePatchNode> SDMXFixturePatcher::FindPatchNodeOfType(UDMXEntityFixtureType* Type, const TSharedPtr<FDMXFixturePatchNode>& IgoredNode) const
{
	if (Type)
	{
		TSharedPtr<FDMXFixturePatchNode> ExistingNode;
		for (const TPair<int32, TSharedPtr<SDMXPatchedUniverse>>& UniverseByID : PatchedUniversesByID)
		{		
			return UniverseByID.Value->FindPatchNodeOfType(Type, IgoredNode);
		}
	}
	return nullptr;
}

void SDMXFixturePatcher::OnEntitiesAddedOrRemoved(UDMXLibrary* DMXLibrary, TArray<UDMXEntity*> Entities)
{
	if (DMXLibrary == GetDMXLibrary())
	{
		RefreshFromLibrary();
	}
}

void SDMXFixturePatcher::OnFixtureTypeChanged(const UDMXEntityFixtureType* FixtureType)
{
	RefreshFromLibrary();
}

void SDMXFixturePatcher::OnFixturePatchSelectionChanged()
{
	const int32 SelectedUniverse = SharedData->GetSelectedUniverse();
	if (PatchedUniversesByID.Contains(SelectedUniverse))
	{
		return;
	}

	const TSharedPtr<SDMXPatchedUniverse>* const UniverseWidgetPtr = PatchedUniversesByID.Find(SelectedUniverse);
	if (IsUniverseSelectionEnabled() || !UniverseWidgetPtr)
	{
		// Refresh if all universes are displayed, or if the Universe was empty previously and now needs to be displayed.
		ShowSelectedUniverse();
	}
	else
	{
		PatchedUniverseScrollBox->ScrollDescendantIntoView(*UniverseWidgetPtr);
	}
}

void SDMXFixturePatcher::OnUniverseSelectionChanged()
{
	if (IsUniverseSelectionEnabled())
	{
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SDMXFixturePatcher::ShowSelectedUniverse));
	}
	else 
	{
		constexpr bool bReconstructWidgets = false;
		GEditor->GetTimerManager()->SetTimerForNextTick(FTimerDelegate::CreateSP(this, &SDMXFixturePatcher::ShowAllPatchedUniverses, bReconstructWidgets));
	}
}

void SDMXFixturePatcher::SelectUniverse(int32 NewUniverseID)
{
	UniverseToSetNextTick = NewUniverseID;
}

int32 SDMXFixturePatcher::GetSelectedUniverse() const
{
	check(SharedData.IsValid());
	return UniverseToSetNextTick != INDEX_NONE ? UniverseToSetNextTick : SharedData->GetSelectedUniverse();
}

void SDMXFixturePatcher::ShowSelectedUniverse()
{
	PatchedUniverseScrollBox->ClearChildren();
	PatchedUniversesByID.Reset();
	
	const int32 SelectedUniverseID = GetSelectedUniverse();
	const TSharedRef<SDMXPatchedUniverse> NewPatchedUniverse =
		SNew(SDMXPatchedUniverse)
		.DMXEditor(DMXEditorPtr)
		.UniverseID(SelectedUniverseID)
		.OnDragEnterChannel(this, &SDMXFixturePatcher::OnDragEnterChannel)
		.OnDropOntoChannel(this, &SDMXFixturePatcher::OnDropOntoChannel);

	PatchedUniverseScrollBox->AddSlot()
		.Padding(FMargin(0.0f, 3.0f, 0.0f, 12.0f))			
		[
			NewPatchedUniverse
		];

	PatchedUniversesByID.Add(SelectedUniverseID, NewPatchedUniverse);
}

void SDMXFixturePatcher::ShowAllPatchedUniverses(bool bForceReconstructWidget)
{
	check(PatchedUniverseScrollBox.IsValid());

	if (bForceReconstructWidget)
	{
		PatchedUniverseScrollBox->ClearChildren();
		PatchedUniversesByID.Reset();
	}

	UDMXLibrary* Library = GetDMXLibrary();
	if (Library)
	{
		TArray<UDMXEntityFixturePatch*> FixturePatches = Library->GetEntitiesTypeCast<UDMXEntityFixturePatch>();

		// Sort by universe ID
		FixturePatches.Sort([](const UDMXEntityFixturePatch& Patch, const UDMXEntityFixturePatch& Other) {
			return Patch.GetUniverseID() < Other.GetUniverseID();
			});

		// Create widgets for all universe with patches		
		for(UDMXEntityFixturePatch* Patch : FixturePatches)
		{
			check(Patch);

			// Ignore patches that are not residing in a universe
			if (Patch->GetUniverseID() < 0)
			{
				continue;
			}

			if (!PatchedUniversesByID.Contains(Patch->GetUniverseID()))
			{
				AddUniverse(Patch->GetUniverseID());
			}
		}

		TMap<int32, TSharedPtr<SDMXPatchedUniverse>> CachedPatchedUniversesByID = PatchedUniversesByID;
		for (const TPair<int32, TSharedPtr<SDMXPatchedUniverse>>& UniverseByIDKvp : CachedPatchedUniversesByID)
		{
			check(UniverseByIDKvp.Value.IsValid());

			if (UniverseByIDKvp.Value->GetPatchedNodes().Num() == 0)
			{
				// Remove universe widgets without patches
				PatchedUniversesByID.Remove(UniverseByIDKvp.Key);
				PatchedUniverseScrollBox->RemoveSlot(UniverseByIDKvp.Value.ToSharedRef());
			}
		}

		// Show last patched universe +1 for convenience of adding patches to a new universe
		TArray<int32> UniverseIDs;
		PatchedUniversesByID.GetKeys(UniverseIDs);
		UniverseIDs.Sort([](int32 FirstID, int32 SecondID) {
			return FirstID > SecondID;
			});

		int32 LastPatchedUniverseID = 0;
		if (UniverseIDs.Num() > 0)
		{
			LastPatchedUniverseID = UniverseIDs[0];
		}
	
		int32 FirstEmptyUniverse = LastPatchedUniverseID + 1;
		AddUniverse(FirstEmptyUniverse);

		const int32 SelectedUniverse = SharedData->GetSelectedUniverse();
		if (PatchedUniversesByID.Contains(SelectedUniverse))
		{
			PatchedUniverseScrollBox->ScrollDescendantIntoView(PatchedUniversesByID[SelectedUniverse]);
		}
	}
}

void SDMXFixturePatcher::AddUniverse(int32 UniverseID)
{
	TSharedRef<SDMXPatchedUniverse> PatchedUniverse =
		SNew(SDMXPatchedUniverse)
		.DMXEditor(DMXEditorPtr)
		.UniverseID(UniverseID)
		.OnDragEnterChannel(this, &SDMXFixturePatcher::OnDragEnterChannel)
		.OnDropOntoChannel(this, &SDMXFixturePatcher::OnDropOntoChannel);

	PatchedUniverseScrollBox->AddSlot()
		.Padding(FMargin(0.0f, 3.0f, 0.0f, 12.0f))
		[
			PatchedUniverse
		];

	PatchedUniversesByID.Add(UniverseID, PatchedUniverse);
}

void SDMXFixturePatcher::OnToggleDisplayAllUniverses(ECheckBoxState CheckboxState)
{
	const int32 SelectedUniverse = SharedData->GetSelectedUniverse();

	switch (ShowAllUniversesCheckBox->GetCheckedState())
	{
	case ECheckBoxState::Checked:
		ShowAllPatchedUniverses();
		return;

	case ECheckBoxState::Unchecked:
		SelectUniverse(SelectedUniverse);
		ShowSelectedUniverse();
		return;

	case ECheckBoxState::Undetermined:
	default:
		checkNoEntry();
	}
}

bool SDMXFixturePatcher::IsUniverseSelectionEnabled() const
{
	check(ShowAllUniversesCheckBox.IsValid());

	switch (ShowAllUniversesCheckBox->GetCheckedState())
	{
	case ECheckBoxState::Checked:
		return false;

	case ECheckBoxState::Unchecked:
		return true;
		
	case ECheckBoxState::Undetermined:
	default:
		checkNoEntry();
	}

	return false;
}

bool SDMXFixturePatcher::HasAnyPorts() const
{
	UDMXLibrary* Library = GetDMXLibrary();
	if (Library)
	{
		const bool bHasAnyPorts =  Library->GetInputPorts().Num() > 0 || Library->GetOutputPorts().Num() > 0;

		return bHasAnyPorts;
	}
	return false;
}

FText SDMXFixturePatcher::GetTooltipText() const
{
	if (!HasAnyPorts())
	{
		return LOCTEXT("NoPorts", "No ports available. Please create add Ports to the DMX Library.");
	}

	return FText::GetEmpty();
}

int32 SDMXFixturePatcher::ClampStartingChannel(int32 StartingChannel, int32 ChannelSpan) const
{
	if (StartingChannel < 1)
	{
		StartingChannel = 1;
	}

	if (StartingChannel + ChannelSpan > DMX_UNIVERSE_SIZE)
	{
		StartingChannel = DMX_UNIVERSE_SIZE - ChannelSpan + 1;
	}

	return StartingChannel;
}

UDMXLibrary* SDMXFixturePatcher::GetDMXLibrary() const
{
	if (TSharedPtr<FDMXEditor> DMXEditor = DMXEditorPtr.Pin())
	{
		return DMXEditor->GetDMXLibrary();
	}
	return nullptr;
}

#undef LOCTEXT_NAMESPACE
