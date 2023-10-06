// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GPUTextureTransfer.h"
#include "Logging/LogMacros.h"
#include "Modules/ModuleInterface.h"

#include <atomic>

DECLARE_LOG_CATEGORY_EXTERN(LogGPUTextureTransfer, Log, All);

namespace UE::GPUTextureTransfer
{
	using TextureTransferPtr = TSharedPtr<ITextureTransfer>;
}

class GPUTEXTURETRANSFER_API FGPUTextureTransferModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	UE::GPUTextureTransfer::TextureTransferPtr GetTextureTransfer();

	static bool IsAvailable();
	static FGPUTextureTransferModule& Get();

private:
	bool LoadGPUDirectBinary();
	void InitializeTextureTransfer();
	void UninitializeTextureTransfer();

private:
	static constexpr uint8 RHI_MAX = static_cast<uint8>(UE::GPUTextureTransfer::ERHI::RHI_MAX);
	void* TextureTransferHandle = nullptr;
	std::atomic<bool> bIsGPUTextureTransferAvailable = false;
	
	TArray<UE::GPUTextureTransfer::TextureTransferPtr> TransferObjects;
};
