// Copyright Epic Games, Inc. All Rights Reserved.

#include "Generation/ChunkDatabaseWriter.h"
#include "HAL/ThreadSafeBool.h"
#include "Containers/Queue.h"
#include "Async/Async.h"
#include "Algo/Transform.h"
#include "Serialization/MemoryWriter.h"
#include "Core/AsyncHelpers.h"
#include "Data/ChunkData.h"
#include "Common/FileSystem.h"
#include "Installer/InstallerError.h"
#include "Installer/ChunkReferenceTracker.h"
#include "Installer/ChunkSource.h"
#include "Interfaces/IBuildInstaller.h"

DECLARE_LOG_CATEGORY_EXTERN(LogChunkDatabaseWriter, Log, All);
DEFINE_LOG_CATEGORY(LogChunkDatabaseWriter);

namespace BuildPatchServices
{
	// Using initial data buffer of 2MB.
	static const int32 DataMessageBufferSize = 1024 * 1024 * 2;

	struct FDataMessage
	{
	public:
		static const int64 CreateFileId = -1;
		static const int64 RenameFileId = -2;

		FDataMessage(FString InFilename, int64 MessageId)
			: Filename(InFilename)
			, DataInfo(MessageId)
		{
			if (Filename.IsEmpty() || MessageId >= 0)
			{
				UE_LOG(LogChunkDatabaseWriter, Error, TEXT("Created action message with no filename or position as ID, WILL RESULT IN ERROR."));
			}
		}

		FDataMessage(int64 InPos)
			: Filename()
			, DataInfo(InPos)
		{
			if (InPos < 0)
			{
				UE_LOG(LogChunkDatabaseWriter, Error, TEXT("Created data message with message ID, WILL RESULT IN ERROR."));
			}
			Memory.Reset(DataMessageBufferSize);
		}

	public:
		FString Filename;
		int64 DataInfo;
		TArray<uint8> Memory;

	private:
		FDataMessage() {}
	};

	class FChunkDatabaseWriter
		: public IChunkDatabaseWriter
	{
	public:
		FChunkDatabaseWriter(IChunkSource* ChunkSource, IFileSystem* FileSystem, IInstallerError* InstallerError, IChunkReferenceTracker* ChunkReferenceTracker, IChunkDataSerialization* ChunkDataSerialization, TArray<FChunkDatabaseFile> ChunkDatabaseList, TFunction<void(bool)> OnComplete);

		~FChunkDatabaseWriter();

	private:
		void ProcessingWorkerThread();
		void OutputWorkerThread();

	private:
		IChunkSource* ChunkSource;
		IFileSystem* FileSystem;
		IInstallerError* InstallerError;
		IChunkReferenceTracker* ChunkReferenceTracker;
		IChunkDataSerialization* ChunkDataSerialization;
		TArray<FChunkDatabaseFile> ChunkDatabaseList;
		TFunction<void(bool)> OnComplete;
		TFuture<void> ProcessingWorkerFuture;
		TFuture<void> OutputWorkerFuture;
		FThreadSafeBool bShouldCancel;
		FThreadSafeBool bProcessingComplete;
		TQueue<TSharedPtr<FDataMessage, ESPMode::ThreadSafe>, EQueueMode::Spsc> DataPipe;
		FEvent* ThreadTrigger;
	};

	FChunkDatabaseWriter::FChunkDatabaseWriter(IChunkSource* InChunkSource, IFileSystem* InFileSystem, IInstallerError* InInstallerError, IChunkReferenceTracker* InChunkReferenceTracker, IChunkDataSerialization* InChunkDataSerialization, TArray<FChunkDatabaseFile> InChunkDatabaseList, TFunction<void(bool)> InOnComplete)
		: ChunkSource(InChunkSource)
		, FileSystem(InFileSystem)
		, InstallerError(InInstallerError)
		, ChunkReferenceTracker(InChunkReferenceTracker)
		, ChunkDataSerialization(InChunkDataSerialization)
		, ChunkDatabaseList(MoveTemp(InChunkDatabaseList))
		, OnComplete(MoveTemp(InOnComplete))
		, bShouldCancel(false)
		, bProcessingComplete(false)
	{
		ThreadTrigger = FPlatformProcess::GetSynchEventFromPool(true);
		ProcessingWorkerFuture = Async(EAsyncExecution::Thread, [this]()
		{
			ProcessingWorkerThread();
		});
		OutputWorkerFuture = Async(EAsyncExecution::Thread, [this]()
		{
			OutputWorkerThread();
		});
	}

	FChunkDatabaseWriter::~FChunkDatabaseWriter()
	{
		bShouldCancel = true;
		ProcessingWorkerFuture.Wait();
		OutputWorkerFuture.Wait();
		FPlatformProcess::ReturnSynchEventToPool(ThreadTrigger);
	}

