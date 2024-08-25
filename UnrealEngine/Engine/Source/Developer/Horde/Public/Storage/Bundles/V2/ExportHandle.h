// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Storage/BlobHandle.h"
#include "PacketHandle.h"

class FExportHandleData;

/** Handle to an export within a packet. */
typedef TBlobHandle<FExportHandleData> FExportHandle;

/** 
 * Implementation of export handle data.
 */
class HORDE_API FExportHandleData final : public FBlobHandleData, public TSharedFromThis<FExportHandleData, ESPMode::ThreadSafe>
{
public:
	static const char Type[];

	FExportHandleData(FPacketHandle InPacket, int32 InExportIdx);
	FExportHandleData(FPacketHandle InPacket, const FUtf8StringView& InFragment);
	~FExportHandleData();

	static void AppendIdentifier(FUtf8String& OutBuffer, int32 ExportIdx);

	// Implementation of FBlobHandle
	virtual bool Equals(const FBlobHandleData& Other) const override;
	virtual uint32 GetHashCode() const override;
	virtual FBlobHandle GetOuter() const override;
	virtual const char* GetType() const override;
	virtual FBlob Read() const override;
	virtual bool TryAppendIdentifier(FUtf8String& OutBuffer) const override;

private:
	static const FUtf8StringView FragmentPrefix;

	FPacketHandle Packet;
	int32 ExportIdx;

	static bool TryParse(const FUtf8StringView& Fragment, int32& OutExportIdx);
};
