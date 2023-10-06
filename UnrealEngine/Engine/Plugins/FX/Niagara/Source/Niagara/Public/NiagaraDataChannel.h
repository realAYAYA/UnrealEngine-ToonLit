// Copyright Epic Games, Inc. All Rights Reserved.

/*==============================================================================
NiagaraDataChannel.h: Code dealing with Niagara Data Channels and their management.

Niagara Data Channels are a system for communication between Niagara Systems and with Game code/BP.

Niagara Data Channels define a common payload and other settings for a particular named Data Channel.
Niagara Data Channel Handlers are the runtime handler class that will provide access to the data channel to it's users and manage it's internal data.

Niagara Systems can read from and write to Data Channels via data interfaces.
Blueprint and game code can also read from and write to Data Channels.
Each of these writes optionally being made visible to Game, CPU and/or GPU Systems.

At the "Game" level, all data is held in LWC compatible types in AoS format.
When making this data available to Niagara Systems it is converted to SWC, SoA layout that is compatible with Niagara simulation.

EXPERIMENTAL:
Data Channels are currently experimental and undergoing heavy development.
Anything and everything can change, including content breaking changes.

Some Current limitations:

Tick Ordering:
Niagara Systems can chose to read the current frame's data or the previous frame.
Reading from the current frame allows zero latency but introduces a frame dependency, i.e. you must ensure that the reader ticks after the writer.
This frame dependency needs work to be more robust and less error prone.
Reading the previous frames data introduces a frame of latency but removes the need to tick later than the writer. Also means you're sure to get a complete frame worth of data.

GPU Support:
Currently GPU support is very limited.
Only Game->GPU and CPUSim->GPU are supported.
Only the Read function of the DI is supported.
GPU simulations always use the current frame's CPU data as this is all pushed to the at the end of the frame.
When GPU->GPU is supported, the frame dependency issue have more meaning for GPU systems.

==============================================================================*/

#pragma once

#include "NiagaraDataChannelPublic.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "NiagaraDataSetCompiledData.h"
#include "NiagaraCommon.h"
#include "NiagaraDataSet.h"
#include "UObject/UObjectIterator.h"
#include "NiagaraDataChannel.generated.h"

DECLARE_STATS_GROUP(TEXT("Niagara Data Channels"), STATGROUP_NiagaraDataChannels, STATCAT_Advanced);

class FRHICommandListImmediate;

class FNiagaraGpuComputeDispatchInterface;
struct FNiagaraDataSetCompiledData;

class UNiagaraDataChannelHandler;
class UNiagaraDataChannelWriter;
class UNiagaraDataChannelReader;
struct FNiagaraDataChannelPublishRequest;
struct FNiagaraDataChannelGameDataLayout;

//////////////////////////////////////////////////////////////////////////

/** Render thread proxy of FNiagaraDataChannelData. */
struct FNiagaraDataChannelDataProxy
{
	FNiagaraDataSet* GPUDataSet = nullptr;
	FNiagaraDataBufferRef PrevFrameData = nullptr;

	#if !UE_BUILD_SHIPPING
	FString DebugName;
	const TCHAR* GetDebugName()const{return *DebugName;}
	#else
	const TCHAR* GetDebugName()const{return nullptr;}
	#endif

	void BeginFrame(bool bKeepPreviousFrameData);
	void EndFrame(FNiagaraGpuComputeDispatchInterface* DispathInterface, FRHICommandListImmediate& CmdList, const TArray<FNiagaraDataBufferRef>& BuffersForGPU);
	void Reset();
};

DECLARE_MULTICAST_DELEGATE_OneParam(FOnDataChannelCreated, const UNiagaraDataChannel*);

UCLASS(Experimental, abstract, EditInlineNew, MinimalAPI)
class UNiagaraDataChannel : public UObject
{
public:
	GENERATED_BODY()

	//UObject Interface
	NIAGARA_API virtual void PostInitProperties()override;
	NIAGARA_API virtual void PostLoad();
	NIAGARA_API virtual void BeginDestroy();
#if WITH_EDITOR
	NIAGARA_API virtual void PreEditChange(FProperty* PropertyAboutToChange)override;
	NIAGARA_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedDataChannel)override;
