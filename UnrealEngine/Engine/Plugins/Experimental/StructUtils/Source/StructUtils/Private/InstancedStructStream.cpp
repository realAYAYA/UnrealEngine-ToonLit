// Copyright Epic Games, Inc. All Rights Reserved.
#include "InstancedStructStream.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InstancedStructStream)

void FInstancedStructStream::Reset()
{
	ForEach([](FStructView View)
	{
		View.GetScriptStruct()->DestroyStruct(View.GetMutableMemory());
	});

	// Cleanup chunks
	for (FChunkHeader* Chunk = First; Chunk != nullptr; Chunk = Chunk->Next)
	{
		for (int32 Index = 0; Index < MaxScriptStructsPerChunk; Index++)
		{
			Chunk->ScriptStructs[Index] = nullptr;
		}
		Chunk->UsedSize = 0;
		Chunk->NumScriptStructs = 0;
		Chunk->NumItems = 0;
	}
	NumItems = 0;

	// Return chunks to freelist.
	if (Head != nullptr)
	{
		check(First != nullptr);
		Head->Next = FreeList;
		FreeList = First;
	}
	
	First = nullptr;
	Head = nullptr;
}

void FInstancedStructStream::Compact()
{
	// Free FreeList chunks
	FChunkHeader* Chunk = FreeList;
	while (Chunk)
	{
		FChunkHeader* NextChunk = Chunk->Next;
		FMemory::Free(Chunk);
		Chunk = NextChunk;
	}
	FreeList = nullptr;
}

void FInstancedStructStream::Clear()
{
	Reset();
	Compact();
}

void FInstancedStructStream::AllocateNewChunk()
{
	check(ChunkSize > 0);

	// Allocate new chunk, use existing chunk from free list if possible.
	FChunkHeader* Chunk = nullptr;
	if (FreeList)
	{
		Chunk = FreeList;
		FreeList = FreeList->Next;
	}
	else
	{
		Chunk = (FChunkHeader*)FMemory::Malloc(sizeof(FChunkHeader) + ChunkSize);
	}
	check(Chunk != nullptr);

	Chunk->UsedSize = 0;
	Chunk->NumScriptStructs = 0;
	Chunk->NumItems = 0;
	Chunk->Next = nullptr;

	// Insert chunk the linked list.
	if (First == nullptr)
	{
		First = Chunk;
	}
	
	if (Head != nullptr)
	{
		Head->Next = Chunk;
	}
	
	Head = Chunk;

	check(First != nullptr);
	check(Head != nullptr);
}

void FInstancedStructStream::AddStructReferencedObjects(class FReferenceCollector& Collector)
{
	for (FChunkHeader* Chunk = First; Chunk != nullptr; Chunk = Chunk->Next)
	{
		// Add reference to the ScriptStruct object.
		for (int32 Index = 0; Index < Chunk->NumScriptStructs; Index++)
		{
			Collector.AddReferencedObject(Chunk->ScriptStructs[Index]);
		}

		// Add references in the struct contents.
		uint8* ItemMemory = reinterpret_cast<uint8*>(Chunk) + sizeof(FChunkHeader);
		for (int32 Index = 0; Index < Chunk->NumItems; Index++)
		{
			FItemHeader& Item = *reinterpret_cast<FItemHeader*>(ItemMemory);
			const UScriptStruct* ScriptStruct = Chunk->ScriptStructs[Item.ScriptStructIndex];
			uint8* StructMemory = ItemMemory + Item.Offset;
			ItemMemory += Item.Size;
			
			if (ScriptStruct->StructFlags & STRUCT_AddStructReferencedObjects)
			{
				ScriptStruct->GetCppStructOps()->AddStructReferencedObjects()(StructMemory, Collector);
			}
			else if (ScriptStruct->StructFlags & STRUCT_IsPlainOldData)
			{
				// Dont bother with POD types.
				continue;
			}
			else
			{
				// The iterator will recursively loop through all structs in structs too.
				for (TPropertyValueIterator<const FObjectProperty> It(ScriptStruct, StructMemory); It; ++It)
				{
					Collector.AddReferencedObject(It.Key()->GetObjectPtrPropertyValueRef(It.Value()));
				}
			}
		}
	}
}
