// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VirtualTexturing.h"
#include "VirtualTextureSpace.h"
#include "RenderTargetPool.h"

#if 0

#define CPU_TEST 1

class FTexturePagePool;

class FVirtualTextureTest : public IVirtualTexture
{
public:
					FVirtualTextureTest( uint32 SizeX, uint32 SizeY, uint32 SizeZ );
	virtual			~FVirtualTextureTest();

	virtual bool	RequestPageData( FRHICommandList& RHICmdList, uint8 vLevel, uint64 vAddress, void* RESTRICT& Location ) /*const*/ override;
	virtual void	ProducePageData( FRHICommandList& RHICmdList, ERHIFeatureLevel::Type FeatureLevel, uint8 vLevel, uint64 vAddress, uint16 pAddress, void* RESTRICT Location ) /*const*/ override;
	virtual void    DumpToConsole(bool verbose) override;
};


class FVirtualTextureTestType : public FRenderResource
{
public:
					FVirtualTextureTestType();
					~FVirtualTextureTestType();

	// FRenderResource interface
	virtual void	InitRHI(FRHICommandListBase& RHICmdList) override;
	virtual void	ReleaseRHI() override;
	
	FRHITexture*	GetPhysicalTexture() const		{ return PhysicalTexture->GetRHI(); }
	FRHITexture*	GetPageTableTexture() const;
	
	FTexturePagePool*		Pool;
#if CPU_TEST == 0	
	FVirtualTextureSpace*	Space;
	IVirtualTexture*		VT;
#endif
	EPixelFormat						PhysicalTextureFormat;
	FIntPoint							PhysicalTextureSize;
	TRefCountPtr< IPooledRenderTarget >	PhysicalTexture;

	const uint32 vPageSize = 128;
	const uint32 pPageBorder = 4;
	const uint32 pPageSize = vPageSize + 2 * pPageBorder;
};

extern TGlobalResource< FVirtualTextureTestType > GVirtualTextureTestType;
#endif