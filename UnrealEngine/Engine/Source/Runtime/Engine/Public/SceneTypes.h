// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHIDefinitions.h"
#include "UObject/ObjectMacros.h"
#include "Templates/RefCounting.h"
#include "Containers/List.h"

#include "SceneTypes.generated.h"

class FLightMap;
class FSceneViewStateInterface;
class FShadowMap;

/** A reference to a light-map. */
typedef TRefCountPtr<FLightMap> FLightMapRef;

/** A reference to a shadow-map. */
typedef TRefCountPtr<FShadowMap> FShadowMapRef;

/** Custom primitive data payload. */
USTRUCT()
struct FCustomPrimitiveData
{
	GENERATED_USTRUCT_BODY()

	inline bool operator==(const FCustomPrimitiveData& Other) const
	{
		return Data == Other.Data;
	}

	static constexpr int32 NumCustomPrimitiveDataFloat4s = 9; // Must match NUM_CUSTOM_PRIMITIVE_DATA in SceneData.ush
	static constexpr int32 NumCustomPrimitiveDataFloats = NumCustomPrimitiveDataFloat4s * 4;

	UPROPERTY(EditAnywhere, Category=Rendering)
	TArray<float> Data;
};

enum class EPrimitiveDirtyState : uint8
{
	None                  = 0U,
	ChangedId             = (1U << 0U),
	ChangedTransform      = (1U << 1U),
	ChangedStaticLighting = (1U << 2U),
	ChangedOther          = (1U << 3U),
	/** The Added flag is a bit special, as it is used to skip invalidations in the VSM, and thus must only be set if the primitive is in fact added
	 * (a previous remove must have been processed by GPU scene, or it is new). If in doubt, don't set this. */
	 Added                = (1U << 4U),
	 Removed              = (1U << 5U), // Only used to make sure we don't process something that has been marked as Removed (more a debug feature, can be trimmed if need be)
	 ChangedAll = ChangedId | ChangedTransform | ChangedStaticLighting | ChangedOther,
	 /** Mark all data as changed and set Added flag. Must ONLY be used when a primitive is added, c.f. Added, above. */
	 AddedMask = ChangedAll | Added,
};
ENUM_CLASS_FLAGS(EPrimitiveDirtyState);

/** 
 * Class used to identify UPrimitiveComponents on the rendering thread without having to pass the pointer around, 
 * Which would make it easy for people to access game thread variables from the rendering thread.
 */
class FPrimitiveComponentId
{
public:

	FPrimitiveComponentId() : PrimIDValue(0)
	{}

	inline bool IsValid() const
	{
		return PrimIDValue > 0;
	}

	inline bool operator==(FPrimitiveComponentId OtherId) const
	{
		return PrimIDValue == OtherId.PrimIDValue;
	}

	friend uint32 GetTypeHash( FPrimitiveComponentId Id )
	{
		return GetTypeHash(Id.PrimIDValue);
	}

	uint32 PrimIDValue;
};

/** 
 * Class used to reference an FSceneViewStateInterface that allows destruction and recreation of all FSceneViewStateInterface's when needed. 
 * This is used to support reloading the renderer module on the fly.
 */
class FSceneViewStateReference
{
public:
	FSceneViewStateReference()
	: Reference(nullptr), ShareOriginTarget(nullptr), ShareOriginRefCount(0)
	{
	}

	ENGINE_API virtual ~FSceneViewStateReference();

	/**
	 * Allocates the Scene view state.
	 */
	ENGINE_API void Allocate(ERHIFeatureLevel::Type FeatureLevel);

	UE_DEPRECATED(5.0, "Allocate must be called with an appropriate RHI Feature Level")
	ENGINE_API void Allocate();

	/**
	  * Mark that a view state shares an origin with another view state, allowing sharing of some internal state, saving memory and performance.
	  * Typically used for cube map faces.  Must be called before "Allocate" is called on the source view state (best practice is to call
	  * immediately after creating the view state).  Multiple view states can point to the same shared origin (for example, the first face of a
	  * cube map), but sharing can't be nested.  Sharing view states must be destroyed before the Target is destroyed.
	  */
	ENGINE_API void ShareOrigin(FSceneViewStateReference* Target);

	/** Destorys the Scene view state. */
	ENGINE_API void Destroy();

	/** Destroys all view states, but does not remove them from the linked list. */
	ENGINE_API static void DestroyAll();

	/** Recreates all view states in the global list. */
	ENGINE_API static void AllocateAll(ERHIFeatureLevel::Type FeatureLevel);

	UE_DEPRECATED(5.0, "AllocateAll must be called with an appropriate RHI Feature Level")
	ENGINE_API static void AllocateAll();

	FSceneViewStateInterface* GetReference()
	{
		return Reference;
	}

private:
	FSceneViewStateInterface* Reference;
	TLinkedList<FSceneViewStateReference*> GlobalListLink;

	FSceneViewStateReference* ShareOriginTarget;

	/** Number of other view states that share this view state's origin. */
	int32 ShareOriginRefCount;

