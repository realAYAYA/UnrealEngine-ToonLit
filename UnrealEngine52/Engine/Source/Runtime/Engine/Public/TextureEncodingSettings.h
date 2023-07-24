// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Engine/DeveloperSettings.h"
#include "Engine/TextureDefines.h"
#include "TextureEncodingSettings.generated.h"


// For encoders that support it (i.e. Oodle), this controls how much time to
// spend on finding better encoding.
// (These need to match the Oodle OodleTex_EncodeEffortLevel values if you are 
// using Oodle.)
UENUM()
enum class ETextureEncodeEffort : uint8
{
	Default = 0		UMETA(ToolTip = "Let the encoder decide what's best."),
	Low = 10		UMETA(ToolTip = "Faster encoding, lower quality. Probably don't ship textures encoded at this effort level"),
	Normal = 20		UMETA(ToolTip = "Reasonable compromise"),
	High = 30		UMETA(ToolTip = "More time, better quality - good for nightlies / unattended cooks.")
};

// enum values must match exactly with OodleTex_RDO_UniversalTiling
UENUM()
enum class ETextureUniversalTiling : uint8
{
	Disabled = 0,
	Enabled_256KB = 1,
	Enabled_64KB = 2
};


// Enum that allows for not overriding what the existing setting is - all the
// other values have the same meaning as ETextureEncodeSpeed
UENUM()
enum class ETextureEncodeSpeedOverride : uint8
{
	Disabled = 255, // don't override.
	Final = 0,
	FinalIfAvailable = 1,
	Fast = 2
};

