// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "RHIDefinitions.h"
#endif
#include "SceneTypes.h"
#include "MaterialShaderQualitySettings.generated.h"

class UShaderPlatformQualitySettings;
enum EShaderPlatform : uint16;

//UCLASS(config = Engine, defaultconfig)
UCLASS()
class MATERIALSHADERQUALITYSETTINGS_API UMaterialShaderQualitySettings : public UObject
{
public:
	GENERATED_UCLASS_BODY()

	UShaderPlatformQualitySettings* GetShaderPlatformQualitySettings(FName PlatformName);

	const UShaderPlatformQualitySettings* GetShaderPlatformQualitySettings(EShaderPlatform ShaderPlatform);

	bool HasPlatformQualitySettings(EShaderPlatform ShaderPlatform, EMaterialQualityLevel::Type QualityLevel);

#if WITH_EDITOR
	// Override GetShaderPlatformQualitySettings() return value with the specified platform's settings.
	// An empty PlatformName or otherwise non existent platform will cause GetShaderPlatformQualitySettings() 
	// to revert to its default behaviour.
	void SetPreviewPlatform(FName PlatformName);
	const FName& GetPreviewPlatform();
#endif

	static UMaterialShaderQualitySettings* Get();

private:
	UShaderPlatformQualitySettings* GetOrCreatePlatformSettings(FName ShaderPlatformName);

	UPROPERTY()
	TMap<FName, TObjectPtr<UShaderPlatformQualitySettings>> ForwardSettingMap;

#if WITH_EDITORONLY_DATA
	UShaderPlatformQualitySettings* PreviewPlatformSettings;
	FName PreviewPlatformName;
#endif

	static class UMaterialShaderQualitySettings* RenderQualitySingleton;
};
