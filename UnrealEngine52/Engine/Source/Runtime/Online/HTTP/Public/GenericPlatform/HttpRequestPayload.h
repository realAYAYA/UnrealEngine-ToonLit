// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "CoreMinimal.h"
#include "HAL/PlatformMath.h"
#include "Templates/SharedPointer.h"

class FArchive;

/**
* Abstraction that encapsulates the location of a request payload
*/
class FRequestPayload
{
public:
	virtual ~FRequestPayload() {}
	/** Get the total content length of the request payload in bytes */
	virtual int32 GetContentLength() const = 0;
	/** Return a reference to the underlying memory buffer. Only valid for in-memory request payloads */
	virtual const TArray<uint8>& GetContent() const = 0;
	/** Check if the request payload is URL encoded. This check is only performed for in-memory request payloads */
	virtual bool IsURLEncoded() const = 0;
	/**
	 * Read part of the underlying request payload into an output buffer.
	 * @param OutputBuffer - the destination memory address where the payload should be copied
	 * @param MaxOutputBufferSize - capacity of OutputBuffer in bytes
	 * @param SizeAlreadySent - how much of payload has previously been sent.
	 * @return Returns the number of bytes copied into OutputBuffer
	 */
	virtual size_t FillOutputBuffer(void* OutputBuffer, size_t MaxOutputBufferSize, size_t SizeAlreadySent) = 0;

	/**
	 * Read part of the underlying request payload into an output buffer.
	 * @param OutputBuffer - the destination memory address where the payload should be copied
	 * @param MaxOutputBufferSize - capacity of OutputBuffer in bytes
	 * @param SizeAlreadySent - how much of payload has previously been sent.
	 * @return Returns the number of bytes copied into OutputBuffer
	 */
	virtual size_t FillOutputBuffer(TArrayView<uint8> OutputBuffer, size_t SizeAlreadySent) = 0;
};

class FRequestPayloadInFileStream : public FRequestPayload
{
public:
	FRequestPayloadInFileStream(TSharedRef<FArchive, ESPMode::ThreadSafe> InFile);
	virtual ~FRequestPayloadInFileStream();
	virtual int32 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;
	virtual bool IsURLEncoded() const override;
	virtual size_t FillOutputBuffer(void* OutputBuffer, size_t MaxOutputBufferSize, size_t SizeAlreadySent) override;
	virtual size_t FillOutputBuffer(TArrayView<uint8> OutputBuffer, size_t SizeAlreadySent) override;
private:
	TSharedRef<FArchive, ESPMode::ThreadSafe> File;
};

class FRequestPayloadInMemory : public FRequestPayload
{
public:
	FRequestPayloadInMemory(const TArray<uint8>& Array);
	FRequestPayloadInMemory(TArray<uint8>&& Array);
	virtual ~FRequestPayloadInMemory();
	virtual int32 GetContentLength() const override;
	virtual const TArray<uint8>& GetContent() const override;
	virtual bool IsURLEncoded() const override;
	virtual size_t FillOutputBuffer(void* OutputBuffer, size_t MaxOutputBufferSize, size_t SizeAlreadySent) override;
	virtual size_t FillOutputBuffer(TArrayView<uint8> OutputBuffer, size_t SizeAlreadySent) override;
private:
	TArray<uint8> Buffer;
};
