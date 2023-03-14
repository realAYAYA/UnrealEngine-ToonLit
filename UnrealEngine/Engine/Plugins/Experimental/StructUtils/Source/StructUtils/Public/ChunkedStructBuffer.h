// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "StructView.h"
#include "ChunkedStructBuffer.generated.h"

/**
 * Buffer where you can append heterogeneous structs, and iterate over them in order (no random access).
 * Note: If you use move assign or Append(&&), make sure to call Compact() or you may end up with large free list.
 */
USTRUCT()
struct STRUCTUTILS_API FChunkedStructBuffer
{
	GENERATED_BODY()

	static constexpr int32 DefaultChunkSize = 16384;

	FChunkedStructBuffer(const int32 InChunkSize = DefaultChunkSize)
		: ChunkSize(InChunkSize)
	{
	}

	FChunkedStructBuffer(const FChunkedStructBuffer& Other)
	{
		ChunkSize = Other.ChunkSize;
		Append(Other);
	}
	
	FChunkedStructBuffer(FChunkedStructBuffer&& Other)
	{
		ChunkSize = Other.ChunkSize;
		Append(Other);
	}

	~FChunkedStructBuffer()
	{
		Clear();
	}

	FChunkedStructBuffer& operator=(const FChunkedStructBuffer& Other)
	{
		Reset();
		Append(Other);
		return *this;
	}
	
	FChunkedStructBuffer& operator=(FChunkedStructBuffer&& Other)
	{
		Reset();
		Append(Other);
		return *this;
	}

	void Append(const FChunkedStructBuffer& Other)
	{
		Other.ForEach([this](FStructView View)
		{
			Add(View);
		});
	}
	
	void Append(FChunkedStructBuffer&& Other)
	{
		if (Other.ChunkSize == ChunkSize)
		{
			// If same chunk size, steal chunks.
			NumItems += Other.NumItems;
			Head->Next = Other.First;
			Head = Other.Head;

			Other.NumItems = 0;
			Other.First = nullptr;
			Other.Head = nullptr;
		}
		else
		{
			Other.ForEach([this](FStructView View)
			{
				Add(View);
			});
		}
		Other.Clear();
	}

	/** Emplaces struct in the buffer. */
	template<typename T, typename... TArgs>
	void Emplace(TArgs&&... InArgs)
	{
		const UScriptStruct* InScriptStruct = TBaseStructure<T>::Get();
		uint8* ItemMemory = AllocItem(InScriptStruct);
		new (ItemMemory) T(Forward<TArgs>(InArgs)...);
	}

	/** Emplaces struct in the buffer, and returns reference to it. */
	template<typename T, typename... TArgs>
	T& Emplace_GetRef(TArgs&&... InArgs)
	{
		const UScriptStruct* InScriptStruct = TBaseStructure<T>::Get();
		uint8* ItemMemory = AllocItem(InScriptStruct);
		return *new (ItemMemory) T(Forward<TArgs>(InArgs)...);
	}

	/** Add new struct in the buffer. */
	template<typename T>
	void Add(const T& InStruct)
	{
		const UScriptStruct* InScriptStruct = TBaseStructure<T>::Get();
		uint8* ItemMemory = AllocItem(InScriptStruct);
		new (ItemMemory) T(InStruct);
	}

	/** Add new struct in the buffer, and return reference to it. */
	template<typename T>
	T& Add_GetRef(const T& InStruct)
	{
		const UScriptStruct* InScriptStruct = TBaseStructure<T>::Get();
		uint8* ItemMemory = AllocItem(InScriptStruct);
		return *new (ItemMemory) T(InStruct);
	}

	/** Adds struct to the buffer based on ScriptStruct and pointer. */
	void Add(const FStructView Struct)
	{
		check(Struct.IsValid());
		const UScriptStruct* InScriptStruct = Struct.GetScriptStruct();
		uint8* ItemMemory = AllocItem(InScriptStruct);
		InScriptStruct->InitializeStruct(ItemMemory);
		InScriptStruct->CopyScriptStruct(ItemMemory, Struct.GetMemory());
	}

	/** Iterates over all structs and calls Function on each item */
	template<typename TFunc>
	void ForEach(TFunc&& Function) const
	{
		for (FChunkHeader* Chunk = First; Chunk != nullptr; Chunk = Chunk->Next)
		{
			uint8* ItemMemory = reinterpret_cast<uint8*>(Chunk) + sizeof(FChunkHeader);
			for (int32 Index = 0; Index < Chunk->NumItems; Index++)
			{
				FItemHeader& Item = *reinterpret_cast<FItemHeader*>(ItemMemory);
				check(Item.ScriptStructIndex < MaxScriptStructsPerChunk);
			
				Function(FStructView(Chunk->ScriptStructs[Item.ScriptStructIndex], ItemMemory + Item.Offset));

				ItemMemory += Item.Size;
			}
		}
	}

