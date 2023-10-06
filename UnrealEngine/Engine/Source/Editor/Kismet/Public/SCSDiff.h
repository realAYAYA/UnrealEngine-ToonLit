// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "CoreMinimal.h"
#include "DiffUtils.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"
#include "Widgets/SWidget.h"

class FSCSEditorTreeNode;
class FSubobjectEditorTreeNode;
class SKismetInspector;
class SSCSEditor;
class SSubobjectBlueprintEditor;
class SWidget;
class UBlueprint;

/** Struct to support diffing the component tree for a blueprint */
class KISMET_API FSCSDiff
{
public:
	FSCSDiff(const class UBlueprint* InBlueprint);

	void HighlightProperty(FName VarName, FPropertySoftPath Property);
	TSharedRef< SWidget > TreeWidget();

	TArray< FSCSResolvedIdentifier > GetDisplayedHierarchy() const;

	const UBlueprint* GetBlueprint() const { return Blueprint; }

protected:
	void OnSCSEditorUpdateSelectionFromNodes(const TArray<TSharedPtr<FSubobjectEditorTreeNode>>& SelectedNodes);
	void OnSCSEditorHighlightPropertyInDetailsView(const class FPropertyPath& InPropertyPath);

private:
	TSharedPtr< class SWidget > ContainerWidget;
	TSharedPtr< class SSubobjectBlueprintEditor > SubobjectEditor;
	TSharedPtr< class SKismetInspector > Inspector;

	/** Blueprint we are inspecting */
	UBlueprint* Blueprint = nullptr;
};
