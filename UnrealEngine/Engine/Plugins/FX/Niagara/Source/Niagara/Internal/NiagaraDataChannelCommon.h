// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraDataSet.h"
#include "NiagaraDataChannelPublic.h"

class UWorld;
class UActorComponentc;
class FNiagaraWorldManager;
class FNiagaraDataBuffer;
class FNiagaraSystemInstance;
class UNiagaraDataChannel;
class UNiagaraDataChannelHandler;
class UNiagaraDataInterfaceDataChannelWrite;
class UNiagaraDataInterfaceDataChannelRead;
struct FNiagaraDataChannelDataProxy;

/** A request to publish data into a Niagara Data Channel.  */
struct FNiagaraDataChannelPublishRequest
{
	/** The buffer containing the data to be published. This can come from a data channel DI or can be the direct contents on a Niagara simulation. */
	FNiagaraDataBufferRef Data;

	/** Game level data if this request comes from the game code. */
	TSharedPtr<FNiagaraDataChannelGameData> GameData = nullptr;

	/** If true, data in this request will be made visible to BP and C++ game code.*/
	bool bVisibleToGame = false;

	/** If true, data in this request will be made visible to Niagara CPU simulations. */
	bool bVisibleToCPUSims = false;

	/** If true, data in this request will be made visible to Niagara GPU simulations. */
	bool bVisibleToGPUSims = false;

	/** 
	LWC Tile for the originator system of this request.
	Allows us to convert from the Niagara Simulation space into LWC coordinates.
	*/
	FVector3f LwcTile = FVector3f::ZeroVector;

#if WITH_NIAGARA_DEBUGGER
	/** Instigator of this write, used for debug tracking */
	FString DebugSource;
#endif

	FNiagaraDataChannelPublishRequest() = default;
	explicit FNiagaraDataChannelPublishRequest(FNiagaraDataBuffer* InData)
		: Data(InData)
	{
	}

	explicit FNiagaraDataChannelPublishRequest(FNiagaraDataBuffer* InData, bool bInVisibleToGame, bool bInVisibleToCPUSims, bool bInVisibleToGPUSims, FVector3f InLwcTile)
	: Data(InData), bVisibleToGame(bInVisibleToGame), bVisibleToCPUSims(bInVisibleToCPUSims), bVisibleToGPUSims(bInVisibleToGPUSims), LwcTile(InLwcTile)
	{
	}
};


/**
Underlying storage class for data channel data.
Some data channels will have many of these and can distribute them as needed to different accessing systems.
For example, some data channel handlers may subdivide the scene such that distant systems are not interacting.
In this case, each subdivision would have it's own FNiagaraDataChannelData and distribute these to the relevant NiagaraSystems.
*/
struct FNiagaraDataChannelData final
{
	UE_NONCOPYABLE(FNiagaraDataChannelData)
	NIAGARA_API explicit FNiagaraDataChannelData(UNiagaraDataChannelHandler* Owner);
	NIAGARA_API ~FNiagaraDataChannelData();

	NIAGARA_API void Reset();

	NIAGARA_API void BeginFrame(UNiagaraDataChannelHandler* Owner);
	NIAGARA_API void EndFrame(UNiagaraDataChannelHandler* Owner);
	NIAGARA_API int32 ConsumePublishRequests(UNiagaraDataChannelHandler* Owner, const ETickingGroup& TickGroup);

	NIAGARA_API FNiagaraDataChannelGameData* GetGameData();
	NIAGARA_API FNiagaraDataBufferRef GetCPUData(bool bPreviousFrame);
	FNiagaraDataChannelDataProxy* GetRTProxy(){ return RTProxy.Get(); }
	
	/** Adds a request to publish some data into the channel on the next tick. */
	NIAGARA_API void Publish(const FNiagaraDataChannelPublishRequest& Request);
		
	/**
	 *Removes all publish requests involving the given dataset.
	 *TODO: REMOVE
	 *This is a hack to get around lifetime issues wrt data buffers/datasets and their compiled data.
	 *We should rework things such that data buffers can exist beyond their owning dataset, including the compiled data detailing their layout.
	 **/
	NIAGARA_API void RemovePublishRequests(const FNiagaraDataSet* DataSet);

	NIAGARA_API const FNiagaraDataSetCompiledData& GetCompiledData(ENiagaraSimTarget SimTarget);

	void SetLwcTile(FVector3f InLwcTile){ LwcTile = InLwcTile; }
	FVector3f GetLwcTile()const { return LwcTile; }
private:

	/** DataChannel data accessible from Game/BP. AoS Layout. LWC types. */
	FNiagaraDataChannelGameDataPtr GameData;

	//		▲	CPU Sim Data can optionally be made visible to the Game Data.
	//		|
	//		|
	//		|
	//		▼	Game/BP Data can optionally be made visible to CPU sims.

	/** DataChannel data accessible to Niagara CPU sims. SoA layout. Non LWC types. */
	FNiagaraDataSet* CPUSimData = nullptr;

	// Cached off buffer with the previous frame's CPU Sim accessible data.
	// Some systems can choose to read this to avoid any current frame tick ordering issues.
	FNiagaraDataBufferRef PrevCPUSimData = nullptr;

	/** Dataset we use for staging game data for the consumption by RT/GPU sims. */
	FNiagaraDataSet* GameDataStaging = nullptr;

	//		▲	GPU Sim Data can optionally be made visible to CPU Sims and Game Data.
	//		|
	//		|
	//		|
	//		▼	CPU Sim and Game Data can optionally be made visible to the GPU.

	/** DataChannel data accessible to Niagara GPU sims. SoA layout. Non LWC types. */
	FNiagaraDataSet* GPUSimData = nullptr;


	/** Data buffers we'll be passing to the RT proxy for uploading to the GPU */
	TArray<FNiagaraDataBufferRef> BuffersForGPU;

	/** Render thread proxy for this data. Owns all RT side data meant for GPU simulations. */
	TUniquePtr<FNiagaraDataChannelDataProxy> RTProxy;

	/** Pending requests to publish data into this data channel. These requests are consumed at tick tick group. */
	TArray<FNiagaraDataChannelPublishRequest> PublishRequests;

	FVector3f LwcTile = FVector3f::ZeroVector;

	/** Critical section protecting shared state for multiple writers publishing from different threads. */
	FCriticalSection PublishCritSec;
};
