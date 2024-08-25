// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineBaseTypes.h"
#include "NiagaraCommon.h"
#include "NiagaraDataChannelPublic.generated.h"

struct FNiagaraDataChannelPublishRequest;
class FNiagaraWorldManager;
class UNiagaraDataChannelAsset;
class UNiagaraDataChannel;
class UNiagaraDataChannelHandler;
struct FNiagaraDataChannelGameData;
struct FNiagaraDataChannelData;
using FNiagaraDataChannelGameDataPtr = TSharedPtr<FNiagaraDataChannelGameData>;
using FNiagaraDataChannelDataPtr = TSharedPtr<FNiagaraDataChannelData>;

/** Niagara Data Channels are a system for communication between Niagara Systems and with game code/Blueprint.

Data channel assets define the payload as well as some transfer settings.
Niagara Systems can read from and write to data channels via data interfaces.
Blueprint and C++ code can also read from and write to data channels using its API functions.

EXPERIMENTAL:
Data Channels are currently experimental and undergoing heavy development.
 */
UCLASS(Experimental, BlueprintType, DisplayName = "Niagara Data Channel", MinimalAPI)
class UNiagaraDataChannelAsset : public UObject
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = DataChannel, Instanced)
	TObjectPtr<UNiagaraDataChannel> DataChannel;
	
	#if WITH_EDITORONLY_DATA
	/** When changing data channel types we cache the old channel and attempt to copy over any common properties from one to the other. */
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraDataChannel> CachedPreChangeDataChannel;
	#endif

public:

#if WITH_EDITOR
	NIAGARA_API virtual void PreEditChange(class FProperty* PropertyAboutToChange)override;
	NIAGARA_API virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)override;
#endif

	UNiagaraDataChannel* Get() const { return DataChannel; }
};

USTRUCT(BlueprintType)
struct FNiagaraDataChannelVariable : public FNiagaraVariableBase
{
	GENERATED_USTRUCT_BODY()
	
	bool Serialize(FArchive& Ar);

#if WITH_EDITORONLY_DATA
	
	/** Can be used to track renamed data channel variables */
	UPROPERTY(meta=(IgnoreForMemberInitializationTest))
	FGuid Version = FGuid::NewGuid();

	NIAGARA_API static bool IsAllowedType(const FNiagaraTypeDefinition& Type);
#endif
	
	NIAGARA_API static FNiagaraTypeDefinition ToDataChannelType(const FNiagaraTypeDefinition& Type);

};

template<>
struct TStructOpsTypeTraits<FNiagaraDataChannelVariable> : public TStructOpsTypeTraitsBase2<FNiagaraDataChannelVariable>
{
	enum
	{
		WithSerializer = true,
	};
};

/**
Minimal set of types and declares required for external users of Niagara Data Channels.
*/

/**
Parameters used when retrieving a specific set of Data Channel Data to read or write.
Many Data Channel types will have multiple internal sets of data and these parameters control which the Channel should return to users for access.
An example of this would be the Islands Data Channel type which will subdivide the world and have a different set of data for each sub division.
It will return to users the correct data for their location based on these parameters.
*/
USTRUCT(BlueprintType)
struct FNiagaraDataChannelSearchParameters
{
	GENERATED_BODY()
	
	FNiagaraDataChannelSearchParameters()
	: bOverrideLocation(false)
	{
	}

	FNiagaraDataChannelSearchParameters(USceneComponent* Owner)
	: OwningComponent(Owner)
	, bOverrideLocation(false)
	{
	}

	FNiagaraDataChannelSearchParameters(USceneComponent* Owner, FVector LocationOverride)
	: OwningComponent(Owner)
	, Location(LocationOverride)
	, bOverrideLocation(true)
	{
	}

	FNiagaraDataChannelSearchParameters(FVector InLocation)
		: OwningComponent(nullptr)
		, Location(InLocation)
		, bOverrideLocation(true)
	{
	}
	
	NIAGARA_API FVector GetLocation()const;
	NIAGARA_API USceneComponent* GetOwner()const { return OwningComponent; }

	/** In cases where there is an owning component such as an object spawning from itself etc, then we pass that component in. Some handlers may only use it's location but others may make use of more data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	TObjectPtr<USceneComponent> OwningComponent = nullptr;

	/** In cases where there is no owning component for data being read or written to a data channel, we simply pass in a location. We can also use this when bOverrideLocaiton is set. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	FVector Location = FVector::ZeroVector;

	/** If true, even if an owning component is set, the data channel should use the Location value rather than the component location. If this is false, the NDC will get any location needed from the owning component. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	uint32 bOverrideLocation : 1;
};

USTRUCT()
struct FNiagaraDataChannelGameDataLayout
{
	GENERATED_BODY();

	/** Map of all variables contained in this DataChannel data and the indices into data arrays for game data. */
	UPROPERTY()
	TMap<FNiagaraVariableBase, int32> VariableIndices;

	/** Helpers for converting LWC types into Niagara simulation SWC types. */
	UPROPERTY()
	TArray<FNiagaraLwcStructConverter> LwcConverters;

	void Init(const TArray<FNiagaraDataChannelVariable>& Variables);
};


#if WITH_NIAGARA_DEBUGGER

/** Hooks into internal NiagaraDataChannels code for debugging and testing purposes. */
class FNiagaraDataChannelDebugUtilities
{
public: 

