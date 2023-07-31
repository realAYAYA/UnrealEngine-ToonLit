// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CborTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"

class FArchive;

/**
* Writer for encoding a stream with the cbor protocol
* @see http://cbor.io
*/
class CBOR_API FCborWriter
{
public:
	/*
	 * Construct a CBOR writer.
	 * @param InStream The stream used to write the CBOR data.
	 * @param InWriterEndianness Specify which endianness should be use to write the archive.
	 * @note CBOR standard endianness is big endian. For interoperability with external tools, the standard endianness should be used. For internal usage, the platform endianness is faster.
	 */
	FCborWriter(FArchive* InStream, ECborEndianness InWriterEndianness = ECborEndianness::Platform);
	~FCborWriter();

public:
	/** @return the archive we are writing to. */
	const FArchive* GetArchive() const;

	/**
	 * Write a container start code.
	 * @param ContainerType container major type, either array or map.
	 * @param NbItem the number of item in the container or negative to indicate indefinite containers.
	 */
	void WriteContainerStart(ECborCode ContainerType, int64 NbItem);

	/** Write a container break code, need a indefinite container context. */
	void WriteContainerEnd();

	/** Write a value. */

	void WriteNull();
	void WriteValue(uint64 Value);
	void WriteValue(int64 Value);
	void WriteValue(bool Value);
	void WriteValue(float Value);
	void WriteValue(double Value);
	void WriteValue(const FString& Value);
	void WriteValue(const char* CString, uint64 Length);
	void WriteValue(const uint8* Bytes, uint64 Length);

private:
	/** Write a uint Value for Header in Ar and return the final generated cbor Header. */
	static FCborHeader WriteUIntValue(FCborHeader Header, FArchive& Ar, uint64 Value);

	/** Validate the current writer context for MajorType. */
	void CheckContext(ECborCode MajorType);

	/** The archive being written to. */
	FArchive* Stream;
	/** The writer context stack. */
	TArray<FCborContext> ContextStack;
	/** Write the CBOR data using the specified endianness. */
	ECborEndianness Endianness;
};

