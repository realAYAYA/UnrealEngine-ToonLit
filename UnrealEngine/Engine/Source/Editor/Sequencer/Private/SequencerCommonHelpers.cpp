// Copyright Epic Games, Inc. All Rights Reserved.

#include "SequencerCommonHelpers.h"

#include "FrameNumberDetailsCustomization.h"
#include "IDetailsView.h"
#include "ISequencerSection.h"
#include "IStructureDetailsView.h"
#include "MVVM/Extensions/ITrackAreaExtension.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/ViewModels/ObjectBindingModel.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/VirtualTrackArea.h"
#include "MVVM/Views/ITrackAreaHotspot.h"
#include "MVVM/Selection/Selection.h"
#include "Modules/ModuleManager.h"
#include "MovieScene.h"
#include "MovieSceneSectionDetailsCustomization.h"
#include "MovieSceneSequence.h"
#include "PropertyEditorModule.h"
#include "PropertyPermissionList.h"
#include "SSequencer.h"
#include "Sequencer.h"
#include "SequencerContextMenus.h"
#include "SequencerSelectedKey.h"
#include "Styling/CoreStyle.h"

void SequencerHelpers::GetAllChannels(TSharedPtr<FViewModel> DataModel, TSet<TSharedPtr<UE::Sequencer::FChannelModel>>& Channels)
{
	using namespace UE::Sequencer;

	if (DataModel)
	{
		constexpr bool bIncludeThis = true;
		for (const FViewModelPtr& Child : DataModel->GetDescendants(bIncludeThis))
		{
			if (TSharedPtr<ITrackAreaExtension> TrackArea = Child.ImplicitCast())
			{
				for (const FViewModelPtr& TrackAreaModel : TrackArea->GetTrackAreaModelList())
				{
					if (TViewModelPtr<FChannelModel> Channel = TrackAreaModel.ImplicitCast())
					{
						Channels.Add(Channel);
					}
				}
			}
			else if (TSharedPtr<FChannelModel> Channel = Child.ImplicitCast())
			{
				Channels.Add(Channel);
			}
		}
	}
}

void SequencerHelpers::GetAllKeyAreas(TSharedPtr<FViewModel> DataModel, TSet<TSharedPtr<IKeyArea>>& Channels)
{
	using namespace UE::Sequencer;

	if (DataModel)
	{
		constexpr bool bIncludeThis = true;
		for (const FViewModelPtr& Child : DataModel->GetDescendants(bIncludeThis))
		{
			if (TSharedPtr<ITrackAreaExtension> TrackArea = Child.ImplicitCast())
			{
				for (const FViewModelPtr& TrackAreaModel : TrackArea->GetTrackAreaModelList())
				{
					if (TViewModelPtr<FChannelModel> Channel = TrackAreaModel.ImplicitCast())
					{
						Channels.Add(Channel->GetKeyArea());
					}
				}
			}
			else if (TSharedPtr<FChannelModel> Channel = Child.ImplicitCast())
			{
				Channels.Add(Channel->GetKeyArea());
			}
		}
	}
}

void SequencerHelpers::GetAllSections(TSharedPtr<FViewModel> DataModel, TSet<TWeakObjectPtr<UMovieSceneSection>>& Sections)
{
	using namespace UE::Sequencer;

	if (DataModel)
	{
		constexpr bool bIncludeThis = true;
		for (TSharedPtr<FSectionModel> Section : TParentFirstChildIterator<FSectionModel>(DataModel, bIncludeThis))
		{
			Sections.Add(Section->GetSection());
		}
	}
}

int32 SequencerHelpers::GetSectionFromTime(TArrayView<UMovieSceneSection* const> InSections, FFrameNumber Time)
{
	FFrameNumber ClosestLowerBound = TNumericLimits<int32>::Max();
	TOptional<int32> MaxOverlapPriority, MaxProximalPriority;

	int32 MostRelevantIndex = INDEX_NONE;

	for (int32 Index = 0; Index < InSections.Num(); ++Index)
	{
		const UMovieSceneSection* Section = InSections[Index];
		if (Section)
		{
			const int32 ThisSectionPriority = Section->GetOverlapPriority();
			TRange<FFrameNumber> SectionRange = Section->GetRange();

			// If the specified time is within the section bounds
			if (SectionRange.Contains(Time))
			{
				if (ThisSectionPriority >= MaxOverlapPriority.Get(ThisSectionPriority))
				{
					MaxOverlapPriority = ThisSectionPriority;
					MostRelevantIndex = Index;
				}
			}
			// Check for nearby sections if there is nothing overlapping
			else if (!MaxOverlapPriority.IsSet() && SectionRange.HasLowerBound())
			{
				const FFrameNumber LowerBoundValue = SectionRange.GetLowerBoundValue();
				// If this section exists beyond the current time, we can choose it if its closest to the time
				if (LowerBoundValue >= Time)
				{
					if (
						(LowerBoundValue < ClosestLowerBound) ||
						(LowerBoundValue == ClosestLowerBound && ThisSectionPriority >= MaxProximalPriority.Get(ThisSectionPriority))
						)
					{
						MostRelevantIndex = Index;
						ClosestLowerBound = LowerBoundValue;
						MaxProximalPriority = ThisSectionPriority;
					}
				}
			}
		}
	}

	// If we didn't find one, use the last one (or return -1)
	if (MostRelevantIndex == -1)
	{
		MostRelevantIndex = InSections.Num() - 1;
	}

	return MostRelevantIndex;
}

