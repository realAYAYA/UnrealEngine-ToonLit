// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Data/Blob.h"
#include "Device/Device.h"
#include "DeviceBuffer_FX.h"
#include "RenderTargetPool.h"
#include "Engine/TextureRenderTarget2D.h"
#include <list>
#include <unordered_map>

typedef cti::continuable<TexPtr>	AsyncTexPtr;
class RenderMaterial_FX;
typedef std::shared_ptr<RenderMaterial_FX> RenderMaterial_FXPtr;
typedef std::shared_ptr<class TexArray>	TexArrayPtr;
class UTextureRenderTarget;
class IRendererModule;

class TEXTUREGRAPHENGINE_API Device_FX : public Device,  public FGCObject
{
private: 
	static size_t					s_maxRenderBatch;

	typedef std::list<TexPtr>		TextureNodeList;

	mutable FCriticalSection		GCLock;							/// Lock to the GC lists
	TextureNodeList					GCTargetTextures;				/// Textures that are GC targets right now

	typedef std::list<TObjectPtr<UTextureRenderTarget2D>> RTList;
	typedef std::unordered_map<HashType, RTList*> RenderTargetCache;

	mutable FCriticalSection		GC_RTCache;						/// Lock to the GC lists

	RenderTargetCache				RTCache;						/// The render target cache that we are currently maintaining
	RenderTargetCache				RTArrayCache;					/// The render target array cache that we are currently maintaining

	RTList							RTUsed;							/// Used render targets

	IRendererModule*				RendererModule;					/// Useful rendering functionalities wrapped in this module

	void							GCTextures();
	virtual void					Free() override;
	virtual void					ClearCache() override;

	//////////////////////////////////////////////////////////////////////////
	/// FGCObject
	//////////////////////////////////////////////////////////////////////////
	virtual void					AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString					GetReferencerName() const override;

	void							FreeCacheInternal(RenderTargetCache& TargetRTCache);
	void							FreeRTList(RTList& RTList);
	void							FreeRenderTarget(HashType HashValue, UTextureRenderTarget2D* RT);

	static UTextureRenderTarget2D*	CreateRenderTarget(const BufferDescriptor& Desc);
	AsyncDeviceBufferRef			SplitToTiles_Internal(const CombineSplitArgs& SplitArgs);

	int32							AllocateRTArrayResource(TexArrayPtr TexArrayObj);
	int32							AllocateRTResource(TexPtr TextureObj);
	int32							InitRTResource(TexPtr TextureObj, UTextureRenderTarget* RT);

protected:
	virtual void					PrintStats() override;

public:
									Device_FX();
	virtual							~Device_FX() override;

	virtual cti::continuable<int32>	Use() const override;
	void							GetStatArray(TArray<FString>& ResourceArrayTiled, TArray<FString>& ResourceArrayUnTiled, TArray<FString>& TooltipListTiled, TArray<FString>& TooltipListUnTiled, FString& TotalStats);
	/// Must be called from the rendering thread
	DeviceBufferRef					CreateFromRT(UTextureRenderTarget2D* RT, const BufferDescriptor& Desc);
	DeviceBufferRef					CreateFromTexture(UTexture2D* TextureObj, const BufferDescriptor& Desc);
	DeviceBufferRef					CreateFromTex(TexPtr TextureObj, bool bInitRaw);
	DeviceBufferRef					CreateFromTexAndRaw(TexPtr TextureObj, RawBufferPtr RawObj);

	virtual AsyncDeviceBufferRef	CombineFromTiles(const CombineSplitArgs& CombineArgs) override;

	AsyncDeviceBufferRef			DrawTilesToBuffer_Deferred(DeviceBufferRef buffer, const T_Tiles<DeviceBufferRef>& tiles);
	AsyncDeviceBufferRef			FillTextureArray_Deferred(DeviceBufferRef buffer, const T_Tiles<DeviceBufferRef>& tiles);

	virtual AsyncDeviceBufferRef	SplitToTiles(const CombineSplitArgs& splitArgs) override;

	TexPtr							AllocateRenderTarget(const BufferDescriptor& Desc);
	TexPtr							AllocateRenderTargetArray(const BufferDescriptor& Desc, int32 NumTilesX, int32 NumTilesY);



	virtual void					Update(float Delta) override;
	void							MarkForCollection(TexPtr TextureObj);
	virtual FString					Name() const override { return "Device_FX"; }
	virtual void					AddNativeTask(DeviceNativeTaskPtr task) override;

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE FRHICommandListImmediate& RHI() const { check(IsInRenderingThread()); return GRHICommandList.GetImmediateCommandList(); }

	//////////////////////////////////////////////////////////////////////////
	/// Static functions
	//////////////////////////////////////////////////////////////////////////
	static void						InitTiles_Texture(BlobUPtr* tiles, size_t NumRows, size_t NumCols, const BufferDescriptor& InTileDesc, bool bInitRaw);
	static void						InitTiles_RenderTargets(BlobUPtr* Tiles, size_t NumRows, size_t NumCols, const BufferDescriptor& InTileDesc, bool bInitRaw);

	static Device_FX*				Get();
};