#endif
	//UObject Interface End.

	const UNiagaraDataChannelAsset* GetAsset()const { return CastChecked<UNiagaraDataChannelAsset>(GetOuter()); }
	TConstArrayView<FNiagaraVariable> GetVariables()const { return Variables; }

	/** If true, we keep our previous frame's data. Some users will prefer a frame of latency to tick dependency. */
	bool KeepPreviousFrameData()const { return bKeepPreviousFrameData; }

	/** Returns the compiled data describing the data layout for DataChannels in this channel. */
	NIAGARA_API const FNiagaraDataSetCompiledData& GetCompiledData(ENiagaraSimTarget SimTarget)const;

	/** Create the appropriate handler object for this data channel. */
	NIAGARA_API virtual UNiagaraDataChannelHandler* CreateHandler(UWorld* OwningWorld) const PURE_VIRTUAL(UNiagaraDataChannel::CreateHandler, {return nullptr;} );
	
	const FNiagaraDataChannelGameDataLayout& GetGameDataLayout()const { return GameDataLayout; }

	NIAGARA_API FNiagaraDataChannelGameDataPtr CreateGameData()const;

	bool IsValid()const;

	#if !UE_BUILD_SHIPPING
	void SetVerboseLogging(bool bValue){ bVerboseLogging = bValue; }
	bool GetVerboseLogging()const { return bVerboseLogging; }
	#endif

	template<typename TFunc>
	static void ForEachDataChannel(TFunc Func);

private:

	/** The variables that define the data contained in this Data Channel. */
	UPROPERTY(EditAnywhere, Category = "Data Channel")
	TArray<FNiagaraVariable> Variables;

	/** If true, we keep our previous frame's data. This comes at a memory and performance cost but allows users to avoid tick order dependency by reading last frame's data. Some users will prefer a frame of latency to tick order dependency. */
	UPROPERTY(EditAnywhere, Category = "Data Channel")
	bool bKeepPreviousFrameData = true;
		
	/**
	Data layout for payloads in Niagara datasets.
	*/
	UPROPERTY(Transient)
	mutable FNiagaraDataSetCompiledData CompiledData;

	UPROPERTY(Transient)
	mutable FNiagaraDataSetCompiledData CompiledDataGPU;

	/** Layout information for any data stored at the "Game" level. i.e. From game code/BP. AoS layout and LWC types. */
	FNiagaraDataChannelGameDataLayout GameDataLayout;
	
	#if !UE_BUILD_SHIPPING
	mutable bool bVerboseLogging = false;
	#endif
};

template<typename TAction>
void UNiagaraDataChannel::ForEachDataChannel(TAction Func)
{
	for(TObjectIterator<UNiagaraDataChannel> It; It; ++It)
	{
		if(*It)
		{
			Func(*It);
		}
	}
}

/**
* A C++ and Blueprint accessible library of utility functions for accessing Niagara DataChannel
*/
UCLASS(Experimental)
class NIAGARA_API UNiagaraDataChannelLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static UNiagaraDataChannelHandler* GetNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannelAsset* Channel);

	/** Initializes and returns the Niagara Data Channel writer to write N elements to the given data channel. */
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static UNiagaraDataChannelWriter* WriteToNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannelAsset* Channel, FNiagaraDataChannelSearchParameters SearchParams, int32 Count, bool bVisibleToGame, bool bVisibleToCPU, bool bVisibleToGPU);

	/** Initializes and returns the Niagara Data Channel reader for the given data channel. */
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static UNiagaraDataChannelReader* ReadFromNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannelAsset* Channel, FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrame);

	static UNiagaraDataChannelHandler* GetNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannel* Channel);
	static UNiagaraDataChannelWriter* WriteToNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannel* Channel, FNiagaraDataChannelSearchParameters SearchParams, int32 Count, bool bVisibleToGame, bool bVisibleToCPU, bool bVisibleToGPU);
	static UNiagaraDataChannelReader* ReadFromNiagaraDataChannel(const UObject* WorldContextObject, UNiagaraDataChannel* Channel, FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrame);
};
