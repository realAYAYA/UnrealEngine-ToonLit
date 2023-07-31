// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Behaviour/RCBehaviourNode.h"
#include "RCBehaviourConditionalNode.generated.h"

/**
 * Compares given property value with controller property value
 */
UCLASS()
class REMOTECONTROLLOGIC_API URCBehaviourConditionalNode : public URCBehaviourNode
{
	GENERATED_BODY()

public:
	
	URCBehaviourConditionalNode();

	//~ Begin URCBehaviourNode interface
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Remote Control Behaviour")
	bool Execute(URCBehaviour* InBehaviour) const override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Remote Control Behaviour")
	bool IsSupported(URCBehaviour* InBehaviour) const override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Remote Control Behaviour")
	void OnPassed(URCBehaviour* InBehaviour) const;

	UClass* GetBehaviourClass() const override;
	//~ End URCBehaviourNode interface
};
