// Copyright Epic Games, Inc. All Rights Reserved.

// Reflection mirrors of C++ structs defined in Core or CoreUObject, those modules are not parsed by the Unreal Header Tool.
// The documentation comments here are only for use in the editor tooltips, and is ignored for the API docs.
// More complete documentation will be found in the files that have the full class definition, listed below.

#pragma once

// Help intellisense to avoid interpreting this file's declaration of FVector etc as it assumes !CPP by default
#ifndef CPP
#define CPP 1
#endif

#if CPP

// Include the real definitions of the noexport classes below to allow the generated cpp file to compile.

#include "PixelFormat.h"

#include "Misc/FallbackStruct.h"
#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"

#include "UObject/TopLevelAssetPath.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/PropertyAccessUtil.h"
#include "Serialization/TestUndeclaredScriptStructObjectReferences.h"

#include "Math/InterpCurvePoint.h"
#include "Math/UnitConversion.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Math/Vector2D.h"
#include "Math/TwoVectors.h"
#include "Math/Plane.h"
#include "Math/Rotator.h"
#include "Math/Quat.h"
#include "Math/IntPoint.h"
#include "Math/IntVector.h"
#include "Math/Color.h"
#include "Math/Box.h"
#include "Math/Box2D.h"
#include "Math/BoxSphereBounds.h"
#include "Math/OrientedBox.h"
#include "Math/Matrix.h"
#include "Math/ScalarRegister.h"
#include "Math/RandomStream.h"
#include "Math/RangeBound.h"
#include "Math/Interval.h"
#include "Math/Sphere.h"

#include "Internationalization/PolyglotTextData.h"

#include "AssetRegistry/ARFilter.h"
#include "AssetRegistry/AssetBundleData.h"
#include "AssetRegistry/AssetData.h"

#include "../../../ApplicationCore/Public/GenericPlatform/ICursor.h"
#include "../../../ApplicationCore/Public/GenericPlatform/IInputInterface.h"

#endif

#if !CPP      //noexport class

/// @cond DOXYGEN_IGNORE

/**
 * Determines case sensitivity options for string comparisons. 
 * @note Mirrored from Engine\Source\Runtime\Core\Public\Containers\UnrealString.h
 */
UENUM()
namespace ESearchCase
{
	enum Type : int
	{
		CaseSensitive,
		IgnoreCase,
	};
}

/**
 * Determines search direction for string operations.
 * @note Mirrored from Engine\Source\Runtime\Core\Public\Containers\UnrealString.h
 */
UENUM()
namespace ESearchDir
{
	enum Type : int
	{
		FromStart,
		FromEnd,
	};
}

/**
 * Enum that defines how the log times are to be displayed.
 * @note Mirrored from Engine\Source\Runtime\Core\Public\Misc\OutputDevice.h
 */
UENUM()
namespace ELogTimes
{
	enum Type : int
	{
		/** Do not display log timestamps. */
		None UMETA(DisplayName = "None"),

		/** Display log timestamps in UTC. */
		UTC UMETA(DisplayName = "UTC"),

		/** Display log timestamps in seconds elapsed since GStartTime. */
		SinceGStartTime UMETA(DisplayName = "Time since application start"),

		/** Display log timestamps in local time. */
		Local UMETA(DisplayName = "Local time"),
	};
}

/** Generic axis enum (mirrored for native use in Axis.h). */
UENUM(BlueprintType, meta=(ScriptName="AxisType"))
namespace EAxis
{
	enum Type : int
	{
		None,
		X,
		Y,
		Z
	};
}

/** Generic axis list enum (mirrored for native use in Axis.h). */
UENUM()
namespace EAxisList
{
	enum Type : int
	{
		None = 0,
		X = 1,
		Y = 2,
		Z = 4,

		Screen = 8,
		XY = X | Y,
		XZ = X | Z,
		YZ = Y | Z,
		XYZ = X | Y | Z,
		All = XYZ | Screen,

		/** alias over Axis YZ since it isn't used when the z-rotation widget is being used */
		ZRotation = YZ,

		/** alias over Screen since it isn't used when the 2d translate rotate widget is being used */
		Rotate2D = Screen,
	};
}

/** Describes shape of an interpolation curve (mirrored from InterpCurvePoint.h). */
UENUM()
enum EInterpCurveMode : int
{
	/** A straight line between two keypoint values. */
	CIM_Linear UMETA(DisplayName="Linear"),
	
	/** A cubic-hermite curve between two keypoints, using Arrive/Leave tangents. These tangents will be automatically
		updated when points are moved, etc.  Tangents are unclamped and will plateau at curve start and end points. */
	CIM_CurveAuto UMETA(DisplayName="Curve Auto"),
	
	/** The out value is held constant until the next key, then will jump to that value. */
	CIM_Constant UMETA(DisplayName="Constant"),
	
	/** A smooth curve just like CIM_Curve, but tangents are not automatically updated so you can have manual control over them (eg. in Curve Editor). */
	CIM_CurveUser UMETA(DisplayName="Curve User"),
	
	/** A curve like CIM_Curve, but the arrive and leave tangents are not forced to be the same, so you can create a 'corner' at this key. */
	CIM_CurveBreak UMETA(DisplayName="Curve Break"),
	
	/** A cubic-hermite curve between two keypoints, using Arrive/Leave tangents. These tangents will be automatically
	    updated when points are moved, etc.  Tangents are clamped and will plateau at curve start and end points. */
	CIM_CurveAutoClamped UMETA(DisplayName="Curve Auto Clamped"),
};

/**
 * Describes the format of a each pixel in a graphics buffer.
 * @warning: When you update this, you must add an entry to GPixelFormats(see RenderUtils.cpp)
 * @warning: When you update this, you must add an entries to PixelFormat.h, usually just copy the generated section on the header into EPixelFormat
 * @warning: The *Tools DLLs will also need to be recompiled if the ordering is changed, but should not need code changes.
 */
UENUM()
enum EPixelFormat : int
{
	PF_Unknown,
	PF_A32B32G32R32F,
	/** UNORM (0..1), corresponds to FColor.  Unpacks as rgba in the shader. */
	PF_B8G8R8A8,
	/** UNORM red (0..1) */
	PF_G8,
	PF_G16,
	PF_DXT1,
	PF_DXT3,
	PF_DXT5,
	PF_UYVY,
	/** Same as PF_FloatR11G11B10 */
	PF_FloatRGB,
	/** RGBA 16 bit signed FP format.  Use FFloat16Color on the CPU. */
	PF_FloatRGBA,
	/** A depth+stencil format with platform-specific implementation, for use with render targets. */
	PF_DepthStencil,
	/** A depth format with platform-specific implementation, for use with render targets. */
	PF_ShadowDepth,
	PF_R32_FLOAT,
	PF_G16R16,
	PF_G16R16F,
	PF_G16R16F_FILTER,
	PF_G32R32F,
	PF_A2B10G10R10,
	PF_A16B16G16R16,
	PF_D24,
	PF_R16F,
	PF_R16F_FILTER,
	PF_BC5,
	/** SNORM red, green (-1..1). Not supported on all RHI e.g. Metal */
	PF_V8U8,
	PF_A1,
	/** A low precision floating point format, unsigned.  Use FFloat3Packed on the CPU. */
	PF_FloatR11G11B10,
	PF_A8,
	PF_R32_UINT,
	PF_R32_SINT,
	PF_PVRTC2,
	PF_PVRTC4,
	PF_R16_UINT,
	PF_R16_SINT,
	PF_R16G16B16A16_UINT,
	PF_R16G16B16A16_SINT,
	PF_R5G6B5_UNORM,
	PF_R8G8B8A8,
	/** Only used for legacy loading; do NOT use! */
	PF_A8R8G8B8,
	/** High precision single channel block compressed, equivalent to a single channel BC5, 8 bytes per 4x4 block. */
	PF_BC4,
	/** UNORM red, green (0..1). */
	PF_R8G8,
	/** ATITC format. */
	PF_ATC_RGB,
	/** ATITC format. */
	PF_ATC_RGBA_E,
	/** ATITC format. */
	PF_ATC_RGBA_I,
	/** Used for creating SRVs to alias a DepthStencil buffer to read Stencil.  Don't use for creating textures. */
	PF_X24_G8,
	PF_ETC1,
	PF_ETC2_RGB,
	PF_ETC2_RGBA,
	PF_R32G32B32A32_UINT,
	PF_R16G16_UINT,
	/** 8.00 bpp */
	PF_ASTC_4x4,
	/** 3.56 bpp */
	PF_ASTC_6x6,
	/** 2.00 bpp */
	PF_ASTC_8x8,
	/** 1.28 bpp */
	PF_ASTC_10x10,
	/** 0.89 bpp */
	PF_ASTC_12x12,
	PF_BC6H,
	PF_BC7,
	PF_R8_UINT,
	PF_L8,
	PF_XGXR8,
	PF_R8G8B8A8_UINT,
	/** SNORM (-1..1), corresponds to FFixedRGBASigned8. */
	PF_R8G8B8A8_SNORM,
	PF_R16G16B16A16_UNORM,
	PF_R16G16B16A16_SNORM,
	PF_PLATFORM_HDR_0,
	PF_PLATFORM_HDR_1,
	PF_PLATFORM_HDR_2,
	PF_NV12,
	PF_R32G32_UINT,
	PF_ETC2_R11_EAC,
	PF_ETC2_RG11_EAC,
	PF_R8,
	PF_B5G5R5A1_UNORM,
	PF_ASTC_4x4_HDR,	
	PF_ASTC_6x6_HDR,	
	PF_ASTC_8x8_HDR,	
	PF_ASTC_10x10_HDR,	
	PF_ASTC_12x12_HDR,
	PF_G16R16_SNORM,
	PF_R8G8_UINT,
	PF_R32G32B32_UINT,
	PF_R32G32B32_SINT,
	PF_R32G32B32F,
	PF_R8_SINT,
	PF_R64_UINT,
	PF_R9G9B9EXP5,
	PF_P010,
	PF_ASTC_4x4_NORM_RG,
	PF_ASTC_6x6_NORM_RG,
	PF_ASTC_8x8_NORM_RG,
	PF_ASTC_10x10_NORM_RG,
	PF_ASTC_12x12_NORM_RG,
	PF_MAX,
};

