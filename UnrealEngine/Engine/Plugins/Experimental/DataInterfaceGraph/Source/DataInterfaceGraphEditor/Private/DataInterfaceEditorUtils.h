// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UDataInterfaceGraph;
class UDataInterfaceGraph_EditorData;

namespace UE::DataInterfaceGraphEditor
{

struct FUtils
{
	static FName ValidateName(const UDataInterfaceGraph_EditorData* InEditorData, const FString& InName);

	static void GetAllGraphNames(const UDataInterfaceGraph_EditorData* InEditorData, TSet<FName>& OutNames);
};

}
