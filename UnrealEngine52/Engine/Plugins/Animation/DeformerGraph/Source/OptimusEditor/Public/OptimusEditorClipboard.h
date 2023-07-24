// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"


class UClass;
class UObject;
class UOptimusNodeGraph;
class UOptimusNode;


class OPTIMUSEDITOR_API FOptimusEditorClipboard
{
public:
	static void SetClipboardFromNodes(const TArray<UOptimusNode*>& InNodes);

	// Creates a self-contained 
	static UOptimusNodeGraph* GetGraphFromClipboardContent(
		UPackage* InTargetPackage
		);

	static bool HasValidClipboardContent();

private:
	static bool CanCreateObjectsFromText(const TCHAR* InBuffer);

	static bool ProcessObjectBuffer(UPackage* InPackage, UObject* InRootOuter, const TCHAR* InBuffer);
	static bool CanCreateClass(const UClass* ObjectClass);
	static bool ProcessPostCreateObject(UObject* InRootOuter, UObject* InNewObject);
};
