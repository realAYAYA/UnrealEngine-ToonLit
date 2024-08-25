// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneChannelEditorData.h"
#include "Channels/MovieSceneChannelOverrideContainer.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "EventHandlers/ISignedObjectEventHandler.h"
#include "Fonts/SlateFontInfo.h"
#include "Internationalization/Text.h"
#include "Layout/Visibility.h"
#include "MVVM/Extensions/IDeletableExtension.h"
#include "MVVM/Extensions/IGeometryExtension.h"
#include "MVVM/Extensions/IKeyExtension.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/IRecyclableExtension.h"
#include "MVVM/Extensions/ITrackAreaExtension.h"
#include "MVVM/Extensions/ITrackLaneExtension.h"
#include "MVVM/Extensions/LinkedOutlinerExtension.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/OutlinerItemModel.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "Tree/ICurveEditorTreeItem.h"
#include "UObject/NameTypes.h"

class FCurveModel;
class FSequencerSectionKeyAreaNode;
class IKeyArea;
class ISequencerSection;
class SWidget;
class UMovieSceneSection;
namespace UE::Sequencer { template <typename T> struct TAutoRegisterViewModelTypeID; }
struct FMovieSceneChannel;
struct FMovieSceneChannelHandle;
struct FSequencerChannelPaintArgs;

namespace UE
{
namespace Sequencer
{

class FSectionModel;
class FSequenceModel;

/**
 * Model for a single channel inside a section.
 * For instance, this represents the "Location.X" channel of a single transform section.
 */
class SEQUENCER_API FChannelModel
	: public FViewModel
	, public FLinkedOutlinerExtension
	, public FGeometryExtensionShim
	, public FLinkedOutlinerComputedSizingShim
	, public ITrackLaneExtension
	, public IRecyclableExtension
	, public IKeyExtension
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FChannelModel, FViewModel, FLinkedOutlinerExtension, IGeometryExtension, ITrackLaneExtension, IRecyclableExtension, IKeyExtension);

	FChannelModel(FName InChannelName, TWeakPtr<ISequencerSection> InSection, FMovieSceneChannelHandle InChannel);
	~FChannelModel();

	void Initialize(TWeakPtr<ISequencerSection> InSection, FMovieSceneChannelHandle InChannel);

	/** Returns the section object that owns the associated channel */
	UMovieSceneSection* GetSection() const;

	/** Returns the associated channel object */
	FMovieSceneChannel* GetChannel() const;

	/** Returns whether this channel has any keyframes on it */
	bool IsAnimated() const;

	/** Returns the channel's name */
	FName GetChannelName() const { return ChannelName; }

	/** Returns the key area for the channel */
	TSharedPtr<IKeyArea> GetKeyArea() const { return KeyArea; }

	/** Create the curve editor model for the associated channel */
	void CreateCurveModels(TArray<TUniquePtr<FCurveModel>>& OutCurveModels);

	/** Returns the desired sizing for the track area row */
	FOutlinerSizing GetDesiredSizing() const;

	int32 CustomPaint(const FSequencerChannelPaintArgs& CustomPaintArgs, int32 LayerId) const;

	/*~ ITrackLaneExtension */
	TSharedPtr<ITrackLaneWidget> CreateTrackLaneView(const FCreateTrackLaneViewParams& InParams) override;
	FTrackLaneVirtualAlignment ArrangeVirtualTrackLaneView() const override;

	/*~ IRecyclableExtension */
	void OnRecycle() override;

	/*~ IKeyExtension */
	bool UpdateCachedKeys(TSharedPtr<FCachedKeys>& OutCachedKeys) const override;
	bool GetFixedExtents(double& OutFixedMin, double& OutFixedMax) const override;
	void DrawKeys(TArrayView<const FKeyHandle> InKeyHandles, TArrayView<FKeyDrawParams> OutKeyDrawParams) override;
	TUniquePtr<FCurveModel> CreateCurveModel() override;

private:

	FLinearColor GetKeyBarColor() const;

private:

	TSharedPtr<IKeyArea> KeyArea;
	FName ChannelName;
};

/**
 * Model for the outliner entry associated with all sections' channels of a given common name.
 * For instance, this represents the "Location.X" entry in the Sequencer outliner.
 */