void SequencerHelpers::GetDescendantNodes(TSharedRef<FViewModel> DataModel, TSet<TSharedRef<FViewModel>>& Nodes)
{
	using namespace UE::Sequencer;

	for (TSharedPtr<FViewModel> ChildNode : DataModel->GetChildren())
	{
		if (ChildNode->IsA<IOutlinerExtension>())
		{
			Nodes.Add(ChildNode.ToSharedRef());
		}

		GetDescendantNodes(ChildNode.ToSharedRef(), Nodes);
	}
}

bool IsSectionSelectedInNode(FSequencer& Sequencer, TSharedPtr<UE::Sequencer::FViewModel> InModel)
{
	using namespace UE::Sequencer;

	const FTrackAreaSelection& Selection = Sequencer.GetViewModel()->GetSelection()->TrackArea;

	if (ITrackAreaExtension* TrackArea = InModel->CastThis<ITrackAreaExtension>())
	{
		for (TSharedPtr<FViewModel> TrackAreaModel : TrackArea->GetTrackAreaModelList())
		{
			constexpr bool bIncludeThis = true;
			for (TSharedPtr<FSectionModel> Section : TParentFirstChildIterator<FSectionModel>(TrackAreaModel, bIncludeThis))
			{
				if (Selection.IsSelected(Section))
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool AreKeysSelectedInNode(FSequencer& Sequencer, TSharedPtr<UE::Sequencer::FViewModel> InModel)
{
	using namespace UE::Sequencer;

	TSet<TSharedPtr<FChannelModel>> Channels;
	SequencerHelpers::GetAllChannels(InModel, Channels);

	const FKeySelection& Selection = Sequencer.GetViewModel()->GetSelection()->KeySelection;

	for (FKeyHandle Key : Selection)
	{
		TSharedPtr<FChannelModel> Channel = Selection.GetModelForKey(Key);
		if (Channels.Contains(Channel))
		{
			return true;
		}
	}

	return false;
}


void SequencerHelpers::PerformDefaultSelection(FSequencer& Sequencer, const FPointerEvent& MouseEvent)
{
	using namespace UE::Sequencer;

	// @todo: selection in transactions
	FHotspotSelectionManager SelectionManager(&MouseEvent, &Sequencer);
	TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = Sequencer.GetViewModel()->CastThisShared<FSequencerEditorViewModel>();
	TSharedPtr<ITrackAreaHotspot> Hotspot = SequencerViewModel->GetHotspot();
	if (TSharedPtr<IMouseHandlerHotspot> MouseHandler = HotspotCast<IMouseHandlerHotspot>(Hotspot))
	{
		MouseHandler->HandleMouseSelection(SelectionManager);
	}
	else
	{
		// No hotspot so clear the selection if we're not adding to it
		SelectionManager.ConditionallyClearSelection();
	}
}

TSharedPtr<SWidget> SequencerHelpers::SummonContextMenu(FSequencer& Sequencer, const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	using namespace UE::Sequencer;

	// @todo sequencer replace with UI Commands instead of faking it

	// Attempt to paste into either the current node selection, or the clicked on track
	TSharedRef<SSequencer> SequencerWidget = StaticCastSharedRef<SSequencer>(Sequencer.GetSequencerWidget());
	const FFrameNumber PasteAtTime = Sequencer.GetLocalTime().Time.FrameNumber;

	// The menu are generated through reflection and sometime the API exposes some recursivity (think about a Widget returning it parent which is also a Widget). Just by reflection
	// it is not possible to determine when the root object is reached. It needs a kind of simulation which is not implemented. Also, even if the recursivity was correctly handled, the possible
	// permutations tend to grow exponentially. Until a clever solution is found, the simple approach is to disable recursively searching those menus. User can still search the current one though.
	// See UE-131257
	const bool bInRecursivelySearchable = false;

	const bool bShouldCloseWindowAfterMenuSelection = true;

	TSharedPtr<FExtender> MenuExtender = MakeShared<FExtender>();

	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, Sequencer.GetCommandBindings(), MenuExtender, false, &FCoreStyle::Get(), true, NAME_None, bInRecursivelySearchable);

	TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = Sequencer.GetViewModel()->CastThisShared<FSequencerEditorViewModel>();
	TSharedPtr<ITrackAreaHotspot> Hotspot = SequencerViewModel->GetHotspot();

	if (Hotspot.IsValid() && Hotspot->PopulateContextMenu(MenuBuilder, MenuExtender, PasteAtTime))
	{
		return MenuBuilder.MakeWidget();
	}
	else if (Sequencer.GetClipboardStack().Num() != 0)
	{
		TSharedPtr<FPasteContextMenu> PasteMenu = FPasteContextMenu::CreateMenu(Sequencer, SequencerWidget->GeneratePasteArgs(PasteAtTime));
		if (PasteMenu.IsValid() && PasteMenu->IsValidPaste())
		{
			PasteMenu->PopulateMenu(MenuBuilder, MenuExtender);

			return MenuBuilder.MakeWidget();
		}
	}

	return nullptr;
}

/** A widget which wraps the section details view which is an FNotifyHook which is used to forward
	changes to the section to sequencer. */
class SSectionDetailsNotifyHookWrapper : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SSectionDetailsNotifyHookWrapper) {}
	SLATE_END_ARGS();

	void Construct(FArguments InArgs) { }

	void SetDetailsAndSequencer(TSharedRef<SWidget> InDetailsPanel, TSharedRef<ISequencer> InSequencer)
	{
		ChildSlot
		[
			InDetailsPanel
		];
		Sequencer = InSequencer;
	}

	//~ FNotifyHook interface
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override
	{
		Sequencer->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
	}

private:
	TSharedPtr<ISequencer> Sequencer;
};


void SequencerHelpers::AddPropertiesMenu(FSequencer& Sequencer, FMenuBuilder& MenuBuilder, const TArray<TWeakObjectPtr<UObject>>& Sections)
{
	using namespace UE::Sequencer;

	TSharedRef<SSectionDetailsNotifyHookWrapper> DetailsNotifyWrapper = SNew(SSectionDetailsNotifyHookWrapper);
	FDetailsViewArgs DetailsViewArgs;
	{
		DetailsViewArgs.bAllowSearch = false;
		DetailsViewArgs.bCustomFilterAreaLocation = true;
		DetailsViewArgs.bCustomNameAreaLocation = true;
		DetailsViewArgs.bHideSelectionTip = true;
		DetailsViewArgs.bLockable = false;
		DetailsViewArgs.bSearchInitialKeyFocus = true;
		DetailsViewArgs.bUpdatesFromSelection = false;
		DetailsViewArgs.bShowOptions = false;
		DetailsViewArgs.bShowModifiedPropertiesOption = false;
		DetailsViewArgs.NotifyHook = &DetailsNotifyWrapper.Get();
		DetailsViewArgs.ColumnWidth = 0.45f;
	}

	// We pass the current scene to the UMovieSceneSection customization so we can get the overall bounds of the section when we change a section from infinite->bounded.
	UMovieScene* CurrentScene = Sequencer.GetFocusedMovieSceneSequence()->GetMovieScene();

	TSharedRef<INumericTypeInterface<double>> NumericTypeInterface = Sequencer.GetNumericTypeInterface();

	TSharedRef<IDetailsView> DetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	DetailsView->RegisterInstancedCustomPropertyTypeLayout("FrameNumber", FOnGetPropertyTypeCustomizationInstance::CreateLambda([=]() {
		return MakeShared<FFrameNumberDetailsCustomization>(NumericTypeInterface); }));
	DetailsView->RegisterInstancedCustomPropertyLayout(UMovieSceneSection::StaticClass(), FOnGetDetailCustomizationInstance::CreateLambda([=]() {
		return MakeShared<FMovieSceneSectionDetailsCustomization>(NumericTypeInterface, CurrentScene); }));
	
	DetailsView->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& PropertyAndParent)
		{			
			return FPropertyEditorPermissionList::Get().DoesPropertyPassFilter(PropertyAndParent.Property.GetOwnerStruct(), PropertyAndParent.Property.GetFName());
		})
	);

	// Let section interfaces further customize the properties details view.
	TSharedRef<FSequencerNodeTree> SequencerNodeTree = Sequencer.GetNodeTree();
	for (TWeakObjectPtr<UObject> Section : Sections)
	{
		if (Section.IsValid())
		{
			TSharedPtr<FSectionModel> SectionHandle = SequencerNodeTree->GetSectionModel(Cast<UMovieSceneSection>(Section));
			if (SectionHandle)
			{
				TSharedPtr<ISequencerSection> SectionInterface = SectionHandle->GetSectionInterface();
				FSequencerSectionPropertyDetailsViewCustomizationParams CustomizationDetails(
					SectionInterface.ToSharedRef(), Sequencer.AsShared(), *SectionHandle->GetParentTrackExtension()->GetTrackEditor().Get());
				TSharedPtr<FObjectBindingModel> ParentObjectBindingNode = SectionHandle->FindAncestorOfType<FObjectBindingModel>();
				if (ParentObjectBindingNode.IsValid())
				{
					CustomizationDetails.ParentObjectBindingGuid = ParentObjectBindingNode->GetObjectGuid();
				}
				SectionInterface->CustomizePropertiesDetailsView(DetailsView, CustomizationDetails);
			}
		}
	}

	Sequencer.OnInitializeDetailsPanel().Broadcast(DetailsView, Sequencer.AsShared());
	DetailsView->SetObjects(Sections);

	DetailsNotifyWrapper->SetDetailsAndSequencer(DetailsView, Sequencer.AsShared());
	DetailsNotifyWrapper->SetEnabled(!Sequencer.IsReadOnly());
	MenuBuilder.AddWidget(DetailsNotifyWrapper, FText::GetEmpty(), true);
}
