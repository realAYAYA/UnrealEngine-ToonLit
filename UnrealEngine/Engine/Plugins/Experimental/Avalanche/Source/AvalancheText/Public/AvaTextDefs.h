// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Text3DComponent.h"
#include "AvaTextDefs.generated.h"

UENUM()
enum class EAvaOutlineType : uint8 
{
	Invalid     UMETA(Hidden),
	None,
	AddOutline,
	OutlineOnly
};

UENUM()
enum class EAvaTextColoringStyle : uint8 
{
	Invalid        UMETA(Hidden),
	Solid,
	Gradient,
	FromTexture,
	CustomMaterial
};

UENUM()
enum class EAvaTextTranslucency : uint8 
{
	Invalid        UMETA(Hidden),
	None,
	Translucent,
	GradientMask,
};

UENUM()
enum class EAvaMaterialMaskOrientation : uint8 
{
	LeftRight UMETA(DisplayName = "Left to Right"),
	RightLeft UMETA(DisplayName = "Right to Left"),
	Custom
};

USTRUCT(BlueprintType)
struct FAvaTextAlignment
{
	GENERATED_BODY()
    
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design")
	EText3DHorizontalTextAlignment HorizontalAlignment = EText3DHorizontalTextAlignment::Left;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design")
	EText3DVerticalTextAlignment VerticalAlignment = EText3DVerticalTextAlignment::FirstLine;
	
	bool operator==(const FAvaTextAlignment& Other) const
	{
		return HorizontalAlignment == Other.HorizontalAlignment && VerticalAlignment == Other.VerticalAlignment;
	}
};

UENUM()
enum class EAvaGradientDirection : uint8 
{
	None        UMETA(Hidden),
	Vertical,
	Horizontal,
	Custom,
};

USTRUCT(BlueprintType)
struct FAvaLinearGradientSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design")
	EAvaGradientDirection Direction = EAvaGradientDirection::Vertical;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design")
	FLinearColor ColorA = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design")
	FLinearColor ColorB = FLinearColor::Black;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Smoothness = 0.1f;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Offset = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Rotation = 0.0f;

	bool operator==(const FAvaLinearGradientSettings& Other) const
	{
		return
			Direction == Other.Direction &&
			ColorA == Other.ColorA &&
			ColorB == Other.ColorB &&
			Smoothness == Other.Smoothness &&
			Offset == Other.Offset &&
			Rotation == Other.Rotation;
	}
};

/**
 * TODO: use this for UAvaText3DComponent in place of separate mask settings
 * Main Blocker from using this now is that it needs custom tracks/sections/property track editor to be keyable for Sequencer
 */
USTRUCT(BlueprintType)
struct FAvaMaterialMaskSettings
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	EAvaMaterialMaskOrientation Orientation = EAvaMaterialMaskOrientation::LeftRight;
	
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Text", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Smoothness = 0.1f;
	
	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Text", meta = (ClampMin = 0.0, ClampMax = 1.0))
	float Offset = 0.5f;

	UPROPERTY(Interp, EditAnywhere, BlueprintReadWrite, Category = "Text", meta = (ClampMin = 0.0, ClampMax = 1.0, EditCondition = "Orientation==EAvaMaterialMaskOrientation::Custom", EditConditionHides))
	float Rotation = 0.0f;

	bool operator==(const FAvaMaterialMaskSettings& Other) const
	{
		return
			Orientation == Other.Orientation &&
			Smoothness == Other.Smoothness &&
			Offset == Other.Offset &&
			Rotation == Other.Rotation;
	}
};

UENUM()
enum class EAvaTextLength : uint8 
{
	Unlimited,
	CharacterCount UMETA(DisplayName = "Maximum Character Count")
};

UENUM()
enum class EAvaTextCase : uint8 
{
	Regular,
	UpperCase,
	LowerCase
};

USTRUCT(BlueprintType)
struct FAvaTextField
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design", meta = (MultiLine = true))
	FText Text;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design")
	EAvaTextLength MaximumLength = EAvaTextLength::Unlimited;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design", meta = (EditCondition = "MaximumLength==EAvaTextLength::CharacterCount", EditConditionHides))
	int32 Count = 20;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Motion Design")
	EAvaTextCase TextCase = EAvaTextCase::Regular;
};

/**
 * Flags to list the desired features of a Text Material
 */
enum class EAvaTextMaterialFeatures : uint8
{
	None         = 0,
	GradientMask = 1 << 0,
	Unlit        = 1 << 1,
	Translucent  = 1 << 2
};
ENUM_CLASS_FLAGS(EAvaTextMaterialFeatures)

struct FAvaTextMaterialSettings
{
	/** The coloring style used for the material (e.g. Solid, Gradient, Textured) */
	EAvaTextColoringStyle ColoringStyle = EAvaTextColoringStyle::Solid;

	/** The features of the material (e.g. Masked, Unlit) */
	EAvaTextMaterialFeatures MaterialFeatures = EAvaTextMaterialFeatures::Unlit;

	FAvaTextMaterialSettings() = default;
	
	FAvaTextMaterialSettings(const EAvaTextColoringStyle InColoringStyle, const EAvaTextMaterialFeatures InMaterialFeatures)
		: ColoringStyle(InColoringStyle)
		, MaterialFeatures(InMaterialFeatures)
	{
	}

	bool operator==(const FAvaTextMaterialSettings& Other) const
	{
		return
			ColoringStyle == Other.ColoringStyle &&
			MaterialFeatures == Other.MaterialFeatures;
	}

	bool operator!=(const FAvaTextMaterialSettings& Other) const
	{
		return !(*this == Other);
	}
	
	friend uint32 GetTypeHash(const FAvaTextMaterialSettings& InMaterialSettings)
	{
		return HashCombineFast(GetTypeHash(InMaterialSettings.ColoringStyle), GetTypeHash(InMaterialSettings.MaterialFeatures));
	}
};
