// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineBaseTypes.h"
#include "NiagaraCommon.h"
#include "NiagaraDataChannelPublic.generated.h"

class FNiagaraWorldManager;
class UNiagaraDataChannelAsset;
class UNiagaraDataChannel;
class UNiagaraDataChannelHandler;
struct FNiagaraDataChannelGameData;
struct FNiagaraDataChannelData;
using FNiagaraDataChannelGameDataPtr = TSharedPtr<FNiagaraDataChannelGameData>;
using FNiagaraDataChannelDataPtr = TSharedPtr<FNiagaraDataChannelData>;

/** Wrapper asset class for UNiagaraDataChannel which is instanced. */
UCLASS(Experimental, DisplayName = "Niagara Data Channel", MinimalAPI)
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

/**
Minimal set of types and declares required for external users of Niagara Data Channels.
*/

/**
Parameters allowing users to search for the correct data channel data to read/write.
Some data channels will sub divide their data internally in various ways, e.g., spacial partition.
These parameters allow users to search for the correct internal data when reading and writing.
*/
USTRUCT(BlueprintType)
struct FNiagaraDataChannelSearchParameters
{
	GENERATED_BODY()

	/** In cases where there is an owning component such as an object spawning from itself etc, then we pass that component in. Some handlers may only use it's location but others may make use of more data. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	TObjectPtr<USceneComponent> OwningComponent = nullptr;

	/** In cases where there is no owning component for data being read or written to a data channel, we simply pass in a location. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Parameters")
	FVector Location = FVector::ZeroVector;

	NIAGARA_API FVector GetLocation()const;
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

	void Init(const TArray<FNiagaraVariable>& Variables);
};


#if !UE_BUILD_SHIPPING

/** Hooks into internal NiagaraDataChannels code for debugging and testing purposes. */
class FNiagaraDataChannelDebugUtilities
{
public: 

	static NIAGARA_API void BeginFrame(FNiagaraWorldManager* WorldMan, float DeltaSeconds);
	static NIAGARA_API void EndFrame(FNiagaraWorldManager* WorldMan, float DeltaSeconds);
	static NIAGARA_API void Tick(FNiagaraWorldManager* WorldMan, float DeltaSeconds, ETickingGroup TickGroup);
	
	static NIAGARA_API UNiagaraDataChannelHandler* FindDataChannelHandler(FNiagaraWorldManager* WorldMan, UNiagaraDataChannel* DataChannel);
};


#endif//UE_BUILD_SHIPPING


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
			T* Dest = ((T*)Data.GetData()) + Index;
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
			T* Src = ((T*)DataPtr) + Index;
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
};
