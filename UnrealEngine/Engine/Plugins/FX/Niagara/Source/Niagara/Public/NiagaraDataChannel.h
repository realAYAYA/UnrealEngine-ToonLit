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

DECLARE_STATS_GROUP(TEXT("Niagara Data Channels"), STATGROUP_NiagaraDataChannels, STATCAT_Niagara);

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

UCLASS(Experimental, abstract, EditInlineNew, MinimalAPI, prioritizeCategories=("Data Channel"))
class UNiagaraDataChannel : public UObject
{
public:
	GENERATED_BODY()

	//UObject Interface
	NIAGARA_API virtual void PostInitProperties() override;
	NIAGARA_API virtual void PostLoad() override;
	NIAGARA_API virtual void BeginDestroy() override;
#if WITH_EDITOR
	NIAGARA_API virtual void PreEditChange(FProperty* PropertyAboutToChange) override;
	NIAGARA_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedDataChannel) override;
#endif
	//UObject Interface End.

	const UNiagaraDataChannelAsset* GetAsset() const { return CastChecked<UNiagaraDataChannelAsset>(GetOuter()); }
	TConstArrayView<FNiagaraDataChannelVariable> GetVariables() const { return ChannelVariables; }

	/** If true, we keep our previous frame's data. Some users will prefer a frame of latency to tick dependency. */
	bool KeepPreviousFrameData() const { return bKeepPreviousFrameData; }

	/** Returns the compiled data describing the data layout for DataChannels in this channel. */
	NIAGARA_API const FNiagaraDataSetCompiledData& GetCompiledData(ENiagaraSimTarget SimTarget) const;

	/** Create the appropriate handler object for this data channel. */
	NIAGARA_API virtual UNiagaraDataChannelHandler* CreateHandler(UWorld* OwningWorld) const PURE_VIRTUAL(UNiagaraDataChannel::CreateHandler, {return nullptr;} );
	
	const FNiagaraDataChannelGameDataLayout& GetGameDataLayout() const { return GameDataLayout; }

	NIAGARA_API FNiagaraDataChannelGameDataPtr CreateGameData() const;

	bool IsValid() const;

	#if WITH_NIAGARA_DEBUGGER
	void SetVerboseLogging(bool bValue){ bVerboseLogging = bValue; }
	bool GetVerboseLogging()const { return bVerboseLogging; }
	#endif

	template<typename TFunc>
	static void ForEachDataChannel(TFunc Func);

	bool ShouldEnforceTickGroupReadWriteOrder() const {return bEnforceTickGroupReadWriteOrder;}
	
	/** If we are enforcing tick group read/write ordering the this returns the final tick group that this NDC can be written to. All reads must happen in Tick groups after this or next frame. */
	ETickingGroup GetFinalWriteTickGroup() const { return FinalWriteTickGroup; }

#if WITH_EDITORONLY_DATA
	/** Can be used to track structural changes that would need recompilation of downstream assets. */
	NIAGARA_API FGuid GetVersion() const { return VersionGuid; }
#endif
	
private:

	//TODO: add default values for editor previews
	
	/** The variables that define the data contained in this Data Channel. */
	UPROPERTY(EditAnywhere, Category = "Data Channel", meta=(EnforceUniqueNames = true))
	TArray<FNiagaraDataChannelVariable> ChannelVariables;

	/** If true, we keep our previous frame's data. This comes at a memory and performance cost but allows users to avoid tick order dependency by reading last frame's data. Some users will prefer a frame of latency to tick order dependency. */
	UPROPERTY(EditAnywhere, Category = "Data Channel")
	bool bKeepPreviousFrameData = true;

	/** If true we ensure that all writes happen in or before the Tick Group specified in EndWriteTickGroup and that all reads happen in tick groups after this. */
	UPROPERTY(EditAnywhere, Category = "Data Channel")
	bool bEnforceTickGroupReadWriteOrder = false;

	/** The final tick group that this data channel can be written to. */
	UPROPERTY(EditAnywhere, Category = "Data Channel", meta=(EditCondition="bEnforceTickGroupReadWriteOrder"))
	TEnumAsByte<ETickingGroup> FinalWriteTickGroup = ETickingGroup::TG_EndPhysics;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FGuid VersionGuid;

	UPROPERTY(meta=(DeprecatedProperty))
	TArray<FNiagaraVariable> Variables_DEPRECATED;
