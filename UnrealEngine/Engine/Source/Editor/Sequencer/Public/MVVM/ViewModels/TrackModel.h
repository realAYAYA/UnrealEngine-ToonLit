// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/OutlinerItemModel.h"
#include "MVVM/Extensions/IGeometryExtension.h"
#include "MVVM/Extensions/IGroupableExtension.h"
#include "MVVM/Extensions/IRenameableExtension.h"
#include "MVVM/Extensions/IResizableExtension.h"
#include "MVVM/Extensions/ISortableExtension.h"
#include "MVVM/Extensions/ITrackAreaExtension.h"
#include "MVVM/Extensions/ITrackExtension.h"
#include "MVVM/Extensions/IDeletableExtension.h"
#include "MVVM/Extensions/IDraggableOutlinerExtension.h"
#include "MVVM/Extensions/IDeletableExtension.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"
#include "EventHandlers/ISignedObjectEventHandler.h"

#include "MovieSceneSignedObject.h"

#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class ISequencer;
class ISequencerSection;
class ISequencerTrackEditor;
class UMovieSceneTrack;

namespace UE
{
namespace Sequencer
{

class FSectionModel;

class SEQUENCER_API FTrackModel
	: public FOutlinerItemModel
	, public IRenameableExtension
	, public IResizableExtension
	, public ITrackExtension
	, public ITrackAreaExtension
	, public IGroupableExtension
	, public ISortableExtension
	, public IDraggableOutlinerExtension
	, public IDeletableExtension
	, public UE::MovieScene::TIntrusiveEventHandler<UE::MovieScene::ISignedObjectEventHandler>
	, public UE::MovieScene::IDeferredSignedObjectFlushSignal
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FTrackModel
		, FOutlinerItemModel
		, IRenameableExtension
		, IResizableExtension
		, ITrackExtension
		, ITrackAreaExtension
		, IGroupableExtension
		, ISortableExtension
		, IDraggableOutlinerExtension
		, IDeletableExtension);

	explicit FTrackModel(UMovieSceneTrack* Track);
	~FTrackModel();

	FViewModelChildren GetTopLevelChannels();

public:

	static EViewModelListType GetTopLevelChannelType();
	static EViewModelListType GetTopLevelChannelGroupType();

	/*~ ITrackExtension */
	UMovieSceneTrack* GetTrack() const override;
	int32 GetRowIndex() const override;
	FViewModelChildren GetSectionModels() override;

	TSharedPtr<ISequencerTrackEditor> GetTrackEditor() const override;

	/*~ ISignedObjectEventHandler */
	void OnModifiedDirectly(UMovieSceneSignedObject*) override;
	void OnModifiedIndirectly(UMovieSceneSignedObject*) override;

	/* IDeferredSignedObjectFlushSignal */
	void OnDeferredModifyFlush() override;

	/*~ IOutlinerExtension */
	FOutlinerSizing GetOutlinerSizing() const override;
	TSharedRef<SWidget> CreateOutlinerView(const FCreateOutlinerViewParams& InParams) override;
	FSlateFontInfo GetLabelFont() const override;
	const FSlateBrush* GetIconBrush() const override;
	FText GetLabel() const override;
	FSlateColor GetLabelColor() const override;
	
	/*~ IDimmableExtension */
	bool IsDimmed() const override;

	/*~ IResizableExtension */
	bool IsResizable() const override;
	void Resize(float NewSize) override;

	/*~ ITrackAreaExtension */
	FTrackAreaParameters GetTrackAreaParameters() const override;
	FViewModelVariantIterator GetTrackAreaModelList() const override;
	FViewModelVariantIterator GetTopLevelChildTrackAreaModels() const override;

	/*~ ICurveEditorTreeItem */
	void CreateCurveModels(TArray<TUniquePtr<FCurveModel>>& OutCurveModels) override;

	/*~ IGroupableIdentifier */
	void GetIdentifierForGrouping(TStringBuilder<128>& OutString) const override;

	/*~ IRenameableExtension */
	bool CanRename() const override;
	void Rename(const FText& NewName) override;
	bool IsRenameValidImpl(const FText& NewName, FText& OutErrorMessage) const override;

	/*~ ISortableExtension */
	void SortChildren() override;
	FSortingKey GetSortingKey() const override;
	void SetCustomOrder(int32 InCustomOrder) override;

	/*~ IDraggableOutlinerExtension */
	bool CanDrag() const override;

	/*~ FOutlinerItemModel */
	bool HasCurves() const override;
	void BuildContextMenu(FMenuBuilder& MenuBuilder) override;
	bool GetDefaultExpansionState() const override;

	/*~ IDeletableExtension */
	bool CanDelete(FText* OutErrorMessage) const override;
	void Delete() override;

private:

	/*~ FViewModel interface */
	void OnConstruct() override;

	void ForceUpdate();

	/** A second children list for the sections inside this track */
	FViewModelListHead SectionList;
	FViewModelListHead TopLevelChannelList;

	/** The actual track wrapped by this data model */
	TWeakObjectPtr<UMovieSceneTrack> WeakTrack;

	// @todo_sequencer_mvvm: move all the track editor behavior into the view model
	TSharedPtr<ISequencerTrackEditor> TrackEditor;

	bool bNeedsUpdate;
};

} // namespace Sequencer
} // namespace UE