/** Mouse cursor types (mirrored from ICursor.h) */
UENUM()
namespace EMouseCursor
{
	enum Type : int
	{
		/** Causes no mouse cursor to be visible. */
		None,

		/** Default cursor (arrow). */
		Default,

		/** Text edit beam. */
		TextEditBeam,

		/** Resize horizontal. */
		ResizeLeftRight,

		/** Resize vertical. */
		ResizeUpDown,

		/** Resize diagonal. */
		ResizeSouthEast,

		/** Resize other diagonal. */
		ResizeSouthWest,

		/** MoveItem. */
		CardinalCross,

		/** Target Cross. */
		Crosshairs,

		/** Hand cursor. */
		Hand,

		/** Grab Hand cursor. */
		GrabHand,

		/** Grab Hand cursor closed. */
		GrabHandClosed,

		/** a circle with a diagonal line through it. */
		SlashedCircle,

		/** Eye-dropper cursor for picking colors. */
		EyeDropper,
	};
}

/** A set of numerical unit types supported by the engine. Mirrored from UnitConversion.h */
UENUM(BlueprintType)
enum class EUnit : uint8
{
	/** Scalar distance/length unit. */
	Micrometers, Millimeters, Centimeters, Meters, Kilometers,
	Inches, Feet, Yards, Miles,
	Lightyears,

	/** Angular unit. */
	Degrees, Radians,

	/** Speed unit. */
	CentimetersPerSecond, MetersPerSecond, KilometersPerHour, MilesPerHour,

	/** Angular speed unit. */
	DegreesPerSecond, RadiansPerSecond,

	/** Acceleration unit. */
	CentimetersPerSecondSquared, MetersPerSecondSquared,

	/** Temperature unit. */
	Celsius, Farenheit, Kelvin,

	/** Mass unit. */
	Micrograms, Milligrams, Grams, Kilograms, MetricTons,
	Ounces, Pounds, Stones,

	/** Density unit. */
	GramsPerCubicCentimeter, GramsPerCubicMeter, KilogramsPerCubicCentimeter, KilogramsPerCubicMeter,

	/** Force unit. */
	Newtons, PoundsForce, KilogramsForce, KilogramCentimetersPerSecondSquared,

	/** Torque unit. */
	NewtonMeters, KilogramCentimetersSquaredPerSecondSquared,

	/** Impulse unit. */
	NewtonSeconds, KilogramCentimeters, KilogramMeters,

	/** Frequency unit. */
	Hertz, Kilohertz, Megahertz, Gigahertz, RevolutionsPerMinute,

	/** Data Size unit. */
	Bytes, Kilobytes, Megabytes, Gigabytes, Terabytes,

	/** Luminous flux unit. */
	Lumens,
	
	/** Luminous intensity unit. */
	Candela,
	
	/** Illuminance unit. */
	Lux,
	
	/** Luminance unit. */
	CandelaPerMeter2,
	
	/** Exposure value unit. */
	ExposureValue,

	/** Time unit. */
	Nanoseconds, Microseconds, Milliseconds, Seconds, Minutes, Hours, Days, Months, Years,

	/** Pixel density unit. */
	PixelsPerInch,

	/** Percentage. */
	Percentage,

	/** Arbitrary multiplier. */
	Multiplier,

	/** Stress unit. */
	Pascals, KiloPascals, MegaPascals, GigaPascals,

	/** Symbolic entry, not specifiable on meta data. */
	Unspecified
};

/**
 * Enum controlling when to emit property change notifications when setting a property value.
 * @note Mirrored from PropertyAccessUtil.h
 */
UENUM(BlueprintType)
enum class EPropertyAccessChangeNotifyMode : uint8
{
	/** Notify only when a value change has actually occurred */
	Default,
	/** Never notify that a value change has occurred */
	Never,
	/** Always notify that a value change has occurred, even if the value is unchanged */
	Always,
};

/**
 * Enumerates supported message dialog category types.
 * @note Mirrored from GenericPlatformMisc.h
 */
UENUM(BlueprintType)
enum class EAppMsgCategory : uint8
{
	Warning,
	Error,
	Success,
	Info,
};

/**
* Enum denoting message dialog return types.
* @note Mirrored from GenericPlatformMisc.h
*/
UENUM(BlueprintType)
namespace EAppReturnType
{
	enum Type : int
	{
		No,
		Yes,
		YesAll,
		NoAll,
		Cancel,
		Ok,
		Retry,
		Continue,
	};
}

/**
* Enum denoting message dialog button choices. Used in combination with EAppReturnType.
* @note Mirrored from GenericPlatformMisc.h
*/
UENUM(BlueprintType)
namespace EAppMsgType
{
	/**
	 * Enumerates supported message dialog button types.
	 */
	enum Type : int
	{
		Ok,
		YesNo,
		OkCancel,
		YesNoCancel,
		CancelRetryContinue,
		YesNoYesAllNoAll,
		YesNoYesAllNoAllCancel,
		YesNoYesAll,
	};
}

/**
 * A struct used as stub for deleted ones. 
 */
USTRUCT(noexport, IsAlwaysAccessible, HasDefaults)
struct FFallbackStruct
{
};

/** A globally unique identifier (mirrored from Guid.h) */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults)
struct FGuid
{
	UPROPERTY(EditAnywhere, SaveGame, Category=Guid)
	int32 A;

	UPROPERTY(EditAnywhere, SaveGame, Category=Guid)
	int32 B;

	UPROPERTY(EditAnywhere, SaveGame, Category=Guid)
	int32 C;

	UPROPERTY(EditAnywhere, SaveGame, Category=Guid)
	int32 D;
};

/**
 * A point or direction FVector in 3d space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector.h
 */
USTRUCT(immutable, noexport, BlueprintType, BlueprintInternalUseOnly, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta = (ScriptDefaultMake, ScriptDefaultBreak, HasNativeMake = "/Script/Engine.KismetMathLibrary.MakeVector", HasNativeBreak = "/Script/Engine.KismetMathLibrary.BreakVector"))
struct FVector3f
{
	UPROPERTY(EditAnywhere, Category = Vector, SaveGame)
	float X;

	UPROPERTY(EditAnywhere, Category = Vector, SaveGame)
	float Y;

	UPROPERTY(EditAnywhere, Category = Vector, SaveGame)
	float Z;
};

/**
 * A point or direction FVector in 3d space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FVector3d
{
	UPROPERTY(EditAnywhere, Category = Vector, SaveGame)
	double X;

	UPROPERTY(EditAnywhere, Category = Vector, SaveGame)
	double Y;

	UPROPERTY(EditAnywhere, Category = Vector, SaveGame)
	double Z;
};

/**
 * A point or direction FVector in 3d space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta = (HasNativeMake = "/Script/Engine.KismetMathLibrary.MakeVector", HasNativeBreak = "/Script/Engine.KismetMathLibrary.BreakVector"))
struct FVector
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vector, SaveGame)
	FLargeWorldCoordinatesReal X;		//~ Alias for float/double depending on LWC status. Note: Will be refactored to double before UE5 ships.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vector, SaveGame)
	FLargeWorldCoordinatesReal Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Vector, SaveGame)
	FLargeWorldCoordinatesReal Z;
};


/**
* A 4-D homogeneous vector.
* @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector4.h
*/
USTRUCT(immutable, noexport, BlueprintType, BlueprintInternalUseOnly, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta = (ScriptDefaultMake, ScriptDefaultBreak, HasNativeMake = "/Script/Engine.KismetMathLibrary.MakeVector4", HasNativeBreak = "/Script/Engine.KismetMathLibrary.BreakVector4"))
struct FVector4f
{
	UPROPERTY(EditAnywhere, Category = Vector4, SaveGame)
	float X;

	UPROPERTY(EditAnywhere, Category = Vector4, SaveGame)
	float Y;

	UPROPERTY(EditAnywhere, Category = Vector4, SaveGame)
	float Z;

	UPROPERTY(EditAnywhere, Category = Vector4, SaveGame)
	float W;
};

/**
* A 4-D homogeneous vector.
* @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector4.h
*/
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FVector4d
{
	UPROPERTY(EditAnywhere, Category = Vector4, SaveGame)
	double X;

	UPROPERTY(EditAnywhere, Category = Vector4, SaveGame)
	double Y;

	UPROPERTY(EditAnywhere, Category = Vector4, SaveGame)
	double Z;

	UPROPERTY(EditAnywhere, Category = Vector4, SaveGame)
	double W;
};


/**
* A 4-D homogeneous vector.
* @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector4.h
*/
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta = (ScriptDefaultMake, ScriptDefaultBreak, HasNativeMake = "/Script/Engine.KismetMathLibrary.MakeVector4", HasNativeBreak = "/Script/Engine.KismetMathLibrary.BreakVector4"))
struct FVector4
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector4, SaveGame)
	FLargeWorldCoordinatesReal X;		//~ Alias for float/double depending on LWC status. Note: Will be refactored to double before UE5 ships.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector4, SaveGame)
	FLargeWorldCoordinatesReal Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector4, SaveGame)
	FLargeWorldCoordinatesReal Z;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector4, SaveGame)
	FLargeWorldCoordinatesReal W;
};


/**
* A vector in 2-D space composed of components (X, Y) with floating point precision.
* @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector2D.h
*/
USTRUCT(immutable, noexport, BlueprintType, BlueprintInternalUseOnly, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta = (ScriptDefaultMake, ScriptDefaultBreak, HasNativeMake="/Script/Engine.KismetMathLibrary.MakeVector2D", HasNativeMake = "/Script/Engine.KismetMathLibrary.MakeVector2D", HasNativeBreak = "/Script/Engine.KismetMathLibrary.BreakVector2D"))
struct FVector2f
{
	UPROPERTY(EditAnywhere, Category=Vector2D, SaveGame)
	float X;

	UPROPERTY(EditAnywhere, Category=Vector2D, SaveGame)
	float Y;
};

/**
* A vector in 2-D space composed of components (X, Y) with floating point precision.
* @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector2D.h
*/
// LWC_TODO: CRITICAL! Name collision in UHT with FVector2D due to case insensitive FNames!
// USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
// struct FVector2d
// {
// 	UPROPERTY(EditAnywhere, Category=Vector2D, SaveGame)
// 	double X;
//
// 	UPROPERTY(EditAnywhere, Category=Vector2D, SaveGame)
// 	double Y;
// };

