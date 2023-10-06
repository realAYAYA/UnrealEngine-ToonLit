// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

////////////////////////////////////////////////////////////////////////////////
/* Indicates if the external store is supported or if an instance should be
   self-hosted by something */
#define WITH_TRACE_STORE 0

namespace UE {
namespace Trace {

////////////////////////////////////////////////////////////////////////////////
class TRACEANALYSIS_API FStoreService
{
public:
	struct FDesc
	{
		const TCHAR*		StoreDir;
		int32				RecorderPort = 0; // 0:auto-assign, -1:off
		int32				ThreadCount  = 0; // <=0:logical CPU count
	};

							~FStoreService() = default;
	static FStoreService*	Create(const FDesc& Desc);
	void					operator delete (void* Addr);
	uint32					GetPort() const;
	uint32					GetRecorderPort() const;

private:
							FStoreService() = default;
							FStoreService(const FStoreService&) = delete;
							FStoreService(const FStoreService&&) = delete;
	void					operator = (const FStoreService&) = delete;
	void					operator = (const FStoreService&&) = delete;
};

} // namespace Trace
} // namespace UE
