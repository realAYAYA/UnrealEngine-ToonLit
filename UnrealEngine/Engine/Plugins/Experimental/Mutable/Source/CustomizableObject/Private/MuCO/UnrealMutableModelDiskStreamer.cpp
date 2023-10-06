// Copyright Epic Games, Inc. All Rights Reserved.

#include "MuCO/UnrealMutableModelDiskStreamer.h"

#include "Async/AsyncFileHandle.h"
#include "HAL/PlatformFile.h"
#include "MuCO/CustomizableObjectSystem.h"
#include "MuR/Model.h"
#include "MuR/MutableTrace.h"

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
FUnrealMutableModelBulkReader::~FUnrealMutableModelBulkReader()
{
	EndStreaming();
}


bool FUnrealMutableModelBulkReader::PrepareStreamingForObject(UCustomizableObject* CustomizableObject)
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
			for (TPair<OPERATION_ID, FReadRequest>& it : Objects[ObjectIndex].CurrentReadRequests)
			{
				it.Value.ReadRequest->WaitCompletion();
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
		{ return d.Model.Pin().Get() == CustomizableObject->GetModel().Get(); })
		!=
		nullptr;

	if (!bAlreadyStreaming)
	{
		FObjectData NewData;
		NewData.Model = TWeakPtr<const mu::Model>(CustomizableObject->GetModel());

#if WITH_EDITOR
		FString FolderPath = CustomizableObject->GetCompiledDataFolderPath(true);
		FString FullFileName = FolderPath + CustomizableObject->GetCompiledDataFileName(false, nullptr, true);

		const TSharedPtr<IAsyncReadFileHandle> ReadFileHandle = MakeShareable(FPlatformFileManager::Get().GetPlatformFile().OpenAsyncRead(*FullFileName));
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
void FUnrealMutableModelBulkReader::CancelStreamingForObject(const UCustomizableObject* CustomizableObject)
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
		if (Objects[ObjectIndex].Model.Pin() == CustomizableObject->GetModel())
		{
			for (TPair<OPERATION_ID, FReadRequest>& it : Objects[ObjectIndex].CurrentReadRequests)
			{
				it.Value.ReadRequest->WaitCompletion();
			}

			Objects.RemoveAtSwap(ObjectIndex);
			break;
		}
	}
}


bool FUnrealMutableModelBulkReader::AreTherePendingStreamingOperationsForObject(const UCustomizableObject* CustomizableObject) const
{
	// This happens in the game thread
	check(IsInGameThread());

	if (!CustomizableObject)
	{
		check(false);
		return false;
	}

	for (int32 ObjectIndex = 0; ObjectIndex < Objects.Num(); ++ObjectIndex)
	{
		if (Objects[ObjectIndex].Model.Pin() == CustomizableObject->GetModel())
		{
			if (!Objects[ObjectIndex].CurrentReadRequests.IsEmpty())
			{
				return true;
			}
		}
	}

	return false;
}

#endif // WITH_EDITOR


void FUnrealMutableModelBulkReader::EndStreaming()
{
	for (FObjectData& o : Objects)
	{
		for (TPair<OPERATION_ID, FReadRequest>& it : o.CurrentReadRequests)
		{
			it.Value.ReadRequest->WaitCompletion();
		}
	}
	Objects.Empty();
}


static int32 StreamPriority = 4;
FAutoConsoleVariableRef CVarStreamPriority(
	TEXT("Mutable.StreamPriority"),
	StreamPriority,
	TEXT(""));


