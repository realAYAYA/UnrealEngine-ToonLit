// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Internationalization/Text.h"
#include "Engine/EngineTypes.h"
#include "DMDefs.generated.h"

class UTexture;
class UDMMaterialStage;

UENUM(BlueprintType)
enum class EDMMaterialPropertyType : uint8
{
	None = 0,
	BaseColor,
	EmissiveColor,
	Opacity,
	OpacityMask,
	Roughness,
	Specular,
	Metallic,
	Normal,
	PixelDepthOffset,
	WorldPositionOffset,
	AmbientOcclusion,
	Anisotropy,
	Refraction,
	Tangent,
	Custom1,
	Custom2,
	Custom3,
	Custom4,
	Any
};

UENUM(BlueprintType)
enum class EDMValueType : uint8
{
	VT_None,
	VT_Bool,
	VT_Float1,
	VT_Float2,
	VT_Float3_RPY,
	VT_Float3_RGB,
	VT_Float3_XYZ,
	VT_Float4_RGBA,
	VT_Float_Any,
	VT_Texture,
	VT_ColorAtlas
};

UENUM(BlueprintType)
enum class EDMUpdateType : uint8
{
	Value,
	Structure
};

UENUM(BlueprintType)
enum class EDMMaterialShadingModel : uint8
{
	Unlit      = EMaterialShadingModel::MSM_Unlit,
	DefaultLit = EMaterialShadingModel::MSM_DefaultLit
};

UENUM(BlueprintType, meta = (DisplayName = "Material Designer UV Source"))
enum class EDMUVSource : uint8
{
	Texture,
	ScreenPosition,
	WorldPosition
};

/**
 * An individual component of a connector (e.g. G from RGB.)
 */
USTRUCT(BlueprintType, Category = "Material Designer", meta = (DisplayName = "Material Designer Stage Connector Channel"))
struct DYNAMICMATERIAL_API FDMMaterialStageConnectorChannel
{
	GENERATED_BODY()

	static constexpr int32 NO_SOURCE = -1;
	static constexpr int32 PREVIOUS_STAGE = 0;
	static constexpr int32 FIRST_STAGE_INPUT = 1;
	static constexpr int32 WHOLE_CHANNEL = 0;
	static constexpr int32 FIRST_CHANNEL = 1;
	static constexpr int32 SECOND_CHANNEL = 2;
	static constexpr int32 THIRD_CHANNEL = 4;
	static constexpr int32 FOURTH_CHANNEL = 8;
	static constexpr int32 TWO_CHANNELS = FIRST_CHANNEL | SECOND_CHANNEL;
	static constexpr int32 THREE_CHANNELS = FIRST_CHANNEL | SECOND_CHANNEL | THIRD_CHANNEL;
	/** Not really needed? Effectively the whole channel! */
	static constexpr int32 FOUR_CHANNELS = FIRST_CHANNEL | SECOND_CHANNEL | THIRD_CHANNEL | FOURTH_CHANNEL;

	/**
	 * The index of the source of this channel
	 * Index 0 is the previous stage, 1+ are the other inputs required by the current stage (e.g. textures, uvs, etc.)
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	int32 SourceIndex = PREVIOUS_STAGE;

	/** When using previous stages, this is the material property the previous stage is using */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	EDMMaterialPropertyType MaterialProperty = EDMMaterialPropertyType::None;

	/** The index of the output connector of the given stage. */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	int32 OutputIndex = 0;

	/**
	 * This can be used to break down float2/3/4 into single pieces of data
	 * A value of 0 will be the original output. A bitmask (1,2,4,8) will reference (and combine) the specific channels.
	 */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Material Designer")
	int32 OutputChannel = WHOLE_CHANNEL;

	bool operator==(const FDMMaterialStageConnectorChannel& Other) const
	{
		return MaterialProperty == Other.MaterialProperty
			&& SourceIndex == Other.SourceIndex
			&& OutputIndex == Other.OutputIndex
			&& OutputChannel == Other.OutputChannel;
	}
};

struct DYNAMICMATERIAL_API FDMUpdateGuard
{
	FDMUpdateGuard()
	{
		++GuardCount;
	}

	virtual ~FDMUpdateGuard()
	{
		--GuardCount;
	}

	static bool CanUpdate()
	{
		return (GuardCount == 0);
	}

private:
	static int32 GuardCount;
};

struct DYNAMICMATERIAL_API FDMInitializationGuard
{
public:
	static bool IsInitializing();

	FDMInitializationGuard();
	virtual ~FDMInitializationGuard();

private:
	static uint32 GuardCount;
};

namespace UE::DynamicMaterial
{
	/** Designed to be used with preprocessor macros. See below. */
	FString DYNAMICMATERIAL_API CreateNodeComment(const ANSICHAR* InFile, int InLine, const ANSICHAR* InFunction, const FString* InComment = nullptr);

	constexpr int32 RenameFlags = REN_DontCreateRedirectors | REN_DoNotDirty | REN_ForceNoResetLoaders | REN_NonTransactional;
}

#define UE_DM_NodeComment_Default UE::DynamicMaterial::CreateNodeComment(__FILE__, __LINE__, __FUNCTION__)
#define UE_DM_NodeComment(Comment) UE::DynamicMaterial::CreateNodeComment(__FILE__, __LINE__, __FUNCTION__, &Comment)
