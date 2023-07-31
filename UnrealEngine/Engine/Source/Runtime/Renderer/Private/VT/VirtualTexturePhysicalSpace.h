// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Containers/CircularBuffer.h"
#include "RendererInterface.h"
#include "TexturePagePool.h"
#include "VirtualTextureShared.h"
#include "VirtualTexturing.h"

struct FVTPhysicalSpaceDescription
{
	uint32 TileSize;
	uint8 Dimensions;
	uint8 NumLayers;
	TEnumAsByte<EPixelFormat> Format[VIRTUALTEXTURE_SPACE_MAXLAYERS];
	bool bContinuousUpdate;
};

inline bool operator==(const FVTPhysicalSpaceDescription& Lhs, const FVTPhysicalSpaceDescription& Rhs)
{
	if (Lhs.TileSize != Rhs.TileSize || 
		Lhs.NumLayers != Rhs.NumLayers || 
		Lhs.Dimensions != Rhs.Dimensions || 
		Lhs.bContinuousUpdate != Rhs.bContinuousUpdate)
	{
		return false;
	}
	for (int32 Layer = 0; Layer < Lhs.NumLayers; ++Layer)
	{
		if (Lhs.Format[Layer] != Rhs.Format[Layer])
		{
			return false;
		}
	}
	return true;
}
inline bool operator!=(const FVTPhysicalSpaceDescription& Lhs, const FVTPhysicalSpaceDescription& Rhs)
{
	return !operator==(Lhs, Rhs);
}

inline uint32 GetTypeHash(const FVTPhysicalSpaceDescription& Desc)
{
	uint32 Hash = GetTypeHash(Desc.TileSize);
	Hash = HashCombine(Hash, GetTypeHash(Desc.Dimensions));
	Hash = HashCombine(Hash, GetTypeHash(Desc.NumLayers));
	Hash = HashCombine(Hash, GetTypeHash(Desc.bContinuousUpdate));
	for (int32 Layer = 0; Layer < Desc.NumLayers; ++Layer)
	{
		Hash = HashCombine(Hash, GetTypeHash(Desc.Format[Layer]));
	}
	return Hash;
}

class FVirtualTexturePhysicalSpace final : public FRenderResource
{
public:
	FVirtualTexturePhysicalSpace(const FVTPhysicalSpaceDescription& InDesc, uint16 InID, int32 InTileWidthHeight, bool bInEnableResidencyMipMapBias);
	virtual ~FVirtualTexturePhysicalSpace();

	inline const FVTPhysicalSpaceDescription& GetDescription() const { return Description; }
	inline const FString& GetFormatString() const { return FormatString; }
	inline EPixelFormat GetFormat(int32 Layer) const { return Description.Format[Layer]; }
	inline uint16 GetID() const { return ID; }
	inline uint32 GetNumTiles() const { return TextureSizeInTiles * TextureSizeInTiles; }
	inline uint32 GetSizeInTiles() const { return TextureSizeInTiles; }
	inline uint32 GetTextureSize() const { return TextureSizeInTiles * Description.TileSize; }
	inline FIntVector GetPhysicalLocation(uint16 pAddress) const { return FIntVector(pAddress % TextureSizeInTiles, pAddress / TextureSizeInTiles, 0); }

	// 16bit page tables allocate 6bits to address TileX/Y, so can only address tiles from 0-63
	inline bool DoesSupport16BitPageTable() const { return TextureSizeInTiles <= 64u; }

	uint32 GetSizeInBytes() const;

	inline const FTexturePagePool& GetPagePool() const { return Pool; }
	inline FTexturePagePool& GetPagePool() { return Pool; }

	// FRenderResource interface
	virtual void InitRHI() override;
	virtual void ReleaseRHI() override;

	inline uint32 AddRef() { return ++NumRefs; }
	inline uint32 Release() { check(NumRefs > 0u); return --NumRefs; }
	inline uint32 GetRefCount() const { return NumRefs; }

	FRHITexture* GetPhysicalTexture(int32 Layer) const
	{
		check(PooledRenderTarget[Layer].IsValid());
		return PooledRenderTarget[Layer]->GetRHI();
	}

	FRHIShaderResourceView* GetPhysicalTextureSRV(int32 Layer, bool bSRGB) const
	{
		return bSRGB ? TextureSRV_SRGB[Layer] : TextureSRV[Layer];
	}

	TRefCountPtr<IPooledRenderTarget> GetPhysicalTexturePooledRenderTarget(int32 Layer) const
	{
		check(PooledRenderTarget[Layer].IsValid());
		return PooledRenderTarget[Layer];
	}

	/** Update internal tracking of residency. This is used to update stats and to calculate a mip bias to keep within the pool budget. */
	void UpdateResidencyTracking(uint32 Frame);
	/** Get dynamic mip bias used to keep within residency budget. */
	inline float GetResidencyMipMapBias() const { return ResidencyMipMapBias; }
	/** Get frame at which residency tracking last saw over-subscription. */
	inline uint32 GetLastFrameOversubscribed() const { return LastFrameOversubscribed; }
	/** Draw residency graph on screen. */
	void DrawResidencyGraph(class FCanvas* Canvas, FBox2D CanvasPosition, bool bDrawKey);
	/** Write residency stats to CSV profiler */
	void UpdateCsvStats() const;

private:
	FVTPhysicalSpaceDescription Description;
	FString FormatString;
	FTexturePagePool Pool;
	TRefCountPtr<IPooledRenderTarget> PooledRenderTarget[VIRTUALTEXTURE_SPACE_MAXLAYERS];
	FShaderResourceViewRHIRef TextureSRV[VIRTUALTEXTURE_SPACE_MAXLAYERS];
	FShaderResourceViewRHIRef TextureSRV_SRGB[VIRTUALTEXTURE_SPACE_MAXLAYERS];

	uint32 TextureSizeInTiles;
	uint32 NumRefs;
	uint16 ID;
	
	bool bEnableResidencyMipMapBias;
	float ResidencyMipMapBias;
	uint32 LastFrameOversubscribed;

#if !UE_BUILD_SHIPPING
	static const int32 HistorySize = 512;
	TCircularBuffer<float> VisibleHistory;
	TCircularBuffer<float> LockedHistory;
	TCircularBuffer<float> MipMapBiasHistory;
	uint32 HistoryIndex;
#endif
};
