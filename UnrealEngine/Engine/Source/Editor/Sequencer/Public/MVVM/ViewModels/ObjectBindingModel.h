// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"
#include "MVVM/ViewModels/OutlinerItemModel.h"
#include "MVVM/Extensions/IRenameableExtension.h"
#include "MVVM/Extensions/ITrackAreaExtension.h"
#include "MVVM/Extensions/IGroupableExtension.h"
#include "MVVM/Extensions/IObjectBindingExtension.h"
#include "MVVM/Extensions/ISortableExtension.h"
#include "MVVM/Extensions/IDraggableOutlinerExtension.h"
#include "MVVM/Extensions/IDeletableExtension.h"

struct FMovieSceneBinding;

class UMovieScene;
class UMovieSceneTrack;
class FMenuBuilder;
class FPropertyPath;
enum class ECheckBoxState : uint8;

namespace UE
{
namespace Sequencer
{

class FSequenceModel;
class FLayerBarModel;
class FTrackModelStorageExtension;

/** Enumeration specifying what kind of object binding this is */
enum class EObjectBindingType
{
	Possessable, Spawnable, Unknown
};

class FObjectBindingModel
	: public FOutlinerItemModel
	, public IObjectBindingExtension
	, public IDraggableOutlinerExtension
	, public ITrackAreaExtension
	, public IGroupableExtension
	, public IRenameableExtension
	, public ISortableExtension
	, public IDeletableExtension
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FObjectBindingModel
		, FOutlinerItemModel
		, IObjectBindingExtension
		, IDraggableOutlinerExtension
		, ITrackAreaExtension
		, IGroupableExtension
		, IRenameableExtension
		, ISortableExtension
		, IDeletableExtension);

	FObjectBindingModel(FSequenceModel* OwnerModel, const FMovieSceneBinding& InBinding);
	~FObjectBindingModel();

	void AddTrack(UMovieSceneTrack* Track);
	void RemoveTrack(UMovieSceneTrack* Track);

	/*~ IObjectBindingExtension */
	FGuid GetObjectGuid() const override;

	/*~ IRenameableExtension */
	bool CanRename() const override;
	void Rename(const FText& NewName) override;

	/*~ IOutlinerExtension */
	FOutlinerSizing GetOutlinerSizing() const override;
	FText GetLabel() const override;
	FSlateColor GetLabelColor() const override;
	FText GetLabelToolTipText() const override;
	const FSlateBrush* GetIconBrush() const override;
	TSharedRef<SWidget> CreateOutlinerView(const FCreateOutlinerViewParams& InParams) override;

	/*~ ITrackAreaExtension */
	FTrackAreaParameters GetTrackAreaParameters() const override;
	FViewModelVariantIterator GetTrackAreaModelList() const override;

	/*~ IGroupableExtension */
	void GetIdentifierForGrouping(TStringBuilder<128>& OutString) const override;

	/*~ ISortableExtension */
	void SortChildren() override;
	FSortingKey GetSortingKey() const override;
	void SetCustomOrder(int32 InCustomOrder) override;

	/*~ FOutlinerItemModel */
	void BuildContextMenu(FMenuBuilder& MenuBuilder) override;
	void BuildOrganizeContextMenu(FMenuBuilder& MenuBuilder) override;
	bool GetDefaultExpansionState() const override;

	/*~ IDraggableOutlinerExtension */
	bool CanDrag() const override;

	/*~ IDeletableExtension */
	bool CanDelete(FText* OutErrorMessage) const override;
	void Delete() override;

public:

	virtual void SetParentBindingID(const FGuid& InObjectBindingID);
	virtual FGuid GetDesiredParentBinding() const;
	virtual EObjectBindingType GetType() const;
	virtual FText GetTooltipForSingleObjectBinding() const;
	virtual const UClass* FindObjectClass() const;
	virtual bool SupportsRebinding() const;

public:

	TSharedRef<SWidget> GetAddTrackMenuContent();

protected:

	/*~ FViewModel interface */
	void OnConstruct() override;

private:

	void AddSpawnOwnershipMenu(FMenuBuilder& MenuBuilder);
	void AddSpawnLevelMenu(FMenuBuilder& MenuBuilder);
	void AddTagMenu(FMenuBuilder& MenuBuilder);
	void AddChangeClassMenu(FMenuBuilder& MenuBuilder);

private:

	void AddPropertyMenuItems(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyableProperties, int32 PropertyNameIndexStart, int32 PropertyNameIndexEnd);
	void HandleAddTrackSubMenuNew(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyablePropertyPaths, int32 PropertyNameIndexStart);
	void HandlePropertyMenuItemExecute(FPropertyPath PropertyPath);

	ECheckBoxState GetTagCheckState(FName TagName);
	void ToggleTag(FName TagName);
	void HandleDeleteTag(FName TagName);
	void HandleAddTag(FName TagName);
	void HandleTemplateActorClassPicked(UClass* ChosenClass);

protected:

	FGuid ObjectBindingID;
	FGuid ParentObjectBindingID;
	FViewModelListHead TrackAreaList;
	TSharedPtr<FLayerBarModel> LayerBar;
	FSequenceModel* OwnerModel;
};

} // namespace Sequencer
} // namespace UE

