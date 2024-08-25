// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Widgets/SCompoundWidget.h"

class FSubobjectEditorTreeNode;
class AActor;
class SBox;
class SSplitter;
class IDetailsView;
class SSubobjectEditor;
class UObject;

/**
 * Simple details view with SubObject Editor
 */
class CHAOSVD_API SChaosVDDetailsView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SChaosVDDetailsView)
		{
		}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/** Updates the current object this view details is viewing */
	void SetSelectedObject(UObject* NewObject);

protected:
	UObject* GetRootContextObject() const { return CurrentObjectInView.IsValid() ? CurrentObjectInView.Get() : nullptr; };
	
	void OnSelectedSubobjectsChanged(const TArray<TSharedPtr<FSubobjectEditorTreeNode>>& SelectedNodes);

	TWeakObjectPtr<UObject> CurrentObjectInView;
	
	TSharedPtr<IDetailsView> DetailsView;

	// The subobject editor provides a tree widget that allows for editing of subobjects
	TSharedPtr<SSubobjectEditor> SubobjectEditor;
};
