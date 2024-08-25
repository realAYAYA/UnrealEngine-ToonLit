// Copyright Epic Games, Inc. All Rights Reserved.

#include "SRCPanelExposedEntitiesGroup.h"

#include "RemoteControlPreset.h"
#include "SRCPanelExposedField.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SEditableTextBox.h"

void SRCPanelExposedEntitiesGroup::Construct(const FArguments& InArgs, EFieldGroupType InFieldGroupType, URemoteControlPreset* Preset)
{
	FieldKey = InArgs._FieldKey;
	GroupType = InFieldGroupType;
	PresetWeak = Preset;
	OnGroupPropertyIdChangedDelegate = InArgs._OnGroupPropertyIdChanged;

	const FMakeNodeWidgetArgs Args = CreateNodeWidgetArgs();
	MakeNodeWidget(Args);
}

SRCPanelTreeNode::FMakeNodeWidgetArgs SRCPanelExposedEntitiesGroup::CreateNodeWidgetArgs()
{
	FMakeNodeWidgetArgs Args;

	if (GroupType == EFieldGroupType::PropertyId)
	{
		Args.PropertyIdWidget = SNew(SBox)
		[
			SNew(SEditableTextBox)
			.Justification(ETextJustify::Center)
			.MinDesiredWidth(50.f)
			.SelectAllTextWhenFocused(true)
			.RevertTextOnEscape(true)
			.ClearKeyboardFocusOnCommit(true)
			.Text_Lambda([this] () { return FText::FromName(FieldKey); })
			.OnTextCommitted(this, &SRCPanelExposedEntitiesGroup::OnPropertyIdTextCommitted)
		];

		Args.OwnerNameWidget = SNullWidget::NullWidget;
	}
	else if (GroupType == EFieldGroupType::Owner)
	{
		Args.PropertyIdWidget = SNullWidget::NullWidget;
		Args.OwnerNameWidget = SNew(SBox)
			.HeightOverride(25)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromName(FieldKey))
			];
	}
	else
	{
		Args.PropertyIdWidget = SNullWidget::NullWidget;
		Args.OwnerNameWidget = SNullWidget::NullWidget;
	}

	Args.NameWidget = SNew(SBox).HeightOverride(25).VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Text(FText::FromString("Group"))
		];
	Args.SubObjectPathWidget = SNullWidget::NullWidget;
	Args.ValueWidget = SNullWidget::NullWidget;
	Args.ResetButton = SNullWidget::NullWidget;

	return Args;
}

void SRCPanelExposedEntitiesGroup::OnPropertyIdTextCommitted(const FText& InText, ETextCommit::Type InTextCommitType)
{
	if (!PresetWeak.IsValid() || PresetWeak.IsStale())
	{
		return;
	}

	const FName NewId = FName(InText.ToString());

	if (FieldKey.Compare(NewId) == 0)
	{
		return;
	}

	URemoteControlPreset* Preset = PresetWeak.Get();
	FieldKey = NewId;
	for (const TSharedPtr<SRCPanelTreeNode>& Child : ChildWidgets)
	{
		TWeakPtr<FRemoteControlField> ExposedField = Preset->GetExposedEntity<FRemoteControlField>(Child->GetRCId());
		if (ExposedField.IsValid())
		{
			ExposedField.Pin()->PropertyId = NewId;
			Child->SetPropertyId(NewId);
			Preset->UpdateIdentifiedField(ExposedField.Pin().ToSharedRef());
		}
	}
	
	OnGroupPropertyIdChangedDelegate.ExecuteIfBound();
}

void SRCPanelExposedEntitiesGroup::AssignChildren(const TArray<TSharedPtr<SRCPanelTreeNode>>& InFieldEntities)
{
	ChildWidgets.Empty();
	if (GroupType == EFieldGroupType::PropertyId)
	{
		for (const TSharedPtr<SRCPanelTreeNode>& Entity : InFieldEntities)
		{
			if (const TSharedPtr<SRCPanelExposedField> ExposedField = StaticCastSharedPtr<SRCPanelExposedField>(Entity))
			{
				if (ExposedField->GetPropertyId() == FieldKey)
				{
					ChildWidgets.Add(Entity);
				}
			}
		}
	}
	else if (GroupType == EFieldGroupType::Owner)
	{
		for (const TSharedPtr<SRCPanelTreeNode>& Entity : InFieldEntities)
		{
			if (const TSharedPtr<SRCPanelExposedField> ExposedField = StaticCastSharedPtr<SRCPanelExposedField>(Entity))
			{
				if (ExposedField->GetOwnerName() == FieldKey)
				{
					ChildWidgets.Add(Entity);
				}
			}
		}
	}
}

void SRCPanelExposedEntitiesGroup::GetNodeChildren(TArray<TSharedPtr<SRCPanelTreeNode>>& OutChildren) const
{
	OutChildren.Append(ChildWidgets);
	SRCPanelTreeNode::GetNodeChildren(OutChildren);
}
