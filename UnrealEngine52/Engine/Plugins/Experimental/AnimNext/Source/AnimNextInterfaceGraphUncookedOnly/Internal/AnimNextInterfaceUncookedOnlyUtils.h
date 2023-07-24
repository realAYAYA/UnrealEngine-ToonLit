// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimNextInterfaceGraph;
class UAnimNextInterfaceGraph_EditorData;
class UAnimNextInterfaceGraph_EdGraph;
class URigVMController;
class URigVMGraph;

namespace UE::AnimNext::InterfaceGraphEditor
{
	class FGraphEditor;
}

namespace UE::AnimNext::InterfaceGraphUncookedOnly
{

struct ANIMNEXTINTERFACEGRAPHUNCOOKEDONLY_API FUtils
{
	static void Compile(UAnimNextInterfaceGraph* InGraph);
	
	static UAnimNextInterfaceGraph_EditorData* GetEditorData(const UAnimNextInterfaceGraph* InAnimNextInterfaceGraph);
	
	static void RecreateVM(UAnimNextInterfaceGraph* InGraph);
};

}