	/**
	 * Iterates over all structs of specified type T and calls Function on each item.
	 * Usage: Buffer.ForEach<FFoo>([](Foo& Item) { ... });
	 */
	template<typename T, typename TFunc>
	void ForEach(TFunc&& Function) const
	{
		for (FChunkHeader* Chunk = First; Chunk != nullptr; Chunk = Chunk->Next)
		{
			// Build mask of the accepted struct in the chunk.
			const int32 StructIndex = Chunk->GetScriptStructIndex(TBaseStructure<T>::Get());
			if (StructIndex == INDEX_NONE)
			{
				continue;
			}
			const uint32 FilterMask = 1U << StructIndex;
			
			uint8* ItemMemory = reinterpret_cast<uint8*>(Chunk) + sizeof(FChunkHeader);
			for (int32 Index = 0; Index < Chunk->NumItems; Index++)
			{
				FItemHeader& Item = *reinterpret_cast<FItemHeader*>(ItemMemory);
				const uint32 StructMask = 1U << Item.ScriptStructIndex;

				if ((FilterMask & StructMask) != 0)
				{
					check(Item.ScriptStructIndex < MaxScriptStructsPerChunk);
					Function(*reinterpret_cast<T*>(ItemMemory + Item.Offset));
				}

				ItemMemory += Item.Size;
			}
		}
	}

	/** Iterates over all structs of specified types and calls Function on each item */
	template<typename TFunc>
	void ForEachFiltered(TArrayView<const UScriptStruct*> AcceptedScriptStructs, TFunc&& Function) const
	{
		for (FChunkHeader* Chunk = First; Chunk != nullptr; Chunk = Chunk->Next)
		{
			// Build mask of all accepted structs in the chunk.
			uint32 FilterMask = 0;
			for (const UScriptStruct* ScriptStruct : AcceptedScriptStructs)
			{
				const int32 StructIndex = Chunk->GetScriptStructIndex(ScriptStruct);
				if (StructIndex != INDEX_NONE)
				{
					FilterMask |= 1U << StructIndex;
				}
			}

			// Chunk does not match any of the structs specified in the filter.
			if (FilterMask == 0)
			{
				continue;
			}
			
			uint8* ItemMemory = reinterpret_cast<uint8*>(Chunk) + sizeof(FChunkHeader);
			for (int32 Index = 0; Index < Chunk->NumItems; Index++)
			{
				FItemHeader& Item = *reinterpret_cast<FItemHeader*>(ItemMemory);
				const uint32 StructMask = 1U << Item.ScriptStructIndex;

				if ((FilterMask & StructMask) != 0)
				{
					check(Item.ScriptStructIndex < MaxScriptStructsPerChunk);
					Function(FStructView(Chunk->ScriptStructs[Item.ScriptStructIndex], ItemMemory + Item.Offset));
				}

				ItemMemory += Item.Size;
			}
		}
	}

	/** Returns struct types in the buffer */
	void GetScriptStructs(TArray<const UScriptStruct*>& OutScriptStructs) const
	{
		OutScriptStructs.Reset();
		for (const FChunkHeader* Chunk = First; Chunk != nullptr; Chunk = Chunk->Next)
		{
			// Add reference to the ScriptStruct object.
			for (int32 Index = 0; Index < Chunk->NumScriptStructs; Index++)
			{
				OutScriptStructs.AddUnique(Chunk->ScriptStructs[Index]);
			}
		}
	}

	/** Resets and clears all structs, keeps internal memory. */
	void Reset();

	/** Releases unused internal memory. */
	void Compact();

	/** Resets and clears all structs, and releases unused internal memory. */
	void Clear();

	/** @return Number of structs added to the buffer. */
	int32 Num() const { return NumItems; }

	/** @return True if the buffer is empty. */
	bool IsEmpty() const { return NumItems == 0; }

	/** @return Chunk size */
	int32 GetChunkSize() const { return ChunkSize; }

	/** @return Number of chunks in use. */
	int32 GetNumUsedChunks() const
	{
		int32 Count = 0;
		for (const FChunkHeader* Chunk = First; Chunk != nullptr; Chunk = Chunk->Next)
		{
			Count++;
		}
		return Count;
	}