/**
 * A vector in 2-D space composed of components (X, Y) with floating point precision.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector2D.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta=(HasNativeMake="/Script/Engine.KismetMathLibrary.MakeVector2D", HasNativeBreak="/Script/Engine.KismetMathLibrary.BreakVector2D"))
struct FVector2D
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector2D, SaveGame)
	FLargeWorldCoordinatesReal X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector2D, SaveGame)
	FLargeWorldCoordinatesReal Y;
};

/** A pair of 3D vectors (mirrored from TwoVectors.h). */
USTRUCT(immutable, BlueprintType, noexport, IsAlwaysAccessible, HasDefaults)
struct FTwoVectors
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TwoVectors, SaveGame)
	FVector v1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TwoVectors, SaveGame)
	FVector v2;
};

/**
 * A plane definition in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Plane.h
 */
USTRUCT(immutable, noexport, BlueprintType, BlueprintInternalUseOnly, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FPlane4f : public FVector3f
{
	UPROPERTY(EditAnywhere, Category=Plane, SaveGame)
	float W;
};

/**
 * A plane definition in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Plane.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FPlane4d : public FVector3d
{
	UPROPERTY(EditAnywhere, Category = Plane, SaveGame)
	double W;
};

/**
 * A plane definition in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Plane.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FPlane : public FVector
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Plane, SaveGame)
	FLargeWorldCoordinatesReal W;
};



/**
 * 3D Ray represented by Origin and (normalized) Direction.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Ray.h
 * @note FRay3f is not currently exposed as a Blueprint type
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FRay3f
{
	UPROPERTY(EditAnywhere, Category = Ray, SaveGame)
	FVector3f Origin;

	UPROPERTY(EditAnywhere, Category = Ray, SaveGame)
	FVector3f Direction;
};

/**
 * 3D Ray represented by Origin and (normalized) Direction.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Ray.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FRay3d
{
	UPROPERTY(EditAnywhere, Category = Ray, SaveGame)
	FVector3d Origin;

	UPROPERTY(EditAnywhere, Category = Ray, SaveGame)
	FVector3d Direction;
};

/**
 * 3D Ray represented by Origin and (normalized) Direction.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Ray.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FRay
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ray, SaveGame)
	FVector Origin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Ray, SaveGame)
	FVector Direction;
};



/**
 * An orthogonal rotation in 3d space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Rotator.h
 */
USTRUCT(immutable, noexport, BlueprintType, BlueprintInternalUseOnly, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta = (ScriptDefaultMake, ScriptDefaultBreak, HasNativeMake = "/Script/Engine.KismetMathLibrary.MakeRotator", HasNativeBreak = "/Script/Engine.KismetMathLibrary.BreakRotator"))
struct FRotator3f
{
	/** Pitch (degrees) around Y axis */
	UPROPERTY(EditAnywhere, Category=Rotator, SaveGame, meta=(DisplayName="Y"))
	float Pitch;

	/** Yaw (degrees) around Z axis */
	UPROPERTY(EditAnywhere, Category=Rotator, SaveGame, meta=(DisplayName="Z"))
	float Yaw;

	/** Roll (degrees) around X axis */
	UPROPERTY(EditAnywhere, Category=Rotator, SaveGame, meta=(DisplayName="X"))
	float Roll;
};

/**
 * An orthogonal rotation in 3d space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Rotator.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FRotator3d
{
	/** Pitch (degrees) around Y axis */
	UPROPERTY(EditAnywhere, Category=Rotator, SaveGame, meta=(DisplayName="Y"))
	double Pitch;

	/** Yaw (degrees) around Z axis */
	UPROPERTY(EditAnywhere, Category=Rotator, SaveGame, meta=(DisplayName="Z"))
	double Yaw;

	/** Roll (degrees) around X axis */
	UPROPERTY(EditAnywhere, Category=Rotator, SaveGame, meta=(DisplayName="X"))
	double Roll;
};

/**
 * An orthogonal rotation in 3d space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Rotator.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta=(HasNativeMake="/Script/Engine.KismetMathLibrary.MakeRotator", HasNativeBreak="/Script/Engine.KismetMathLibrary.BreakRotator"))
struct FRotator
{
	/** Pitch (degrees) around Y axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Rotator, SaveGame, meta=(DisplayName="Y"))
	FLargeWorldCoordinatesReal Pitch;

	/** Yaw (degrees) around Z axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Rotator, SaveGame, meta=(DisplayName="Z"))
	FLargeWorldCoordinatesReal Yaw;

	/** Roll (degrees) around X axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Rotator, SaveGame, meta=(DisplayName="X"))
	FLargeWorldCoordinatesReal Roll;
};



/**
 * 3D Sphere represented by Center and Radius.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Sphere.h
 * @note FSphere3f is not currently exposed as a Blueprint type
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FSphere3f
{
	UPROPERTY(EditAnywhere, Category = Sphere, SaveGame)
	FVector3f Center;

	UPROPERTY(EditAnywhere, Category = Sphere, SaveGame, meta = (DisplayName = "Radius"))
	float W;
};
/**
 * 3D Sphere represented by Center and Radius.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Sphere.h
 * @note FSphere3d is not currently exposed as a Blueprint type
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FSphere3d
{
	UPROPERTY(EditAnywhere, Category = Sphere, SaveGame)
	FVector3d Center;

	UPROPERTY(EditAnywhere, Category = Sphere, SaveGame, meta = (DisplayName = "Radius"))
	double W;
};
/**
 * 3D Sphere represented by Center and Radius.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Sphere.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FSphere
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sphere, SaveGame)
	FVector Center;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Sphere, SaveGame, meta = (DisplayName = "Radius"))
	FLargeWorldCoordinatesReal W;
};



/**
 * Quaternion.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Quat.h
 */
USTRUCT(immutable, noexport, BlueprintType, BlueprintInternalUseOnly, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta = (ScriptDefaultMake, ScriptDefaultBreak, HasNativeMake = "/Script/Engine.KismetMathLibrary.MakeQuat", HasNativeBreak = "/Script/Engine.KismetMathLibrary.BreakQuat"))
struct FQuat4f
{
	UPROPERTY(EditAnywhere, Category=Quat, SaveGame)
	float X;

	UPROPERTY(EditAnywhere, Category=Quat, SaveGame)
	float Y;

	UPROPERTY(EditAnywhere, Category=Quat, SaveGame)
	float Z;

	UPROPERTY(EditAnywhere, Category=Quat, SaveGame)
	float W;

};


/**
 * Quaternion.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Quat.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FQuat4d
{
	UPROPERTY(EditAnywhere, Category = Quat, SaveGame)
	double X;

	UPROPERTY(EditAnywhere, Category = Quat, SaveGame)
	double Y;

	UPROPERTY(EditAnywhere, Category = Quat, SaveGame)
	double Z;

	UPROPERTY(EditAnywhere, Category = Quat, SaveGame)
	double W;

};


/**
 * Quaternion.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Quat.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta=(HasNativeMake ="/Script/Engine.KismetMathLibrary.MakeQuat", HasNativeBreak="/Script/Engine.KismetMathLibrary.BreakQuat"))
struct FQuat
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Quat, SaveGame)
	FLargeWorldCoordinatesReal X;		//~ Alias for float/double depending on LWC status. Note: Will be refactored to double before UE5 ships.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Quat, SaveGame)
	FLargeWorldCoordinatesReal Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Quat, SaveGame)
	FLargeWorldCoordinatesReal Z;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Quat, SaveGame)
	FLargeWorldCoordinatesReal W;
};


/**
 * A packed normal.
 * @note The full C++ class is located here: Engine\Source\Runtime\RenderCore\Public\PackedNormal.h
 */
USTRUCT(immutable, noexport)
struct FPackedNormal
{
	UPROPERTY(EditAnywhere, Category=PackedNormal, SaveGame)
	uint8 X;

	UPROPERTY(EditAnywhere, Category=PackedNormal, SaveGame)
	uint8 Y;

	UPROPERTY(EditAnywhere, Category=PackedNormal, SaveGame)
	uint8 Z;

	UPROPERTY(EditAnywhere, Category=PackedNormal, SaveGame)
	uint8 W;

};

/**
 * A packed basis vector.
 * @note The full C++ class is located here: Engine\Source\Runtime\RenderCore\Public\PackedNormal.h
 */
USTRUCT(immutable, noexport)
struct FPackedRGB10A2N
{
	UPROPERTY(EditAnywhere, Category = PackedBasis, SaveGame)
	int32 Packed;
};

/**
 * A packed vector.
 * @note The full C++ class is located here: Engine\Source\Runtime\RenderCore\Public\PackedNormal.h
 */
USTRUCT(immutable, noexport)
struct FPackedRGBA16N
{
	UPROPERTY(EditAnywhere, Category = PackedNormal, SaveGame)
	int32 XY;

	UPROPERTY(EditAnywhere, Category = PackedNormal, SaveGame)
	int32 ZW;
};

