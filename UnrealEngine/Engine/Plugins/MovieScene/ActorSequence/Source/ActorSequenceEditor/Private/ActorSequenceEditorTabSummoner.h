// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WorkflowOrientedApp/WorkflowUObjectDocuments.h"


class FBlueprintEditor;
class SActorSequenceEditorWidgetImpl;
class UActorSequence;
class UActorSequenceComponent;


class SActorSequenceEditorWidget
	: public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SActorSequenceEditorWidget){}
	SLATE_END_ARGS();

	void Construct(const FArguments&, TWeakPtr<FBlueprintEditor> InBlueprintEditor);
	void AssignSequence(UActorSequence* NewActorSequence);
	UActorSequence* GetSequence() const;
	FText GetDisplayLabel() const;

private:

	TWeakPtr<SActorSequenceEditorWidgetImpl> Impl;
};


struct FActorSequenceEditorSummoner
	: public FWorkflowTabFactory
{
	FActorSequenceEditorSummoner(TSharedPtr<FBlueprintEditor> BlueprintEditor);

	virtual TSharedRef<SWidget> CreateTabBody(const FWorkflowTabSpawnInfo& Info) const override;

protected:

	TWeakObjectPtr<UActorSequenceComponent> WeakComponent;
	TWeakPtr<FBlueprintEditor> WeakBlueprintEditor;
};