	void FChunkDatabaseWriter::ProcessingWorkerThread()
	{
		bool bSuccess = true;

		// For every entry in the provided ChunkDatabaseList, create the chunkdb, and send serialized data to the output thread for it.
		for (int32 ChunkDatabaseIdx = 0; ChunkDatabaseIdx < ChunkDatabaseList.Num() && bSuccess && !bShouldCancel && !InstallerError->HasError(); ++ChunkDatabaseIdx)
		{
			const FChunkDatabaseFile& ChunkDatabaseFile = ChunkDatabaseList[ChunkDatabaseIdx];
			UE_LOG(LogChunkDatabaseWriter, Log, TEXT("Start processing chunk database %s"), *ChunkDatabaseFile.DatabaseFilename);

			// Send file create message.
			const FString TmpDatabaseFilename = ChunkDatabaseFile.DatabaseFilename + TEXT("tmp");
			DataPipe.Enqueue(MakeShareable(new FDataMessage(TmpDatabaseFilename, FDataMessage::CreateFileId)));
			ThreadTrigger->Trigger();

			// Populate header with all required entries.
			FChunkDatabaseHeader ChunkDbHeader;
			Algo::Transform(ChunkDatabaseFile.DataList, ChunkDbHeader.Contents, [](const FGuid& DataId)
			{
				return FChunkLocation{DataId, 0, 0};
			});
			int64 FileDataPos = 0;

			// Write the header.
			TUniquePtr<FDataMessage> DataMessage(new FDataMessage(FileDataPos));
			TUniquePtr<FMemoryWriter> MemoryWriter(new FMemoryWriter(DataMessage->Memory));
			*MemoryWriter << ChunkDbHeader;
			FileDataPos += DataMessage->Memory.Num();
			DataPipe.Enqueue(MakeShareable(DataMessage.Release()));
			ThreadTrigger->Trigger();

			// Serialize and write each of the chunk files.
			for (int32 ChunkDataIdx = 0; ChunkDataIdx < ChunkDatabaseFile.DataList.Num() && bSuccess && !bShouldCancel && !InstallerError->HasError(); ChunkDataIdx++)
			{
				const FGuid& ChunkDataId = ChunkDatabaseFile.DataList[ChunkDataIdx];
				IChunkDataAccess* ChunkDataAccess = ChunkSource->Get(ChunkDataId);
				bSuccess = ChunkDataAccess != nullptr;
				if (bSuccess)
				{
					// Prepare new message.
					DataMessage.Reset(new FDataMessage(FileDataPos));
					MemoryWriter.Reset(new FMemoryWriter(DataMessage->Memory));

					// Write to message.
					EChunkSaveResult SaveResult = ChunkDataSerialization->SaveToArchive(*MemoryWriter, ChunkDataAccess);
					bSuccess = SaveResult == EChunkSaveResult::Success;
					if (!bSuccess)
					{
						const TCHAR* ErrorCode = SaveResult == EChunkSaveResult::FileCreateFail ? ConstructionErrorCodes::FileCreateFail
						                       : SaveResult == EChunkSaveResult::SerializationError ? ConstructionErrorCodes::SerializationError
						                       : ConstructionErrorCodes::UnknownFail;
						InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ErrorCode);
					}

					// Set the positional data in the header.
					FChunkLocation& Location = ChunkDbHeader.Contents[ChunkDataIdx];
					Location.ByteStart = FileDataPos;
					Location.ByteSize = DataMessage->Memory.Num();

					// Advance file position.
					FileDataPos += Location.ByteSize;

					// Send the data message.
					DataPipe.Enqueue(MakeShareable(DataMessage.Release()));
					ThreadTrigger->Trigger();

					// Pop the chunk we just saved out.
					bSuccess = ChunkReferenceTracker->PopReference(ChunkDataId);
					if (!bSuccess)
					{
						InstallerError->SetError(EBuildPatchInstallError::InitializationError, InitializationErrorCodes::ChunkReferenceTracking);
					}
				}
				if (!bSuccess)
				{
					UE_LOG(LogChunkDatabaseWriter, Error, TEXT("    Failed chunk %s"), *ChunkDataId.ToString());
					// We set a catch all error here, which only applies if our own loop above, or the chunk source, did not already set its specific error condition.
					InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::UnknownFail);
				}
			}
			if (bSuccess)
			{
				// Write back the header with all chunk positions now filled out accurately.
				ChunkDbHeader.DataSize = FileDataPos - ChunkDbHeader.HeaderSize;
				DataMessage.Reset(new FDataMessage(0));
				MemoryWriter.Reset(new FMemoryWriter(DataMessage->Memory));
				*MemoryWriter << ChunkDbHeader;
				DataPipe.Enqueue(MakeShareable(DataMessage.Release()));
				ThreadTrigger->Trigger();
			}

