// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Build.h"
#include "Templates/Function.h"
#include "Misc/CoreMisc.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"

#define TEXTURE_PROFILER_ENABLED WITH_ENGINE && !UE_BUILD_SHIPPING

#if TEXTURE_PROFILER_ENABLED

class FRHITexture;

/**
* FTextureProfiler class. This manages recording and reporting texture allocations in the RHI
*/
class RHI_API FTextureProfiler
{
	static FTextureProfiler* Instance;
public:
	// Singleton interface
	static FTextureProfiler* Get();

	void Init();

	void DumpTextures(bool RenderTargets, bool CombineTextureNames, bool AsCSV, FOutputDevice& OutputDevice);

	void AddTextureAllocation(FRHITexture* UniqueTexturePtr, size_t Size, uint32 Alignment, size_t AllocationWaste);
	void UpdateTextureAllocation(FRHITexture* UniqueTexturePtr, size_t Size, uint32 Alignment, size_t AllocationWaste);
	void RemoveTextureAllocation(FRHITexture* UniqueTexturePtr);
	void UpdateTextureName(FRHITexture* UniqueTexturePtr);

private:

	FTextureProfiler() = default;
	FTextureProfiler(const FTextureProfiler&) = delete;
	FTextureProfiler(FTextureProfiler&&) = delete;

	void Update();

	// Should only be used by FTextureDetails
	const char* GetTextureNameString(FName TextureName);

	FCriticalSection TextureMapCS;

	class FTexureDetails
	{
	public:
		FTexureDetails() = default;
		FTexureDetails(FRHITexture* Texture, size_t InSize, uint32 InAlign, size_t InAllocationWaste);
		~FTexureDetails();
		
		void SetName(FName InTextureName);

		void ResetPeakSize();
		void SetValues(const FTexureDetails& Values);
		FName GetTextureName() const { return TextureName; }
		const char* GetTextureNameString() const { check(TextureNameString != nullptr);  return TextureNameString; }

		FTexureDetails& operator+=(const FTexureDetails& Other);
		FTexureDetails& operator-=(const FTexureDetails& Other);
		FTexureDetails& operator=(const FTexureDetails& Other) = default;

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
	TMap<void*, FTexureDetails> TexturesMap;

	// Keep track of the totals separately to reduce the cost of rounding error for sizes
	FTexureDetails TotalTextureSize;
	FTexureDetails TotalRenderTargetSize;
	TMap<FName, FTexureDetails> CombinedTextureSizes;
	TMap<FName, FTexureDetails> CombinedRenderTargetSizes;
	friend class FTexureDetails;
};
#endif //TEXTURE_PROFILER_ENABLED