	/** @return Number of  chunks in freelist. */
	int32 GetNumFreeChunks() const
	{
		int32 Count = 0;
		for (const FChunkHeader* Chunk = FreeList; Chunk != nullptr; Chunk = Chunk->Next)
		{
			Count++;
		}
		return Count;
	}

	void AddStructReferencedObjects(class FReferenceCollector& Collector);

protected:

	/** Number of unique struct types that can be stored per chunk. */
	static constexpr int32 MaxScriptStructsPerChunk = 8;

	/** Header for each item in a chunk. Allocated before the item in tge chunk memory. */
	struct FItemHeader
	{
		int32 Size;					/** Size of the item including this header */
		int16 Offset;				/** relative offset to the item data */ 
		int16 ScriptStructIndex;	/** Index in chunk ScriptStruct array to the type of the item */ 
	};

	/** Header for a chunk of items. */
	struct FChunkHeader
	{
		/** @returns Index to the ScriptStruct stored in the chunk, or INDEX_NONE if cannot store the ScriptStruct. */
		int32 GetScriptStructIndex(const UScriptStruct* InScriptStruct)
		{
			for (int32 Index = 0; Index < NumScriptStructs; Index++)
			{
				if (ScriptStructs[Index] == InScriptStruct)
				{
					return Index;
				}
			}

			if (NumScriptStructs < MaxScriptStructsPerChunk)
			{
				ScriptStructs[NumScriptStructs++] = InScriptStruct;
				return NumScriptStructs - 1;
			}
			
			return INDEX_NONE;
		}
		
		const UScriptStruct* ScriptStructs[MaxScriptStructsPerChunk];	/** Types ScriptStructs stored in this chunk. */
		int32 NumScriptStructs;											/** Number of ScriptsStructs */
		int32 UsedSize;													/** Size of the used memory in the chunk. */
		int32 NumItems;													/** Number of items in the chunks */
		FChunkHeader* Next;												/** Pointer to next chunk. */
	};

	/** Allocates a new chunk and makes it the head of the chunk list. */
	void AllocateNewChunk();

	/** Allocates an item of specified type. */
	uint8* AllocItem(const UScriptStruct* InScriptStruct)
	{
		check(InScriptStruct);

		const int32 MinAlignment = InScriptStruct->GetMinAlignment();
		const int32 ScriptStructSize = InScriptStruct->GetStructureSize();
		check((Align(sizeof(FItemHeader), MinAlignment) + ScriptStructSize) < ChunkSize);

		// Make sure there's a chunk prsent.
		if (Head == nullptr)
		{
			AllocateNewChunk();
		}
		check(Head != nullptr);

		// Make sure there's space for the struct type and allocation
		int32 ScriptStructIndex = Head->GetScriptStructIndex(InScriptStruct);
		int32 HeaderOffset = Head->UsedSize;
		int32 ItemOffset = Align(Head->UsedSize + sizeof(FItemHeader), MinAlignment);
		
		if (ScriptStructIndex == INDEX_NONE || (ItemOffset + ScriptStructSize) > ChunkSize)
		{
			AllocateNewChunk();
			ScriptStructIndex = Head->GetScriptStructIndex(InScriptStruct);
			HeaderOffset = Head->UsedSize;
			ItemOffset = Align(Head->UsedSize + sizeof(FItemHeader), MinAlignment);
			check(ScriptStructIndex != INDEX_NONE);
			check((ItemOffset + ScriptStructSize) <= ChunkSize)
		}

		uint8* ChunkMemory = reinterpret_cast<uint8*>(Head) + sizeof(FChunkHeader);
		Head->UsedSize = ItemOffset + ScriptStructSize;
		Head->NumItems++;

		FItemHeader& ItemHeader = *reinterpret_cast<FItemHeader*>(ChunkMemory + HeaderOffset);
		ItemHeader.Size = (ItemOffset + ScriptStructSize) - HeaderOffset;
		ItemHeader.Offset = ItemOffset - HeaderOffset;
		ItemHeader.ScriptStructIndex = ScriptStructIndex;

		NumItems++;
		
		return ChunkMemory + ItemOffset;
	}

	FChunkHeader* First = nullptr;		/** Pointer to the first chunk in use */
	FChunkHeader* Head = nullptr;		/** Pointer to the last chunk in use */
	FChunkHeader* FreeList = nullptr;	/** Free list of unused chunks */
	int32 NumItems = 0;					/** Number of items stored in the buffer */
	int32 ChunkSize = 0;				/** Size of each chunk */
};


template<>
struct TStructOpsTypeTraits<FChunkedStructBuffer> : public TStructOpsTypeTraitsBase2<FChunkedStructBuffer>
{
	enum
	{
		WithAddStructReferencedObjects = true,
	};
};