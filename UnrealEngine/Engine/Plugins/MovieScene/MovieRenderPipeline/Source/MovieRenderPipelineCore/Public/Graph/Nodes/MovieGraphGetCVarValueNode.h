// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Graph/MovieGraphNode.h"

#include "MovieGraphGetCVarValueNode.generated.h"

/** A node which can get a specific console variable's value. */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphGetCVarValueNode : public UMovieGraphSettingNode
{
	GENERATED_BODY()
public:
	UMovieGraphGetCVarValueNode() = default;

	virtual TArray<FMovieGraphPinProperties> GetOutputPinProperties() const override;
	virtual FString GetNodeInstanceName() const override { return Name; }

#if WITH_EDITOR
	virtual FText GetNodeTitle(const bool bGetDescriptive = false) const override;
	virtual FText GetMenuCategory() const override;
	virtual FLinearColor GetNodeTitleColor() const override;
	virtual FSlateIcon GetIconAndTint(FLinearColor& OutColor) const override;
	
	//~ Begin UObject interface
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//~ End UObject interface
#endif

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Overrides, meta = (InlineEditConditionToggle))
	uint8 bOverride_Name : 1;

	/** The name of the CVar that will have its value fetched. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings", meta = (EditCondition = "bOverride_Name"))
	FString Name;
};