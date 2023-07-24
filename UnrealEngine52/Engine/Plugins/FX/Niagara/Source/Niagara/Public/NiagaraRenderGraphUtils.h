// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "RenderGraphResources.h"

//////////////////////////////////////////////////////////////////////////
// Helper for wrapping a persistent RW buffer
struct NIAGARA_API FNiagaraPooledRWBuffer
{
	void Initialize(FRDGBuilder& GraphBuilder, const TCHAR* ResourceName, EPixelFormat PixelFormat, const FRDGBufferDesc& BufferDesc);
	void Initialize(FRDGBuilder& GraphBuilder, const TCHAR* ResourceName, EPixelFormat PixelFormat, const uint32 BytesPerElemenet, const uint32 NumElements, EBufferUsageFlags UsageFlags = EBufferUsageFlags::None);
	void Release();

	bool IsValid() const { return Buffer.IsValid(); }


	const TRefCountPtr<FRDGPooledBuffer>& GetPooledBuffer() const { return Buffer; }
	FRDGBufferRef GetOrCreateBuffer(FRDGBuilder& GraphBuilder);
	FRDGBufferSRVRef GetOrCreateSRV(FRDGBuilder& GraphBuilder);
	FRDGBufferUAVRef GetOrCreateUAV(FRDGBuilder& GraphBuilder);

	void EndGraphUsage();

private:
	TRefCountPtr<FRDGPooledBuffer>	Buffer;
	EPixelFormat					PixelFormat = PF_Unknown;

	// Transient for the lifetime of the GraphBuilder, call PostSimulate to clear
	FRDGBufferRef					TransientRDGBuffer = nullptr;
	FRDGBufferSRVRef				TransientRDGSRV = nullptr;
	FRDGBufferUAVRef				TransientRDGUAV = nullptr;
#if STATS
	uint64							NumBytes = 0;
#endif
};

//////////////////////////////////////////////////////////////////////////
// Helper for wrapping a persistent RW texture
struct NIAGARA_API FNiagaraPooledRWTexture
{
	void Initialize(FRDGBuilder& GraphBuilder, const TCHAR* ResourceName, const FRDGTextureDesc& TextureDesc);
	void Release();

	bool IsValid() const { return Texture.IsValid(); }

	const TRefCountPtr<IPooledRenderTarget>& GetPooledTexture() const { return Texture; }
	FRDGTextureRef GetOrCreateTexture(FRDGBuilder& GraphBuilder);
	FRDGTextureSRVRef GetOrCreateSRV(FRDGBuilder& GraphBuilder);
	FRDGTextureUAVRef GetOrCreateUAV(FRDGBuilder& GraphBuilder);

	void CopyToTexture(FRDGBuilder& GraphBuilder, FRHITexture* DestinationTexture, const TCHAR* NameIfNotRegistered);

	void EndGraphUsage();

private:
	TRefCountPtr<IPooledRenderTarget>	Texture;

	// Transient for the lifetime of the GraphBuilder, call PostSimulate to clear
	FRDGTextureRef						TransientRDGTexture = nullptr;
	FRDGTextureSRVRef					TransientRDGSRV = nullptr;
	FRDGTextureUAVRef					TransientRDGUAV = nullptr;
#if STATS
	uint64								NumBytes = 0;
#endif
};
