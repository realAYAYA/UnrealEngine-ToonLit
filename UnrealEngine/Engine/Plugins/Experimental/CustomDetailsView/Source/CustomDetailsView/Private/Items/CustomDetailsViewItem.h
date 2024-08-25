// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Items/CustomDetailsViewItemBase.h"
#include "Containers/Array.h"
#include "Delegates/IDelegateInstance.h"
#include "Layout/Visibility.h"
#include "Styling/SlateTypes.h"
#include "Templates/SharedPointer.h"

class FCustomDetailsViewRootItem;
class FDetailColumnSizeData;
class ICustomDetailsView;
class IDetailTreeNode;
class IPropertyRowGenerator;
class SSplitter;
enum class EDetailNodeType;
struct FCustomDetailsViewArgs;

class FCustomDetailsViewItem : public FCustomDetailsViewItemBase
{
public:
	explicit FCustomDetailsViewItem(const TSharedRef<SCustomDetailsView>& InCustomDetailsView
		, const TSharedPtr<ICustomDetailsViewItem>& InParentItem
		, const TSharedPtr<IDetailTreeNode>& InDetailTreeNode);

	virtual ~FCustomDetailsViewItem() override;

	void InitWidget(const TSharedRef<IDetailTreeNode>& InDetailTreeNode);

	TArray<TSharedPtr<ICustomDetailsViewItem>> GenerateChildren(const TSharedRef<ICustomDetailsViewItem>& InParentItem
		, const TArray<TSharedRef<IDetailTreeNode>>& InDetailTreeNodes);

	//~ Begin ICustomDetailsViewItem
	virtual IDetailsView* GetDetailsView() const override;
	virtual void RefreshItemId() override;
	virtual void RefreshChildren(TSharedPtr<ICustomDetailsViewItem> InParentOverride = nullptr) override;
	virtual const TArray<TSharedPtr<ICustomDetailsViewItem>>& GetChildren() const override final { return Children; }
	virtual void SetResetToDefaultOverride(const FResetToDefaultOverride& InOverride) override;
	//~ End ICustomDetailsViewItem

	//~ Begin FCustomDetailsViewItemBase
	virtual void AddExtensionWidget(const TSharedRef<SSplitter>& InSplitter
		, const FDetailColumnSizeData& InColumnSizeData
		, const FCustomDetailsViewArgs& InViewArgs) override;

	virtual TSharedRef<SWidget> MakeEditConditionWidget() override;
	//~ End FCustomDetailsViewItemBase

	bool HasEditConditionToggle() const;
	EVisibility GetEditConditionVisibility() const;
	ECheckBoxState GetEditConditionCheckState() const;
	void OnEditConditionCheckChanged(ECheckBoxState InCheckState);

	void OnKeyframeClicked();
	bool IsKeyframeVisible() const;

	bool IsResetToDefaultVisible() const;
	void UpdateResetToDefault(float InDeltaTime);

	void OnResetToDefaultClicked();
	bool CanResetToDefault() const;

	FText GetResetToDefaultToolTip() const;
	FSlateIcon GetResetToDefaultIcon() const;

	TSharedPtr<IDetailTreeNode> GetRowTreeNode() const
	{
		return DetailTreeNodeWeak.Pin();
	}

	TSharedPtr<IPropertyHandle> GetRowPropertyHandle() const
	{
		return PropertyHandle;
	}

protected:
	/** Generate details context menu based on property handle */
	virtual TSharedPtr<SWidget> GenerateContextMenuWidget() override;

	/** The Property Handle of this Detail Tree Node. Can be null */
	TSharedPtr<IPropertyHandle> PropertyHandle;

	/** Cached list of Children gotten since this Item was last refreshed/generated */
	TArray<TSharedPtr<ICustomDetailsViewItem>> Children;

	/** Weak pointer to the Detail Tree Node this Item represents */
	TWeakPtr<IDetailTreeNode> DetailTreeNodeWeak;

	/** Handle to the delegate handling Updating Reset To Default Visibility State Post Slate App Tick */
	FDelegateHandle UpdateResetToDefaultHandle;

	/** Cached value of the visibility state of the ResetToDefault Widget */
	bool bResetToDefaultVisible = false;

	bool IsStruct() const;

	bool HasParentStruct() const;
};
