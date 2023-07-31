// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ARTextures.h"
#include "Misc/Guid.h"

#if PLATFORM_ANDROID
#include "arcore_c_api.h"
#endif

#include "GoogleARCoreTexture.generated.h"


UCLASS()
class GOOGLEARCOREBASE_API UARCoreCameraTexture : public UARTexture
{
	GENERATED_UCLASS_BODY()
	
public:
	// UTexture interface implementation
	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_TextureExternal; }
	// End UTexture interface
	
	uint32 GetTextureId() const;
};


UCLASS()
class GOOGLEARCOREBASE_API UARCoreDepthTexture : public UARTexture
{
	GENERATED_UCLASS_BODY()
	
public:
	// UTexture interface implementation
	virtual FTextureResource* CreateResource() override;
	virtual EMaterialValueType GetMaterialType() const override { return MCT_Texture2D; }
	// End UTexture interface
	
#if PLATFORM_ANDROID
	void UpdateDepthImage(const ArSession* SessionHandle, const ArImage* Image);
#endif
};
