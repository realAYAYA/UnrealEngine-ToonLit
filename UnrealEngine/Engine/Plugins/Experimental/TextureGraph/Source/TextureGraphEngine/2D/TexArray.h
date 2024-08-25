// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Tex.h"
#include "Helper/Promise.h"

class UTexture2D;
class UTextureRenderTarget2DArray;

class TexArray;

typedef std::shared_ptr<TexArray>		TexArrayPtr;

class TEXTUREGRAPHENGINE_API TexArray : public Tex
{
	friend class Device_FX;

protected:
	TObjectPtr<UTextureRenderTarget2DArray>	RTArray = nullptr;			/// The render target array
	int32							XTiles = 0;
	int32							YTiles = 0;
	int32							Index = 0;
	void							InitRTArray(bool ForceFloat = false);

public:
									TexArray();
									TexArray(Tex& TexObj, int32 XTilesTotal, int32 YTilesTotal);
									TexArray(const TexDescriptor& Desc, UTextureRenderTarget2DArray* rtArray);
									TexArray(const TexDescriptor& Desc,  int32 XTilesTotal, int32 YTilesTotal);
	virtual							~TexArray() override;
	virtual void					AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual bool					IsArray() override { return true; }
	virtual UTexture*				GetTexture() const override;
	virtual bool					IsNull() const override;

	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE UTextureRenderTarget2DArray* GetRenderTargetArray() const { return RTArray; }
};