class SEQUENCER_API FChannelGroupModel
	: public FViewModel
	, public ITrackAreaExtension
	, public IRecyclableExtension
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FChannelGroupModel, FViewModel, ITrackAreaExtension, IRecyclableExtension);

	FChannelGroupModel(FName InChannelName, const FText& InDisplayText);
	FChannelGroupModel(FName InChannelName, const FText& InDisplayText, const FText& InTooltipText);
	FChannelGroupModel(FName InChannelName, const FText& InDisplayText, FGetMovieSceneTooltipText InGetTooltipTextDelegate);
	~FChannelGroupModel();

	/** Returns whether any of the channels within this group have any keyframes on them */
	bool IsAnimated() const;

	/** Returns the common name for all channels in this group */
	FName GetChannelName() const { return ChannelName; }

	/** Returns the label for this group */
	FText GetDisplayText() const { return DisplayText; }

	/** Returns the tooltip for this group */
	FText GetTooltipText() const;

	/** Gets all the channel models in this group */
	TArrayView<const TWeakViewModelPtr<FChannelModel>> GetChannels() const;
	/** Adds a channel model to this group */
	void AddChannel(TWeakViewModelPtr<FChannelModel> InChannel);

	/** Get the key area of the channel associated with the given section */
	TSharedPtr<IKeyArea> GetKeyArea(TSharedPtr<FSectionModel> InOwnerSection) const;

	/** Get the key area of the channel associated with the given section */
	TSharedPtr<IKeyArea> GetKeyArea(const UMovieSceneSection* InOwnerSection) const;

	/** Get the channel model at the given index in the list of channels */
	TSharedPtr<FChannelModel> GetChannel(int32 Index) const;

	/** Get the channel model of the channel associated with the given section */
	TSharedPtr<FChannelModel> GetChannel(TSharedPtr<FSectionModel> InOwnerSection) const;

	/** Get the channel model of the channel associated with the given section */
	TSharedPtr<FChannelModel> GetChannel(const UMovieSceneSection* InOwnerSection) const;

	/** Get the key areas of all channels */
	TArray<TSharedRef<IKeyArea>> GetAllKeyAreas() const;

	/** Gets a serial number representing if the list of channels has changed */
	uint32 GetChannelsSerialNumber() const;

	/** Notifies this view-model that the list of channels has changed */
	void OnChannelOverridesChanged();

public:

	/*~ ITrackAreaExtension */
	FTrackAreaParameters GetTrackAreaParameters() const override;
	FViewModelVariantIterator GetTrackAreaModelList() const override;

	/*~ IRecyclableExtension */
	void OnRecycle() override;

	void CreateCurveModels(TArray<TUniquePtr<FCurveModel>>& OutCurveModels);
	bool HasCurves() const;
	TOptional<FString> GetUniquePathName() const;

	void BuildChannelOverrideMenu(FMenuBuilder& MenuBuilder);

	void CleanupChannels();

protected:

	TArray<TWeakViewModelPtr<FChannelModel>> Channels;
	uint32 ChannelsSerialNumber;
	FName ChannelName;
	FText DisplayText;
	FGetMovieSceneTooltipText GetTooltipTextDelegate;
};


/**
 * Model for the outliner entry associated with all sections' channels of a given common name.
 * For instance, this represents the "Location.X" entry in the Sequencer outliner.
 */
class SEQUENCER_API FChannelGroupOutlinerModel
	: public TOutlinerModelMixin<FChannelGroupModel>
	, public ICompoundOutlinerExtension
	, public IDeletableExtension
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FChannelGroupOutlinerModel, FChannelGroupModel, FOutlinerItemModelMixin, ICompoundOutlinerExtension, IDeletableExtension);

	FChannelGroupOutlinerModel(FName InChannelName, const FText& InDisplayText, FGetMovieSceneTooltipText InGetTooltipTextDelegate);
	~FChannelGroupOutlinerModel();

public:

	/*~ ICompoundOutlinerExtension */
	FOutlinerSizing RecomputeSizing() override;

	/*~ IOutlinerExtension */
	FOutlinerSizing GetOutlinerSizing() const override;
	FText GetLabel() const override;
	FSlateFontInfo GetLabelFont() const override;
	FText GetLabelToolTipText() const override;
	TSharedPtr<SWidget> CreateOutlinerViewForColumn(const FCreateOutlinerViewParams& InParams, const FName& InColumnName) override;

	/*~ ICurveEditorTreeItem */
	void CreateCurveModels(TArray<TUniquePtr<FCurveModel>>& OutCurveModels) override;

	/*~ IDeletableExtension */
	bool CanDelete(FText* OutErrorMessage) const override;
	void Delete() override;

	/*~ ICurveEditorTreeItemExtension */
	bool HasCurves() const override;
	void BuildContextMenu(FMenuBuilder& MenuBuilder) override;
	TOptional<FString> GetUniquePathName() const override;

private:

	EVisibility GetKeyEditorVisibility() const;
	
private:

	FOutlinerSizing ComputedSizing;
};


/**
 * Utility class for building menus that add, edit, and remove channel overrides.
 */
class SEQUENCER_API FChannelGroupOverrideHelper
{
public:

	static void BuildChannelOverrideMenu(FMenuBuilder& MenuBuilder, TSharedPtr<FSequenceModel> SequenceModel);

private:

	static bool GetChannelsFromOutlinerSelection(TSharedPtr<FSequenceModel> SequenceModel, TArray<TViewModelPtr<FChannelModel>>& OutChannelModels);
	static void BuildChannelOverrideMenu(FMenuBuilder& MenuBuilder, TSharedPtr<FSequenceModel> SequenceModel, UMovieSceneChannelOverrideContainer::FOverrideCandidates OverrideCandidates);
	static void OverrideChannels(TSharedPtr<FSequenceModel> SequenceModel, TSubclassOf<UMovieSceneChannelOverrideContainer> OverrideClass);
	static void RemoveChannelOverrides(TSharedPtr<FSequenceModel> SequenceModel);
};

} // namespace Sequencer
} // namespace UE

