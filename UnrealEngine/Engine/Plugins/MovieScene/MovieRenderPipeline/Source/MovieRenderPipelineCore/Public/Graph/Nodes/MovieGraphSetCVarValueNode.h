// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"

#include "MovieGraphSetCVarValueNode.generated.h"

/** A node which can set a specific console variable's value. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphSetCVarValueNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()
public:
	UMovieGraphSetCVarValueNode() = default;

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
	uint8 bOverride_Name : 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_Value : 1;

	/** The name of the CVar having its value set. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (EditCondition = "bOverride_Name"))
	FString Name;

	/** The new value of the CVar. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (EditCondition = "bOverride_Value"))
	float Value;
};