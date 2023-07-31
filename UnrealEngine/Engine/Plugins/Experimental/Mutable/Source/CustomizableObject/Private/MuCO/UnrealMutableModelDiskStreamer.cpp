// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealMutableModelDiskStreamer.h"

#include "Async/AsyncFileHandle.h"
#include "Containers/UnrealString.h"
#include "CoreGlobals.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "HAL/IConsoleManager.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Misc/AssertionMacros.h"
#include "MuCO/CustomizableObject.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuR/Model.h"
#include "MuR/MutableTrace.h"
#include "MuR/Ptr.h"
#include "Serialization/Archive.h"
#include "Stats/Stats2.h"
#include "Templates/UnrealTemplate.h"
#include "Trace/Detail/Channel.h"

#if WITH_EDITOR
#include "HAL/PlatformFileManager.h"
#endif


//-------------------------------------------------------------------------------------------------
#if WITH_EDITOR

UnrealMutableOutputStream::UnrealMutableOutputStream(FArchive& ar)
	: m_ar(ar)
{
}


void UnrealMutableOutputStream::Write(const void* pData, uint64 size)
{
	// TODO: ugly const cast
	m_ar.Serialize(const_cast<void*>(pData), size);
}

#endif

//-------------------------------------------------------------------------------------------------
UnrealMutableInputStream::UnrealMutableInputStream(FArchive& ar)
	: m_ar(ar)
{
}


void UnrealMutableInputStream::Read(void* pData, uint64 size)
{
	m_ar.Serialize(pData, size);
}


//-------------------------------------------------------------------------------------------------
FUnrealMutableModelBulkStreamer::FUnrealMutableModelBulkStreamer(FArchive* InMainDataArchive, FArchive* InStreamedDataArchive)
{
#if WITH_EDITOR
	MainDataArchive = InMainDataArchive;
	StreamedDataArchive = InStreamedDataArchive;
	CurrentWriteFile = nullptr;
#endif
}

FUnrealMutableModelBulkStreamer::~FUnrealMutableModelBulkStreamer()
{
	EndStreaming();
}


void FUnrealMutableModelBulkStreamer::OpenWriteFile(const char* strModelName, uint64 key0)
{
#if WITH_EDITOR
	if (key0 == 0) // Model
	{
		check(MainDataArchive);
		CurrentWriteFile = MainDataArchive;
	}
	else
	{
		check(StreamedDataArchive);
		CurrentWriteFile = StreamedDataArchive;
	}
#endif
}


void FUnrealMutableModelBulkStreamer::Write(const void* pBuffer, uint64 size)
{
#if WITH_EDITOR
	check(CurrentWriteFile);
	CurrentWriteFile->Serialize(const_cast<void*>(pBuffer), size);
#endif
}


void FUnrealMutableModelBulkStreamer::CloseWriteFile()
{
#if WITH_EDITOR
	CurrentWriteFile = nullptr;
#endif
}


bool FUnrealMutableModelBulkStreamer::PrepareStreamingForObject(UCustomizableObject* CustomizableObject)
{
	// This happens in the game thread
	check(IsInGameThread());

	if (!CustomizableObject)
	{
		check(false);
		return false;
	}

	// See if we can free previuously allocated resources
	for (int32 ObjectIndex = 0; ObjectIndex < Objects.Num(); )
	{
		if (!Objects[ObjectIndex].Model.Pin())
		{
			// The CustomizableObject is gone, so we won't be streaming for it anymore.
			for (TPair<OPERATION_ID, IAsyncReadRequest*>& it : Objects[ObjectIndex].CurrentReadRequests)
			{
				it.Value->WaitCompletion();
				delete it.Value;
			}

			for (IAsyncReadFileHandle* ReadFileHandle : Objects[ObjectIndex].ReadFileHandles)
			{
				delete ReadFileHandle;
			}

			Objects.RemoveAtSwap(ObjectIndex);
		}
		else
		{
			++ObjectIndex;
		}
	}

	// Is the object already prepared for streaming?
	bool bAlreadyStreaming = Objects.FindByPredicate(
		[CustomizableObject](const FObjectData& d) 
		{ return d.Model.Pin() == CustomizableObject->GetModel(); })
		!= 
		nullptr;

	if (!bAlreadyStreaming)
	{
		FObjectData NewData;
		NewData.Model = mu::WeakPtr<mu::Model>(CustomizableObject->GetModel());

#if WITH_EDITOR
		FString FolderPath = CustomizableObject->GetCompiledDataFolderPath(true);
		FString FullFileName = FolderPath + CustomizableObject->GetCompiledDataFileName(false);

		IAsyncReadFileHandle* ReadFileHandle = FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*FullFileName);
		if (!ReadFileHandle)
		{
			UE_LOG(LogMutable, Warning, TEXT("Streaming: Customizable Object %s is missing the Editor BulkData."), *CustomizableObject->GetName());
			return false;
		}

		NewData.ReadFileHandles.Add(ReadFileHandle);
#else
		const UCustomizableObjectBulk* BulkData = CustomizableObject->GetStreamableBulkData();
		if (!BulkData)
		{
			UE_LOG(LogMutable, Warning, TEXT("Streaming: Customizable Object %s is missing the BulkData export."), *CustomizableObject->GetName());
			return false;
		}

		NewData.ReadFileHandles = BulkData->GetAsyncReadFileHandles();
#endif


		if (NewData.ReadFileHandles.IsEmpty())
		{
			UE_LOG(LogMutable, Warning, TEXT("Streaming: Customizable Object %s read file handles empty."), *CustomizableObject->GetName());

			check(false);
			return false;
		}

		NewData.StreamableBlocks = CustomizableObject->HashToStreamableBlock;
		if (NewData.StreamableBlocks.IsEmpty())
		{
			UE_LOG(LogMutable, Warning, TEXT("Streaming: Customizable Object %s has no data to stream."), *CustomizableObject->GetName());

#if !WITH_EDITOR
			check(false);
#endif
			return false;
		}

		Objects.Add(MoveTemp(NewData));
	}

	return true;
}


