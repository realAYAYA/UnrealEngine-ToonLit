// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Async/Future.h"
#include "Containers/Array.h"
#include "Containers/StringView.h"
#include "Experimental/ZenServerInterface.h"
#include "HAL/CriticalSection.h"
#include "HAL/Platform.h"
#include "IO/IoDispatcher.h"
#include "Misc/StringBuilder.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Templates/UniquePtr.h"

class FCbPackage;
class FCbObject;

namespace UE {
	namespace Zen {
		struct FZenHttpRequestPool;
	}

/**
 * HTTP protocol implementation of Zen Store client interface
 */
class IOSTOREUTILITIES_API FZenStoreHttpClient
{
public:
	FZenStoreHttpClient();
	FZenStoreHttpClient(FStringView HostName, uint16 Port);
	FZenStoreHttpClient(UE::Zen::FServiceSettings&& InSettings);
	~FZenStoreHttpClient();

	bool TryCreateProject(FStringView InProjectId, FStringView InOplogId, FStringView ServerRoot, 
					FStringView EngineRoot, FStringView ProjectRoot,
					FStringView ProjectFilePath);
	bool TryCreateOplog(FStringView InProjectId, FStringView InOplogId, FStringView InOplogLifetimeMarkerPath, bool bFullBuild);

	void InitializeReadOnly(FStringView InProjectId, FStringView InOplogId);

	bool IsConnected() const;

	void StartBuildPass();
	TIoStatusOr<uint64> EndBuildPass(FCbPackage OpEntry);

	TIoStatusOr<uint64> AppendOp(FCbPackage OpEntry);

	TIoStatusOr<uint64> GetChunkSize(const FIoChunkId& Id);
	TIoStatusOr<FIoBuffer> ReadChunk(const FIoChunkId& Id, uint64 Offset = 0, uint64 Size = ~0ull);
	TIoStatusOr<FIoBuffer> ReadOpLogAttachment(FStringView Id);

#if UE_WITH_ZEN
	const TCHAR* GetHostName() const { return ZenService.GetInstance().GetHostName(); }
	uint16 GetPort() const { return ZenService.GetInstance().GetPort(); }
	const UE::Zen::FZenServiceInstance& GetZenServiceInstance() const { return ZenService.GetInstance(); }
#else // Default to localhost:8558 for platforms where Zen wouldn't be supported yet
	const TCHAR* GetHostName() const { return TEXT("localhost"); }
	uint16 GetPort() const { return 8558; }
#endif

	TFuture<TIoStatusOr<FCbObject>> GetOplog();
	TFuture<TIoStatusOr<FCbObject>> GetFiles();
	TFuture<TIoStatusOr<FCbObject>> GetChunkInfos();

	static const UTF8CHAR* FindOrAddAttachmentId(FUtf8StringView AttachmentText);
	static const UTF8CHAR* FindAttachmentId(FUtf8StringView AttachmentText);

private:
	TIoStatusOr<FIoBuffer> ReadOpLogUri(FStringBuilderBase& ChunkUri, uint64 Offset = 0, uint64 Size = ~0ull);

	static const uint32 PoolEntryCount;
	struct SaltGenerator
	{
		SaltGenerator();
		inline int32_t Next()
		{
			const uint32_t A = ++GOpCounter;
			return static_cast<int32_t>((A ^ (SaltBase + (A << 6) + (A >> 2))) & 0x7fffffffu);
		}
	private:
		static std::atomic<uint32> GOpCounter;
		const uint32_t SaltBase;
	};
#if UE_WITH_ZEN
	UE::Zen::FScopeZenService ZenService;
#endif
	TUniquePtr<Zen::FZenHttpRequestPool> RequestPool;
	SaltGenerator SaltGen;
	FString OplogPath;
	FString OplogNewEntryPath;
	FString OplogPrepNewEntryPath;
	FString TempDirPath;
	const uint64 StandaloneThresholdBytes = 1 * 1024 * 1024;
	bool bAllowRead = false;
	bool bAllowEdit = false;
	bool bConnectionSucceeded = false;
};

}
