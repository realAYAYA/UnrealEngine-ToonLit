// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

#include "CalibratedMapFormat.h"
#include "Engine/EngineTypes.h"
#include "Engine/Texture.h"
#include "Models/LensModel.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "LensData.generated.h"


/**
 * Information about the lens rig
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FLensInfo
{
	GENERATED_BODY()

public:
	bool operator==(const FLensInfo& Other) const
	{
		return LensModelName == Other.LensModelName
			&& LensSerialNumber == Other.LensSerialNumber
			&& LensModel == Other.LensModel
			&& ImageDimensions == Other.ImageDimensions
			&& SensorDimensions == Other.SensorDimensions;
	}
	
public:

	/** Model name of the lens */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens Info")
	FString LensModelName;

	/** Serial number of the lens */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens Info")
	FString LensSerialNumber;

	/** Model of the lens (spherical, anamorphic, etc...) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens Info")
	TSubclassOf<ULensModel> LensModel;

	/** Width and height of the calibrated camera's sensor, in millimeters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens Info")
	FVector2D SensorDimensions = FVector2D(23.76f, 13.365f);

	/** Camera image size, in pixels */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lens Info")
	FIntPoint ImageDimensions = FIntPoint(1920, 1080);

};

/**
 * Lens distortion parameters
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FDistortionInfo
{
	GENERATED_BODY()

public:
	/** Generic array of floating-point lens distortion parameters */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	TArray<float> Parameters;
};

/**
 * Normalized focal length information for both width and height dimension
 * If focal length is in pixel, normalize using pixel dimensions
 * If focal length is in mm, normalize using sensor dimensions
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FFocalLengthInfo
{
	GENERATED_BODY()

public:

	/** Value expected to be normalized (unitless) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	FVector2D FxFy = FVector2D(1.0f, (16.0f / 9.0f));
};

/**
 * Pre generate STMap and normalized focal length information
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FSTMapInfo
{
	GENERATED_BODY()

public:
	/** Pre calibrated UVMap/STMap */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	TObjectPtr<UTexture> DistortionMap = nullptr;

	/** Calibrated map format */
	UPROPERTY(EditAnywhere, Category = "Format", meta = (ShowOnlyInnerProperties))
	FCalibratedMapFormat MapFormat;
};

/**
 * Lens camera image center parameters
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FImageCenterInfo
{
	GENERATED_BODY()

public:
	/** Value expected to be normalized [0,1] */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", meta = (DisplayName = "Image Center"))
	FVector2D PrincipalPoint = FVector2D(0.5f, 0.5f);
};

/**
 * Lens nodal point offset
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FNodalPointOffset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nodal point")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Nodal point")
	FQuat RotationOffset = FQuat::Identity;
};

/**
* Distortion data evaluated for given FZ pair based on lens parameters
*/
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FDistortionData
{
	GENERATED_BODY()

	public:

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Distortion")
	TArray<FVector2D> DistortedUVs;

	/** Estimated overscan factor based on distortion to have distorted cg covering full size */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Distortion")
	float OverscanFactor = 1.0f;
};

/**
 * Base struct for point info wrapper which holds focus and zoom
 * Child classes should hold the point info itself
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FDataTablePointInfoBase
{
	GENERATED_BODY()

	FDataTablePointInfoBase()
		: Focus(0.f)
		, Zoom(0.f)
	{}

	FDataTablePointInfoBase(const float InFocus, const float InZoom)
		: Focus(InFocus)
		, Zoom(InZoom)
	{}

	/** Point Focus Value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Point")
	float Focus;

	/** Point Zoom Value */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Point")
	float Zoom;
};


/**
 * Distortion Point Info struct
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FDistortionPointInfo : public FDataTablePointInfoBase
{
	GENERATED_BODY()

	using TypeInfo = FDistortionInfo;

	FDistortionPointInfo()
		: FDataTablePointInfoBase()
	{}

	FDistortionPointInfo(const float InFocus, const float InZoom, const FDistortionInfo& InDistortionInfo)
		: FDataTablePointInfoBase(InFocus, InZoom)
		, DistortionInfo(InDistortionInfo)
	{}

	/** Lens distortion parameter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Point")
	FDistortionInfo DistortionInfo;
};


/**
 * Focal Length Point Info struct
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FFocalLengthPointInfo : public FDataTablePointInfoBase
{
	GENERATED_BODY()

	using TypeInfo = FFocalLengthInfo;

	FFocalLengthPointInfo()
		: FDataTablePointInfoBase()
	{}

	FFocalLengthPointInfo(const float InFocus, const float InZoom, const TypeInfo& InFocalLengthInfo)
		: FDataTablePointInfoBase(InFocus, InZoom)
		, FocalLengthInfo(InFocalLengthInfo)
	{}

	/** Focal Length parameter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Point")
	FFocalLengthInfo FocalLengthInfo;
};

/**
 * ST Map Point Info struct
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FSTMapPointInfo : public FDataTablePointInfoBase
{
	GENERATED_BODY()

	using TypeInfo = FSTMapInfo;

	FSTMapPointInfo()
		: FDataTablePointInfoBase()
	{}

	FSTMapPointInfo(const float InFocus, const float InZoom, const TypeInfo& InSTMapInfo)
		: FDataTablePointInfoBase(InFocus, InZoom)
		, STMapInfo(InSTMapInfo)
	{}

	/** ST Map parameter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Point")
	FSTMapInfo STMapInfo;
};

/**
 * Image Center Point Info struct
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FImageCenterPointInfo : public FDataTablePointInfoBase
{
	GENERATED_BODY()

	using TypeInfo = FImageCenterInfo;

	FImageCenterPointInfo()
		: FDataTablePointInfoBase()
	{}

	FImageCenterPointInfo(const float InFocus, const float InZoom, const TypeInfo& InImageCenterInfo)
		: FDataTablePointInfoBase(InFocus, InZoom)
		, ImageCenterInfo(InImageCenterInfo)
	{}

	/** Image Center parameter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Point")
	FImageCenterInfo ImageCenterInfo;
};

/**
 * Nodal Point Point Info struct
 */
USTRUCT(BlueprintType)
struct CAMERACALIBRATIONCORE_API FNodalOffsetPointInfo : public FDataTablePointInfoBase
{
	GENERATED_BODY()

	using TypeInfo = FNodalPointOffset;

	FNodalOffsetPointInfo()
		: FDataTablePointInfoBase()
	{}

	FNodalOffsetPointInfo(const float InFocus, const float InZoom, const TypeInfo& InNodalPointOffset)
		: FDataTablePointInfoBase(InFocus, InZoom)
		, NodalPointOffset(InNodalPointOffset)
	{}

	/** Nodal Point parameter */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Point")
	FNodalPointOffset NodalPointOffset;
};