/**
 * Screen coordinates.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntPoint.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FIntPoint
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IntPoint, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IntPoint, SaveGame)
	int32 Y;
};

USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FInt32Point
{
	UPROPERTY(EditAnywhere, Category = IntPoint, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, Category = IntPoint, SaveGame)
	int32 Y;
};

USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FInt64Point
{
	UPROPERTY(EditAnywhere, Category=IntPoint, SaveGame)
	int64 X;

	UPROPERTY(EditAnywhere, Category=IntPoint, SaveGame)
	int64 Y;
};

/**
 * Screen coordinates.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntPoint.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUintPoint
{
	UPROPERTY(EditAnywhere, Category = IntPoint, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, Category = IntPoint, SaveGame)
	int32 Y;
};

USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUint32Point
{
	UPROPERTY(EditAnywhere, Category = IntPoint, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, Category = IntPoint, SaveGame)
	int32 Y;
};

USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUint64Point
{
	UPROPERTY(EditAnywhere, Category = IntPoint, SaveGame)
	int64 X;

	UPROPERTY(EditAnywhere, Category = IntPoint, SaveGame)
	int64 Y;
};

/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FInt32Vector2
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int32 Y;
};

/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FInt64Vector2
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int64 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int64 Y;
};

/**
 * An integer vector in 4D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FIntVector2
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int32 Y;
};

/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUint32Vector2
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint32 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint32 Y;
};

/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUint64Vector2
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint64 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint64 Y;
};

/**
 * An integer vector in 4D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUintVector2
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint32 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint32 Y;
};


/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FInt32Vector
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int32 Y;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int32 Z;
};

/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FInt64Vector
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int64 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int64 Y;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int64 Z;
};

/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FIntVector
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IntVector, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IntVector, SaveGame)
	int32 Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IntVector, SaveGame)
	int32 Z;
};


/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUint32Vector
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint32 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint32 Y;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint32 Z;
};

/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUint64Vector
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint64 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint64 Y;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint64 Z;
};

/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUintVector
{
	UPROPERTY(EditAnywhere, Category=IntVector, SaveGame)
	uint32 X;

	UPROPERTY(EditAnywhere, Category=IntVector, SaveGame)
	uint32 Y;

	UPROPERTY(EditAnywhere, Category=IntVector, SaveGame)
	uint32 Z;
};


/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FInt32Vector4
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int32 Y;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int32 Z;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int32 W;
};

/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FInt64Vector4
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int64 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int64 Y;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int64 Z;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	int64 W;
};

/**
 * An integer vector in 4D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FIntVector4
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IntVector4, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IntVector4, SaveGame)
	int32 Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IntVector4, SaveGame)
	int32 Z;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = IntVector4, SaveGame)
	int32 W;
};

/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUint32Vector4
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint32 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint32 Y;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint32 Z;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint32 W;
};

/**
 * An integer vector in 3D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUint64Vector4
{
	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint64 X;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint64 Y;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint64 Z;

	UPROPERTY(EditAnywhere, Category = IntVector, SaveGame)
	uint64 W;
};

/**
 * An integer vector in 4D space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FUintVector4
{
	UPROPERTY(EditAnywhere, Category = IntVector4, SaveGame)
	uint32 X;

	UPROPERTY(EditAnywhere, Category = IntVector4, SaveGame)
	uint32 Y;

	UPROPERTY(EditAnywhere, Category = IntVector4, SaveGame)
	uint32 Z;

	UPROPERTY(EditAnywhere, Category = IntVector4, SaveGame)
	uint32 W;
};


/**
 * Stores a color with 8 bits of precision per channel. (BGRA).
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Color.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor)
struct FColor
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Color, SaveGame, meta=(ClampMin="0", ClampMax="255"))
	uint8 B;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Color, SaveGame, meta=(ClampMin="0", ClampMax="255"))
	uint8 G;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Color, SaveGame, meta=(ClampMin="0", ClampMax="255"))
	uint8 R;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Color, SaveGame, meta=(ClampMin="0", ClampMax="255"))
	uint8 A;

};

/**
 * A linear, 32-bit/component floating point RGBA color.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Color.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor)
struct FLinearColor
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LinearColor, SaveGame)
	float R;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LinearColor, SaveGame)
	float G;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LinearColor, SaveGame)
	float B;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LinearColor, SaveGame)
	float A;

};

/**
 * A point or direction FVector in 3d space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Box.h
 */
USTRUCT(immutable, noexport, BlueprintType, BlueprintInternalUseOnly, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FBox3f
{
	UPROPERTY(EditAnywhere, Category = Box, SaveGame, meta=(EditCondition="IsValid"))
	FVector3f Min;

	UPROPERTY(EditAnywhere, Category = Box, SaveGame, meta=(EditCondition="IsValid"))
	FVector3f Max;

	UPROPERTY(EditAnywhere, Category = Box, SaveGame, meta=(ScriptName="IsValid"))
	bool IsValid;
};


/**
 * A point or direction FVector in 3d space.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Box.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FBox3d
{
	UPROPERTY(EditAnywhere, Category = Box, SaveGame, meta=(EditCondition="IsValid"))
	FVector3d Min;

	UPROPERTY(EditAnywhere, Category = Box, SaveGame, meta=(EditCondition="IsValid"))
	FVector3d Max;

	UPROPERTY(EditAnywhere, Category = Box, SaveGame, meta=(ScriptName="IsValid"))
	bool IsValid;
};

/**
 * A bounding box.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Box.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta=(HasNativeMake="/Script/Engine.KismetMathLibrary.MakeBox"))
struct FBox
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Box, SaveGame, meta=(EditCondition="IsValid"))
	FVector Min;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Box, SaveGame, meta=(EditCondition="IsValid"))
	FVector Max;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Box, SaveGame, meta=(ScriptName="IsValid"))
	bool IsValid;
};

/**
 * A rectangular 2D Box.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Box2D.h
 */
USTRUCT(immutable, noexport, BlueprintType, BlueprintInternalUseOnly, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FBox2f
{
	UPROPERTY(EditAnywhere, Category=Box2D, SaveGame, meta=(EditCondition="bIsValid"))
	FVector2f Min;

	UPROPERTY(EditAnywhere, Category=Box2D, SaveGame, meta=(EditCondition="bIsValid"))
	FVector2f Max;

	UPROPERTY(EditAnywhere, Category=Box2D, SaveGame, meta=(ScriptName="bIsValid"))
	bool bIsValid;
};

/**
* A rectangular 2D Box.
* @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Box2D.h
*/
// LWC_TODO: CRITICAL! Name collision in UHT with FBox2D due to case insensitive FNames!
// USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
// struct FBox2d
// {
// 	UPROPERTY(EditAnywhere, Category=Box2D, SaveGame, meta=(EditCondition="bIsValid"))
// 	FVector2d Min;
//
// 	UPROPERTY(EditAnywhere, Category=Box2D, SaveGame, meta=(EditCondition="bIsValid"))
// 	FVector2d Max;
//
// 	UPROPERTY(EditAnywhere, Category=Box2D, SaveGame, meta=(ScriptName="bIsValid"))
// 	bool bIsValid;
// };

/**
* A rectangular 2D Box.
* @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Box2D.h
*/
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta=(HasNativeMake="/Script/Engine.KismetMathLibrary.MakeBox2D"))
struct FBox2D
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Box2D, SaveGame, meta=(EditCondition="bIsValid"))
	FVector2D Min;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Box2D, SaveGame, meta=(EditCondition="bIsValid"))
	FVector2D Max;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Box2D, SaveGame, meta=(ScriptName="bIsValid"))
	bool bIsValid;
};

/**
 * A bounding box and bounding sphere with the same origin.
 * @note The full C++ class is located here : Engine\Source\Runtime\Core\Public\Math\BoxSphereBounds.h
 */
USTRUCT(noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FBoxSphereBounds3f
{
	/** Holds the origin of the bounding box and sphere. */
	UPROPERTY(EditAnywhere, Category = BoxSphereBounds, SaveGame)
	FVector3f Origin;

	/** Holds the extent of the bounding box, which is half the size of the box in 3D space */
	UPROPERTY(EditAnywhere, Category = BoxSphereBounds, SaveGame)
	FVector3f BoxExtent;

	/** Holds the radius of the bounding sphere. */
	UPROPERTY(EditAnywhere, Category = BoxSphereBounds, SaveGame)
	float SphereRadius;
};

/**
 * A bounding box and bounding sphere with the same origin.
 * @note The full C++ class is located here : Engine\Source\Runtime\Core\Public\Math\BoxSphereBounds.h
 */
USTRUCT(noexport, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FBoxSphereBounds3d
{
	/** Holds the origin of the bounding box and sphere. */
	UPROPERTY(EditAnywhere, Category = BoxSphereBounds, SaveGame)
	FVector3d Origin;

	/** Holds the extent of the bounding box, which is half the size of the box in 3D space */
	UPROPERTY(EditAnywhere, Category = BoxSphereBounds, SaveGame)
	FVector3d BoxExtent;

	/** Holds the radius of the bounding sphere. */
	UPROPERTY(EditAnywhere, Category = BoxSphereBounds, SaveGame)
	double SphereRadius;
};

/**
 * A bounding box and bounding sphere with the same origin.
 * @note The full C++ class is located here : Engine\Source\Runtime\Core\Public\Math\BoxSphereBounds.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, IsCoreType, meta = (HasNativeMake = "/Script/Engine.KismetMathLibrary.MakeBoxSphereBounds", HasNativeBreak = "/Script/Engine.KismetMathLibrary.BreakBoxSphereBounds"))
struct FBoxSphereBounds
{
	/** Holds the origin of the bounding box and sphere. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BoxSphereBounds, SaveGame)
	FVector Origin;

	/** Holds the extent of the bounding box, which is half the size of the box in 3D space */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BoxSphereBounds, SaveGame)
	FVector BoxExtent;

	/** Holds the radius of the bounding sphere. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BoxSphereBounds, SaveGame)
	FLargeWorldCoordinatesReal SphereRadius;
};

/**
 * Structure for arbitrarily oriented boxes (i.e. not necessarily axis-aligned).
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\OrientedBox.h
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults)
struct FOrientedBox
{
	/** Holds the center of the box. */
	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	FVector Center;

	/** Holds the x-axis vector of the box. Must be a unit vector. */
	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	FVector AxisX;
	
	/** Holds the y-axis vector of the box. Must be a unit vector. */
	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	FVector AxisY;
	
	/** Holds the z-axis vector of the box. Must be a unit vector. */
	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	FVector AxisZ;

	/** Holds the extent of the box along its x-axis. */
	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	FLargeWorldCoordinatesReal ExtentX;
	
	/** Holds the extent of the box along its y-axis. */
	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	FLargeWorldCoordinatesReal ExtentY;

	/** Holds the extent of the box along its z-axis. */
	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	FLargeWorldCoordinatesReal ExtentZ;
};

/**
 * A 4x4 matrix.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Matrix.h
 */