			// Send file rename message.
			DataPipe.Enqueue(MakeShareable(new FDataMessage(ChunkDatabaseFile.DatabaseFilename, FDataMessage::RenameFileId)));
			ThreadTrigger->Trigger();
		}

		// Mark completed.
		bProcessingComplete = true;
		ThreadTrigger->Trigger();
		UE_LOG(LogChunkDatabaseWriter, Log, TEXT("Processer complete! bSuccess:%d"), bSuccess);
	}

	void FChunkDatabaseWriter::OutputWorkerThread()
	{
		bool bSuccess = true;

		TArray<FString> FilesCreated;
		TSharedPtr<FDataMessage, ESPMode::ThreadSafe> DataMessage;
		TUniquePtr<FArchive> CurrentFile;
		while (bSuccess && !bShouldCancel && !InstallerError->HasError())
		{
			// See if we have a message.
			if (DataPipe.Dequeue(DataMessage))
			{
				// Process a file create message.
				if (DataMessage->DataInfo == FDataMessage::CreateFileId)
				{
					UE_LOG(LogChunkDatabaseWriter, Log, TEXT("Writing chunk database %s"), *DataMessage->Filename);
					CurrentFile = FileSystem->CreateFileWriter(*DataMessage->Filename);
					FilesCreated.Add(DataMessage->Filename);
					if (CurrentFile.IsValid() == false)
					{
						bSuccess = false;
						UE_LOG(LogChunkDatabaseWriter, Error, TEXT("Failed to create file with name %s"), *DataMessage->Filename);
						InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::FileCreateFail);
					}
				}
				// Process a data serialize.
				else if (CurrentFile.IsValid() && DataMessage->DataInfo >= 0)
				{
					if (CurrentFile->Tell() != DataMessage->DataInfo)
					{
						CurrentFile->Seek(DataMessage->DataInfo);
					}
					CurrentFile->Serialize(DataMessage->Memory.GetData(), DataMessage->Memory.Num());
				}
				// Process a file rename message.
				else if (CurrentFile.IsValid() && DataMessage->DataInfo == FDataMessage::RenameFileId)
				{
					const FString OldFilename = CurrentFile->GetArchiveName();
					bSuccess = CurrentFile->Close();
					CurrentFile.Reset();
					if (bSuccess)
					{
						FileSystem->MoveFile(*DataMessage->Filename, *OldFilename);
						UE_LOG(LogChunkDatabaseWriter, Log, TEXT("Chunk database complete, renamed %s"), *DataMessage->Filename);
					}
					else
					{
						UE_LOG(LogChunkDatabaseWriter, Error, TEXT("Serialisation error reported on file close %s"), *OldFilename);
						InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::SerializationError);
					}
				}
				// An error if we do not have a file open and we were sent any message other than create.
				else
				{
					bSuccess = false;
					UE_LOG(LogChunkDatabaseWriter, Error, TEXT("Output fail, message without a file"));
					InstallerError->SetError(EBuildPatchInstallError::FileConstructionFail, ConstructionErrorCodes::MissingFileInfo);
				}
			}
			// Quit if no more messages
			else if (bProcessingComplete)
			{
				break;
			}
			// Wait up to 1 second for an enqueue trigger.
			else
			{
				ThreadTrigger->Wait(1000);
				ThreadTrigger->Reset();
			}
		}

		// Close the last open file.
		CurrentFile.Reset();

		// Check whether the process was canceled or an error occurred.
		bSuccess = bSuccess && !bShouldCancel && !InstallerError->HasError();
		UE_LOG(LogChunkDatabaseWriter, Log, TEXT("Writer complete! bSuccess:%d"), bSuccess);

		// Delete any created files if we failed.
		if (!bSuccess)
		{
			UE_LOG(LogChunkDatabaseWriter, Error, TEXT("Chunkdb generation failed. All created files will be deleted."));
			for (const FString& FileToDelete : FilesCreated)
			{
				FileSystem->DeleteFile(*FileToDelete);
				UE_LOG(LogChunkDatabaseWriter, Log, TEXT("Deleted %s"), *FileToDelete);
			}
		}

		// We're done so call the complete callback.
		AsyncHelpers::ExecuteOnGameThread(OnComplete, bSuccess).Wait();
	}

	IChunkDatabaseWriter* FChunkDatabaseWriterFactory::Create(IChunkSource* ChunkSource, IFileSystem* FileSystem, IInstallerError* InstallerError, IChunkReferenceTracker* ChunkReferenceTracker, IChunkDataSerialization* ChunkDataSerialization, TArray<FChunkDatabaseFile> ChunkDatabaseList, TFunction<void(bool)> OnComplete)
	{
		check(ChunkSource != nullptr);
		check(FileSystem != nullptr);
		check(InstallerError != nullptr);
		check(ChunkReferenceTracker != nullptr);
		return new FChunkDatabaseWriter(ChunkSource, FileSystem, InstallerError, ChunkReferenceTracker, ChunkDataSerialization, MoveTemp(ChunkDatabaseList), MoveTemp(OnComplete));
	}
}
