// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Channels/MovieSceneChannelEditorData.h"
#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Fonts/SlateFontInfo.h"
#include "Internationalization/Text.h"
#include "MVVM/Extensions/IDeletableExtension.h"
#include "MVVM/Extensions/IGeometryExtension.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/IRecyclableExtension.h"
#include "MVVM/Extensions/ITrackAreaExtension.h"
#include "MVVM/Extensions/ITrackLaneExtension.h"
#include "MVVM/Extensions/LinkedOutlinerExtension.h"
#include "MVVM/ICastable.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/ViewModels/OutlinerItemModel.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/ViewModels/ViewModelHierarchy.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

class FSequencerSectionCategoryNode;
class SWidget;
class UMovieSceneSection;
namespace UE::Sequencer { template <typename T> struct TAutoRegisterViewModelTypeID; }

namespace UE
{
namespace Sequencer
{

/**
 * Model for a single channel category inside a section.
 * For instance, this represents the "Location" category of a single transform section, which would
 * contain the X, Y, and Z translation channels.
 */
class SEQUENCER_API FCategoryModel
	: public FViewModel
	, public FLinkedOutlinerExtension
	, public FGeometryExtensionShim
	, public ITrackLaneExtension
	, public FLinkedOutlinerComputedSizingShim
{
public:
	UE_SEQUENCER_DECLARE_CASTABLE(FCategoryModel, FViewModel, FLinkedOutlinerExtension, IGeometryExtension, ITrackLaneExtension);

	explicit FCategoryModel(FName InCategoryName);

	/** Whether any of the channels within this category are animated.  */
	bool IsAnimated() const;

	/** Returns the category's name */
	FName GetCategoryName() const { return CategoryName; }

	/** Returns the desired sizing for the track area row */
	virtual FOutlinerSizing GetDesiredSizing() const;

	/*~ ITrackLaneExtension */
	TSharedPtr<ITrackLaneWidget> CreateTrackLaneView(const FCreateTrackLaneViewParams& InParams) override;
	FTrackLaneVirtualAlignment ArrangeVirtualTrackLaneView() const override;

private:

	FLinearColor GetKeyBarColor() const;

private:

	FViewModelListHead Children;
	FName CategoryName;
};

/**
 * Model for the outliner entry associated with all sections' channel categories of a given common name.
 * For instance, this represents the "Location" category entry in the Sequence outliner, which would contain
 * the X, Y, and Z translation channels of all the corresponding sections in the track area.
 */
class SEQUENCER_API FCategoryGroupModel
	: public FOutlinerItemModel
	, public ITrackAreaExtension
	, public ICompoundOutlinerExtension
	, public IDeletableExtension
	, public IRecyclableExtension
{
public:
	UE_SEQUENCER_DECLARE_CASTABLE(FCategoryGroupModel, FOutlinerItemModel, ITrackAreaExtension, ICompoundOutlinerExtension, IDeletableExtension, IRecyclableExtension);

	explicit FCategoryGroupModel(FName InCategoryName, const FText& InDisplayText, FGetMovieSceneTooltipText InGetGroupTooltipTextDelegate);

	~FCategoryGroupModel();

	/**
	 * @return Whether any of the channels within this category group have any keyframes on them
	 */
	bool IsAnimated() const;

	FName GetCategoryName() const
	{
		return CategoryName;
	}

	FText GetDisplayText() const
	{
		return DisplayText;
	}

	void AddCategory(TWeakViewModelPtr<FCategoryModel> InCategory);

	TArrayView<const TWeakViewModelPtr<FCategoryModel>> GetCategories() const;

public:

	/*~ ICompoundOutlinerExtension */
	FOutlinerSizing RecomputeSizing() override;

	/*~ IOutlinerExtension */
	FOutlinerSizing GetOutlinerSizing() const override;
	FText GetLabel() const override;
	FSlateFontInfo GetLabelFont() const override;
	FText GetLabelToolTipText() const override;
	TSharedPtr<SWidget> CreateOutlinerViewForColumn(const FCreateOutlinerViewParams& InParams, const FName& InColumnName) override;

	/*~ ITrackAreaExtension */
	FTrackAreaParameters GetTrackAreaParameters() const override;
	FViewModelVariantIterator GetTrackAreaModelList() const override;

	/*~ IDeletableExtension */
	bool CanDelete(FText* OutErrorMessage) const override;
	void Delete() override;

	/*~ IRecyclableExtension */
	void OnRecycle() override;

private:

	TArray<TWeakViewModelPtr<FCategoryModel>> Categories;
	FName CategoryName;
	FText DisplayText;
	FGetMovieSceneTooltipText GetGroupTooltipTextDelegate;
	FOutlinerSizing ComputedSizing;
};

} // namespace Sequencer
} // namespace UE

