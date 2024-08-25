// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphNode.h"

#include "PCGNode.h"
#include "PCGPin.h"
#include "PCGSettings.h"
#include "PCGSubsystem.h"

#include "PCGEditorModule.h"

#include "ToolMenu.h"
#include "ToolMenuSection.h"
#include "Framework/Commands/GenericCommands.h"

#define LOCTEXT_NAMESPACE "PCGEditorGraphNode"

UPCGEditorGraphNode::UPCGEditorGraphNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = true;
}

FText UPCGEditorGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	if (!PCGNode)
	{
		return NSLOCTEXT("PCGEditorGraphNode", "UnnamedNodeTitle", "Unnamed Node");
	}

	if (TitleType == ENodeTitleType::FullTitle)
	{
		return PCGNode->GetNodeTitle(EPCGNodeTitleType::FullTitle);
	}
	else
	{
		return PCGNode->GetNodeTitle(EPCGNodeTitleType::ListView);
	}
}

void UPCGEditorGraphNode::GetNodeContextMenuActions(UToolMenu* Menu, class UGraphNodeContextMenuContext* Context) const
{
	if (!Context->Node)
	{
		return;
	}

	FToolMenuSection& Section = Menu->AddSection("EdGraphSchemaGeneral", LOCTEXT("GeneralHeader", "General"));
	Section.AddMenuEntry(FGenericCommands::Get().Delete);
	Section.AddMenuEntry(FGenericCommands::Get().Cut);
	Section.AddMenuEntry(FGenericCommands::Get().Copy);
	Section.AddMenuEntry(FGenericCommands::Get().Duplicate);

	Super::GetNodeContextMenuActions(Menu, Context);
}

void UPCGEditorGraphNode::AllocateDefaultPins()
{
	if (PCGNode)
	{
		CreatePins(PCGNode->GetInputPins(), PCGNode->GetOutputPins());
	}
}

void UPCGEditorGraphNode::OnRenameNode(const FString& NewName)
{
	if (!GetCanRenameNode())
	{
		return;
	}

	if(NewName.Len() >= NAME_SIZE)
	{
		UE_LOG(LogPCGEditor, Error, TEXT("New name for PCG node is too long."));
		return;
	}

	if (!PCGNode)
	{
		return;
	}

	if (PCGNode->GetAuthoredTitleLine().ToString() != NewName)
	{
		Modify();
		PCGNode->Modify();
		PCGNode->NodeTitle = FName(*NewName);
	}
}

#undef LOCTEXT_NAMESPACE
