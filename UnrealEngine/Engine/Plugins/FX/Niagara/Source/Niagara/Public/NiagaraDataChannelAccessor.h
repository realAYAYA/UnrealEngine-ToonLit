// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraDataChannelPublic.h"
#include "NiagaraDataChannel.h"
#include "NiagaraDataChannelAccessor.generated.h"

class UNiagaraDataChannelHandler;
struct FNiagaraSpawnInfo;
struct FNiagaraVariableBase;

/** 
Initial simple API for reading and writing data in a data channel from game code / BP. 
Likely to be replaced in the near future with a custom BP node and a helper struct.
*/


UCLASS(Experimental, BlueprintType, MinimalAPI)
class UNiagaraDataChannelReader : public UObject
{
	GENERATED_BODY()
private:

	FNiagaraDataChannelDataPtr Data = nullptr;
	bool bReadingPreviousFrame = false;

	template<typename T>
	bool ReadData(const FNiagaraVariableBase& Var, int32 Index, T& OutData)const;

public:

	UPROPERTY()
	TObjectPtr<UNiagaraDataChannelHandler> Owner;
	
	/** Call before each access to the data channel to grab the correct data to read. */
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API bool InitAccess(FNiagaraDataChannelSearchParameters SearchParams, bool bReadPrevFrameData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API int32 Num()const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API double ReadFloat(FName VarName, int32 Index, bool& IsValid)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API FVector2D ReadVector2D(FName VarName, int32 Index, bool& IsValid)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API FVector ReadVector(FName VarName, int32 Index, bool& IsValid)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API FVector4 ReadVector4(FName VarName, int32 Index, bool& IsValid)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API FQuat ReadQuat(FName VarName, int32 Index, bool& IsValid)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API FLinearColor ReadLinearColor(FName VarName, int32 Index, bool& IsValid)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API int32 ReadInt(FName VarName, int32 Index, bool& IsValid)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API uint8 ReadEnum(FName VarName, int32 Index, bool& IsValid)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API bool ReadBool(FName VarName, int32 Index, bool& IsValid)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API FVector ReadPosition(FName VarName, int32 Index, bool& IsValid)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API FNiagaraID ReadID(FName VarName, int32 Index, bool& IsValid)const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "Niagara DataChannel"))
	NIAGARA_API FNiagaraSpawnInfo ReadSpawnInfo(FName VarName, int32 Index, bool& IsValid)const;
};

UCLASS(Experimental, BlueprintType, MinimalAPI)
class UNiagaraDataChannelWriter : public UObject
{
	GENERATED_BODY()
private:

	/** Local data buffers we're writing into. */
	FNiagaraDataChannelGameDataPtr Data = nullptr;

	template<typename T>
	void WriteData(const FNiagaraVariableBase& Var, int32 Index, const T& InData);

public:

	UPROPERTY()
	TObjectPtr<UNiagaraDataChannelHandler> Owner;
	
