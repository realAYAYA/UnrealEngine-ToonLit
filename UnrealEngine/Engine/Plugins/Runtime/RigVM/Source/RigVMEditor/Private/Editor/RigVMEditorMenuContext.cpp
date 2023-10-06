// Copyright Epic Games, Inc. All Rights Reserved.

#include "Editor/RigVMEditorMenuContext.h"

#include "RigVMBlueprint.h"
#include "Editor/RigVMEditor.h"
#include "Framework/Application/SlateApplication.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigVMEditorMenuContext)

void URigVMEditorMenuContext::Init(TWeakPtr<FRigVMEditor> InRigVMEditor, const FRigVMEditorGraphMenuContext& InGraphMenuContext)
{
	WeakRigVMEditor = InRigVMEditor;
	GraphMenuContext = InGraphMenuContext;
}

URigVMBlueprint* URigVMEditorMenuContext::GetRigVMBlueprint() const
{
	if (const TSharedPtr<FRigVMEditor> Editor = WeakRigVMEditor.Pin())
	{
		return Editor->GetRigVMBlueprint();
	}
	
	return nullptr;
}

URigVMHost* URigVMEditorMenuContext::GetRigVMHost() const
{
	if (URigVMBlueprint* RigBlueprint = GetRigVMBlueprint())
	{
		if (URigVMHost* Host = Cast<URigVMHost>(RigBlueprint->GetObjectBeingDebugged()))
		{
			return Host;
		}
	}
	return nullptr;
}

bool URigVMEditorMenuContext::IsAltDown() const
{
	return FSlateApplication::Get().GetModifierKeys().IsAltDown();
}

FRigVMEditorGraphMenuContext URigVMEditorMenuContext::GetGraphMenuContext()
{
	return GraphMenuContext;
}

FRigVMEditor* URigVMEditorMenuContext::GetRigVMEditor() const
{
	if (const TSharedPtr<FRigVMEditor> Editor = WeakRigVMEditor.Pin())
	{
		return Editor.Get();
	}
	return nullptr;
}