	static TLinkedList<FSceneViewStateReference*>*& GetSceneViewStateList();

	void AllocateInternal(ERHIFeatureLevel::Type FeatureLevel);
};

/** different light component types */
enum ELightComponentType
{
	LightType_Directional = 0,
	LightType_Point,
	LightType_Spot,
	LightType_Rect,
	LightType_MAX,
	LightType_NumBits = 2
};

/**
 * The types of interactions between a light and a primitive.
 */
enum ELightMapInteractionType
{
	LMIT_None	= 0,
	LMIT_GlobalVolume = 1,
	LMIT_Texture = 2,

	LMIT_NumBits = 3
};

enum EShadowMapInteractionType
{
	SMIT_None = 0,
	SMIT_GlobalVolume = 1,
	SMIT_Texture = 2,

	SMIT_NumBits = 3
};

/** Quality levels that a material can be compiled for. */
namespace EMaterialQualityLevel
{
	enum Type
	{
		Low,
		High,
		Medium,
		Epic,
		Num
	};
}

ENGINE_API FString LexToString(EMaterialQualityLevel::Type QualityLevel);

//
//	EMaterialProperty
//
UENUM(BlueprintType)
enum EMaterialProperty
{
	MP_EmissiveColor = 0 UMETA(DisplayName = "Emissive"),
	MP_Opacity UMETA(DisplayName = "Opacity"),
	MP_OpacityMask UMETA(DisplayName = "Opacity Mask"),
	MP_DiffuseColor UMETA(Hidden),			// used in Lightmass, not exposed to user, computed from: BaseColor, Metallic				Also used in Strata which uses Albedo/F0 parameterrization
	MP_SpecularColor UMETA(Hidden),			// used in Lightmass, not exposed to user, derived from: SpecularColor, Metallic, Specular	Also used in Strata which uses Albedo/F0 parameterrization
	MP_BaseColor UMETA(DisplayName = "Diffuse"),
	MP_Metallic UMETA(DisplayName = "Metallic"),
	MP_Specular UMETA(DisplayName = "Specular"),
	MP_Roughness UMETA(DisplayName = "Roughness "),
	MP_Anisotropy UMETA(DisplayName = "Anisotropy"),
	MP_Normal UMETA(DisplayName = "Normal"),
	MP_Tangent UMETA(DisplayName = "Tangent"),
	MP_WorldPositionOffset UMETA(Hidden),
	MP_WorldDisplacement_DEPRECATED UMETA(Hidden),
	MP_TessellationMultiplier_DEPRECATED UMETA(Hidden),
	MP_SubsurfaceColor UMETA(DisplayName = "Subsurface"),
	MP_CustomData0 UMETA(Hidden),
	MP_CustomData1 UMETA(Hidden),
	MP_AmbientOcclusion UMETA(DisplayName = "Ambient Occlusion"),
	MP_Refraction UMETA(DisplayName = "Refraction"),
	MP_CustomizedUVs0 UMETA(Hidden),
	MP_CustomizedUVs1 UMETA(Hidden),
	MP_CustomizedUVs2 UMETA(Hidden),
	MP_CustomizedUVs3 UMETA(Hidden),
	MP_CustomizedUVs4 UMETA(Hidden),
	MP_CustomizedUVs5 UMETA(Hidden),
	MP_CustomizedUVs6 UMETA(Hidden),
	MP_CustomizedUVs7 UMETA(Hidden),
	MP_PixelDepthOffset UMETA(Hidden),
	MP_ShadingModel UMETA(Hidden),
	MP_FrontMaterial UMETA(Hidden),

	//^^^ New material properties go above here ^^^^
	MP_MaterialAttributes UMETA(Hidden),
	MP_CustomOutput UMETA(Hidden),
	MP_MAX UMETA(DisplayName = "None"),
};

/** Blend modes supported for simple element rendering */
enum ESimpleElementBlendMode
{
	SE_BLEND_Opaque = 0,
	SE_BLEND_Masked,
	SE_BLEND_Translucent,
	SE_BLEND_Additive,
	SE_BLEND_Modulate,
	SE_BLEND_MaskedDistanceField,
	SE_BLEND_MaskedDistanceFieldShadowed,
	SE_BLEND_TranslucentDistanceField,
	SE_BLEND_TranslucentDistanceFieldShadowed,
	SE_BLEND_AlphaComposite,
	SE_BLEND_AlphaHoldout,
	// Like SE_BLEND_Translucent, but modifies destination alpha
	SE_BLEND_AlphaBlend,
	// Like SE_BLEND_Translucent, but reads from an alpha-only texture
	SE_BLEND_TranslucentAlphaOnly,
	SE_BLEND_TranslucentAlphaOnlyWriteAlpha,

	SE_BLEND_RGBA_MASK_START,
	SE_BLEND_RGBA_MASK_END = SE_BLEND_RGBA_MASK_START + 31, //Using 5bit bit-field for red, green, blue, alpha and desaturation

	SE_BLEND_MAX
};
