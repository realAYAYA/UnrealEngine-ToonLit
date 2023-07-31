// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "MuCO/CustomizableObject.h"
#include "MuR/RefCounted.h"
#include "MuR/Serialisation.h"

class FArchive;
class IAsyncReadFileHandle;
class IAsyncReadRequest;
class UCustomizableObject;
namespace mu { class Model; }
struct FMutableStreamableBlock;


// Implementation fo the basic mutable streams.
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
#endif


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
class CUSTOMIZABLEOBJECT_API FUnrealMutableModelBulkStreamer : public mu::ModelStreamer
{
public:
	// 
	FUnrealMutableModelBulkStreamer(FArchive* InMainDataArchive = nullptr, FArchive* InStreamedDataArchive = nullptr);
	~FUnrealMutableModelBulkStreamer();
	
	// Own interface

	/** Make sure that the provided object can stream data. */
	bool PrepareStreamingForObject(UCustomizableObject* Object);

#if WITH_EDITOR
	/** Cancel any further streaming operations for the given object. This is necessary if the object compiled data is
	 * going to be modified. This can only happen in the editor, when recompiling.
	 * Any additional straming requests for this object will fail.
	 */
	void CancelStreamingForObject(const UCustomizableObject* CustomizableObject);
#endif

	/** Release all the pending resources. This disables treamings for all objects. */
	void EndStreaming();
	
	// mu::ModelStreamer interface
	void OpenWriteFile(const char* strModelName, uint64 key0) override;
	void Write(const void* pBuffer, uint64 size) override;
	void CloseWriteFile() override;
	
	OPERATION_ID BeginReadBlock(const mu::Model*, uint64 key0, void* pBuffer, uint64 size) override;
	bool IsReadCompleted(OPERATION_ID) override;
	void EndRead(OPERATION_ID) override;

protected:

#if WITH_EDITOR
	// Write
	// Non-owned pointer to an archive where we'll store the main model data (non-streamable)
	FArchive* MainDataArchive = nullptr;

	// Non-owned pointer to an archive where we'll store the resouces (streamable)
	FArchive* StreamedDataArchive = nullptr;

	FArchive* CurrentWriteFile = nullptr;
#endif

	/** Streaming data for one object. */
	struct FObjectData
	{
		mu::WeakPtr<mu::Model> Model;
		TArray<IAsyncReadFileHandle*> ReadFileHandles;
		TMap<OPERATION_ID, IAsyncReadRequest*> CurrentReadRequests;
		TMap<uint64, FMutableStreamableBlock> StreamableBlocks;
	};

	TArray<FObjectData> Objects;

	/** This is used to generate unique ids for read requests. */
	OPERATION_ID LastOperationID = 0;
};
