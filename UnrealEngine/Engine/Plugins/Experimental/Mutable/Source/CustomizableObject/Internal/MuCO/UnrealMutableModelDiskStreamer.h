// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuCO/CustomizableObject.h"
#include "MuR/Serialisation.h"

class FArchive;
class IAsyncReadFileHandle;
class IAsyncReadRequest;
class UCustomizableObject;
namespace mu { class Model; }
struct FMutableStreamableBlock;

class UnrealMutableInputStream : public mu::InputStream
{
public:

	UnrealMutableInputStream(FArchive& ar);

	// mu::InputStream interface
	void Read(void* pData, uint64 size) override;

private:
	FArchive& m_ar;
};


// Implementation of a mutable streamer using bulk storage.
class CUSTOMIZABLEOBJECT_API FUnrealMutableModelBulkReader : public mu::ModelReader
{
public:
	// 
	~FUnrealMutableModelBulkReader();

	// Own interface

	/** Make sure that the provided object can stream data. */
	bool PrepareStreamingForObject(UCustomizableObject* Object);

#if WITH_EDITOR
	/** Cancel any further streaming operations for the given object. This is necessary if the object compiled data is
	 * going to be modified. This can only happen in the editor, when recompiling.
	 * Any additional streaming requests for this object will fail.
	 */
	void CancelStreamingForObject(const UCustomizableObject* CustomizableObject);

	/** Checks if there are any streaming operations for the parameter object.
	* @return true if there are streaming operations in flight
	*/
	bool AreTherePendingStreamingOperationsForObject(const UCustomizableObject* CustomizableObject) const;
#endif

	/** Release all the pending resources. This disables treamings for all objects. */
	void EndStreaming();

	// mu::ModelReader interface
	OPERATION_ID BeginReadBlock(const mu::Model*, uint64 key0, void* pBuffer, uint64 size, TFunction<void(bool bSuccess)>* CompletionCallback) override;
	bool IsReadCompleted(OPERATION_ID) override;
	void EndRead(OPERATION_ID) override;

protected:

	struct FReadRequest
	{
		TSharedPtr<IAsyncReadRequest> ReadRequest = nullptr;
		TSharedPtr<FAsyncFileCallBack> FileCallback;
	};
	
	/** Streaming data for one object. */
	struct FObjectData
	{
		TWeakPtr<const mu::Model> Model;
		TMap<OPERATION_ID, FReadRequest> CurrentReadRequests;
		TMap<uint64, FMutableStreamableBlock> StreamableBlocks;
		FString BulkFilePrefix;

		TMap<uint32, TSharedPtr<IAsyncReadFileHandle>> ReadFileHandles;
	};

	TArray<FObjectData> Objects;

	FCriticalSection FileHandlesCritical;

	/** This is used to generate unique ids for read requests. */
	OPERATION_ID LastOperationID = 0;
};


#if WITH_EDITOR

class UnrealMutableOutputStream : public mu::OutputStream
{
public:

	UnrealMutableOutputStream(FArchive& ar);

	// mu::OutputStream interface
	void Write(const void* pData, uint64 size) override;

private:
	FArchive& m_ar;
};


// Implementation of a mutable streamer using bulk storage.
class CUSTOMIZABLEOBJECT_API FUnrealMutableModelBulkWriter : public mu::ModelWriter
{
public:
	// 
	FUnrealMutableModelBulkWriter(FArchive* InMainDataArchive = nullptr, FArchive* InStreamedDataArchive = nullptr);

	// mu::ModelWriter interface
	void OpenWriteFile(uint64 key0) override;
	void Write(const void* pBuffer, uint64 size) override;
	void CloseWriteFile() override;

protected:

	// Non-owned pointer to an archive where we'll store the main model data (non-streamable)
	FArchive* MainDataArchive = nullptr;

	// Non-owned pointer to an archive where we'll store the resouces (streamable)
	FArchive* StreamedDataArchive = nullptr;

	FArchive* CurrentWriteFile = nullptr;

};

#endif
