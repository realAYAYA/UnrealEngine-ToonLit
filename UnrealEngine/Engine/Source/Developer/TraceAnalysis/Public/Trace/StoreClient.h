// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringFwd.h"
#include "Containers/StringView.h"
#include "HAL/Platform.h"
#include "Templates/UniquePtr.h"
#include "Trace/DataStream.h"

namespace UE {
namespace Trace {

class IInDataStream;

////////////////////////////////////////////////////////////////////////////////
class TRACEANALYSIS_API FStoreClient
{
public:
	struct TRACEANALYSIS_API FStatus
	{
		FUtf8StringView	GetStoreDir() const;
		uint32			GetRecorderPort() const;
		uint32			GetStorePort() const;
		bool			GetSponsored() const;
		uint32			GetChangeSerial() const;
		uint32			GetSettingsSerial() const;
		void			GetWatchDirectories(TArray<FString>& OutDirs) const;
	};

	struct TRACEANALYSIS_API FVersion
	{
		uint32			GetMajorVersion() const;
		uint32			GetMinorVersion() const;
		FUtf8StringView GetConfiguration() const;
	};

	struct TRACEANALYSIS_API FTraceInfo
	{
		FUtf8StringView	GetName() const;
		uint32			GetId() const;
		uint64			GetSize() const;
		uint64			GetTimestamp() const;
		FUtf8StringView GetUri() const;
	};

	struct FTraceData
		: public TUniquePtr<IInDataStream>
	{
		using TUniquePtr<IInDataStream>::TUniquePtr;
	};

						~FStoreClient() = default;
	static FStoreClient* Connect(const TCHAR* Host, uint32 Port = 0);
	bool				Reconnect(const TCHAR* Host, uint32 Port);
	void				operator delete (void* Addr);
	bool				IsValid() const;
	uint32				GetStoreAddress() const;
	uint32				GetStorePort() const;
	const FStatus*		GetStatus() const;
	const FVersion*		GetVersion() const;
	uint32				GetTraceCount() const;
	const FTraceInfo*	GetTraceInfo(uint32 Index) const;
	const FTraceInfo*	GetTraceInfoById(uint32 Id) const;
	FTraceData			ReadTrace(uint32 Id) const;
	bool				SetStoreDirectories(const TCHAR* StoreDir, const TArray<FString>& AddWatchDirs, const TArray<FString>& RemoveWatchDirs);
	bool				SetSponsored(bool bSponsored);

	struct TRACEANALYSIS_API FSessionInfo
	{
		uint32			GetId() const;
		uint32			GetTraceId() const;
		uint32			GetIpAddress() const;
		uint32			GetControlPort() const;
	};
	uint32				GetSessionCount() const;
	const FSessionInfo* GetSessionInfo(uint32 Index) const;
	const FSessionInfo* GetSessionInfoById(uint32 Id) const;
	const FSessionInfo* GetSessionInfoByTraceId(uint32 TraceId) const;
	const FSessionInfo* GetSessionInfoByGuid(const FGuid& TraceGuid) const;

private:
						FStoreClient() = default;
						FStoreClient(const FStoreClient&) = delete;
						FStoreClient(const FStoreClient&&) = delete;
	void				operator = (const FStoreClient&) = delete;
	void				operator = (const FStoreClient&&) = delete;
};

} // namespace Trace
} // namespace UE
