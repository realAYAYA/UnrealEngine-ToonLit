// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UDataInterfaceGraph;
class UDataInterfaceGraph_EditorData;
class UDataInterfaceGraph_EdGraph;
class URigVMController;
class URigVMGraph;

namespace UE::DataInterfaceGraphEditor
{
	class FGraphEditor;
}

namespace UE::DataInterfaceGraphUncookedOnly
{

struct DATAINTERFACEGRAPHUNCOOKEDONLY_API FUtils
{
	static void Compile(UDataInterfaceGraph* InGraph);
	
	static UDataInterfaceGraph_EditorData* GetEditorData(const UDataInterfaceGraph* InDataInterfaceGraph);
	
	static void RecreateVM(UDataInterfaceGraph* InGraph);
};

}