#if WITH_EDITOR
void FUnrealMutableModelBulkStreamer::CancelStreamingForObject(const UCustomizableObject* CustomizableObject)
{
	// This happens in the game thread
	check(IsInGameThread());

	if (!CustomizableObject)
	{
		check(false);
	}

	// See if we can free previuously allocated resources
	for (int32 ObjectIndex = 0; ObjectIndex < Objects.Num(); ++ObjectIndex)
	{
		if (Objects[ObjectIndex].Model.Pin()==CustomizableObject->GetModel())
		{
			for (TPair<OPERATION_ID, IAsyncReadRequest*>& it : Objects[ObjectIndex].CurrentReadRequests)
			{
				it.Value->WaitCompletion();
				delete it.Value;
			}

			for (IAsyncReadFileHandle* ReadFileHandle : Objects[ObjectIndex].ReadFileHandles)
			{
				delete ReadFileHandle;
			}

			Objects.RemoveAtSwap(ObjectIndex);
			break;
		}
	}
}

#endif // WITH_EDITOR


void FUnrealMutableModelBulkStreamer::EndStreaming()
{
	for (FObjectData& o : Objects)
	{
		for (TPair<OPERATION_ID, IAsyncReadRequest*>& it : o.CurrentReadRequests)
		{
			it.Value->WaitCompletion();
			delete it.Value;
		}

		for (IAsyncReadFileHandle* ReadFileHandle : o.ReadFileHandles)
		{
			delete ReadFileHandle;
		}
	}
	Objects.Empty();
}


static int32 StreamPriority = 4;
FAutoConsoleVariableRef CVarStreamPriority(
	TEXT("Mutable.StreamPriority"),
	StreamPriority,
	TEXT(""));


mu::ModelStreamer::OPERATION_ID FUnrealMutableModelBulkStreamer::BeginReadBlock(const mu::Model* Model, uint64 Key, void* pBuffer, uint64 size)
{
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableModelBulkStreamer::OpenReadFile);

	UE_LOG(LogMutable, Verbose, TEXT("Streaming: reading data %08lx."), Key);

	// Find the object we are streaming for
	FObjectData* ObjectData = Objects.FindByPredicate(
		[Model](const FObjectData& d) {return d.Model.Pin()==Model; });

	if (!ObjectData)
	{
		// The object has been unloaded. Streaming is not possible. 
		// This may happen in the editor if we are recompiling an object but we still have instances from the old
		// object that have progressive mip generation.
		return -1;
	}

	mu::ModelStreamer::OPERATION_ID Result = 0;

	check(!ObjectData->ReadFileHandles.IsEmpty());

	// this generally cannot fail because it is async
	if (ObjectData->StreamableBlocks.Contains(Key))
	{
		Result = ++LastOperationID;

		const FMutableStreamableBlock& Block = ObjectData->StreamableBlocks[Key];

		int32 BulkDataOffsetInFile = 0;
#if WITH_EDITOR
		BulkDataOffsetInFile = sizeof(MutableCompiledDataStreamHeader);
#endif

		IAsyncReadRequest* ReadRequest = ObjectData->ReadFileHandles[Block.FileIndex]->ReadRequest(BulkDataOffsetInFile + Block.Offset, size, (EAsyncIOPriorityAndFlags)StreamPriority, nullptr, reinterpret_cast<uint8*>(pBuffer));
		ObjectData->CurrentReadRequests.Add(Result, ReadRequest);
	}
	else
	{
		// File Handle not found! This shouldn't really happen.
		UE_LOG(LogMutable, Error, TEXT("Streaming Block not found!"));
		check(false);
	}

	INC_DWORD_STAT(STAT_MutableStreamingOps);

	return Result;
}


bool FUnrealMutableModelBulkStreamer::IsReadCompleted(mu::ModelStreamer::OPERATION_ID OperationId)
{
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableModelBulkStreamer::IsReadCompleted);

	for (FObjectData& o : Objects)
	{
		if (IAsyncReadRequest** ReadRequest = o.CurrentReadRequests.Find(OperationId))
		{
			return (*ReadRequest)->PollCompletion();
		}
	}

	UE_LOG(LogMutable, Error, TEXT("Operation not found in IsReadCompleted."));
	check(false);
	return true;
}


void FUnrealMutableModelBulkStreamer::EndRead(mu::ModelStreamer::OPERATION_ID OperationId)
{
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableModelBulkStreamer::EndRead);

	bool bFound = false;
	for (FObjectData& o : Objects)
	{
		IAsyncReadRequest** ReadRequest = o.CurrentReadRequests.Find(OperationId);
		if (ReadRequest)
		{
			if (*ReadRequest)
			{
				bool bCompleted = (*ReadRequest)->WaitCompletion();
				if (!bCompleted)
				{
					UE_LOG(LogMutable, Error, TEXT("Operation failed to complete in EndRead."));
					check(false);
				}

				delete* ReadRequest;
			}
			o.CurrentReadRequests.Remove(OperationId);
			bFound = true;
			break;
		}
	}
	
	if (!bFound)
	{
		UE_LOG(LogMutable, Error, TEXT("Operation not found in EndRead."));
		check(false);
	}
}