// 
// Encoding can either use the "Final" or "Fast" speeds, for supported encoders (e.g. Oodle)
// These settings have no effect on encoders that don't support encode speed
//
UCLASS(config = Engine, defaultconfig, meta = (DisplayName = "Texture Encoding"))
class ENGINE_API UTextureEncodingProjectSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	// If true, Final encode speed enables rate-distortion optimization on supported encoders to
	// decrease *on disc* size of textures in compressed package files.
	// This rate-distortion tradeoff is controlled via "Lambda". The "LossyCompressionAmount" parameter on
	// textures is used to control it. Specific LossyCompressionAmount values correspond to
	// to RDO lambdas of:
	// 
	//	None - Disable RDO for this texture.
	//	Lowest - 1 (Least distortion)
	//	Low - 10
	//	Medium - 20
	//	High - 30
	//	Highest - 40
	// 
	// If set to Default, then the LossyCompressionAmount in the LODGroup for the texture is
	// used. If that is also Default, then the RDOLambda in these settings is used.
	//
	// Note that any distortion introduced is on top of, and likely less than, any introduced by the format itself.
	UPROPERTY(config, EditAnywhere, Category = EncodeSpeedSettings, meta = (ConfigRestartRequired = true))
	uint32 bFinalUsesRDO : 1;

	// Ignored if UsesRDO is false. This value is used if a given texture is using "Default" LossyCompressionAmount.
	// Otherwise, the value of LossyCompressionAmount is translated in to a fixed lambda (see UsesRDO tooltip).
	// 
	// Low values (1) represent highest quality (least distortion) results.
	UPROPERTY(config, EditAnywhere, Category = EncodeSpeedSettings, meta = (DisplayName = "Final RDO Lambda", UIMin = 1, UIMax = 100, ClampMin = 1, ClampMax = 100, EditCondition = bFinalUsesRDO, ConfigRestartRequired = true))
	int8 FinalRDOLambda;

	// Specifies how much time to take trying for better encoding results.
	UPROPERTY(config, EditAnywhere, Category = EncodeSpeedSettings, meta = (ConfigRestartRequired = true))
	ETextureEncodeEffort FinalEffortLevel;	

	// Specifies how to assume textures are laid out on disc. This only applies to Oodle with RDO
	// enabled. 256 KB is a good middle ground. Enabling this will decrease the on-disc
	// sizes of textures for platforms with exposed texture tiling (i.e. consoles), but will slightly increase
	// sizes of textures for platforms with opaque tiling (i.e. desktop).
	UPROPERTY(config, EditAnywhere, Category = EncodeSpeedSettings, meta = (ConfigRestartRequired = true))
	ETextureUniversalTiling FinalUniversalTiling;

	// If true, Final encode speed enables rate-distortion optimization on supported encoders to
	// decrease *on disc* size of textures in compressed package files.
	// This rate-distortion tradeoff is controlled via "Lambda". The "LossyCompressionAmount" parameter on
	// textures is used to control it. Specific LossyCompressionAmount values correspond to
	// to RDO lambdas of:
	// 
	//	None - Disable RDO for this texture.
	//	Lowest - 1 (Least distortion)
	//	Low - 10
	//	Medium - 20
	//	High - 30
	//	Highest - 40
	// 
	// If set to Default, then the LossyCompressionAmount in the LODGroup for the texture is
	// used. If that is also Default, then the RDOLambda in these settings is used.
	//
	// Note that any distortion introduced is on top of, and likely less than, any introduced by the format itself.
	UPROPERTY(config, EditAnywhere, Category = EncodeSpeedSettings, meta = (ConfigRestartRequired = true))
	uint32 bFastUsesRDO : 1;

	// Ignored if UsesRDO is false. This value is used if a given texture is using "Default" LossyCompressionAmount.
	// Otherwise, the value of LossyCompressionAmount is translated in to a fixed lambda (see UsesRDO tooltip).
	// 
	// Low values (1) represent highest quality (least distortion) results.
	UPROPERTY(config, EditAnywhere, Category = EncodeSpeedSettings, meta = (DisplayName = "Fast RDO Lambda", UIMin = 1, UIMax = 100, ClampMin = 1, ClampMax = 100, EditCondition = bFastUsesRDO, ConfigRestartRequired = true))
	int8 FastRDOLambda;

	// Specifies how much time to take trying for better encode results.
	UPROPERTY(config, EditAnywhere, Category = EncodeSpeedSettings, meta = (ConfigRestartRequired = true))
	ETextureEncodeEffort FastEffortLevel;

	// Specifies how to assume textures are laid out on disc. This only applies to Oodle with RDO
	// enabled. 256 KB is a good middle ground. Enabling this will decrease the on-disc
	// sizes of textures for platforms with exposed texture tiling (i.e. consoles), but will slightly increase
	// sizes of textures for platforms with opaque tiling (i.e. desktop).
	UPROPERTY(config, EditAnywhere, Category = EncodeSpeedSettings, meta = (ConfigRestartRequired = true))
	ETextureUniversalTiling FastUniversalTiling;

	// Which encode speed non interactive editor sessions will use (i.e. commandlets)
	UPROPERTY(config, EditAnywhere, Category = EncodeSpeeds, meta = (ConfigRestartRequired = true))
	ETextureEncodeSpeed CookUsesSpeed;

	// Which encode speed everything else uses.
	UPROPERTY(config, EditAnywhere, Category = EncodeSpeeds, meta = (ConfigRestartRequired = true))
	ETextureEncodeSpeed EditorUsesSpeed;
};

UCLASS(config = EditorPerProjectUserSettings, defaultconfig, meta = (DisplayName = "Texture Encoding"))
class ENGINE_API UTextureEncodingUserSettings : public UDeveloperSettings
{
	GENERATED_UCLASS_BODY()

	// Local machine/project setting to force an encode speed, if desired.
	// See the Engine "Texture Encoding" section for details.
	UPROPERTY(config, EditAnywhere, Category = EncodeSpeeds, meta = (ConfigRestartRequired = true))
	ETextureEncodeSpeedOverride ForceEncodeSpeed;
};

//
// Separate type so that the engine can check for custom encoding set in the texture
// editor module without needing to depend on it.
//
class FTextureEditorCustomEncode
{
public:
	// If we want to override Oodle specific encoding settings, we set this to true.
	bool bUseCustomEncode = false;

	// [0,100]
	uint8 OodleRDOLambda = 0;

	// enum ETextureEncodeEffort
	uint8 OodleEncodeEffort = 0;

	// enum ETextureUniversalTiling
	uint8 OodleUniversalTiling = 0;

};