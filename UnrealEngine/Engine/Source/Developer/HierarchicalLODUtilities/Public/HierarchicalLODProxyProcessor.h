// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"
#include "Containers/Ticker.h"
#include "GameFramework/WorldSettings.h"
#include "MeshUtilities.h"
#include "IMeshReductionInterfaces.h"
#include "UObject/StrongObjectPtr.h"

class ALODActor;
class UHLODProxy;

class HIERARCHICALLODUTILITIES_API FHierarchicalLODProxyProcessor : public FTSTickerObjectBase
{
public:		
	FHierarchicalLODProxyProcessor();
	~FHierarchicalLODProxyProcessor();
	
	/** Begin FTSTickerObjectBase */
	virtual bool Tick(float DeltaTime) override;
	/** End FTSTickerObjectBase */

	/**
	* AddProxyJob
	* @param InLODActor - LODActor for which the mesh will be generated
	* @param InProxy - The proxy mesh used to store the mesh
	* @param LODSetup - Simplification settings structure
	* @return FGuid - Guid for the job
	*/
	FGuid AddProxyJob(ALODActor* InLODActor, UHLODProxy* InProxy, const FHierarchicalSimplification& LODSetup);
	
	/** Callback function used for processing finished mesh generation jobs	
	* @param InGuid - Guid of the finished job
	* @param InAssetsToSync - Assets data created by the job
	*/
	void ProcessProxy(const FGuid InGuid, TArray<UObject*>& InAssetsToSync);
	
	/** Returns the callback delegate which will be passed onto ProxyLOD function */
	FCreateProxyDelegate& GetCallbackDelegate();
		
	bool IsProxyGenerationRunning() const;
protected:
	/** Called when the map has changed*/
	void OnMapChange(uint32 MapFlags);

	/** Called when the current level has changed */
	void OnNewCurrentLevel();

	/** Clears the processing data array/map */
	void ClearProcessingData();
protected:
	/** Structure storing the data required during processing */
	struct FProcessData
	{
		/** LODActor instance for which a proxy is generated */
		ALODActor* LODActor;
		/** Proxy mesh where the rendering data is stored */
		UHLODProxy* Proxy;
		/** Array with resulting asset objects from proxy generation (StaticMesh/Materials/Textures) */
		TArray<TStrongObjectPtr<UObject>> AssetObjects;
		/** HLOD settings structure used for creating the proxy */
		FHierarchicalSimplification LODSetup;
	};
private:
	/** Map and array used to store job data */
	TMap<FGuid, FProcessData*> JobActorMap;
	TArray<FProcessData*> ToProcessJobs;
	/** Delegate to pass onto */
	FCreateProxyDelegate CallbackDelegate;	
	/** Critical section to keep JobActorMap/ToProcessJobs access thread-safe */
	FCriticalSection StateLock;	
};
