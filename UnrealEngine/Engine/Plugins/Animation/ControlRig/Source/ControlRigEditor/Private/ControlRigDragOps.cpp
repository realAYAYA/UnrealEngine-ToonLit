// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlRigDragOps.h"
#include "Editor/SRigHierarchyTagWidget.h"

//////////////////////////////////////////////////////////////
/// FRigElementHierarchyDragDropOp
///////////////////////////////////////////////////////////

TSharedRef<FRigElementHierarchyDragDropOp> FRigElementHierarchyDragDropOp::New(const TArray<FRigElementKey>& InElements)
{
	TSharedRef<FRigElementHierarchyDragDropOp> Operation = MakeShared<FRigElementHierarchyDragDropOp>();
	Operation->Elements = InElements;
	Operation->Construct();
	return Operation;
}

TSharedPtr<SWidget> FRigElementHierarchyDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(STextBlock)
			.Text(FText::FromString(GetJoinedElementNames()))
			//.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
		];
}

FString FRigElementHierarchyDragDropOp::GetJoinedElementNames() const
{
	TArray<FString> ElementNameStrings;
	for (const FRigElementKey& Element: Elements)
	{
		ElementNameStrings.Add(Element.Name.ToString());
	}
	return FString::Join(ElementNameStrings, TEXT(","));
}

bool FRigElementHierarchyDragDropOp::IsDraggingSingleConnector() const
{
	if(Elements.Num() == 1)
	{
		return Elements[0].Type == ERigElementType::Connector;
	}
	return false;
}

bool FRigElementHierarchyDragDropOp::IsDraggingSingleSocket() const
{
	if(Elements.Num() == 1)
	{
		return Elements[0].Type == ERigElementType::Socket;
	}
	return false;
}

//////////////////////////////////////////////////////////////
/// FRigHierarchyTagDragDropOp
///////////////////////////////////////////////////////////

TSharedRef<FRigHierarchyTagDragDropOp> FRigHierarchyTagDragDropOp::New(TSharedPtr<SRigHierarchyTagWidget> InTagWidget)
{
	TSharedRef<FRigHierarchyTagDragDropOp> Operation = MakeShared<FRigHierarchyTagDragDropOp>();
	Operation->Text = InTagWidget->Text.Get();
	Operation->Identifier = InTagWidget->Identifier.Get();
	Operation->Construct();
	return Operation;
}

TSharedPtr<SWidget> FRigHierarchyTagDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(STextBlock)
			.Text(Text)
		];
}

//////////////////////////////////////////////////////////////
/// FModularRigModuleDragDropOp
///////////////////////////////////////////////////////////

TSharedRef<FModularRigModuleDragDropOp> FModularRigModuleDragDropOp::New(const TArray<FString>& InElements)
{
	TSharedRef<FModularRigModuleDragDropOp> Operation = MakeShared<FModularRigModuleDragDropOp>();
	Operation->Elements = InElements;
	Operation->Construct();
	return Operation;
}

TSharedPtr<SWidget> FModularRigModuleDragDropOp::GetDefaultDecorator() const
{
	return SNew(SBorder)
		.Visibility(EVisibility::Visible)
		.BorderImage(FAppStyle::GetBrush("Menu.Background"))
		[
			SNew(STextBlock)
			.Text(FText::FromString(GetJoinedElementNames()))
			//.Font(FAppStyle::Get().GetFontStyle("FontAwesome.10"))
		];
}

FString FModularRigModuleDragDropOp::GetJoinedElementNames() const
{
	TArray<FString> ElementNameStrings;
	for (const FString& Element: Elements)
	{
		ElementNameStrings.Add(Element);
	}
	return FString::Join(ElementNameStrings, TEXT(","));
}
