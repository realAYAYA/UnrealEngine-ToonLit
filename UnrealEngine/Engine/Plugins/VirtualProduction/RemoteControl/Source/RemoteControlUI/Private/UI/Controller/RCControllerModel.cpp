// Copyright Epic Games, Inc. All Rights Reserved.

#include "RCControllerModel.h"

#include "IDetailTreeNode.h"
#include "RemoteControlPreset.h"
#include "RCVirtualProperty.h"
#include "UI/SRemoteControlPanel.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "FRCControllerModel"

FRCControllerModel::FRCControllerModel(URCVirtualPropertyBase* InVirtualProperty, const TSharedRef<IDetailTreeNode>& InTreeNode, const TSharedPtr<SRemoteControlPanel> InRemoteControlPanel)
	: FRCLogicModeBase(InRemoteControlPanel)
	, VirtualPropertyWeakPtr(InVirtualProperty)
	, DetailTreeNodeWeakPtr(InTreeNode)
{
	if(ensure(InVirtualProperty))
	{
		if(InVirtualProperty->DisplayName.IsNone())
			InVirtualProperty->DisplayName = InVirtualProperty->PropertyName;

		SAssignNew(ControllerNameTextBox, SInlineEditableTextBlock)
			.Text(FText::FromName(InVirtualProperty->DisplayName))
			.OnTextCommitted_Raw(this, &FRCControllerModel::OnControllerNameCommitted);
	}

	Id = FGuid::NewGuid();
}

TSharedRef<SWidget> FRCControllerModel::GetWidget() const
{
	const FNodeWidgets NodeWidgets = DetailTreeNodeWeakPtr.Pin()->CreateNodeWidgets();

	const TSharedRef<SHorizontalBox> FieldWidget = SNew(SHorizontalBox);
	if (NodeWidgets.ValueWidget)
	{
		FieldWidget->AddSlot()
			.Padding(FMargin(10.0f, 2.0f))
			.HAlign(HAlign_Left)
			[
				NodeWidgets.ValueWidget.ToSharedRef()
			];
	}
	else if (NodeWidgets.WholeRowWidget)
	{
		FieldWidget->AddSlot()
			.Padding(FMargin(10.0f, 2.0f))
			[
				NodeWidgets.WholeRowWidget.ToSharedRef()
			];
	}

	return FieldWidget;
}

TSharedRef<SWidget> FRCControllerModel::GetNameWidget() const
{
	return SNew(SBox).Padding(10.f, 2.f)
		[
			ControllerNameTextBox.ToSharedRef()
		];
}

URCVirtualPropertyBase* FRCControllerModel::GetVirtualProperty() const
{
	return VirtualPropertyWeakPtr.Get();
}

const FName FRCControllerModel::GetPropertyName() const
{
	if (const URCVirtualPropertyBase* VirtualProperty = GetVirtualProperty())
	{
		return VirtualProperty->PropertyName;
	}

	return NAME_None;
}

TSharedPtr<FRCBehaviourModel> FRCControllerModel::GetSelectedBehaviourModel() const
{
	return SelectedBehaviourModelWeakPtr.Pin();
}

void FRCControllerModel::UpdateSelectedBehaviourModel(TSharedPtr<FRCBehaviourModel> InModel)
{
	SelectedBehaviourModelWeakPtr = InModel;
}

void FRCControllerModel::OnControllerNameCommitted(const FText& InNewControllerName, ETextCommit::Type InCommitInfo)
{
	if (URemoteControlPreset* Preset = GetPreset())
	{
		if(URCVirtualPropertyBase* Controller = GetVirtualProperty())
		{
			FName OldName = Controller->DisplayName;
			Controller->DisplayName = *InNewControllerName.ToString();
			ControllerNameTextBox->SetText(InNewControllerName);
			Preset->OnControllerRenamed().Broadcast(Preset, OldName, *InNewControllerName.ToString());
		}
	}
}

void FRCControllerModel::EnterRenameMode()
{
	ControllerNameTextBox->EnterEditingMode();
}

FName FRCControllerModel::GetControllerDisplayName()
{
	if (URCVirtualPropertyBase* Controller = GetVirtualProperty())
	{
		return Controller->DisplayName;
	}

	return NAME_None;
}

#undef LOCTEXT_NAMESPACE