USTRUCT(immutable, noexport, BlueprintType, BlueprintInternalUseOnly, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FMatrix44f
{
	UPROPERTY(EditAnywhere, Category=Matrix, SaveGame)
	FPlane4f XPlane;

	UPROPERTY(EditAnywhere, Category=Matrix, SaveGame)
	FPlane4f YPlane;

	UPROPERTY(EditAnywhere, Category=Matrix, SaveGame)
	FPlane4f ZPlane;

	UPROPERTY(EditAnywhere, Category=Matrix, SaveGame)
	FPlane4f WPlane;

};

/**
 * A 4x4 matrix.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Matrix.h
 */
USTRUCT(immutable, noexport, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FMatrix44d
{
	UPROPERTY(EditAnywhere, Category = Matrix, SaveGame)
	FPlane4d XPlane;

	UPROPERTY(EditAnywhere, Category = Matrix, SaveGame)
	FPlane4d YPlane;

	UPROPERTY(EditAnywhere, Category = Matrix, SaveGame)
	FPlane4d ZPlane;

	UPROPERTY(EditAnywhere, Category = Matrix, SaveGame)
	FPlane4d WPlane;

};

/**
 * A 4x4 matrix.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Matrix.h
 */
USTRUCT(immutable, noexport, BlueprintType, HasDefaults, HasNoOpConstructor, IsCoreType)
struct FMatrix
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Matrix, SaveGame)
	FPlane XPlane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Matrix, SaveGame)
	FPlane YPlane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Matrix, SaveGame)
	FPlane ZPlane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Matrix, SaveGame)
	FPlane WPlane;

};

/** 
 * Describes one specific point on an interpolation curve.
 * @note This is a mirror of TInterpCurvePoint<float>, defined in InterpCurvePoint.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor)
struct FInterpCurvePointFloat
{
	/** Float input value that corresponds to this key (eg. time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointFloat)
	float InVal;

	/** Float output value type when input is equal to InVal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointFloat)
	float OutVal;

	/** Tangent of curve arriving at this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointFloat)
	float ArriveTangent;

	/** Tangent of curve leaving this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointFloat)
	float LeaveTangent;

	/** Interpolation mode between this point and the next one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointFloat)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;

};

/** 
 * Describes an entire curve that is used to compute a float output value from a float input.
 * @note This is a mirror of TInterpCurve<float>, defined in InterpCurve.h
 */
USTRUCT(noexport, BlueprintType)
struct FInterpCurveFloat
{
	/** Holds the collection of interpolation points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveFloat)
	TArray<FInterpCurvePointFloat> Points;

	/** Specify whether the curve is looped or not */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveFloat)
	bool bIsLooped;

	/** Specify the offset from the last point's input key corresponding to the loop point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveFloat)
	float LoopKeyOffset;
};

/** 
 * Describes one specific point on an interpolation curve.
 * @note This is a mirror of TInterpCurvePoint<FVector2D>, defined in InterpCurvePoint.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor)
struct FInterpCurvePointVector2D
{
	/** Float input value that corresponds to this key (eg. time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector2D)
	float InVal;

	/** 2D vector output value of when input is equal to InVal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector2D)
	FVector2D OutVal;

	/** Tangent of curve arriving at this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector2D)
	FVector2D ArriveTangent;

	/** Tangent of curve leaving this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector2D)
	FVector2D LeaveTangent;

	/** Interpolation mode between this point and the next one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector2D)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;
};

/** 
 * Describes an entire curve that is used to compute a 2D vector output value from a float input.
 * @note This is a mirror of TInterpCurve<FVector2D>, defined in InterpCurve.h
 */
USTRUCT(noexport, BlueprintType)
struct FInterpCurveVector2D
{
	/** Holds the collection of interpolation points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector2D)
	TArray<FInterpCurvePointVector2D> Points;

	/** Specify whether the curve is looped or not */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector2D)
	bool bIsLooped;

	/** Specify the offset from the last point's input key corresponding to the loop point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector2D)
	float LoopKeyOffset;
};

/** 
 * Describes one specific point on an interpolation curve.
 * @note This is a mirror of TInterpCurvePoint<FVector>, defined in InterpCurvePoint.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor)
struct FInterpCurvePointVector
{
	/** Float input value that corresponds to this key (eg. time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector)
	float InVal;

	/** 3D vector output value of when input is equal to InVal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector)
	FVector OutVal;

	/** Tangent of curve arriving at this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector)
	FVector ArriveTangent;

	/** Tangent of curve leaving this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector)
	FVector LeaveTangent;

	/** Interpolation mode between this point and the next one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;
};

/** 
 * Describes an entire curve that is used to compute a 3D vector output value from a float input.
 * @note This is a mirror of TInterpCurve<FVector>, defined in InterpCurve.h
 */
USTRUCT(noexport, BlueprintType)
struct FInterpCurveVector
{
	/** Holds the collection of interpolation points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector)
	TArray<FInterpCurvePointVector> Points;

	/** Specify whether the curve is looped or not */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector)
	bool bIsLooped;

	/** Specify the offset from the last point's input key corresponding to the loop point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector)
	float LoopKeyOffset;
};

/**
 * Describes one specific point on an interpolation curve.
 * @note This is a mirror of TInterpCurvePoint<FQuat>, defined in InterpCurvePoint.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor)
struct FInterpCurvePointQuat
{
	/** Float input value that corresponds to this key (eg. time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointQuat)
	float InVal;

	/** Quaternion output value of when input is equal to InVal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointQuat)
	FQuat OutVal;

	/** Tangent of curve arriving at this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointQuat)
	FQuat ArriveTangent;

	/** Tangent of curve leaving this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointQuat)
	FQuat LeaveTangent;

	/** Interpolation mode between this point and the next one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointQuat)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;
};

/**
 * Describes an entire curve that is used to compute a quaternion output value from a float input.
 * @note This is a mirror of TInterpCurve<FQuat>, defined in InterpCurve.h
 */
USTRUCT(noexport, BlueprintType)
struct FInterpCurveQuat
{
	/** Holds the collection of interpolation points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveQuat)
	TArray<FInterpCurvePointQuat> Points;

	/** Specify whether the curve is looped or not */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveQuat)
	bool bIsLooped;

	/** Specify the offset from the last point's input key corresponding to the loop point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveQuat)
	float LoopKeyOffset;
};

/**
 * Describes one specific point on an interpolation curve.
 * @note This is a mirror of TInterpCurvePoint<FTwoVectors>, defined in InterpCurvePoint.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor)
struct FInterpCurvePointTwoVectors
{
	/** Float input value that corresponds to this key (eg. time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointTwoVectors)
	float InVal;

	/** Two 3D vectors output value of when input is equal to InVal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointTwoVectors)
	FTwoVectors OutVal;

	/** Tangent of curve arriving at this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointTwoVectors)
	FTwoVectors ArriveTangent;

	/** Tangent of curve leaving this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointTwoVectors)
	FTwoVectors LeaveTangent;

	/** Interpolation mode between this point and the next one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointTwoVectors)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;
};

/**
 * Describes an entire curve that is used to compute two 3D vector values from a float input.
 * @note This is a mirror of TInterpCurve<FTwoVectors>, defined in InterpCurve.h
 */
USTRUCT(noexport, BlueprintType)
struct FInterpCurveTwoVectors
{
	/** Holds the collection of interpolation points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveTwoVectors)
	TArray<FInterpCurvePointTwoVectors> Points;

	/** Specify whether the curve is looped or not */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveTwoVectors)
	bool bIsLooped;

	/** Specify the offset from the last point's input key corresponding to the loop point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveTwoVectors)
	float LoopKeyOffset;
};

/**
 * Describes one specific point on an interpolation curve.
 * @note This is a mirror of TInterpCurvePoint<FLinearColor>, defined in InterpCurvePoint.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor)
struct FInterpCurvePointLinearColor
{
	/** Float input value that corresponds to this key (eg. time). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointLinearColor)
	float InVal;

	/** Color output value of when input is equal to InVal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointLinearColor)
	FLinearColor OutVal;

	/** Tangent of curve arriving at this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointLinearColor)
	FLinearColor ArriveTangent;

	/** Tangent of curve leaving this point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointLinearColor)
	FLinearColor LeaveTangent;

	/** Interpolation mode between this point and the next one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointLinearColor)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;
};

/**
 * Describes an entire curve that is used to compute a color output value from a float input.
 * @note This is a mirror of TInterpCurve<FLinearColor>, defined in InterpCurve.h
 */
USTRUCT(noexport, BlueprintType)
struct FInterpCurveLinearColor
{
	/** Holds the collection of interpolation points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveLinearColor)
	TArray<FInterpCurvePointLinearColor> Points;

	/** Specify whether the curve is looped or not */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveLinearColor)
	bool bIsLooped;

	/** Specify the offset from the last point's input key corresponding to the loop point */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveLinearColor)
	float LoopKeyOffset;
};

/**
 * Transform composed of Quat/Translation/Scale.
 * @note This is implemented in either TransformVectorized.h or TransformNonVectorized.h depending on the platform.
 */
USTRUCT(immutable, noexport, BlueprintType, BlueprintInternalUseOnly, IsAlwaysAccessible, HasDefaults, IsCoreType, meta = (ScriptDefaultMake, ScriptDefaultBreak, HasNativeMake = "/Script/Engine.KismetMathLibrary.MakeTransform", HasNativeBreak = "/Script/Engine.KismetMathLibrary.BreakTransform"))
struct FTransform3f
{
	/** Rotation of this transformation, as a quaternion. */
	UPROPERTY(EditAnywhere, Category = Transform, SaveGame)
	FQuat4f Rotation;

	/** Translation of this transformation, as a vector. */
	UPROPERTY(EditAnywhere, Category = Transform, SaveGame)
	FVector3f Translation;

	/** 3D scale (always applied in local space) as a vector. */
	UPROPERTY(EditAnywhere, Category = Transform, SaveGame)
	FVector3f Scale3D;
};

/**
 * Transform composed of Quat/Translation/Scale.
 * @note This is implemented in either TransformVectorized.h or TransformNonVectorized.h depending on the platform.
 */
USTRUCT(immutable, noexport, IsAlwaysAccessible, HasDefaults, IsCoreType)
struct FTransform3d
{
	/** Rotation of this transformation, as a quaternion. */
	UPROPERTY(EditAnywhere, Category = Transform, SaveGame)
	FQuat4d Rotation;

	/** Translation of this transformation, as a vector. */
	UPROPERTY(EditAnywhere, Category = Transform, SaveGame)
	FVector3d Translation;

	/** 3D scale (always applied in local space) as a vector. */
	UPROPERTY(EditAnywhere, Category = Transform, SaveGame)
	FVector3d Scale3D;
};

