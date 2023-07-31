// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IDisplayClusterPostProcess;


/**
 * nDisplay output post process factory interface
 */
class IDisplayClusterPostProcessFactory
{
public:
	virtual ~IDisplayClusterPostProcessFactory() = default;

public:
	/**
	* Creates a output post process instance
	*
	* @param PostProcessType - PostProcess type, same as specified on registration (useful if the same factory is registered for multiple postprocess types)
	* @param PostProcessPolicyId - ID of a postprocess
	* @param InConfigurationPostProcess - postprocess configuration
	*
	* @return - Post process instance
	*/
	virtual TSharedPtr<IDisplayClusterPostProcess, ESPMode::ThreadSafe> Create(const FString& PostProcessId, const struct FDisplayClusterConfigurationPostprocess* InConfigurationPostProcess) = 0;
	
};
