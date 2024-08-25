// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "TextureSet.h"
#include "2D/TextureType.h"
#include "FxMat/RenderMaterial_BP.h"

struct FMaterialMappingInfo;
typedef TFunction<void(TiledBlobPtr)> TextureReadyCallback;

class RenderMesh;
typedef std::shared_ptr<RenderMesh>	RenderMeshPtr;


class TEXTUREGRAPHENGINE_API TargetTextureSet : public TextureSet
{
protected:

	mutable TMap<FName, TextureReadyCallback> Callbacks;		/// The callbacks to call when texture is set

	int32							Id = -1;					/// The ID/index into the list of target texture sets
	FString							Name;						/// Display name of this target set

	RenderMeshPtr					Mesh = nullptr;				/// The render mesh that this this texture set is targetting. 
																/// One texture set can only target one render mesh at a time.
	

	TMap<FName, TiledBlobRef>		BoundTexturesMap;			///Textures currently bound and dont want deallocation until new textures assigned
	int32							RenderCount = 0;			/// The number of times this layer has been rendered to

	virtual void					InitTex(int32 InTypeIndex) override;
	virtual void					BindOnTextureUpdate(RenderMaterial_BPPtr InMaterial, FMaterialMappingInfo MaterialMappingInfo) const;
	virtual void					RegisterCallback(TextureReadyCallback Callback, FMaterialMappingInfo MaterialMappingInfo) const;
	virtual void					UnRegisterCallback(FName TextureName) const;
public:
									TargetTextureSet(int32 InId, const FString& InName, RenderMeshPtr InMesh, int32 InWidth, int32 InHeight);
	virtual							~TargetTextureSet() override;

	void							SetMesh(RenderMeshPtr InMesh);
	
	AsyncBufferResultPtr			BindTo(RenderMaterial_BPPtr Material, TArray<struct FMaterialMappingInfo> MaterialMappingInfos);
	AsyncBufferResultPtr			BindTo(RenderMaterial_BPPtr Material, struct FMaterialMappingInfo MaterialMappingInfo);
	AsyncBufferResultPtr			UnbindFrom(RenderMaterial_BPPtr Material);
	
	virtual void					SetTexture(FName TextureName, TiledBlobRef InTexture) override;
	virtual void					FreeAt(FName TextureName) override;
	virtual void					Free() override;
	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	//////////////////////////////////////////////////////////////////////////
	FORCEINLINE int32				GetId() const { return Id; }
	FORCEINLINE FString				GetName() const { return Name; }
	FORCEINLINE RenderMeshPtr		GetMesh() const { return Mesh; }
	FORCEINLINE int32				GetRenderCount() const { return RenderCount; }
	
};

typedef std::unique_ptr<TargetTextureSet>	TargetTextureSetPtr;
typedef std::vector<TargetTextureSetPtr>	TargetTextureSetPtrVec;

