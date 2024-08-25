// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaRundownPageRemoteControlWidgetUtils.h"
#include "IDetailTreeNode.h"
#include "PropertyHandle.h"
#include "SlateOptMacros.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "AvaRundownPageRemoteControlWidgetUtils"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> FAvaRundownPageRemoteControlWidgetUtils::CreateNodeValueWidget(const TSharedPtr<IDetailTreeNode>& InNode)
{
	FNodeWidgets NodeWidgets = InNode->CreateNodeWidgets();

	TSharedRef<SHorizontalBox> FieldWidget = SNew(SHorizontalBox);

	if (NodeWidgets.ValueWidget)
	{
		FieldWidget->AddSlot()
			.Padding(FMargin(3.0f, 2.0f))
			.HAlign(HAlign_Right)
			.FillWidth(1.0f)
			[
				NodeWidgets.ValueWidget.ToSharedRef()
			];
	}
	else if (NodeWidgets.WholeRowWidget)
	{
		FieldWidget->AddSlot()
			.Padding(FMargin(3.0f, 2.0f))
			.FillWidth(1.0f)
			[
				NodeWidgets.WholeRowWidget.ToSharedRef()
			];
	}

	return FieldWidget;
}

bool FAvaRundownPageRemoteControlWidgetUtils::FindPropertyHandleRecursive(const TSharedPtr<IPropertyHandle>& InPropertyHandle, const FString& InPropertyNameOrPath, EFindNodeMethod InFindMethod)
{
	if (InPropertyHandle && InPropertyHandle->IsValidHandle())
	{
		uint32 ChildrenCount = 0;
		InPropertyHandle->GetNumChildren(ChildrenCount);
		for (uint32 Index = 0; Index < ChildrenCount; ++Index)
		{
			TSharedPtr<IPropertyHandle> ChildHandle = InPropertyHandle->GetChildHandle(Index);
			if (FindPropertyHandleRecursive(ChildHandle, InPropertyNameOrPath, InFindMethod))
			{
				return true;
			}
		}

		if (InPropertyHandle->GetProperty())
		{
			if (InFindMethod == EFindNodeMethod::Path)
			{
				if (InPropertyHandle->GeneratePathToProperty() == InPropertyNameOrPath)
				{
					return true;
				}
			}
			else if (InPropertyHandle->GetProperty()->GetName() == InPropertyNameOrPath)
			{
				return true;
			}
		}
	}

	return false;
}

TSharedPtr<IDetailTreeNode> FAvaRundownPageRemoteControlWidgetUtils::FindTreeNodeRecursive(const TSharedRef<IDetailTreeNode>& InRootNode, const FString& InPropertyNameOrPath, EFindNodeMethod InFindMethod)
{
	TArray<TSharedRef<IDetailTreeNode>> Children;
	InRootNode->GetChildren(Children);
	for (TSharedRef<IDetailTreeNode>& Child : Children)
	{
		TSharedPtr<IDetailTreeNode> FoundNode = FindTreeNodeRecursive(Child, InPropertyNameOrPath, InFindMethod);
		if (FoundNode.IsValid())
		{
			return FoundNode;
		}
	}

	TSharedPtr<IPropertyHandle> Handle = InRootNode->CreatePropertyHandle();
	if (FindPropertyHandleRecursive(Handle, InPropertyNameOrPath, InFindMethod))
	{
		return InRootNode;
	}

	return nullptr;
}

TSharedPtr<IDetailTreeNode> FAvaRundownPageRemoteControlWidgetUtils::FindNode(const TArray<TSharedRef<IDetailTreeNode>>& InRootNodes, const FString& InQualifiedPropertyName, EFindNodeMethod InFindMethod)
{
	for (const TSharedRef<IDetailTreeNode>& CategoryNode : InRootNodes)
	{
		TSharedPtr<IDetailTreeNode> FoundNode = FindTreeNodeRecursive(CategoryNode, InQualifiedPropertyName, InFindMethod);
		if (FoundNode.IsValid())
		{
			return FoundNode;
		}
	}

	return nullptr;
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

#undef LOCTEXT_NAMESPACE
