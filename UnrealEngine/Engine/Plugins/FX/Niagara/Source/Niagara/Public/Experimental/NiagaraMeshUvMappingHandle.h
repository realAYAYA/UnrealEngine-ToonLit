// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FMeshUvMappingBufferProxy;
struct FMeshUvMapping;
struct FSkeletalMeshUvMapping;
struct FStaticMeshUvMapping;

struct FMeshUvMappingUsage
{
	FMeshUvMappingUsage() = default;
	FMeshUvMappingUsage(bool InRequiresCpuAccess, bool InrequiresGpuAccess)
		: RequiresCpuAccess(InRequiresCpuAccess)
		, RequiresGpuAccess(InrequiresGpuAccess)
	{}

	bool IsValid() const { return RequiresCpuAccess || RequiresGpuAccess; }

	bool RequiresCpuAccess = false;
	bool RequiresGpuAccess = false;
};

struct NIAGARA_API FMeshUvMappingHandleBase
{
	FMeshUvMappingHandleBase() = default;
	FMeshUvMappingHandleBase(const FMeshUvMappingHandleBase& Other) = delete;
	~FMeshUvMappingHandleBase();
	FMeshUvMappingHandleBase& operator=(const FMeshUvMappingHandleBase& Other) = delete;
	explicit operator bool() const;

	const FMeshUvMappingBufferProxy* GetQuadTreeProxy() const;
	int32 GetUvSetIndex() const;
	int32 GetLodIndex() const;
	void PinAndInvalidateHandle();

	FMeshUvMappingUsage Usage;

protected:
	FMeshUvMappingHandleBase(FMeshUvMappingUsage InUsage, const TSharedPtr<FMeshUvMapping>& InUvMappingData, bool bNeedsDataImmediately);
	FMeshUvMappingHandleBase(FMeshUvMappingHandleBase&& Other) noexcept;
	FMeshUvMappingHandleBase& operator=(FMeshUvMappingHandleBase&& Other) noexcept;

	TSharedPtr<FMeshUvMapping> UvMappingData;
};


template<typename MappingType>
struct NIAGARA_API TTypedMeshUvMappingHandle : public FMeshUvMappingHandleBase
{
	TTypedMeshUvMappingHandle() = default;
	TTypedMeshUvMappingHandle(FMeshUvMappingUsage InUsage, const TSharedPtr<MappingType>& InUvMappingData, bool bNeedsDataImmediately)
		: FMeshUvMappingHandleBase(InUsage, InUvMappingData, bNeedsDataImmediately) {}

	TTypedMeshUvMappingHandle(TTypedMeshUvMappingHandle<MappingType>&& Other) noexcept
		: FMeshUvMappingHandleBase(Other) {}

	TTypedMeshUvMappingHandle<MappingType>& operator=(TTypedMeshUvMappingHandle<MappingType>&& Other) noexcept
	{
		FMeshUvMappingHandleBase::operator=(MoveTemp(Other));
		return *this;
	}

	const MappingType* GetMappingData() const
	{
		if (UvMappingData)
		{
			return static_cast<const MappingType*>(UvMappingData.Get());
		}

		return nullptr;
	}
};

using FSkeletalMeshUvMappingHandle = TTypedMeshUvMappingHandle<FSkeletalMeshUvMapping>;
using FStaticMeshUvMappingHandle = TTypedMeshUvMappingHandle<FStaticMeshUvMapping>;