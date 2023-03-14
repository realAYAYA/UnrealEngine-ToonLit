// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPath.h"
#include "Agents/MLAdapterAgent.h"
#include "NeuralNetwork.h"
#include "MLAdapterAgent_Inference.generated.h"

/**
 * Inference agents have a neural network that can process senses and output actuations via their Think method. You 
 * can create a blueprint of this class to easily wire-up an agent that functions entirely inside the Unreal Engine.
 * @see UMLAdapterNoRPCManager - we do not usually need to open the RPC interface if we are exclusively using inference agents.
 */
UCLASS(Blueprintable, EditInlineNew)
class MLADAPTER_API UMLAdapterAgent_Inference : public UMLAdapterAgent
{
	GENERATED_BODY()
public:
	/** Tries to load the neural network from the NeuralNetworkPath. */
	virtual void PostInitProperties() override;

	/** Process observations from the sensors with the neural network and populate the actuators with data. */
	virtual void Think(const float DeltaTime) override;

	/** The path to the neural network asset to be loaded. */
	UPROPERTY(EditDefaultsOnly, Category = MLAdapter, meta = (AllowedClasses = "NeuralNetwork"))
	FSoftObjectPath NeuralNetworkPath;

	/** The neural network controlling this agent. */
	UPROPERTY()
	TObjectPtr<UNeuralNetwork> Brain;
};