mu::ModelReader::OPERATION_ID FUnrealMutableModelBulkReader::BeginReadBlock(const mu::Model* Model, uint64 Key, void* pBuffer, uint64 size, TFunction<void(bool bSuccess)>* CompletionCallback)
{
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableModelBulkStreamer::OpenReadFile);

	UE_LOG(LogMutable, VeryVerbose, TEXT("Streaming: reading data %08llu."), Key);

	// Find the object we are streaming for
	FObjectData* ObjectData = Objects.FindByPredicate(
		[Model](const FObjectData& d) {return d.Model.Pin().Get() == Model; });

	if (!ObjectData)
	{
		// The object has been unloaded. Streaming is not possible. 
		// This may happen in the editor if we are recompiling an object but we still have instances from the old
		// object that have progressive mip generation.
		if (CompletionCallback)
		{
			(*CompletionCallback)(false);    		
		}
		return -1;
	}

	// this generally cannot fail because it is async
	if (!ObjectData->StreamableBlocks.Contains(Key))
	{
		// File Handle not found! This shouldn't really happen.
		UE_LOG(LogMutable, Error, TEXT("Streaming Block not found!"));
		check(false);

		if (CompletionCallback)
		{
			(*CompletionCallback)(false);    		
		}
		return -1;
	}

	OPERATION_ID Result = ++LastOperationID;

	const FMutableStreamableBlock& Block = ObjectData->StreamableBlocks[Key];

	int32 BulkDataOffsetInFile = 0;
#if WITH_EDITOR
	BulkDataOffsetInFile = sizeof(MutableCompiledDataStreamHeader);
#endif

	FReadRequest ReadRequest;

	if (CompletionCallback)
	{
		ReadRequest.FileCallback = MakeShared<TFunction<void(bool, IAsyncReadRequest*)>>([CompletionCallbackCapture = *CompletionCallback](bool bWasCancelled, IAsyncReadRequest*) -> void
		{
			CompletionCallbackCapture(!bWasCancelled);			
		});		
	}
	
	check(!ObjectData->ReadFileHandles.IsEmpty());
	ReadRequest.ReadRequest = MakeShareable(ObjectData->ReadFileHandles[Block.FileIndex]->ReadRequest(
		BulkDataOffsetInFile + Block.Offset,
		size,
		(EAsyncIOPriorityAndFlags)StreamPriority,
		ReadRequest.FileCallback.Get(),
		reinterpret_cast<uint8*>(pBuffer)));

	ObjectData->CurrentReadRequests.Add(Result, ReadRequest);

	INC_DWORD_STAT(STAT_MutableStreamingOps);

	return Result;
}


bool FUnrealMutableModelBulkReader::IsReadCompleted(mu::ModelReader::OPERATION_ID OperationId)
{
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableModelBulkStreamer::IsReadCompleted);

	for (FObjectData& o : Objects)
	{
		if (FReadRequest* ReadRequest = o.CurrentReadRequests.Find(OperationId))
		{
			return ReadRequest->ReadRequest->PollCompletion();
		}
	}

	UE_LOG(LogMutable, Error, TEXT("Operation not found in IsReadCompleted."));
	check(false);
	return true;
}


void FUnrealMutableModelBulkReader::EndRead(mu::ModelReader::OPERATION_ID OperationId)
{
	MUTABLE_CPUPROFILER_SCOPE(FUnrealMutableModelBulkStreamer::EndRead);

	bool bFound = false;
	for (FObjectData& o : Objects)
	{
		FReadRequest* ReadRequest = o.CurrentReadRequests.Find(OperationId);
		if (ReadRequest)
		{
			if (ReadRequest->ReadRequest)
			{
				bool bCompleted = ReadRequest->ReadRequest->WaitCompletion();
				if (!bCompleted)
				{
					UE_LOG(LogMutable, Error, TEXT("Operation failed to complete in EndRead."));
					check(false);
				}
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


#if WITH_EDITOR

//-------------------------------------------------------------------------------------------------
FUnrealMutableModelBulkWriter::FUnrealMutableModelBulkWriter(FArchive* InMainDataArchive, FArchive* InStreamedDataArchive)
{
	MainDataArchive = InMainDataArchive;
	StreamedDataArchive = InStreamedDataArchive;
	CurrentWriteFile = nullptr;
}


void FUnrealMutableModelBulkWriter::OpenWriteFile(uint64 key0)
{
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
}


void FUnrealMutableModelBulkWriter::Write(const void* pBuffer, uint64 size)
{
	check(CurrentWriteFile);
	CurrentWriteFile->Serialize(const_cast<void*>(pBuffer), size);
}


void FUnrealMutableModelBulkWriter::CloseWriteFile()
{
	CurrentWriteFile = nullptr;
}

#endif

