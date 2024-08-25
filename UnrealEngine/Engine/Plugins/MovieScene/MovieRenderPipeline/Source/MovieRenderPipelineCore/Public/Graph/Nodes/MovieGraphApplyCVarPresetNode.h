// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"

#include "MovieGraphApplyCVarPresetNode.generated.h"

class IMovieSceneConsoleVariableTrackInterface;

/** A node which can apply a console variable preset. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphApplyCVarPresetNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()
	
public:
	UMovieGraphApplyCVarPresetNode() = default;

	virtual FString GetNodeInstanceName() const override;
	virtual EMovieGraphBranchRestriction GetBranchRestriction() const override;

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FText GetKeywords() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;

	//~ Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface
#endif

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_ConsoleVariablePreset : 1;

	/** The console variable preset that should be applied. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (EditCondition = "bOverride_ConsoleVariablePreset"))
	TScriptInterface<IMovieSceneConsoleVariableTrackInterface> ConsoleVariablePreset;
};