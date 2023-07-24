// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

/**
Base class for Niagara DataChannel Handlers.
Data Channel handlers are the runtime counterpart to Data Channels.
They control how data being written to the Data Channel is stored and how to expose data being read from the Data Channel.
For example, the simplest handler is the UNiagaraDataChannelHandler_Global which just keeps all data in a single large set that is used by all systems.
Some more complex handlers may want to divide up the scene in various different ways to better match particular use cases.

*/


#include "NiagaraDataChannelCommon.h"
#include "NiagaraDataChannelHandler.generated.h"

/** Base class for all UNiagaraDataChannelHandler's Render Thread Proxies. */
struct FNiagaraDataChannelHandlerRTProxyBase
{
};

UCLASS(Experimental, abstract, BlueprintType)
class NIAGARA_API UNiagaraDataChannelHandler : public UObject
{
public:

	GENERATED_BODY()

	virtual ~UNiagaraDataChannelHandler();

	virtual void Init(const UNiagaraDataChannel* InChannel);

	/** Returns the DataChannel data for the given DataChannel name and location. */
	virtual void Tick(float DeltaTime, ETickingGroup TickGroup, FNiagaraWorldManager* OwningWorld);

	/** Returns the data for this data channel given the system instance. Possibly some handlers will subdivide spacially etc so may not return the same data for the different systems. */
	virtual void GetData(FNiagaraSystemInstance* SystemInstance, FNiagaraDataBuffer*& OutCPUData, bool bGetLastFrameData)  PURE_VIRTUAL(UNiagaraDataChannelHandler::GetData, );	

	/** Gets the GPU data set with data for the given system. This API will likely be reworked as GPU support is fleshed out and more complex handlers are created. */
	virtual void GetDataGPU(FNiagaraSystemInstance* SystemInstance, FNiagaraDataSet*& OutCPUData) PURE_VIRTUAL(UNiagaraDataChannelHandler::GetDataGPU, );	

	/** Adds a request to publish some data into the channel on the next tick. */
	virtual void Publish(const FNiagaraDataChannelPublishRequest& Request);
		
	/**
	 *Removes all publish requests involving the given dataset.
	 *TODO: REMOVE
	 *This is a hack to get around lifetime issues wrt data buffers/datasets and their compiled data.
	 *We should rework things such that data buffers can exist beyond their owning dataset, including the compiled data detailing their layout.
	 **/
	virtual void RemovePublishRequests(const FNiagaraDataSet* DataSet);

	/** Returns the game level DataChannel data store. */
	virtual FNiagaraDataChannelGameDataPtr GetGameData(UActorComponent* OwningComponent) PURE_VIRTUAL(UNiagaraDataChannelHandler::GetGameData, return nullptr; );

	/** Utility allowing easy casting of proxy to it's proper derived type. */
	template<typename T>
	T* GetRTProxyAs() 
	{
		T* TypedProxy = static_cast<T*>(RTProxy.Get());
		check(TypedProxy != nullptr);
		return TypedProxy;
	}

	const UNiagaraDataChannel* GetDataChannel()const { return DataChannel; }

	UFUNCTION(BlueprintCallable, Category="Data Channel")
	UNiagaraDataChannelWriter* GetDataChannelWriter();

	UFUNCTION(BlueprintCallable, Category = "Data Channel")
	UNiagaraDataChannelReader* GetDataChannelReader();

protected:

	UPROPERTY()
	TObjectPtr<const UNiagaraDataChannel> DataChannel;

	/** Per frame requests to publish data from Niagara Systems into the main world DataChannels buffers. */
	TArray<FNiagaraDataChannelPublishRequest> PublishRequests;

	/** Data buffers we'll be passing to the RT for uploading to the GPU */
	TArray<FNiagaraDataBuffer*> BuffersForGPU;

	/** Render Thread Proxy for this handler. Each handler type must create it's proxy inside it's constructor. */
	TUniquePtr<FNiagaraDataChannelHandlerRTProxyBase> RTProxy;

	/** Helper object allowing BP to write data in this channel. */
	UPROPERTY()
	TObjectPtr<UNiagaraDataChannelWriter> Writer;

	/** Helper object allowing BP to read data in this channel. */
	UPROPERTY()
	TObjectPtr<UNiagaraDataChannelReader> Reader;
};