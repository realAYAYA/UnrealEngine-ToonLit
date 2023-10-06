// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AnimNodes/AnimNode_BlendListBase.h"
#include "AnimNode_BlendListByBool.generated.h"

// This node is effectively a 'branch', picking one of two input poses based on an input Boolean value
USTRUCT(BlueprintInternalUseOnly)
struct FAnimNode_BlendListByBool : public FAnimNode_BlendListBase
{
	GENERATED_BODY()
private:
#if WITH_EDITORONLY_DATA
	// Which input should be connected to the output?
	UPROPERTY(EditAnywhere, Category=Runtime, meta=(PinShownByDefault, FoldProperty))
	bool bActiveValue = false;
#endif
	
public:	
	FAnimNode_BlendListByBool() = default;

	// Get which input should be connected to the output
	ANIMGRAPHRUNTIME_API bool GetActiveValue() const;
	
protected:
	ANIMGRAPHRUNTIME_API virtual int32 GetActiveChildIndex() override;
	virtual FString GetNodeName(FNodeDebugData& DebugData) override { return DebugData.GetNodeName(this); }
};