/**
 * Transform composed of Quat/Translation/Scale.
 * @note This is implemented in either TransformVectorized.h or TransformNonVectorized.h depending on the platform.
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, IsCoreType, meta=(HasNativeMake="/Script/Engine.KismetMathLibrary.MakeTransform", HasNativeBreak="/Script/Engine.KismetMathLibrary.BreakTransform"))
struct FTransform
{
	/** Rotation of this transformation, as a quaternion. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Transform, SaveGame)
	FQuat Rotation;

	/** Translation of this transformation, as a vector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Transform, SaveGame)
	FVector Translation;

	/** 3D scale (always applied in local space) as a vector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Transform, SaveGame)
	FVector Scale3D;
};

/**
 * Thread-safe random number generator that can be manually seeded.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\RandomStream.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, HasNoOpConstructor, meta = (HasNativeMake = "/Script/Engine.KismetMathLibrary.MakeRandomStream", HasNativeBreak = "/Script/Engine.KismetMathLibrary.BreakRandomStream"))
struct FRandomStream
{
public:
	/** Holds the initial seed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RandomStream, SaveGame)
	int32 InitialSeed;
	
	/** Holds the current seed. */
	UPROPERTY()
	int32 Seed;
};

/** 
 * A value representing a specific point date and time over a wide range of years.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Misc\DateTime.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, meta=(HasNativeMake="/Script/Engine.KismetMathLibrary.MakeDateTime", HasNativeBreak="/Script/Engine.KismetMathLibrary.BreakDateTime"))
struct FDateTime
{
	int64 Ticks;
};

/** 
 * A frame number value, representing discrete frames since the start of timing.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Misc\FrameNumber.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults)
struct FFrameNumber
{
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=FrameNumber)
	int32 Value;
};

/** 
 * A frame rate represented as a fraction comprising 2 integers: a numerator (number of frames), and a denominator (per second).
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Misc\FrameRate.h
 */
USTRUCT(noexport, BlueprintType, meta=(HasNativeMake="/Script/Engine.KismetMathLibrary.MakeFrameRate", HasNativeBreak="/Script/Engine.KismetMathLibrary.BreakFrameRate"))
struct FFrameRate
{
	/** The numerator of the framerate represented as a number of frames per second (e.g. 60 for 60 fps) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=FrameRate)
	int32 Numerator;

	/** The denominator of the framerate represented as a number of frames per second (e.g. 1 for 60 fps) */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category=FrameRate)
	int32 Denominator;
};

/** 
 * Represents a time by a context-free frame number, plus a sub frame value in the range [0:1). 
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Misc\FrameTime.h
 * @note The 'SubFrame' field is private to match its C++ class declaration in the header above.
 */
USTRUCT(noexport, BlueprintType)
struct FFrameTime
{
	/** Count of frames from start of timing */
	UPROPERTY(BlueprintReadWrite, Category=FrameTime)
	FFrameNumber FrameNumber;
	
private:
	/** Time within a frame, always between >= 0 and < 1 */
	UPROPERTY(BlueprintReadWrite, Category=FrameTime, meta=(AllowPrivateAccess="true"))
	float SubFrame;
};

/** 
 * A frame time qualified by a frame rate context.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Misc\QualifiedFrameTime.h
 */
USTRUCT(noexport, BlueprintType, meta=(ScriptName="QualifiedTime", HasNativeMake="/Script/Engine.KismetMathLibrary.MakeQualifiedFrameTime", HasNativeBreak="/Script/Engine.KismetMathLibrary.BreakQualifiedFrameTime"))
struct FQualifiedFrameTime
{
	/** The frame time */
	UPROPERTY(BlueprintReadWrite, Category=QualifiedFrameTime)
	FFrameTime Time;

	/** The rate that this frame time is in */
	UPROPERTY(BlueprintReadWrite, Category=QualifiedFrameTime)
	FFrameRate Rate;
};

/** 
 * A timecode that stores time in HH:MM:SS format with the remainder of time represented by an integer frame count. 
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Misc\TimeCode.h
 */
USTRUCT(noexport, BlueprintType)
struct FTimecode
{
	UPROPERTY(BlueprintReadWrite, Category=Timecode)
	int32 Hours;

	UPROPERTY(BlueprintReadWrite, Category=Timecode)
	int32 Minutes;

	UPROPERTY(BlueprintReadWrite, Category=Timecode)
	int32 Seconds;

	UPROPERTY(BlueprintReadWrite, Category=Timecode)
	int32 Frames;

	/** If true, this Timecode represents a Drop Frame timecode used to account for fractional frame rates in NTSC play rates. */
	UPROPERTY(BlueprintReadWrite, Category= Timecode)
	bool bDropFrameFormat;
};

/** 
 * A time span value, which is the difference between two dates and times.
 * @note The full C++ class is located here: Engine\Source\Runtime\Core\Public\Misc\Timespan.h
 */
USTRUCT(immutable, noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, meta=(HasNativeMake="/Script/Engine.KismetMathLibrary.MakeTimespan", HasNativeBreak="/Script/Engine.KismetMathLibrary.BreakTimespan"))
struct FTimespan
{
	int64 Ticks;
};

/**
 * A struct that can reference a top level asset such as '/Path/To/Package.AssetName'
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\TopLevelAssetPath.h
 */
USTRUCT(noexport, BlueprintType, meta = (HasNativeMake = "/Script/Engine.KismetSystemLibrary.MakeTopLevelAssetPath", HasNativeBreak = "/Script/Engine.KismetSystemLibrary.BreakTopLevelAssetPath"))
struct FTopLevelAssetPath
{
private:
	/** Name of the package containing the asset e.g. /Path/To/Package */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadWrite, Category = TopLevelAssetPath, meta = (AllowPrivateAccess = "true"))
	FName PackageName;
	/** Name of the asset within the package e.g. 'AssetName' */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadWrite, Category = TopLevelAssetPath, meta = (AllowPrivateAccess = "true"))
	FName AssetName;
};

/** 
 * A struct that contains a string reference to an object, either a top level asset or a subobject.
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\SoftObjectPath.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, meta=(HasNativeMake="/Script/Engine.KismetSystemLibrary.MakeSoftObjectPath", HasNativeBreak="/Script/Engine.KismetSystemLibrary.BreakSoftObjectPath"))
struct FSoftObjectPath
{
	/** Asset path, patch to a top level object in a package */
	UPROPERTY()
	FTopLevelAssetPath AssetPath;

	/** Optional FString for subobject within an asset */
	UPROPERTY()
	FString SubPathString;
};

/** 
 * A struct that contains a string reference to a class, can be used to make soft references to classes.
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\SoftObjectPath.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, meta=(HasNativeMake="/Script/Engine.KismetSystemLibrary.MakeSoftClassPath", HasNativeBreak="/Script/Engine.KismetSystemLibrary.BreakSoftClassPath"))
struct FSoftClassPath : public FSoftObjectPath
{
};

/** 
 * A type of primary asset, used by the Asset Manager system.
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\PrimaryAssetId.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults)
struct FPrimaryAssetType
{
	/** The Type of this object, by default its base class's name */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadWrite, Category = PrimaryAssetType)
	FName Name;
};

/** 
 * This identifies an object as a "primary" asset that can be searched for by the AssetManager and used in various tools
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\PrimaryAssetId.h
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults)
struct FPrimaryAssetId
{
	/** The Type of this object, by default its base class's name */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadWrite, Category = PrimaryAssetId)
	FPrimaryAssetType PrimaryAssetType;

	/** The Name of this object, by default its short name */
	UPROPERTY(EditAnywhere, SaveGame, BlueprintReadWrite, Category = PrimaryAssetId)
	FName PrimaryAssetName;
};

/** Enumerates the valid types of range bounds (mirrored from RangeBound.h) */
UENUM(BlueprintType)
namespace ERangeBoundTypes
{
	enum Type : int
	{
		/**
		* The range excludes the bound.
		*/
		Exclusive,

		/**
		* The range includes the bound.
		*/
		Inclusive,

		/**
		* The bound is open.
		*/
		Open
	};
}

/**
 * Defines a single bound for a range of values.
 * @note This is a mirror of TRangeBound<float>, defined in RangeBound.h
 * @note Fields are private to match the C++ declaration in the header above.
 */
USTRUCT(noexport, BlueprintType)
struct FFloatRangeBound
{
private:
	/** Holds the type of the bound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	TEnumAsByte<ERangeBoundTypes::Type> Type;

	/** Holds the bound's value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	float Value;
};

/**
 * A contiguous set of floats described by lower and upper bound values.
 * @note This is a mirror of TRange<float>, defined in Range.h
 * @note Fields are private to match the C++ declaration in the header above.
 */
USTRUCT(noexport, BlueprintType)
struct FFloatRange
{
private:
	/** Holds the range's lower bound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	FFloatRangeBound LowerBound;

	/** Holds the range's upper bound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	FFloatRangeBound UpperBound;
};


/**
 * Defines a single bound for a range of values.
 * @note This is a mirror of TRangeBound<double>, defined in RangeBound.h
 * @note Fields are private to match the C++ declaration in the header above.
 */
USTRUCT(noexport, BlueprintType)
struct FDoubleRangeBound
{
private:
	/** Holds the type of the bound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	TEnumAsByte<ERangeBoundTypes::Type> Type;

	/** Holds the bound's value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	double Value;
};

/**
 * A contiguous set of doubles described by lower and upper bound values.
 * @note This is a mirror of TRange<double>, defined in Range.h
 * @note Fields are private to match the C++ declaration in the header above.
 */
USTRUCT(noexport, BlueprintType)
struct FDoubleRange
{
private:
	/** Holds the range's lower bound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	FDoubleRangeBound LowerBound;

	/** Holds the range's upper bound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	FDoubleRangeBound UpperBound;
};

/**
 * Defines a single bound for a range of values.
 * @note This is a mirror of TRangeBound<int32>, defined in RangeBound.h
 * @note Fields are private to match the C++ declaration in the header above.
 */
USTRUCT(noexport, BlueprintType)
struct FInt32RangeBound
{
private:
	/** Holds the type of the bound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	TEnumAsByte<ERangeBoundTypes::Type> Type;

	/** Holds the bound's value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	int32 Value;
};

/**
 * A contiguous set of floats described by lower and upper bound values.
 * @note This is a mirror of TRange<int32>, defined in Range.h
 * @note Fields are private to match the C++ declaration in the header above.
 */
USTRUCT(noexport, BlueprintType)
struct FInt32Range
{
private:
	/** Holds the range's lower bound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	FInt32RangeBound LowerBound;

	/** Holds the range's upper bound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	FInt32RangeBound UpperBound;
};

/**
 * Defines a single bound for a range of frame numbers.
 * @note This is a mirror of TRangeBound<FFrameNumber>, defined in RangeBound.h
 * @note Fields are private to match the C++ declaration in the header above.
 */
USTRUCT(noexport, BlueprintType)
struct FFrameNumberRangeBound
{
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	TEnumAsByte<ERangeBoundTypes::Type> Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	FFrameNumber Value;
};

/**
 * A contiguous set of frame numbers described by lower and upper bound values.
 * @note This is a mirror of TRange<FFrameNumber>, defined in Range.h
 * @note Fields are private to match the C++ declaration in the header above.
 */
USTRUCT(noexport, BlueprintType)
struct FFrameNumberRange
{
private:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	FFrameNumberRangeBound LowerBound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range, meta=(AllowPrivateAccess="true"))
	FFrameNumberRangeBound UpperBound;
};

/**
 * An interval of floats, defined by inclusive min and max values
 * @note This is a mirror of TInterval<float>, defined in Interval.h
 */
USTRUCT(noexport, BlueprintType)
struct FFloatInterval
{
	/** Values must be >= Min */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Interval)
	float Min;

