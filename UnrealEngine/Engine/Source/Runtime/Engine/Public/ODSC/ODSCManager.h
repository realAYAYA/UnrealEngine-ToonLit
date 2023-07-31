// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "RHIDefinitions.h"
#include "Containers/Ticker.h"
#include "ShaderCompiler.h"

class FODSCThread;

/**
 * Responsible for processing shader compile responses from the ODSC Thread.
 * Interface for submitting shader compile requests to the ODSC Thread.
 */
class ENGINE_API FODSCManager
	: public FTSTickerObjectBase
{
public:

	// FODSCManager

	/**
	 * Constructor
	 */
	FODSCManager();

	/**
	 * Destructor
	 */
	virtual ~FODSCManager();

	// FTSTickerObjectBase

	/**
	 * FTSTicker callback
	 *
	 * @param DeltaSeconds - time in seconds since the last tick
	 *
	 * @return false if no longer needs ticking
	 */
	bool Tick(float DeltaSeconds) override;

	/**
	 * Add a request to compile a shader.  The results are submitted and processed in an async manner.
	 *
	 * @param MaterialsToCompile - List of material names to submit compiles for.
	 * @param ShaderPlatform - Which shader platform to compile for.
	 * @param RecompileCommandType - Whether we should recompile changed or global shaders.
	 *
	 * @return false if no longer needs ticking
	 */
	void AddThreadedRequest(const TArray<FString>& MaterialsToCompile, const FString& ShaderTypesToLoad, EShaderPlatform ShaderPlatform, ERHIFeatureLevel::Type FeatureLevel, EMaterialQualityLevel::Type QualityLevel, ODSCRecompileCommand RecompileCommandType);

	/**
	 * Add a request to compile a pipeline (VS/PS) of shaders.  The results are submitted and processed in an async manner.
	 *
	 * @param ShaderPlatform - Which shader platform to compile for.
	 * @param FeatureLevel - Which feature level to compile for.
	 * @param QualityLevel - Which material quality level to compile for.
	 * @param MaterialName - The name of the material to compile.
	 * @param VertexFactoryName - The name of the vertex factory type we should compile.
	 * @param PipelineName - The name of the shader pipeline we should compile.
	 * @param ShaderTypeNames - The shader type names of all the shader stages in the pipeline.
	 *
	 * @return false if no longer needs ticking
	 */
	void AddThreadedShaderPipelineRequest(EShaderPlatform ShaderPlatform, ERHIFeatureLevel::Type FeatureLevel, EMaterialQualityLevel::Type QualityLevel, const FString& MaterialName, const FString& VertexFactoryName, const FString& PipelineName, const TArray<FString>& ShaderTypeNames);

	/** Returns true if we would actually add a request when calling AddThreadedShaderPipelineRequest. */
	inline bool IsHandlingRequests() const { return Thread != nullptr; }

private:

	void OnEnginePreExit();
	void StopThread();

	/** Handles communicating directly with the cook on the fly server. */
	FODSCThread* Thread = nullptr;
};

/** The global shader ODSC manager. */
extern ENGINE_API FODSCManager* GODSCManager;
