// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Map.h"
#include "CoreMinimal.h"
#include "CudaWrapper.h"
#include "Delegates/Delegate.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
DECLARE_MULTICAST_DELEGATE(FOnPostCUDAInit)

DECLARE_LOG_CATEGORY_EXTERN(LogCUDA, Log, All);
class CUDA_API FCUDAModule : public IModuleInterface
{
public:
	static CUDA_DRIVER_API_FUNCTION_LIST CUDA()
	{
		return FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").DriverApiPtrs;
	}

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	// Determines if the CUDA Driver API is available for use
	bool IsAvailable();

	// Retrieves the function pointer list for the CUDA Driver API
	const CUDA_DRIVER_API_FUNCTION_LIST* DriverAPI();

	// Retrieves the CUDA context for the GPU device currently in use by the Vulkan RHI
	CUcontext GetCudaContext();
	
	// Retrieves the device index for the current context
	uint32 GetCudaDeviceIndex() const;
	
	// Retrieves or creates the CUDA context for the specified GPU device
	CUcontext GetCudaContextForDevice(int DeviceIndex);

	// Called after CUDA is loaded successfully
	FOnPostCUDAInit OnPostCUDAInit;

	bool IsDeviceIndexRHISelected(int DeviceIndex);

private:
	bool LoadCuda();
	void UnloadCuda();

	void InitCuda();
	bool IsRHISelectedDevice(CUdevice cuDevice);

	void* DriverLibrary;
	CUDA_DRIVER_API_FUNCTION_LIST DriverApiPtrs;

	uint32 rhiDeviceIndex;
	TMap<uint32, CUcontext> contextMap;
};