	/** Values must be <= Max */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Interval)
	float Max;
};

/**
 * An interval of integers, defined by inclusive min and max values
 * @note This is a mirror of TInterval<int32>, defined in Interval.h
 */
USTRUCT(noexport, BlueprintType)
struct FInt32Interval
{
	/** Values must be >= Min */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Interval)
	int32 Min;

	/** Values must be <= Max */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Interval)
	int32 Max;
};

/** Categories of localized text (mirrored in LocalizedTextSourceTypes.h */
UENUM(BlueprintType)
enum class ELocalizedTextSourceCategory : uint8
{
	Game,
	Engine,
	Editor,
};

/**
 * Polyglot data that may be registered to the text localization manager at runtime.
 * @note This struct is mirrored in PolyglotTextData.h
 */
USTRUCT(noexport, BlueprintType)
struct FPolyglotTextData
{
	/**
	 * The category of this polyglot data.
	 * @note This affects when and how the data is loaded into the text localization manager.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PolyglotData)
	ELocalizedTextSourceCategory Category;

	/**
	 * The native culture of this polyglot data.
	 * @note This may be empty, and if empty, will be inferred from the native culture of the text category.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PolyglotData)
	FString NativeCulture;

	/**
	 * The namespace of the text created from this polyglot data.
	 * @note This may be empty.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PolyglotData)
	FString Namespace;

	/**
	 * The key of the text created from this polyglot data.
	 * @note This must not be empty.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PolyglotData)
	FString Key;

	/**
	 * The native string for this polyglot data.
	 * @note This must not be empty (it should be the same as the originally authored text you are trying to replace).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PolyglotData)
	FString NativeString;

	/**
	 * Mapping between a culture code and its localized string.
	 * @note The native culture may also have a translation in this map.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PolyglotData)
	TMap<FString, FString> LocalizedStrings;

	/**
	 * True if this polyglot data is a minimal patch, and that missing translations should be
	 * ignored (falling back to any LocRes data) rather than falling back to the native string.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=PolyglotData)
	bool bIsMinimalPatch;

	/**
	 * Transient cached text instance from registering this polyglot data with the text localization manager.
	 */
	UPROPERTY(Transient)
	FText CachedText;
};

/** Report level of automation events (mirrored in AutomationEvent.h). */
UENUM()
enum class EAutomationEventType : uint8
{
	Info,
	Warning,
	Error
};

/** Event emitted by automation system (mirrored in AutomationEvent.h). */
USTRUCT(noexport)
struct FAutomationEvent
{
	UPROPERTY()
	EAutomationEventType Type;

	UPROPERTY()
	FString Message;

	UPROPERTY()
	FString Context;

	UPROPERTY()
	FGuid Artifact;
};

/** Information about the execution of an automation task (mirrored in AutomationEvent.h). */
USTRUCT(noexport)
struct FAutomationExecutionEntry
{
	UPROPERTY()
	FAutomationEvent Event;

	UPROPERTY()
	FString Filename;

	UPROPERTY()
	int32 LineNumber;

	UPROPERTY()
	FDateTime Timestamp;
};


/**
 * Represents a single input device such as a gamepad, keyboard, or mouse.
 *
 * Has a globally unique identifier.
 * 
 * Opaque struct for the FInputDeviceId struct defined in CoreMiscDefines.h
 */
USTRUCT(noexport, BlueprintType)
struct FInputDeviceId
{
	GENERATED_BODY()
private:
	
	UPROPERTY(VisibleAnywhere, Category = "PlatformInputDevice")
	int32 InternalId = -1;
};

/**
 * Handle that defines a local user on this platform.
 * This used to be just a typedef int32 that was used interchangeably as ControllerId and LocalUserIndex.
 * Moving forward these will be allocated by the platform application layer.
 *
 * Opaque struct for the FPlatformUserId struct defined in CoreMiscDefines.h
 */
USTRUCT(noexport, BlueprintType)
struct FPlatformUserId
{
	GENERATED_BODY()
private:
	
	UPROPERTY(VisibleAnywhere, Category = "PlatformInputDevice")
	int32 InternalId = -1;
};

/**
 * Represents the connection status of a given FInputDeviceId
 */
UENUM(BlueprintType)
enum class EInputDeviceConnectionState : uint8
{
	/** This is not a valid input device */
	Invalid,

	/** It is not known if this device is connected or not */
	Unknown,

	/** Device is definitely connected */
	Disconnected,

	/** Definitely connected and powered on */
	Connected
};

/**
 * Represents input device triggers that are available
 *
 * NOTE: Make sure to keep this type in sync with the reflected version in IInputInterface.h!
 */
UENUM(BlueprintType)
enum class EInputDeviceTriggerMask : uint8
{
	None		= 0x00,
	Left		= 0x01,
	Right		= 0x02,
	All			= Left | Right
};

/**
 * Data about an input device's current state
 */
USTRUCT(noexport, BlueprintType)
struct FPlatformInputDeviceState
{
	GENERATED_BODY()

	/** The platform user that this input device belongs to */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "PlatformInputDevice")
	FPlatformUserId OwningPlatformUser = PLATFORMUSERID_NONE;

	/** The connection state of this input device */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category = "PlatformInputDevice")
	EInputDeviceConnectionState ConnectionState = EInputDeviceConnectionState::Invalid;
};

/** Enum used by DataValidation plugin to see if an asset has been validated for correctness (mirrored in UObjectGlobals.h)*/
UENUM(BlueprintType)
enum class EDataValidationResult : uint8
{
	/** Asset has failed validation */
	Invalid,
	/** Asset has passed validation */
	Valid,
	/** Asset has not yet been validated */
	NotValidated
};

/**
 * A struct to serve as a filter for Asset Registry queries. (mirrored in ARFilter.h)
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults, meta = (HasNativeMake = "/Script/Engine.KismetSystemLibrary.MakeARFilter", HasNativeBreak = "/Script/Engine.KismetSystemLibrary.BreakARFilter"))
struct FARFilter
{
	/** The filter component for package names */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AssetRegistry)
	TArray<FName> PackageNames;

	/** The filter component for package paths */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AssetRegistry)
	TArray<FName> PackagePaths;

#if WITH_EDITORONLY_DATA
	/** The filter component containing specific object paths. Deprecated. */
	UPROPERTY()
	TArray<FName> ObjectPaths;
#endif

	/** The filter component containing specific object paths */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AssetRegistry)
	TArray<FSoftObjectPath> SoftObjectPaths;

	/** [DEPRECATED] - Class names are now represented by path names. Please use ClassPaths instead. */
	UPROPERTY(BlueprintReadWrite, Category = AssetRegistry, meta=(DeprecatedProperty, DeprecationMessage="Short asset class names must be converted to full asset pathnames. Use ClassPaths instead."))
	TArray<FName> ClassNames;

	/** The filter component for class path names. Instances of the specified classes, but not subclasses (by default), will be included. Derived classes will be included only if bRecursiveClasses is true. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AssetRegistry)
	TArray<FTopLevelAssetPath> ClassPaths;

	/** The filter component for properties marked with the AssetRegistrySearchable flag */
	TMultiMap<FName, TOptional<FString>> TagsAndValues;

	/** [DEPRECATED] - Class names are now represented by path names. Please use RecursiveClassPathsExclusionSet instead. */
	UPROPERTY(BlueprintReadWrite, Category = AssetRegistry, meta=(DeprecatedProperty, DeprecationMessage="Short asset class names must be converted to full asset pathnames. Use RecursiveClassPathsExclusionSet instead."))
	TSet<FName> RecursiveClassesExclusionSet;

	/** Only if bRecursiveClasses is true, the results will exclude classes (and subclasses) in this list */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AssetRegistry)
	TSet<FTopLevelAssetPath> RecursiveClassPathsExclusionSet;

	/** If true, PackagePath components will be recursive */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AssetRegistry)
	bool bRecursivePaths = false;

	/** If true, subclasses of ClassNames will also be included and RecursiveClassesExclusionSet will be excluded. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AssetRegistry)
	bool bRecursiveClasses = false;

	/** If true, only on-disk assets will be returned. Be warned that this is rarely what you want and should only be used for performance reasons */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = AssetRegistry)
	bool bIncludeOnlyOnDiskAssets = false;

	/** The exclusive filter component for package flags. Only assets without any of the specified flags will be returned. */
	uint32 WithoutPackageFlags = 0;

	/** The inclusive filter component for package flags. Only assets with all of the specified flags will be returned. */
	uint32 WithPackageFlags = 0;
};

USTRUCT(noexport)
struct FAssetBundleEntry
{
	/** Specific name of this bundle */
	UPROPERTY()
	FName BundleName;

#if WITH_EDITORONLY_DATA
	/** List of string assets contained in this bundle */
	UPROPERTY(meta=(DeprecatedProperty, DeprecationMessage="Asset bundles may only contain top level asset paths which are referenced through the AssetPaths property."))
	TArray<FSoftObjectPath> BundleAssets;
#endif

