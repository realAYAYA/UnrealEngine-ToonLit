// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Foundation.h"

////////////////////////////////////////////////////////////////////////////////
class FStoreService
{
public:
	struct FDesc
	{
		FPath				StoreDir;
		int32				StorePort		= 0; // <=0:auto-assign
		int32				RecorderPort	= 0; // 0:auto-assign, -1:off
		int32				ThreadCount		= 0; // <=0:logical CPU count
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

/* vim: set noexpandtab : */
