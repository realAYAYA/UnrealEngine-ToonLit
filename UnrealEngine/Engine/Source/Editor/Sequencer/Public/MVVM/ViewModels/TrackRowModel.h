// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/OutlinerItemModel.h"
#include "MVVM/Extensions/ITrackExtension.h"
#include "MVVM/Extensions/ITrackAreaExtension.h"
#include "MVVM/Extensions/ILockableExtension.h"
#include "MVVM/Extensions/IRenameableExtension.h"
#include "MVVM/Extensions/IResizableExtension.h"
#include "MVVM/Extensions/IDeletableExtension.h"

#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class ISequencer;
class ISequencerTrackEditor;
class UMovieSceneTrack;

namespace UE
{
namespace Sequencer
{

class FSectionModel;

class SEQUENCER_API FTrackRowModel
	: public FMuteSoloOutlinerItemModel
	, public ITrackAreaExtension
	, public ILockableExtension
	, public ITrackExtension
	, public IRenameableExtension
	, public IResizableExtension
	, public IDeletableExtension
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FTrackRowModel, FMuteSoloOutlinerItemModel, ITrackAreaExtension, ILockableExtension, ITrackExtension, IDeletableExtension, IMutableExtension, ISoloableExtension, IRenameableExtension);

	explicit FTrackRowModel(UMovieSceneTrack* InTrack, int32 InRowIndex);
	~FTrackRowModel();

	void Initialize();

	FViewModelChildren GetTopLevelChannels();

	/*~ FOutlinerItemModel */
	void BuildContextMenu(FMenuBuilder& MenuBuilder) override;
	void CreateCurveModels(TArray<TUniquePtr<FCurveModel>>& OutCurveModels) override;

	/*~ IOutlinerExtension */
	FOutlinerSizing GetOutlinerSizing() const override;
	TSharedPtr<SWidget> CreateOutlinerViewForColumn(const FCreateOutlinerViewParams& InParams, const FName& InColumnName) override;
	FText GetLabel() const override;
	FSlateColor GetLabelColor() const override;
	FSlateFontInfo GetLabelFont() const override;
	const FSlateBrush* GetIconBrush() const override;

	/*~ IRenameableExtension */
	bool CanRename() const override;
	void Rename(const FText& NewName) override;
	bool IsRenameValidImpl(const FText& NewName, FText& OutErrorMessage) const override;

	/*~ IResizableExtension */
	bool IsResizable() const override;
	void Resize(float NewSize) override;

	/*~ ITrackExtension */
	UMovieSceneTrack* GetTrack() const override;
	int32 GetRowIndex() const override;
	FViewModelChildren GetSectionModels() override;
	TSharedPtr<ISequencerTrackEditor> GetTrackEditor() const override { return TrackEditor; }

	/*~ ITrackAreaExtension */
	FTrackAreaParameters GetTrackAreaParameters() const override;
	FViewModelVariantIterator GetTrackAreaModelList() const override;
	FViewModelVariantIterator GetTopLevelChildTrackAreaModels() const override;

	/*~ IDimmableExtension */
	bool IsDimmed() const override;

	/*~ IDeletableExtension */
	bool CanDelete(FText* OutErrorMessage) const override;
	void Delete() override;

	/*~ ILockableExtension Interface */
	ELockableLockState GetLockState() const override;
	void SetIsLocked(bool bIsLocked) override;

private:

	FViewModelListHead SectionList;
	FViewModelListHead TopLevelChannelList;
	TWeakObjectPtr<UMovieSceneTrack> WeakTrack;
	int32 RowIndex;

	// @todo_sequencer_mvvm: move all the track editor behavior into the view model
	TSharedPtr<ISequencerTrackEditor> TrackEditor;

	friend class FTrackModel;
};

} // namespace Sequencer
} // namespace UE

