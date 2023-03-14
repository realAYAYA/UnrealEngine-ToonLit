// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/EngineTypes.h"
#include "Engine/DeveloperSettings.h"

#include "TextureImportSettings.generated.h"

struct FPropertyChangedEvent;

UCLASS(config=Editor, defaultconfig, meta=(DisplayName="Texture Import"))
class TEXTUREUTILITIESCOMMON_API UTextureImportSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config, EditAnywhere, Category = VirtualTextures, meta = (
		DisplayName = "Auto Virtual Texturing Size",
		ToolTip = "Automatically enable the 'Virtual Texture Streaming' texture setting for textures larger than or equal to this size. This setting will not affect existing textures in the project."))
	int32 AutoVTSize;

public:

	//~ Begin UObject Interface

	virtual void PostInitProperties() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	//~ End UObject Interface
};