	UPROPERTY()
	TArray<FTopLevelAssetPath> AssetPaths;
};

/** A struct with a list of asset bundle entries. If one of these is inside a UObject it will get automatically exported as the asset registry tag AssetBundleData */
USTRUCT(noexport, IsAlwaysAccessible, HasDefaults)
struct FAssetBundleData
{
	/** List of bundles defined */
	UPROPERTY()
	TArray<FAssetBundleEntry> Bundles;
};

/**
 * A struct to hold important information about an assets found by the Asset Registry
 * This struct is transient and should never be serialized
 */
USTRUCT(noexport, BlueprintType, IsAlwaysAccessible, HasDefaults)
struct FAssetData
{
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FName ObjectPath;
#endif
	/** The name of the package in which the asset is found, this is the full long package name such as /Game/Path/Package */
	UPROPERTY(BlueprintReadOnly, Category = AssetData, transient)
	FName PackageName;
	/** The path to the package in which the asset is found, this is /Game/Path with the Package stripped off */
	UPROPERTY(BlueprintReadOnly, Category = AssetData, transient)
	FName PackagePath;
	/** The name of the asset without the package */
	UPROPERTY(BlueprintReadOnly, Category = AssetData, transient)
	FName AssetName;
	/** The name of the asset's class */
	UPROPERTY(BlueprintReadOnly, Category = AssetData, transient, meta=(DeprecatedProperty, DeprecationMessage="Short asset class name must be converted to full asset pathname. Use AssetClassPath instead."))
	FName AssetClass;
	/** The path name of the asset's class */
	UPROPERTY(BlueprintReadOnly, Category = AssetData, transient)
	FTopLevelAssetPath AssetClassPath;

	/** Asset package flags */
	uint32 PackageFlags = 0;
	
#if WITH_EDITORONLY_DATA
	/** If the outer of this object is not PackageName, it is the object specified by this path. Not exposed to blueprints except through 'To Soft Object Path'. */
	FName OptionalOuterPath;
#endif

	/** The map of values for properties that were marked AssetRegistrySearchable or added by GetAssetRegistryTags */
	FAssetDataTagMapSharedView TagsAndValues;
	TSharedPtr<FAssetBundleData, ESPMode::ThreadSafe> TaggedAssetBundles;
	/** The IDs of the pakchunks this asset is located in for streaming install.  Empty if not assigned to a chunk */
	TArray<int32, TInlineAllocator<2>> ChunkIDs;
};

USTRUCT(noexport, IsAlwaysAccessible, HasDefaults)
struct FTestUninitializedScriptStructMembersTest
{
	UPROPERTY(Transient)
	TObjectPtr<UObject> UninitializedObjectReference;

	UPROPERTY(Transient)
	TObjectPtr<UObject> InitializedObjectReference;

	UPROPERTY(Transient)
	float UnusedValue;
};

USTRUCT(noexport, IsAlwaysAccessible, HasDefaults)
struct FTestUndeclaredScriptStructObjectReferencesTest
{
	UPROPERTY(Transient)
	TObjectPtr<UObject> StrongObjectPointer;

	UPROPERTY(Transient)
	TSoftObjectPtr<UObject> SoftObjectPointer;

	UPROPERTY(Transient)
	FSoftObjectPath SoftObjectPath;
	
	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> WeakObjectPointer;
};

/**
 * Direct base class for all UE objects
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\Object.h
 */
UCLASS(abstract, noexport, MatchedSerializers)
class UObject
{
	GENERATED_BODY()
public:

	UObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	UObject(FVTableHelper& Helper);
	~UObject();
	
	/**
	 * Executes some portion of the ubergraph.
	 *
	 * @param	EntryPoint	The entry point to start code execution at.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(BlueprintInternalUseOnly = "true"))
	void ExecuteUbergraph(int32 EntryPoint);
};

/**
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\Class.h
 */

UCLASS(abstract, noexport, intrinsic, Config = Engine)
class UField : public UObject
{
	GENERATED_BODY()
public:
	UField(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UEnum : public UField
{
	GENERATED_BODY()
public:
	UEnum(const FObjectInitializer& ObjectInitialzer);
};

UCLASS(noexport, intrinsic, MatchedSerializers, Config = Engine)
class UStruct : public UField
{
	GENERATED_BODY()
public:
	UStruct(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, MatchedSerializers, Config = Engine)
class UScriptStruct : public UStruct
{
	GENERATED_BODY()
public:
	UScriptStruct(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UFunction : public UStruct
{
	GENERATED_BODY()
public:
	UFunction(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, within=Package, Config = Engine)
class UClass : public UStruct
{
	GENERATED_BODY()
public:
	UClass(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\Interface.h
 */

UCLASS(abstract, noexport, intrinsic, interface, Config = Engine)
class UInterface : public UObject
{
	GENERATED_BODY()
public:
	UInterface(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

class IInterface
{
	GENERATED_BODY()
};


/**
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\Package.h
 */

UCLASS(noexport, intrinsic, Config = Engine)
class UPackage : public UObject
{
	GENERATED_BODY()
};

/**
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\CoreNet.h
 */

UCLASS(noexport, intrinsic, abstract, transient, Config = Engine)
class UPackageMap : public UObject
{
	GENERATED_BODY()
public:
	UPackageMap(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\MetaData.h
 */

UCLASS(noexport, intrinsic, MatchedSerializers, Config = Engine)
class UMetaData : public UObject
{
	GENERATED_BODY()
public:
	UMetaData(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\UnrealTypePrivate.h
 */

UCLASS(noexport, intrinsic, abstract, Config = Engine)
class UProperty : public UField
{
	GENERATED_BODY()
public:
	UProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UStructProperty : public UProperty
{
	GENERATED_BODY()
public:
	UStructProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UEnumProperty : public UProperty
{
	GENERATED_BODY()
public:
	UEnumProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UBoolProperty : public UProperty
{
	GENERATED_BODY()
public:
	UBoolProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(abstract, noexport, intrinsic, Config = Engine)
class UNumericProperty : public UProperty
{
	GENERATED_BODY()
public:
	UNumericProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UIntProperty : public UNumericProperty
{
	GENERATED_BODY()
public:
	UIntProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UInt8Property : public UNumericProperty
{
	GENERATED_BODY()
public:
	UInt8Property(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UInt16Property : public UNumericProperty
{
	GENERATED_BODY()
public:
	UInt16Property(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UInt64Property : public UNumericProperty
{
	GENERATED_BODY()
public:
	UInt64Property(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UByteProperty : public UNumericProperty
{
	GENERATED_BODY()
public:
	UByteProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UUint16Property : public UNumericProperty
{
	GENERATED_BODY()
public:
	UUint16Property(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UUint32Property : public UNumericProperty
{
	GENERATED_BODY()
public:
	UUint32Property(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UUint64Property : public UNumericProperty
{
	GENERATED_BODY()
public:
	UUint64Property(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UFloatProperty : public UNumericProperty
{
	GENERATED_BODY()
public:
	UFloatProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UDoubleProperty : public UNumericProperty
{
	GENERATED_BODY()
public:
	UDoubleProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(abstract, noexport, intrinsic, Config = Engine)
class UObjectPropertyBase : public UProperty
{
	GENERATED_BODY()
public:
	UObjectPropertyBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UObjectProperty : public UObjectPropertyBase
{
	GENERATED_BODY()
public:
	UObjectProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UClassProperty : public UObjectProperty
{
	GENERATED_BODY()
public:
	UClassProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class USoftObjectProperty : public UObjectPropertyBase
{
	GENERATED_BODY()
public:
	USoftObjectProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class USoftClassProperty : public USoftObjectProperty
{
	GENERATED_BODY()
public:
	USoftClassProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UWeakObjectProperty : public UObjectPropertyBase
{
	GENERATED_BODY()
public:
	UWeakObjectProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class ULazyObjectProperty : public UObjectPropertyBase
{
	GENERATED_BODY()
public:
	ULazyObjectProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UInterfaceProperty : public UProperty
{
	GENERATED_BODY()
public:
	UInterfaceProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UDelegateProperty : public UProperty
{
	GENERATED_BODY()
public:
	UDelegateProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(abstract, noexport, intrinsic, Config = Engine)
class UMulticastDelegateProperty : public UProperty
{
	GENERATED_BODY()
public:
	UMulticastDelegateProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UMulticastInlineDelegateProperty : public UMulticastDelegateProperty
{
	GENERATED_BODY()
public:
	UMulticastInlineDelegateProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UMulticastSparseDelegateProperty : public UMulticastDelegateProperty
{
	GENERATED_BODY()
public:
	UMulticastSparseDelegateProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UNameProperty : public UProperty
{
	GENERATED_BODY()
public:
	UNameProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UStrProperty : public UProperty
{
	GENERATED_BODY()
public:
	UStrProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UTextProperty : public UProperty
{
	GENERATED_BODY()
public:
	UTextProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UArrayProperty : public UProperty
{
	GENERATED_BODY()
public:
	UArrayProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class USetProperty : public UProperty
{
	GENERATED_BODY()
public:
	USetProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

UCLASS(noexport, intrinsic, Config = Engine)
class UMapProperty : public UProperty
{
	GENERATED_BODY()
public:
	UMapProperty(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};

/**
 * @note The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\SoftObjectPath.h
 */

/** Structure for file paths that are displayed in the editor with a picker UI. */
USTRUCT(noexport, BlueprintType)
struct FFilePath
{
	GENERATED_BODY()

	/**
	* The path to the file.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = FilePath)
	FString FilePath;
};

/** Structure for directory paths that are displayed in the editor with a picker UI. */
USTRUCT(noexport, BlueprintType)
struct FDirectoryPath
{
	GENERATED_BODY()

	/**
	* The path to the directory.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Path)
	FString Path;
};

/** Structure for templated strings that are displayed in the editor with a allowed args. */
USTRUCT(noexport, BlueprintType)
struct FTemplateString
{
	GENERATED_BODY()

	/**
	* The format string.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Template)
	FString Template;
};

/// @endcond

#endif