	/** Call before each batch of writes to allocate the data we'll be writing to. */
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (bVisibleToGame="true", bVisibleToCPU="true", bVisibleToGPU="true", Keywords = "niagara DataChannel", AdvancedDisplay = "DebugSource", AutoCreateRefTerm="DebugSource"))
	NIAGARA_API bool InitWrite(FNiagaraDataChannelSearchParameters SearchParams, int32 Count, bool bVisibleToGame, bool bVisibleToCPU, bool bVisibleToGPU, const FString& DebugSource);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API int32 Num()const;

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WriteFloat(FName VarName, int32 Index, double InData);
	
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WriteVector2D(FName VarName, int32 Index, FVector2D InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WriteVector(FName VarName, int32 Index, FVector InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WriteVector4(FName VarName, int32 Index, FVector4 InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WriteQuat(FName VarName, int32 Index, FQuat InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WriteLinearColor(FName VarName, int32 Index, FLinearColor InData);
	
	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WriteInt(FName VarName, int32 Index, int32 InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WriteEnum(FName VarName, int32 Index, uint8 InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WriteBool(FName VarName, int32 Index, bool InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WriteSpawnInfo(FName VarName, int32 Index, FNiagaraSpawnInfo InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WritePosition(FName VarName, int32 Index, FVector InData);

	UFUNCTION(BlueprintCallable, Category = NiagaraDataChannel, meta = (Keywords = "niagara DataChannel"))
	NIAGARA_API void WriteID(FName VarName, int32 Index, FNiagaraID InData);


};



/** Base clas for template data channel writer objects. Provides common, non-template functionality. */
struct FNiagaraDataChannelGameDataWriterBase
{
	FNiagaraDataChannelGameDataWriterBase(FNiagaraDataChannelGameDataPtr InData)
	: Data(InData)
	{
	}

	/** Call before we begin writing to this data. */
	NIAGARA_API void BeginWrite();

	/** Publish the contents of this writer's Data buffer to the given data channel data. */
	NIAGARA_API void Publish(FNiagaraDataChannelDataPtr& Destination);

	//Binds a specific variable to an index.
	NIAGARA_API bool BindVariable(int32 VariableIndex, const FNiagaraVariableBase& Var);

	// Set the size of buffers for all variables.
	NIAGARA_API void SetNum(int32 Num);

	// Reserve space in buffers for all variables.
	NIAGARA_API void Reserve(int32 Num);

	/** Adds count elements to all variable buffers. */
	NIAGARA_API int32 Add(int32 Count = 1);

	FNiagaraDataChannelGameDataPtr& GetData(){ return Data; }

protected:
	FNiagaraDataChannelGameDataPtr Data = nullptr;
};

//Templated utility writer class.
//Provides an interface for pushing specific types easily into Niagara Data Channels.
template<typename T>
struct FNiagaraDataChannelGameDataWriter : public FNiagaraDataChannelGameDataWriterBase
{
	FNiagaraDataChannelGameDataWriter(FNiagaraDataChannelGameDataPtr InData)
		: FNiagaraDataChannelGameDataWriterBase(InData)
	{
	}

	/** Write data to a specific variable index. */
	bool Write(int32 Index, const T& InData)
	{
		//static_assert(false, "All implementations must provide a write function");
		return false;
	}
};

/**
* Below is an example implementation of FNiagaraDataChannelGameDataWriter.
* It allows pushing of a simple set of data into a Niagara Data Channel.
struct FExampleNDCData
{
	FVector Position;
	FVector PrevPosition;
	FVector Velocity;
	FNiagaraID ID;
};

template<typename T>
struct NIAGARA_API FNiagaraDataChannelGameDataWriter<FExampleNDCData> : public FNiagaraDataChannelGameDataWriterBase
{
	FNiagaraDataChannelGameDataWriter(FNiagaraDataChannelGameDataPtr InData)
		: FNiagaraDataChannelGameDataWriterBase(InData)
	{
		if (FNiagaraDataChannelGameData* DataPtr = Data.Get())
		{
			PositionBuffer = DataPtr->FindVariableBuffer(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Position")));
			PrevPositionBuffer = = DataPtr->FindVariableBuffer(FNiagaraVariable(FNiagaraTypeDefinition::GetPositionDef(), TEXT("PrevPosition")));
			VelocityBuffer = = DataPtr->FindVariableBuffer(FNiagaraVariable(FNiagaraTypeHelper::GetVectorDef(), TEXT("Velocity")));
			IDBuffer = = DataPtr->FindVariableBuffer(FNiagaraVariable(FNiagaraTypeDefinition::GetIDDef(), TEXT("ID")));

			if(PositionBuffer == nullptr || PreviousPositionBuffer == nullptr || VelocityBuffer == nullptr || IDBuffer == nullptr)
			{
				Data.Reset();
			}
		}
	}

	bool Write(int32 Index, const FLWProjectileNDCData& InData);
	{
		if(Data)
		{
			check(PositionBuffer && PreviousPositionBuffer && VelocityBuffer && IDBuffer);

			PositionBuffer->Write(InData.Position);
			PrevPositionBuffer->Write(InData.PrevPosition);
			VelocityBuffer->Write(InData.Velocity);
			IDBuffer->Write(InData.ID);
		}
	}

	const FNiagaraDataChannelVariableBuffer* PositionBuffer = nullptr;
	const FNiagaraDataChannelVariableBuffer* PrevPositionBuffer = nullptr;
	const FNiagaraDataChannelVariableBuffer* VelocityBuffer = nullptr;
	const FNiagaraDataChannelVariableBuffer* IDBuffer = nullptr;
};

*/

//////////////////////////////////////////////////////////////////////////
// FNiagaraDataChannelGameDataGroupedWriter 
// A utility class allowing for many persistent items to be written each frame to one or more Data Channels in efficient batches.
// Items are grouped together for allocation and publishing to the NDC by the destination NDC data they'll write to based on the FNiagaraDataChannelSearchParameters.
// Must use a type the has an implementation of FNiagaraDataChannelGameDataWriter.
// Usage:
// 
// Construct:
// Create the FNiagaraDataChannelGameDataGroupedWriter with desired type.
// 
// Add/Remove
// When new Items you want to track are created call AddItem(). This will return a GroupIndex that you should store with each Item.
// Items can be added to be written into different Niagara Data Channels if needed. Items are grouped by the specific NDC data they write to.
// Call RemoveItem when the items you're tracking are destroyed.
// 
// Writing:
// Ideally write once per frame.
// To write, construct an FScopedWriter from your FNiagaraDataChannelGameDataGroupedWriter.
// This new scoped writer will allocate space to write as many items as have been added in each group via AddItem.
// Writing more than this will not cause any major harm but will cause additional allocations.
// Results will be published to the NDC when the scoped writer is destroyed.
// Call Append(), passing each item's GroupIndex and the data to write.

struct FNiagaraDataChannelGameDataGroupedWriterBase
{
	NIAGARA_API FNiagaraDataChannelDataPtr FindDataChannelData(UWorld* World, const UNiagaraDataChannel* NDC, const FNiagaraDataChannelSearchParameters& ItemSearchParams);
};

template<typename T>
struct FNiagaraDataChannelGameDataGroupedWriter : public FNiagaraDataChannelGameDataGroupedWriterBase
{
private:
	struct FGroupInfo
	{
		//A writer helper for each group;
		FNiagaraDataChannelGameDataWriter<T> Writer;
				
		int32 NumItems = 0;

		FGroupInfo(const UNiagaraDataChannel* DataChannel, FNiagaraDataChannelGameDataGroupedWriter& Owner)
		: Writer(DataChannel ? DataChannel->CreateGameData() : nullptr)
		{
		}
	};

	TMap<FNiagaraDataChannelDataPtr, int32> GroupMap;
	TArray<FGroupInfo> Groups;

public:

	/**
	 Scoped utility writer class for appending items to a FNiagaraDataChannelGameDataGroupedWriter.
	*/
	struct FScopedWriter
	{
	public:
		FScopedWriter(FNiagaraDataChannelGameDataGroupedWriter<T>& InOwner)	: Owner(InOwner)
		{
			Owner.BeginWrite();
		}

		~FScopedWriter()
		{
			Owner.EndWrite();
		}

		void Append(int32 GroupIdx, const T& InData)
		{
			FNiagaraDataChannelGameDataWriter<T>& Writer = Owner.GetWriter(GroupIdx);
			int32 DataIndex = Writer.Add();
			Writer.Write(DataIndex, InData);
		}

	private:
		FNiagaraDataChannelGameDataGroupedWriter<T>& Owner;
	};

	FScopedWriter CreateScopedWriter(){ return FScopedWriter(*this); }

	void BeginWrite()
	{
		for (auto It = GroupMap.CreateIterator(); It; ++It)
		{
			FNiagaraDataChannelDataPtr& Data = It.Key();
			int32 Index = It.Value();

			if (Groups.IsValidIndex(Index) && Groups[Index].NumItems > 0)
			{
				FGroupInfo& Group = Groups[Index];
				Group.Writer.BeginWrite();
				Group.Writer.Reserve(Group.NumItems);
			}
			else
			{
				It.RemoveCurrent();
			}
		}
	}

	void EndWrite()
	{
		for (auto& GroupPair : GroupMap)
		{
			FNiagaraDataChannelDataPtr& Data = GroupPair.Key;
			int32 Index = GroupPair.Value;
			if (Groups.IsValidIndex(Index))
			{
				Groups[Index].Writer.Publish(Data);
			}
		}
	}

	/** 
	Adds a new item to be written to the correct data channel group based on the search params.
	Returns the group index this item now belongs to.
	*/
	int32 AddItem(UWorld* World, const UNiagaraDataChannel* NDC, const FNiagaraDataChannelSearchParameters& ItemSearchParams)
	{
		if(FNiagaraDataChannelDataPtr DataChannelData = FindDataChannelData(World, NDC, ItemSearchParams))
		{
			int32 LocalGroupIndex = INDEX_NONE;
			if (int32* FoundGroup = GroupMap.Find(DataChannelData))
			{
				LocalGroupIndex = *FoundGroup;
			}
			else
			{
				//We don't have an group for this data. Create a new group.
				//Try to reuse an old group first.
				LocalGroupIndex = Groups.IndexOfByPredicate([](const auto& Info) { return Info.NumItems == 0; });
				if (LocalGroupIndex == INDEX_NONE)
				{
					LocalGroupIndex = Groups.Num();
					Groups.Emplace(NDC, *this);
				}
				GroupMap.Add(DataChannelData) = LocalGroupIndex;
			}

			++Groups[LocalGroupIndex].NumItems;
			return LocalGroupIndex;
		}
		return INDEX_NONE;
	}

	void RemoveItem(int32 GroupIndex)
	{
		--Groups[GroupIndex].NumItems;
	}

	/** Returns the writer object for the given group index. */
	FNiagaraDataChannelGameDataWriter<T>& GetWriter(int32 GroupIndex)
	{
		return Groups[GroupIndex].Writer;
	}

	/** Returns the number of items in a given group. */
	int32 GetNumItems(int32 GroupIndex)const
	{
		return Groups[GroupIndex].NumItems;
	}
};
