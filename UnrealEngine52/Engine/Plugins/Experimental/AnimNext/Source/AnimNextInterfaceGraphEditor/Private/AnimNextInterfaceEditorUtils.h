// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UAnimNextInterfaceGraph;
class UAnimNextInterfaceGraph_EditorData;

namespace UE::AnimNext::InterfaceGraphEditor
{

struct FUtils
{
	static FName ValidateName(const UAnimNextInterfaceGraph_EditorData* InEditorData, const FString& InName);

	static void GetAllGraphNames(const UAnimNextInterfaceGraph_EditorData* InEditorData, TSet<FName>& OutNames);
};

}
