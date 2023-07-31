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
		uint32			GetChangeSerial() const;
	};

	struct TRACEANALYSIS_API FTraceInfo
	{
		FUtf8StringView	GetName() const;
		uint32			GetId() const;
		uint64			GetSize() const;
		uint64			GetTimestamp() const;
		//const TCHAR*	GetMetadata(const TCHAR* Key) const;
		//template <typename Lambda> uint32 ReadMetadata(Lambda&& Callback) const;
	};

	struct FTraceData
		: public TUniquePtr<IInDataStream>
	{
		using TUniquePtr<IInDataStream>::TUniquePtr;
	};
	
						~FStoreClient() = default;
	static FStoreClient*Connect(const TCHAR* Host, uint32 Port=0);
	void				operator delete (void* Addr);
	bool				IsValid() const;
	uint32				GetStoreAddress() const;
	uint32				GetStorePort() const;
	const FStatus*		GetStatus();
	uint32				GetTraceCount();
	const FTraceInfo*	GetTraceInfo(uint32 Index);
	const FTraceInfo*	GetTraceInfoById(uint32 Id);
	FTraceData			ReadTrace(uint32 Id);
#if 0
	template <typename Lambda> uint32 GetTraceInfos(uint32 StartIndex, uint32 Count, Lambda&& Callback) const;
#endif // 0

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
#if 0
	template <typename Lambda> uint32	GetSessionInfos(uint32 StartIndex, uint32 Count, Lambda&& Callback) const;
#endif // 0

#if 0
	// -------
	class IStoreSubscriber
	{
		virtual void	OnStoreEvent() = 0;
	};
	bool				Subscribe(IStoreSubscriber* Subscriber);
	bool				Unsubscribe(IStoreSubscriber* Subscriber);
#endif // 0

private:
						FStoreClient() = default;
						FStoreClient(const FStoreClient&) = delete;
						FStoreClient(const FStoreClient&&) = delete;
	void				operator = (const FStoreClient&) = delete;
	void				operator = (const FStoreClient&&) = delete;
};

} // namespace Trace
} // namespace UE
