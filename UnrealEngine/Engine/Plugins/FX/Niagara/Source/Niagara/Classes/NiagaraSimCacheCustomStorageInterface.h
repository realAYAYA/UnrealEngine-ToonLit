// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCommon.h"
#include "UObject/Interface.h"
#include "NiagaraSimCacheCustomStorageInterface.generated.h"

struct FNiagaraSimCacheFeedbackContext;
class FNiagaraSystemInstance;

// Interface for UObjects to implement renderable mesh
UINTERFACE()
class UNiagaraSimCacheCustomStorageInterface : public UInterface
{
	GENERATED_BODY()
};

/**
The current API for storing data inside a simulation cache.
This is highly experimental and the API will change as we split editor / runtime data storage.

See INiagaraDataInterfaceSimCacheVisualizer to implement a custom visualizer widget for the stored data.
*/
class INiagaraSimCacheCustomStorageInterface
{
	GENERATED_BODY()

public:
	/**
	Called when we begin to write data into a simulation cache.
	Returning nullptr means you are not going to cache any data for the simulation.
	The object returned will be stored directly into the cache file, so you are expected to manage the size of the object and store data appropriately.
	*/
	virtual UObject* SimCacheBeginWrite(UObject* SimCache, FNiagaraSystemInstance* NiagaraSystemInstance, const void* OptionalPerInstanceData, FNiagaraSimCacheFeedbackContext& FeedbackContext) const { return nullptr; }

	/**
	Called when we are ready to write data into the simulation cache.
	This is always called in sequence, i.e. 0, 1, 2, etc, we will never jump around frames.
	*/
	virtual bool SimCacheWriteFrame(UObject* StorageObject, int FrameIndex, FNiagaraSystemInstance* SystemInstance, const void* OptionalPerInstanceData, FNiagaraSimCacheFeedbackContext& FeedbackContext) const { return true; }

	/**
	Called when we complete writing data into the simulation cache.
	Note: This is called using the Class Default Object not the instance the object was created from
	*/
	virtual bool SimCacheEndWrite(UObject* StorageObject) const { return true; }

	/**
	Read a frame of data from the simulation cache.
	*/
	virtual bool SimCacheReadFrame(UObject* StorageObject, int FrameA, int FrameB, float Interp, FNiagaraSystemInstance* SystemInstance, void* OptionalPerInstanceData) { return true; }

	/**
	Called when the simulation cache has finished reading a frame.
	Only DataInterfaces with PerInstanceData will have this method called on them.
	*/
	virtual void SimCachePostReadFrame(void* OptionalPerInstanceData, FNiagaraSystemInstance* SystemInstance) {}

	/**
	Called to compare a frame between two separate simulation cache storages
	This will be called on the CDO object since we do not have the actual data interface.
	*/
	virtual bool SimCacheCompareFrame(UObject* LhsStorageObject, UObject* RhsStorageObject, int FrameIndex, TOptional<float> Tolerance, FString& OutErrors) const { OutErrors = TEXT("Compare not implemented"); return false; }

	/**
	This function allows you to preserve a list of attributes when building a renderer only cache.
	The UsageContext will be either a UNiagaraSystem or a UNiagaraEmitter and can be used to scope your variables accordingly.
	For example, if you were to require 'Particles.MyAttribute' in order to process the cache results you would need to convert
	this into 'MyEmitter.Particles.MyAttribute' by checking the UsageContext is a UNiagaraEmitter and then creating the variable from the unique name.
	*/
	virtual TArray<FNiagaraVariableBase> GetSimCacheRendererAttributes(UObject* UsageContext) const { return TArray<FNiagaraVariableBase>(); }
};
