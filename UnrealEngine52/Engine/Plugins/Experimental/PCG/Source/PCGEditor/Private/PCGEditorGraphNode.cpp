// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorGraphNode.h"

#include "PCGEditorModule.h"
#include "PCGNode.h"
#include "PCGSettings.h"
#include "PCGSubsystem.h"

#include "Framework/Commands/GenericCommands.h"
#include "PCGPin.h"
#include "ToolMenu.h"
#include "ToolMenuSection.h"

#define LOCTEXT_NAMESPACE "PCGEditorGraphNode"

namespace UPCGEditorGraphNodeHelpers
{
	// Info to aid element cache analysis / debugging
	void GetGraphCacheDebugInfo(const UPCGNode* InNode, bool& bOutDebuggingEnabled, uint32& OutNumCacheEntries)
	{
		UWorld* World = GEditor ? (GEditor->PlayWorld ? GEditor->PlayWorld.Get() : GEditor->GetEditorWorldContext().World()) : nullptr;
		UPCGSubsystem* Subsystem = UPCGSubsystem::GetInstance(World);
		bOutDebuggingEnabled = Subsystem && Subsystem->IsGraphCacheDebuggingEnabled();

		if (bOutDebuggingEnabled)
		{
			IPCGElement* Element = (InNode && InNode->GetSettings()) ? InNode->GetSettings()->GetElement().Get() : nullptr;
			OutNumCacheEntries = Element ? Subsystem->GetGraphCacheEntryCount(Element) : 0;
		}
	}
}

UPCGEditorGraphNode::UPCGEditorGraphNode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bCanRenameNode = true;
}

FText UPCGEditorGraphNode::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	constexpr int32 NodeTitleMaxSize = 70;

	FString Result;
	if (PCGNode)
	{
		Result = PCGNode->GetNodeTitle().ToString();
		Result = (Result.Len() > NodeTitleMaxSize) ? Result.Left(NodeTitleMaxSize) : Result;
	}
	else
	{
		Result = FString(TEXT("Unnamed node"));
	}

	// Debug info - append how many copies of this element are currently in the cache to the node title
	bool bDebuggingEnabled;
	uint32 NumCacheEntries;
	UPCGEditorGraphNodeHelpers::GetGraphCacheDebugInfo(PCGNode.Get(), bDebuggingEnabled, NumCacheEntries);
	if (bDebuggingEnabled)
	{
		Result = FString::Format(TEXT("{0} [{1}]"), { Result, NumCacheEntries });
	}

	return FText::FromString(Result);
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

	if(PCGNode->GetNodeTitle().ToString() != NewName)
	{
		PCGNode->Modify();
		PCGNode->NodeTitle = FName(*NewName);
	}
}

#undef LOCTEXT_NAMESPACE
