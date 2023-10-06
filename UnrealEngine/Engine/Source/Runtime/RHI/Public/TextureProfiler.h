// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Build.h"
#include "Templates/Function.h"
#include "Misc/CoreMisc.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"

#define TEXTURE_PROFILER_ENABLED (WITH_ENGINE && !UE_BUILD_SHIPPING)

#if TEXTURE_PROFILER_ENABLED

class FRHITexture;

/**
* FTextureProfiler class. This manages recording and reporting texture allocations in the RHI
*/
class FTextureProfiler
{
	static FTextureProfiler* Instance;
public:
	// Singleton interface
	RHI_API static FTextureProfiler* Get();

	RHI_API void Init();

	RHI_API void DumpTextures(bool RenderTargets, bool CombineTextureNames, bool AsCSV, FOutputDevice& OutputDevice);

	RHI_API void AddTextureAllocation(FRHITexture* UniqueTexturePtr, size_t Size, uint32 Alignment, size_t AllocationWaste);
	RHI_API void UpdateTextureAllocation(FRHITexture* UniqueTexturePtr, size_t Size, uint32 Alignment, size_t AllocationWaste);
	RHI_API void RemoveTextureAllocation(FRHITexture* UniqueTexturePtr);
	RHI_API void UpdateTextureName(FRHITexture* UniqueTexturePtr);

private:

	FTextureProfiler() = default;
	FTextureProfiler(const FTextureProfiler&) = delete;
	FTextureProfiler(FTextureProfiler&&) = delete;

	RHI_API void Update();

	// Should only be used by FTextureDetails
	RHI_API const char* GetTextureNameString(FName TextureName);

	FCriticalSection TextureMapCS;

	class FTextureDetails
	{
	public:
		FTextureDetails() = default;
		RHI_API FTextureDetails(FRHITexture* Texture, size_t InSize, uint32 InAlign, size_t InAllocationWaste);
		RHI_API ~FTextureDetails();
		
		RHI_API void SetName(FName InTextureName);

		RHI_API void ResetPeakSize();
		RHI_API void SetValues(const FTextureDetails& Values);
		FName GetTextureName() const { return TextureName; }
		const char* GetTextureNameString() const { check(TextureNameString != nullptr);  return TextureNameString; }

		RHI_API FTextureDetails& operator+=(const FTextureDetails& Other);
		RHI_API FTextureDetails& operator-=(const FTextureDetails& Other);
		FTextureDetails& operator=(const FTextureDetails& Other) = default;

		size_t Size = 0;
		size_t PeakSize = 0;
		uint32 Align = 0;
		size_t AllocationWaste = 0;
		int Count = 0;
		bool IsRenderTarget = false;

	private:
		FName TextureName;

		// This memory is not owned, do not delete
		const char* TextureNameString = nullptr;
	};

	// Allocated so that a reference to the string can be stored in FTextureDetails
	TMap<FName, const char*> TextureNameStrings;
	TMap<void*, FTextureDetails> TexturesMap;

	// Keep track of the totals separately to reduce the cost of rounding error for sizes
	FTextureDetails TotalTextureSize;
	FTextureDetails TotalRenderTargetSize;
	TMap<FName, FTextureDetails> CombinedTextureSizes;
	TMap<FName, FTextureDetails> CombinedRenderTargetSizes;
	friend class FTextureDetails;
};
#endif //TEXTURE_PROFILER_ENABLED
