// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVVM/ViewModels/TrackRowModel.h"

#include "MVVM/Extensions/ITrackExtension.h"
#include "MVVM/ViewModels/SectionModel.h"
#include "MVVM/ViewModels/SequenceModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "MVVM/ViewModels/FolderModel.h"
#include "MVVM/ViewModels/TrackModelLayoutBuilder.h"
#include "MVVM/Views/SOutlinerTrackView.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/Selection/Selection.h"

#include "SequencerNodeTree.h"
#include "ISequencer.h"
#include "ISequencerSection.h"
#include "ISequencerTrackEditor.h"
#include "SequencerUtilities.h"
#include "SequencerCommonHelpers.h"
#include "SSequencer.h"

#include "MovieSceneTrack.h"
#include "MovieSceneFolder.h"
#include "MovieSceneSection.h"
#include "MovieSceneTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieScenePrimitiveMaterialTrack.h"
#include "EntitySystem/IMovieSceneBlenderSystemSupport.h"

#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "TrackRowModel"

namespace UE::Sequencer
{

FTrackRowModel::FTrackRowModel(UMovieSceneTrack* Track, int32 InRowIndex)
	: SectionList(EViewModelListType::TrackArea)
	, TopLevelChannelList(FTrackModel::GetTopLevelChannelGroupType())
	, WeakTrack(Track)
	, RowIndex(InRowIndex)
{
	RegisterChildList(&SectionList);
	RegisterChildList(&TopLevelChannelList);

	FName Identifier = Track->GetFName();
	Identifier.SetNumber(InRowIndex);
	SetIdentifier(Identifier);
}

FTrackRowModel::~FTrackRowModel()
{
}

void FTrackRowModel::Initialize()
{
	UMovieSceneTrack* Track = GetTrack();
	TSharedPtr<FSequenceModel> SequenceModel = FindAncestorOfType<FSequenceModel>();
	if (Track && SequenceModel)
	{
		TrackEditor = SequenceModel->GetSequencer()->GetTrackEditor(Track);
		ensure(TrackEditor);
		SetExpansion(TrackEditor->GetDefaultExpansionState(Track));
	}
}

FViewModelChildren FTrackRowModel::GetTopLevelChannels()
{
	return GetChildrenForList(&TopLevelChannelList);
}

UMovieSceneTrack* FTrackRowModel::GetTrack() const
{
	return WeakTrack.Get();
}

int32 FTrackRowModel::GetRowIndex() const
{
	return RowIndex;
}

FViewModelChildren FTrackRowModel::GetSectionModels()
{
	return GetChildrenForList(&SectionList);
}

FOutlinerSizing FTrackRowModel::GetOutlinerSizing() const
{
	FViewDensityInfo Density = GetEditor()->GetViewDensity();

	float Height = Density.UniformHeight.Get(SequencerLayoutConstants::SectionAreaDefaultHeight);
	for (TSharedPtr<FSectionModel> Section : SectionList.Iterate<FSectionModel>())
	{
		Height = Section->GetSectionInterface()->GetSectionHeight(Density);
		break;
	}
	return FOutlinerSizing(Height);
}

FTrackAreaParameters FTrackRowModel::GetTrackAreaParameters() const
{
	FTrackAreaParameters Params;
	Params.LaneType = ETrackAreaLaneType::Nested;
	Params.TrackLanePadding.Bottom = 1.f;
	return Params;
}

FViewModelVariantIterator FTrackRowModel::GetTrackAreaModelList() const
{
	return &SectionList;
}

FViewModelVariantIterator FTrackRowModel::GetTopLevelChildTrackAreaModels() const
{
	return &TopLevelChannelList;
}

void FTrackRowModel::CreateCurveModels(TArray<TUniquePtr<FCurveModel>>& OutCurveModels)
{
	TViewModelPtr<FChannelGroupModel> ChannelGroup = TopLevelChannelList.GetHead().ImplicitCast();
	if (ChannelGroup)
	{
		ChannelGroup->CreateCurveModels(OutCurveModels);
	}
}

bool FTrackRowModel::IsDimmed() const
{
	UMovieSceneTrack* Track = GetTrack();

	if (Track && Track->IsRowEvalDisabled(GetRowIndex()))
	{
		return true;
	}

	return FOutlinerItemModel::IsDimmed();
}

ELockableLockState FTrackRowModel::GetLockState() const
{
	int32 NumSections = 0;
	int32 NumLockedSections = 0;

	for (const TViewModelPtr<FSectionModel>& Section : SectionList.Iterate<FSectionModel>())
	{
		++NumSections;

		UMovieSceneSection* SectionObject = Section->GetSection();
		if (SectionObject && SectionObject->IsLocked())
		{
			++NumLockedSections;
		}
	}

	if (NumSections == 0 || NumLockedSections == 0)
	{
		return ELockableLockState::None;
	}
	return NumLockedSections == NumSections ? ELockableLockState::Locked : ELockableLockState::PartiallyLocked;
}

void FTrackRowModel::SetIsLocked(bool bInIsLocked)
{
	for (const TViewModelPtr<FSectionModel>& Section : SectionList.Iterate<FSectionModel>())
	{
		UMovieSceneSection* SectionObject = Section->GetSection();
		if (SectionObject)
		{
			SectionObject->Modify();
			SectionObject->SetIsLocked(bInIsLocked);
		}
	}
}

FSlateFontInfo FTrackRowModel::GetLabelFont() const
{
	bool bAllAnimated = false;
	TViewModelPtr<FChannelGroupModel> TopLevelChannel = TopLevelChannelList.GetHead().ImplicitCast();
	if (TopLevelChannel)
	{
		for (const TViewModelPtr<FChannelModel>& ChannelModel : TopLevelChannel->GetDescendantsOfType<FChannelModel>())
		{
			FMovieSceneChannel* Channel = ChannelModel->GetChannel();
			if (!Channel || Channel->GetNumKeys() == 0)
			{
				return FOutlinerItemModel::GetLabelFont();
			}
			else
			{
				bAllAnimated = true;
			}
		}
		if (bAllAnimated == true)
		{
			return FAppStyle::GetFontStyle("Sequencer.AnimationOutliner.ItalicFont");
		}
	}
	return FOutlinerItemModel::GetLabelFont();
}

const FSlateBrush* FTrackRowModel::GetIconBrush() const
{
	return TrackEditor ? TrackEditor->GetIconBrush() : nullptr;
}

FText FTrackRowModel::GetLabel() const
{
	UMovieSceneTrack* Track = GetTrack();
	return Track ? Track->GetTrackRowDisplayName(RowIndex) : FText::GetEmpty();
}

FSlateColor FTrackRowModel::GetLabelColor() const
{
	UMovieSceneTrack* Track = GetTrack();
	TSharedPtr<FSequenceModel> SequenceModel = FindAncestorOfType<FSequenceModel>();
	if (!Track || !SequenceModel)
	{
		return FSlateColor::UseForeground();
	}

	TSharedPtr<ISequencer> Sequencer = SequenceModel->GetSequencer();
	if (!Sequencer)
	{
		return FSlateColor::UseForeground();
	}
	FMovieSceneLabelParams LabelParams;
	LabelParams.bIsDimmed = IsDimmed();
	LabelParams.Player = Sequencer.Get();
	LabelParams.SequenceID = SequenceModel->GetSequenceID();
	if (TViewModelPtr<FObjectBindingModel> ObjectBindingModel = FindAncestorOfType<FObjectBindingModel>())
	{
		LabelParams.BindingID = ObjectBindingModel->GetObjectGuid();					
		// If the object binding model has an invalid binding, we want to use its label color, as it may be red or gray depending on situation
		// and we want the children of that to have the same color.
		// Otherwise, we can use the track's label color below
		TArrayView<TWeakObjectPtr<> > BoundObjects = LabelParams.Player->FindBoundObjects(LabelParams.BindingID, LabelParams.SequenceID);
		if (BoundObjects.Num() == 0)
		{
			return ObjectBindingModel->GetLabelColor();
		}
	}
	return Track->GetLabelColor(LabelParams);
}

TSharedPtr<SWidget> FTrackRowModel::CreateOutlinerViewForColumn(const FCreateOutlinerViewParams& InParams, const FName& InColumnName)
{
	FBuildColumnWidgetParams Params(SharedThis(this), InParams);
	return TrackEditor->BuildOutlinerColumnWidget(Params, InColumnName);
}

bool FTrackRowModel::CanRename() const
{
	UMovieSceneNameableTrack* NameableTrack = Cast<UMovieSceneNameableTrack>(GetTrack());
	return NameableTrack && NameableTrack->CanRename();
}

void FTrackRowModel::Rename(const FText& NewName)
{
	UMovieSceneNameableTrack* NameableTrack = ::Cast<UMovieSceneNameableTrack>(GetTrack());

	if (NameableTrack && !NameableTrack->GetTrackRowDisplayName(GetRowIndex()).EqualTo(NewName))
	{
		const FScopedTransaction Transaction(NSLOCTEXT("SequencerTrackRowNode", "RenameTrackRow", "Rename Track Row"));
		NameableTrack->SetTrackRowDisplayName(NewName, GetRowIndex());

		SetIdentifier(FName(*NewName.ToString()));

		// HACK: this should not exist but is required to make renaming emitters work in niagara
		if (TSharedPtr<FSequenceModel> OwnerModel = FindAncestorOfType<FSequenceModel>())
		{
			OwnerModel->GetSequencer()->NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::TrackValueChanged);
		}
	}
}

