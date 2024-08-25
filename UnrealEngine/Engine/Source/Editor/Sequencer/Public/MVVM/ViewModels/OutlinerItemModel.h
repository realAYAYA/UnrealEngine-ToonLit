// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVM/ICastable.h"
#include "MVVM/ViewModels/ViewModel.h"
#include "MVVM/Extensions/ICurveEditorTreeItemExtension.h"
#include "MVVM/Extensions/IDimmableExtension.h"
#include "MVVM/Extensions/IGeometryExtension.h"
#include "MVVM/Extensions/IHoveredExtension.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/Extensions/IPinnableExtension.h"
#include "MVVM/Extensions/IMutableExtension.h"
#include "MVVM/Extensions/ISoloableExtension.h"
#include "MVVM/Extensions/HierarchicalCacheExtension.h"
#include "CurveEditorTypes.h"
#include "Tree/ICurveEditorTreeItem.h"

class UMovieSceneSequence;
class FSequencer;

namespace UE
{
namespace Sequencer
{

class FSequencerEditorViewModel;

class SEQUENCER_API FOutlinerItemModelMixin
	: public FOutlinerExtensionShim
	, public FGeometryExtensionShim
	, public FPinnableExtensionShim
	, public FHoveredExtensionShim
	, public IDimmableExtension
	, public FCurveEditorTreeItemExtensionShim
	, public ICurveEditorTreeItem
{
public:

	using Implements = TImplements<IOutlinerExtension, IGeometryExtension, IPinnableExtension, IHoveredExtension, IDimmableExtension, ICurveEditorTreeItemExtension>;

	FOutlinerItemModelMixin();

	TSharedPtr<FSequencerEditorViewModel> GetEditor() const;
	
	/*~ IOutlinerExtension */
	FName GetIdentifier() const override;
	bool IsExpanded() const override;
	void SetExpansion(bool bInIsExpanded) override;
	bool IsFilteredOut() const override;
	TSharedPtr<SWidget> CreateContextMenuWidget(const FCreateOutlinerContextMenuWidgetParams& InParams) override;
	FSlateColor GetLabelColor() const override;

	/*~ ICurveEditorTreeItemExtension */
	virtual bool HasCurves() const override;
	virtual TSharedPtr<ICurveEditorTreeItem> GetCurveEditorTreeItem() const override;
	virtual TOptional<FString> GetUniquePathName() const override;

	/*~ ICurveEditorTreeItem */
	virtual TSharedPtr<SWidget> GenerateCurveEditorTreeWidget(const FName& InColumnName, TWeakPtr<FCurveEditor> InCurveEditor, FCurveEditorTreeItemID InTreeItemID, const TSharedRef<ITableRow>& InTableRow) override;
	virtual void CreateCurveModels(TArray<TUniquePtr<FCurveModel>>& OutCurveModels) override;
	virtual bool PassesFilter(const FCurveEditorTreeFilter* InFilter) const override;

	/*~ IPinnableExtension */
	bool IsPinned() const override;

	/*~ IDimmableExtension */
	bool IsDimmed() const override;

protected:

	/** Get context menu contents. */
	virtual void BuildContextMenu(FMenuBuilder& MenuBuilder);
	virtual void BuildOrganizeContextMenu(FMenuBuilder& MenuBuilder);

	/** Set identifier for computing node paths */
	void SetIdentifier(FName InNewIdentifier);

	/** Get the default expansion state if it wasn't saved in the movie-scene data */
	virtual bool GetDefaultExpansionState() const;

	/** Set expansion state without saving it in the movie-scene data */
	void SetExpansionWithoutSaving(bool bInIsExpanded);

	void BuildSectionColorTintsContextMenu(FMenuBuilder& MenuBuilder);

private:

	virtual FViewModel* AsViewModel() = 0;
	virtual const FViewModel* AsViewModel() const = 0;

protected:
	FViewModelListHead OutlinerChildList;

private:

	bool IsRootModelPinned() const;
	void ToggleRootModelPinned();

	ECheckBoxState SelectedModelsSoloState() const;
	void ToggleSelectedModelsSolo();

	ECheckBoxState SelectedModelsMuteState() const;
	void ToggleSelectedModelsMuted();

private:

	ICastable* CastableThis;
	FName TreeItemIdentifier;
	FCurveEditorTreeItemID CurveEditorItemID;
	mutable bool bInitializedExpansion;
	mutable bool bInitializedPinnedState;
};

//
// Note: You must add the base class you use as a template parameter to the UE_SEQUENCER_DECLARE_CASTABLE list
// 
template<typename BaseType>
class TOutlinerModelMixin : public BaseType, public FOutlinerItemModelMixin
{
public:

	template<typename... ArgTypes>
	TOutlinerModelMixin(ArgTypes&&... InArgs)
		: BaseType(Forward<ArgTypes>(InArgs)...)
	{
		this->RegisterChildList(&this->OutlinerChildList);
	}

	TOutlinerModelMixin(const TOutlinerModelMixin<BaseType>&) = delete;
	TOutlinerModelMixin<BaseType> operator=(const TOutlinerModelMixin<BaseType>&) = delete;

	TOutlinerModelMixin(TOutlinerModelMixin<BaseType>&&) = delete;
	TOutlinerModelMixin<BaseType> operator=(TOutlinerModelMixin<BaseType>&&) = delete;

	virtual FViewModel* AsViewModel() { return this; }
	virtual const FViewModel* AsViewModel() const { return this; }
};

class SEQUENCER_API FOutlinerItemModel : public TOutlinerModelMixin<FViewModel>
{
public:
	UE_SEQUENCER_DECLARE_CASTABLE(FOutlinerItemModel, FViewModel, FOutlinerItemModelMixin);
};

class SEQUENCER_API FMuteSoloOutlinerItemModel
	: public FOutlinerItemModel
	, public IMutableExtension
	, public ISoloableExtension
{
public:

	UE_SEQUENCER_DECLARE_CASTABLE(FMuteSoloOutlinerItemModel, FOutlinerItemModel, IMutableExtension, ISoloableExtension);

	/*~ ISoloableExtension */
	bool IsSolo() const override;
	void SetIsSoloed(bool bIsSoloed) override;

	/*~ IMutableExtension */
	bool IsMuted() const override;
	void SetIsMuted(bool bIsMuted) override;
};


class SEQUENCER_API FOutlinerCacheExtension
	: public FHierarchicalCacheExtension
{
public:

	UE_SEQUENCER_DECLARE_VIEW_MODEL_TYPE_ID(FOutlinerCacheExtension);

	FOutlinerCacheExtension()
	{
		ModelListFilter = EViewModelListType::Outliner;
	}
};


} // namespace Sequencer
} // namespace UE