	static NIAGARA_API void BeginFrame(FNiagaraWorldManager* WorldMan, float DeltaSeconds);
	static NIAGARA_API void EndFrame(FNiagaraWorldManager* WorldMan, float DeltaSeconds);
	static NIAGARA_API void Tick(FNiagaraWorldManager* WorldMan, float DeltaSeconds, ETickingGroup TickGroup);
	
	static NIAGARA_API UNiagaraDataChannelHandler* FindDataChannelHandler(FNiagaraWorldManager* WorldMan, UNiagaraDataChannel* DataChannel);

	static void LogWrite(const FNiagaraDataChannelPublishRequest& WriteRequest, const UNiagaraDataChannel* DataChannel, const ETickingGroup& TickGroup);
	static void DumpAllWritesToLog();

	static FNiagaraDataChannelDebugUtilities& Get();
	static void TearDown();

private:
	struct FChannelWriteRequest
	{
		TSharedPtr<FNiagaraDataChannelGameData> Data;
		bool bVisibleToGame = false;
		bool bVisibleToCPUSims = false;
		bool bVisibleToGPUSims = false;
		ETickingGroup TickGroup;
		FString DebugSource;
	};
	struct FFrameDebugData
	{
		uint64 FrameNumber;
		TArray<FChannelWriteRequest> WriteRequests;
	};
	
	static FString ToJson(FNiagaraDataChannelGameData* Data);
	static FString TickGroupToString(const ETickingGroup& TickGroup);
	TArray<FFrameDebugData> FrameData;
};


#endif//WITH_NIAGARA_DEBUGGER


/** Buffer containing a single FNiagaraVariable's data at the game level. AoS layout. LWC Types. */
struct FNiagaraDataChannelVariableBuffer
{
	TArray<uint8> Data;
	TArray<uint8> PrevData;
	int32 Size = 0;

	void Init(const FNiagaraVariableBase& Var)
	{
		//Position types are a special case where we have to store an LWC Vector in game level data and convert to a simulation friendly FVector3f as it enters Niagara.
		if (Var.GetType() == FNiagaraTypeDefinition::GetPositionDef())
		{
			Size = FNiagaraTypeHelper::GetVectorDef().GetSize();
		}
		else
		{
			Size = Var.GetSizeInBytes();
		}
	}

	void Empty() { Data.Empty(); }

	void BeginFrame(bool bKeepPrevious)
	{
		if (bKeepPrevious)
		{
			Swap(Data, PrevData);
		}
		Data.Reset();
	}

	template<typename T>
	bool Write(int32 Index, const T& InData)
	{
		check(sizeof(T) == Size);
		if (Index >= 0 && Index < Num())
		{
			T* Dest = reinterpret_cast<T*>(Data.GetData()) + Index;
			*Dest = InData;

			return true;
		}
		return false;
	}

	template<typename T>
	bool Read(int32 Index, T& OutData, bool bPreviousFrameData)const
	{
		check(sizeof(T) == Size);

		//Should we fallback to previous if we don't have current?
		//bPreviousFrameData |= Data.Num() == 0;

		int32 NumElems = bPreviousFrameData ? PrevNum() : Num();
		if (Index >= 0 && Index < NumElems)
		{
			const uint8* DataPtr = bPreviousFrameData ? PrevData.GetData() : Data.GetData();
			const T* Src = reinterpret_cast<const T*>(DataPtr) + Index;
			OutData = *Src;

			return true;
		}
		OutData = T();
		return false;
	}

	void SetNum(int32 Num)
	{
		Data.SetNumZeroed(Size * Num);
	}

	void Reserve(int32 Num)
	{
		Data.Reserve(Size * Num);
	}

	int32 Num()const { return Data.Num() / Size; }
	int32 PrevNum()const { return PrevData.Num() / Size; }

	int32 GetElementSize()const { return Size; }
};


/** Storage for game level DataChannels generated by BP / C++ */
struct FNiagaraDataChannelGameData : public TSharedFromThis<FNiagaraDataChannelGameData>
{
private:

	/** Per variable storage buffers. */
	TArray<FNiagaraDataChannelVariableBuffer> VariableData;

	int32 NumElements = 0;
	int32 PrevNumElements = 0;

	TWeakObjectPtr<const UNiagaraDataChannel> DataChannel;

public:

	NIAGARA_API void Init(const UNiagaraDataChannel* InDataChannel);
	NIAGARA_API void Empty();
	NIAGARA_API void BeginFrame();
	NIAGARA_API void SetNum(int32 NewNum);
	NIAGARA_API void Reserve(int32 NewNum);
	int32 Num() const { return NumElements; }
	int32 PrevNum()const { return PrevNumElements; }

	int32 Add(int32 Count=1)
	{
		int32 Ret = NumElements;
		SetNum(Ret + Count);
		return Ret;
	}

	NIAGARA_API FNiagaraDataChannelVariableBuffer* FindVariableBuffer(const FNiagaraVariableBase& Var);

	NIAGARA_API void WriteToDataSet(FNiagaraDataBuffer* DestBuffer, int32 DestStartIdx, FVector3f SimulationLwcTile);

	NIAGARA_API void AppendFromGameData(const FNiagaraDataChannelGameData& GameData);

	NIAGARA_API void AppendFromDataSet(const FNiagaraDataBuffer* SrcBuffer, FVector3f SimulationLwcTile);

	const UNiagaraDataChannel* GetDataChannel()const { return DataChannel.Get(); }

	const TConstArrayView<FNiagaraDataChannelVariableBuffer> GetVariableBuffers()const { return VariableData; }
	void SetFromSimCache(const FNiagaraVariableBase& SourceVar, TConstArrayView<uint8> Data, int32 Size);
};
