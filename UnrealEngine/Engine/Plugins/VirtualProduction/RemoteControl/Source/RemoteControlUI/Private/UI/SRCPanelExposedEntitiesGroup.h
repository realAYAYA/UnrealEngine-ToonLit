// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SRCPanelExposedEntity.h"
#include "SRCPanelTreeNode.h"
#include "Widgets/SCompoundWidget.h"

/** Grouping type available */
enum class EFieldGroupType
{
	None,
	PropertyId,
	Owner
};

ENUM_CLASS_FLAGS(EFieldGroupType)

/** Delegate called when the PropertyId of a group change */
DECLARE_DELEGATE(FOnGroupPropertyIdChanged)

/** Widget that is used as a parent for field widget */
class SRCPanelExposedEntitiesGroup : public SRCPanelTreeNode
{
	SLATE_BEGIN_ARGS(SRCPanelExposedEntitiesGroup)
	{}

	SLATE_EVENT(FOnGroupPropertyIdChanged, OnGroupPropertyIdChanged)
	SLATE_ARGUMENT(FName, FieldKey)

	SLATE_END_ARGS()

	//~ Begin SRCPanelTreeNode interface

	/** Get get this node's type. */
	virtual ENodeType GetRCType() const override { return ENodeType::FieldGroup; }

	//~ End SRCPanelTreeNode interface

	void Construct(const FArguments& InArgs, EFieldGroupType InFieldGroupType, URemoteControlPreset* Preset);

	/**
	 * Take the current fields in RC and assign them to its child widgets based on the group key
	 * @param InFieldEntities Current fields exposed in RC
	 */
	void AssignChildren(const TArray<TSharedPtr<SRCPanelTreeNode>>& InFieldEntities);

	/** Get this tree node's children. */
	virtual void GetNodeChildren(TArray<TSharedPtr<SRCPanelTreeNode>>& OutChildren) const override;

	/** Returns true if this tree node has children. */
	virtual bool HasChildren() const override { return true; }

	/** Get the group type of this group */
	EFieldGroupType GetGroupType() const { return GroupType; }

	/** Get the field key of this group */
	FName GetFieldKey() const { return FieldKey; }

private:
	/** Create the node Args for this class that will be used to create the widget */
	FMakeNodeWidgetArgs CreateNodeWidgetArgs();

	/**
	 * Called when the text label is renamed
	 * @param InText New Text
	 * @param InTextCommitType Type of commit, look at ETextCommit for more information
	 */
	void OnPropertyIdTextCommitted(const FText& InText, ETextCommit::Type InTextCommitType);

private:
	/** Child widgets of this group */
	TArray<TSharedPtr<SRCPanelTreeNode>> ChildWidgets;

	/** Field key value of this group */
	FName FieldKey;

	/** Field group type of this group */
	EFieldGroupType GroupType = EFieldGroupType::None;

	/** Weak ptr to the preset where this group reside */
	TWeakObjectPtr<URemoteControlPreset> PresetWeak;

	/** Delegate called when the group type is PropertyId and the value change */
	FOnGroupPropertyIdChanged OnGroupPropertyIdChangedDelegate;
};