bool FTrackRowModel::IsRenameValidImpl(const FText& NewName, FText& OutErrorMessage) const
{
	UMovieSceneNameableTrack* NameableTrack = ::Cast<UMovieSceneNameableTrack>(GetTrack());
	if (NameableTrack)
	{
		return NameableTrack->ValidateDisplayName(NewName, OutErrorMessage);
	}
	return false;
}

bool FTrackRowModel::IsResizable() const
{
	UMovieSceneTrack* Track = GetTrack();
	TSharedPtr<ISequencerTrackEditor> TrackEditorPtr = GetTrackEditor();
	return Track && TrackEditorPtr && TrackEditorPtr->IsResizable(Track);
}

void FTrackRowModel::Resize(float NewSize)
{
	UMovieSceneTrack* Track = GetTrack();
	TSharedPtr<ISequencerTrackEditor> TrackEditorPtr = GetTrackEditor();
	if (Track && TrackEditorPtr && TrackEditorPtr->IsResizable(Track))
	{
		TrackEditorPtr->Resize(NewSize, Track);
	}
}

void FTrackRowModel::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	UMovieSceneTrack* Track = GetTrack();
	if (!Track)
	{
		return;
	}

	TSharedPtr<ISequencer> Sequencer = GetEditor()->GetSequencer();

	const int32 TrackRowIndex = GetRowIndex();

	TSharedPtr<ISequencerTrackEditor> TrackEditorPtr = GetTrackEditor();
	TrackEditorPtr->BuildTrackContextMenu(MenuBuilder, Track);

	if (Track && Track->GetSupportedBlendTypes().Num() > 0)
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("AddSection", "Add Section"),
			FText(),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){
				FSequencerUtilities::PopulateMenu_CreateNewSection(SubMenuBuilder, TrackRowIndex + 1, Track, Sequencer);
			})
		);	
	}

	// Add menu items for selecting a blender
	IMovieSceneBlenderSystemSupport* BlenderSystemSupport = Cast<IMovieSceneBlenderSystemSupport>(Track);
	if (BlenderSystemSupport)
	{
		TArray<TSubclassOf<UMovieSceneBlenderSystem>> BlenderTypes;
		BlenderSystemSupport->GetSupportedBlenderSystems(BlenderTypes);

		if (BlenderTypes.Num() > 1)
		{
			MenuBuilder.AddSubMenu(
				LOCTEXT("BlendingAlgorithmSubMenu", "Blending Algorithm"),
				FText(),
				FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){
					FSequencerUtilities::PopulateMenu_BlenderSubMenu(SubMenuBuilder, Track, Sequencer);
				})
			);
		}
	}

	// Find sections in the track to add batch properties for
	TArray<TWeakObjectPtr<UObject>> TrackSections;
	
	for (TViewModelPtr<ITrackExtension> TrackExtension : Sequencer->GetViewModel()->GetSelection()->Outliner.Filter<ITrackExtension>())
	{
		for (UMovieSceneSection* Section : TrackExtension->GetSections())
		{
			TrackSections.Add(Section);
		}
	}
	
	for (TSharedPtr<FViewModel> TrackAreaModel : GetTrackAreaModelList())
	{
		constexpr bool bIncludeThis = true;
		for (TSharedPtr<FSectionModel> Section : TParentFirstChildIterator<FSectionModel>(TrackAreaModel, bIncludeThis))
		{
			if (UMovieSceneSection* SectionObject = Section->GetSection())
			{
				TrackSections.AddUnique(SectionObject);
			}
		}
	}
		
	if (TrackSections.Num())
	{
		MenuBuilder.AddSubMenu(
			TrackSections.Num() > 1 ? LOCTEXT("BatchEditSections", "Batch Edit Sections") : LOCTEXT("EditSection", "Edit Section"),
			FText(),
			FNewMenuDelegate::CreateLambda([=](FMenuBuilder& SubMenuBuilder){
				SequencerHelpers::AddPropertiesMenu(static_cast<FSequencer&>(*Sequencer), SubMenuBuilder, TrackSections);
			})
		);
	}

	TViewModelPtr<FChannelGroupModel> ChannelGroup = TopLevelChannelList.GetHead().ImplicitCast();
	if (ChannelGroup)
	{
		ChannelGroup->BuildChannelOverrideMenu(MenuBuilder);
	}

	FOutlinerItemModel::BuildContextMenu(MenuBuilder);
}

bool FTrackRowModel::CanDelete(FText* OutErrorMessage) const
{
	return true;
}

void FTrackRowModel::Delete()
{
	UMovieSceneTrack* Track = GetTrack();
	if (!Track)
	{
		return;
	}

	// Remove from a parent folder if necessary.
	if (TViewModelPtr<FFolderModel> ParentFolder = CastParent<FFolderModel>())
	{
		ParentFolder->GetFolder()->Modify();
		ParentFolder->GetFolder()->RemoveChildTrack(Track);
	}

	// Remove sub tracks belonging to this row only
	Track->Modify();
	Track->SetFlags(RF_Transactional);

	for (TSharedPtr<FSectionModel> SectionModel : SectionList.Iterate<FSectionModel>())
	{
		UMovieSceneSection* Section = SectionModel->GetSection();
		if (Section)
		{
			Track->RemoveSection(*Section);
		}
	}

	Track->UpdateEasing();
	Track->FixRowIndices();
}

} // namespace UE::Sequencer

#undef LOCTEXT_NAMESPACE

