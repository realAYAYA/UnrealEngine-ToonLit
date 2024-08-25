// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Guid.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"
#include "MVVM/ViewModels/OutlinerItemModel.h"
#include  "MVVM/ViewModels/BindingLifetimeOverlayModel.h"
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
class FStructOnScope;
struct FMovieSceneDynamicBinding;
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

class SEQUENCER_API FObjectBindingModel
	: public FMuteSoloOutlinerItemModel
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
		, FMuteSoloOutlinerItemModel
		, IObjectBindingExtension
		, IDraggableOutlinerExtension
		, ITrackAreaExtension
		, IGroupableExtension
		, IRenameableExtension
		, ISortableExtension
		, IDeletableExtension);

	FObjectBindingModel(FSequenceModel* OwnerModel, const FMovieSceneBinding& InBinding);
	~FObjectBindingModel();

	static EViewModelListType GetTopLevelChildTrackAreaGroupType();

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
	TSharedPtr<SWidget> CreateOutlinerViewForColumn(const FCreateOutlinerViewParams& InParams, const FName& InColumnName) override;

	/*~ ITrackAreaExtension */
	FTrackAreaParameters GetTrackAreaParameters() const override;
	FViewModelVariantIterator GetTrackAreaModelList() const override;
	FViewModelVariantIterator GetTopLevelChildTrackAreaModels() const override;

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
	virtual FSlateColor GetInvalidBindingLabelColor() const { return FLinearColor::Red; }

public:

	TSharedRef<SWidget> GetAddTrackMenuContent();

	/** Build a sub-menu for editing the given dynamic binding */
	void AddDynamicBindingMenu(FMenuBuilder& MenuBuilder, FMovieSceneDynamicBinding& DynamicBinding);

protected:

	/*~ FViewModel interface */
	void OnConstruct() override;

private:

	void AddPropertyMenuItems(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyableProperties, int32 PropertyNameIndexStart, int32 PropertyNameIndexEnd);
	void HandleAddTrackSubMenuNew(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyablePropertyPaths, int32 PropertyNameIndexStart);
	void HandlePropertyMenuItemExecute(FPropertyPath PropertyPath);

	void AddTagMenu(FMenuBuilder& MenuBuilder);
	ECheckBoxState GetTagCheckState(FName TagName);
	void ToggleTag(FName TagName);
	void HandleDeleteTag(FName TagName);
	void HandleAddTag(FName TagName);
	void HandleTemplateActorClassPicked(UClass* ChosenClass);

	void OnFinishedChangingDynamicBindingProperties(const FPropertyChangedEvent& ChangeEvent, TSharedPtr<FStructOnScope> ValueStruct);

protected:

	FGuid ObjectBindingID;
	FGuid ParentObjectBindingID;
	FViewModelListHead TrackAreaList;
	FViewModelListHead TopLevelChildTrackAreaList;
	TSharedPtr<FLayerBarModel> LayerBar;
	TSharedPtr<FBindingLifetimeOverlayModel> BindingLifetimeOverlayModel;
	FSequenceModel* OwnerModel;
};

} // namespace Sequencer
} // namespace UE

