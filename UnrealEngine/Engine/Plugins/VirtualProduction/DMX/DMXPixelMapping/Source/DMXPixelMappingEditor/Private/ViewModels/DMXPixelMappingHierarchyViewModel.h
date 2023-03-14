// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DMXPixelMappingEditorCommon.h"
#include "DMXPixelMappingComponentReference.h"

#include "Types/SlateEnums.h"
#include "Delegates/Delegate.h"
#include "Types/SlateEnums.h"

class FDMXPixelMappingHierarchyItemWidgetModel
	: public TSharedFromThis<FDMXPixelMappingHierarchyItemWidgetModel>
{
public:
	/** Root Item Constructor */
	FDMXPixelMappingHierarchyItemWidgetModel(FDMXPixelMappingToolkitPtr InToolkit);

	/** Child Item Constructor */
	FDMXPixelMappingHierarchyItemWidgetModel(FDMXPixelMappingComponentReference InReference, FDMXPixelMappingToolkitPtr InToolkit);

	FText GetText() const;

	bool OnVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage);

	void OnNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo);

	void RequestBeginRename();

	void OnSelection();

	bool IsSelected() const { return bIsSelected; }

	void RefreshSelection();

	bool ContainsSelection();

	void GetFilterStrings(TArray<FString>& OutStrings) const { OutStrings.Add(GetText().ToString()); }

	const FDMXPixelMappingComponentReference& GetReference() const { return Reference; }

	void GatherChildren(FDMXPixelMappingHierarchyItemWidgetModelArr& Children);

private:
	void GetChildren(FDMXPixelMappingHierarchyItemWidgetModelArr& Children);

	void InitializeChildren();

	void UpdateSelection();

public:
	FSimpleDelegate RenameEvent;

private:
	FDMXPixelMappingHierarchyItemWidgetModelArr Models;

	FDMXPixelMappingToolkitWeakPtr ToolkitWeakPtr;

	FDMXPixelMappingComponentReference Reference;

	bool bInitialized;

	bool bIsSelected;

	TSet<FDMXPixelMappingComponentReference> SelectedComponentReferences;
};