#endif

	/**
	Data layout for payloads in Niagara datasets.
	*/
	UPROPERTY(Transient)
	mutable FNiagaraDataSetCompiledData CompiledData;

	UPROPERTY(Transient)
	mutable FNiagaraDataSetCompiledData CompiledDataGPU;

	/** Layout information for any data stored at the "Game" level. i.e. From game code/BP. AoS layout and LWC types. */
	FNiagaraDataChannelGameDataLayout GameDataLayout;
	
	#if WITH_NIAGARA_DEBUGGER
	mutable bool bVerboseLogging = false;
	#endif
};

template<typename TAction>
void UNiagaraDataChannel::ForEachDataChannel(TAction Func)
{
	for(TObjectIterator<UNiagaraDataChannel> It; It; ++It)
	{
		UNiagaraDataChannel* NDC = *It;
		if (NDC && 
			NDC->HasAnyFlags(RF_ClassDefaultObject | RF_Transient) == false &&
			Cast<UNiagaraDataChannelAsset>(NDC->GetOuter()) != nullptr)
		{
			Func(*It);
		}
	}
}

UENUM(BlueprintType)
enum class ENiagartaDataChannelReadResult : uint8
{
	Success,
	Failure
};

/**
* A C++ and Blueprint accessible library of utility functions for accessing Niagara DataChannel
*/
UCLASS(Experimental)
class NIAGARA_API UNiagaraDataChannelLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

	UFUNCTION(BlueprintInternalUseOnly, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static UNiagaraDataChannelHandler* GetNiagaraDataChannel(const UObject* WorldContextObject, const UNiagaraDataChannelAsset* Channel);

	/**
	 * Initializes and returns the Niagara Data Channel writer to write N elements to the given data channel.
	 *
	 * @param WorldContextObject	World to execute in
	 * @param Channel				The channel to write to
	 * @param SearchParams			Parameters used when retrieving a specific set of Data Channel Data to read or write like the islands data channel type.
	 * @param Count					The number of elements to write 
	 * @param bVisibleToGame	If true, the data written to this data channel is visible to Blueprint and C++ logic reading from it
	 * @param bVisibleToCPU	If true, the data written to this data channel is visible to Niagara CPU emitters
	 * @param bVisibleToGPU	If true, the data written to this data channel is visible to Niagara GPU emitters
	 * @param DebugSource	Instigator for this write, used in the debug hud to track writes to the data channel from different sources
	 */
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, DisplayName="Write To Niagara Data Channel (Batch)", meta = (AdvancedDisplay = "SearchParams, DebugSource", Keywords = "niagara DataChannel", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true", AutoCreateRefTerm="DebugSource"))
	static UNiagaraDataChannelWriter* WriteToNiagaraDataChannel(const UObject* WorldContextObject, const UNiagaraDataChannelAsset* Channel, FNiagaraDataChannelSearchParameters SearchParams, int32 Count, UPARAM(DisplayName = "Visible to Blueprint") bool bVisibleToGame, UPARAM(DisplayName = "Visible to Niagara CPU") bool bVisibleToCPU, UPARAM(DisplayName = "Visible to Niagara GPU") bool bVisibleToGPU, const FString& DebugSource);

	/**
	 * Initializes and returns the Niagara Data Channel reader for the given data channel.
	 *
	 * @param WorldContextObject	World to execute in
	 * @param Channel				The channel to read from
	 * @param SearchParams			Parameters used when retrieving a specific set of Data Channel Data to read or write like the islands data channel type.
	 * @param bReadPreviousFrame	True if this reader will read the previous frame's data. If false, we read the current frame.
	 *								Reading the current frame allows for zero latency reads, but any data elements that are generated after this reader is used are missed.
	 *								Reading the previous frame's data introduces a frame of latency but ensures we never miss any data as we have access to the whole frame.
	 */
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, DisplayName="Read From Niagara Data Channel (Batch)", meta = (AdvancedDisplay = "SearchParams", Keywords = "niagara DataChannel", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static UNiagaraDataChannelReader* ReadFromNiagaraDataChannel(const UObject* WorldContextObject, const UNiagaraDataChannelAsset* Channel, FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrame);

	/**
	 * Returns the number of readable elements in the given data channel
	 *
	 * @param WorldContextObject	World to execute in
	 * @param Channel				The channel to read from
	 * @param SearchParams			Parameters used when retrieving a specific set of Data Channel Data to read or write like the islands data channel type.
	 * @param bReadPreviousFrame	True if this reader will read the previous frame's data. If false, we read the current frame.
	 *								Reading the current frame allows for zero latency reads, but any data elements that are generated after this reader is used are missed.
	 *								Reading the previous frame's data introduces a frame of latency but ensures we never miss any data as we have access to the whole frame.
	 */
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (bReadPreviousFrame="true", AdvancedDisplay = "SearchParams, bReadPreviousFrame", Keywords = "niagara DataChannel num size", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static int32 GetDataChannelElementCount(const UObject* WorldContextObject, const UNiagaraDataChannelAsset* Channel, FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrame);

	/**
	 * Reads a single entry from the given data channel, if possible.
	 *
	 * @param WorldContextObject	World to execute in
	 * @param Channel				The channel to read from
	 * @param Index					The data index to read from
	 * @param SearchParams			Parameters used when retrieving a specific set of Data Channel Data to read or write like the islands data channel type.
	 * @param bReadPreviousFrame	True if this reader will read the previous frame's data. If false, we read the current frame.
	 *								Reading the current frame allows for zero latency reads, but any data elements that are generated after this reader is used are missed.
	 *								Reading the previous frame's data introduces a frame of latency but ensures we never miss any data as we have access to the whole frame.
	 * @param ReadResult			Used by Blueprint for the return value
	 */
	UFUNCTION(BlueprintInternalUseOnly, Category = NiagaraDataChannel, DisplayName="Read From Niagara Data Channel", meta = (bReadPreviousFrame="true", AdvancedDisplay = "SearchParams, bReadPreviousFrame", ExpandEnumAsExecs="ReadResult", Keywords = "niagara DataChannel", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void ReadFromNiagaraDataChannelSingle(const UObject* WorldContextObject, const UNiagaraDataChannelAsset* Channel, int32 Index, FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrame, ENiagartaDataChannelReadResult& ReadResult);

	/**
	 * Writes a single element to a Niagara Data Channel. The element won't be immediately visible to readers, as it needs to be processed first. The earliest point it can be read is in the next tick group.
	 *
	 * @param WorldContextObject	World to execute in
	 * @param Channel				The channel to write to
	 * @param SearchParams			Parameters used when retrieving a specific set of Data Channel Data to read or write like the islands data channel type.
	 * @param bVisibleToBlueprint	If true, the data written to this data channel is visible to Blueprint and C++ logic reading from it
	 * @param bVisibleToNiagaraCPU	If true, the data written to this data channel is visible to Niagara CPU emitters
	 * @param bVisibleToNiagaraGPU	If true, the data written to this data channel is visible to Niagara GPU emitters
	 */
	UFUNCTION(BlueprintInternalUseOnly, Category = NiagaraDataChannel, DisplayName="Write To Niagara Data Channel", meta = (bVisibleToBlueprint="true", bVisibleToNiagaraCPU="true", bVisibleToNiagaraGPU="true", AdvancedDisplay = "bVisibleToBlueprint, bVisibleToNiagaraCPU, bVisibleToNiagaraGPU, SearchParams", Keywords = "niagara DataChannel event writer", WorldContext = "WorldContextObject", UnsafeDuringActorConstruction = "true"))
	static void WriteToNiagaraDataChannelSingle(const UObject* WorldContextObject, const UNiagaraDataChannelAsset* Channel, FNiagaraDataChannelSearchParameters SearchParams, bool bVisibleToBlueprint, bool bVisibleToNiagaraCPU, bool bVisibleToNiagaraGPU);

	static UNiagaraDataChannelHandler* FindDataChannelHandler(const UObject* WorldContextObject, const UNiagaraDataChannel* Channel);
	static UNiagaraDataChannelWriter* CreateDataChannelWriter(const UObject* WorldContextObject, const UNiagaraDataChannel* Channel, FNiagaraDataChannelSearchParameters SearchParams, int32 Count, bool bVisibleToGame, bool bVisibleToCPU, bool bVisibleToGPU, const FString& DebugSource);
	static UNiagaraDataChannelReader* CreateDataChannelReader(const UObject* WorldContextObject, const UNiagaraDataChannel* Channel, FNiagaraDataChannelSearchParameters SearchParams, bool bReadPreviousFrame);
};
