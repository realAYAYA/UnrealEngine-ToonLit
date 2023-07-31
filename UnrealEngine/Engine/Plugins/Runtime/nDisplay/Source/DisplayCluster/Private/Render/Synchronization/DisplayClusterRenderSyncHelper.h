// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"


/**
 * RHI agnostic helper interface
 */
class IDisplayClusterRenderSyncHelper
{
public:
	virtual ~IDisplayClusterRenderSyncHelper() = default;

public:
	virtual bool WaitForVBlank()
	{
		return false;
	}

	virtual bool IsWaitForVBlankSupported()
	{
		return false;
	}
};


/**
 * Syncrhonization helper implementation. The main purpose of this class
 * is to solve the combinatoric complexity Platform/RHI/SyncPolicy without
 * making the whole sync policies hierarchy overcomplicated.
 */
class FDisplayClusterRenderSyncHelper
{
private:
	FDisplayClusterRenderSyncHelper();

public:
	// Singleton access
	static IDisplayClusterRenderSyncHelper& Get();

protected:
	// Factory method to instantiate helper instance of proper type
	static TUniquePtr<IDisplayClusterRenderSyncHelper> CreateHelper();

#if PLATFORM_WINDOWS
private:
	// DX helper implementation
	struct FDCRSHelperDX : public IDisplayClusterRenderSyncHelper
	{
		virtual bool WaitForVBlank() override;
		virtual bool IsWaitForVBlankSupported() override;
	};
#endif

private:
	// Vulkan helper implementation
	struct FDCRSHelperVulkan : public IDisplayClusterRenderSyncHelper
	{
		virtual bool WaitForVBlank() override;
		virtual bool IsWaitForVBlankSupported() override;
	};

private:
	// Stub implementation for unknown RHI
	struct FDCRSHelperNull : public IDisplayClusterRenderSyncHelper
	{
	};

private:
	// Helper instance
	static TUniquePtr<IDisplayClusterRenderSyncHelper> Instance;

	// Singleton access mutex
	static FCriticalSection CritSecInternals;
};
