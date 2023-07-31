// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Library/DMXImport.h"

#include "DMXImportGDTF.generated.h"

class UDMXGDTFAssetImportData;

class UTexture2D;


UENUM(BlueprintType)
enum class EDMXImportGDTFType : uint8
{
    Multiply,
    Override
};


UENUM(BlueprintType)
enum class EDMXImportGDTFSnap : uint8
{
    Yes,
    No,
    On,
    Off
};

UENUM(BlueprintType)
enum class EDMXImportGDTFMaster : uint8
{
    None,
    Grand,
    Group
};

UENUM(BlueprintType)
enum class EDMXImportGDTFDMXInvert : uint8
{
    Yes,
    No
};

UENUM(BlueprintType)
enum class EDMXImportGDTFLampType : uint8
{
    Discharge,
    Tungsten,
    Halogen,
    LED
};

UENUM(BlueprintType)
enum class EDMXImportGDTFBeamType : uint8
{
    Wash,
    Spot,
    None
};

UENUM(BlueprintType)
enum class EDMXImportGDTFPrimitiveType : uint8
{
    Undefined,
    Cube,
    Cylinder,
    Sphere,
    Base,
    Yoke,
    Head,
    Scanner,
    Conventional,
    Pigtail
};

UENUM(BlueprintType)
enum class EDMXImportGDTFPhysicalUnit : uint8
{
    None,
    Percent,
    Length,
    Mass,
    Time,
    Temperature,
    LuminousIntensity,
    Angle,
    Force,
    Frequency,
    Current,
    Voltage,
    Power,
    Energy,
    Area,
    Volume,
    Speed,
    Acceleration,
    AngularSpeed,
    AngularAccc,
    WaveLength,
    ColorComponent
};

UENUM(BlueprintType)
enum class EDMXImportGDTFMode : uint8
{
    Custom,
    sRGB,
    ProPhoto,
    ANSI
};

UENUM(BlueprintType)
enum class EDMXImportGDTFInterpolationTo : uint8
{
    Linear,
    Step,
    Log
};

USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXImportGDTFActivationGroup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Name;
};

USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXImportGDTFFeature
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Name;
};

USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXImportGDTFFeatureGroup
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FString Pretty;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFFeature> Features;
};

USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXImportGDTFAttribute
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FString Pretty;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFActivationGroup ActivationGroup;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFFeature Feature;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FString MainAttribute;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    EDMXImportGDTFPhysicalUnit PhysicalUnit = EDMXImportGDTFPhysicalUnit::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXColorCIE Color;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFFilter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXColorCIE Color;
};


USTRUCT(BlueprintType)
struct FDMXImportGDTFWheelSlot
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXColorCIE Color;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFFilter Filter;

    UPROPERTY(VisibleAnywhere, Category = "Fixture Type")
    TObjectPtr<UTexture2D> MediaFileName = nullptr;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFWheel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFWheelSlot> Slots;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFMeasurementPoint
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float WaveLength = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float Energy = 0.f;
};


USTRUCT(BlueprintType)
struct FDMXImportGDTFMeasurement
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float Physical = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float LuminousIntensity = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float Transmission = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    EDMXImportGDTFInterpolationTo InterpolationTo = EDMXImportGDTFInterpolationTo::Linear;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFMeasurementPoint> MeasurementPoints;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFEmitter
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXColorCIE Color;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float DominantWaveLength = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FString DiodePart;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFMeasurement Measurement;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFColorSpace
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    EDMXImportGDTFMode Mode = EDMXImportGDTFMode::sRGB;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXColorCIE Red;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXColorCIE Green;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXColorCIE Blue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXColorCIE WhitePoint;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFDMXProfiles
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFCRIs
{
    GENERATED_BODY()
};


USTRUCT(BlueprintType)
struct FDMXImportGDTFModel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float Length = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float Width = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float Height = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    EDMXImportGDTFPrimitiveType PrimitiveType = EDMXImportGDTFPrimitiveType::Undefined;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFGeometryBase
{
	GENERATED_BODY()

	FDMXImportGDTFGeometryBase()
		: Position(FMatrix::Identity)
	{}

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Model;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FMatrix Position;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFBeam
    : public FDMXImportGDTFGeometryBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    EDMXImportGDTFLampType LampType = EDMXImportGDTFLampType::Discharge;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float PowerConsumption = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float LuminousFlux = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float ColorTemperature = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float BeamAngle = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float FieldAngle = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float BeamRadius = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    EDMXImportGDTFBeamType BeamType = EDMXImportGDTFBeamType::Wash;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    uint8 ColorRenderingIndex = 0;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFTypeAxis
    : public FDMXImportGDTFGeometryBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFBeam> Beams;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFGeneralAxis
    : public FDMXImportGDTFGeometryBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFTypeAxis> Axis;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFTypeGeometry
    : public FDMXImportGDTFGeometryBase
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFFilterBeam
    : public FDMXImportGDTFGeometryBase
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFFilterColor
    : public FDMXImportGDTFGeometryBase
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFFilterGobo
    : public FDMXImportGDTFGeometryBase
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFFilterShaper
    : public FDMXImportGDTFGeometryBase
{
    GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFBreak
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    int32 DMXOffset = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    uint8 DMXBreak = 0;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFGeometryReference
    : public FDMXImportGDTFGeometryBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFBreak> Breaks;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFGeneralGeometry
    : public FDMXImportGDTFGeometryBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFGeneralAxis Axis;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFTypeGeometry Geometry;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFFilterBeam FilterBeam;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFFilterColor FilterColor;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFFilterGobo FilterGobo;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFFilterShaper FilterShaper;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFGeometryReference GeometryReference;
};

USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXImportGDTFDMXValue
{
    GENERATED_BODY()

    FDMXImportGDTFDMXValue()
        : Value(0)
        , ValueSize(1)
    {
    }

    FDMXImportGDTFDMXValue(const FString& InDMXValueStr);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    int32 Value;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    uint8 ValueSize;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFChannelSet
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFDMXValue DMXFrom;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float PhysicalFrom = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float PhysicalTo = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    int32 WheelSlotIndex = 0;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFChannelFunction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFAttribute Attribute;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FString OriginalAttribute;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFDMXValue DMXFrom;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFDMXValue DMXValue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float PhysicalFrom = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float PhysicalTo = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float RealFade = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFWheel Wheel;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFEmitter Emitter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFFilter Filter;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    EDMXImportGDTFDMXInvert DMXInvert = EDMXImportGDTFDMXInvert::No;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FString ModeMaster;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFDMXValue ModeFrom;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFDMXValue ModeTo;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFChannelSet> ChannelSets;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFLogicalChannel
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFAttribute Attribute;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    EDMXImportGDTFSnap Snap = EDMXImportGDTFSnap::No;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    EDMXImportGDTFMaster Master = EDMXImportGDTFMaster::None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float MibFade = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    float DMXChangeTimeLimit = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFChannelFunction ChannelFunction;
};

USTRUCT(BlueprintType)
struct DMXRUNTIME_API FDMXImportGDTFDMXChannel
{
    GENERATED_BODY()

	/** Parses the offset of the channel. Returns false if no valid offset is specified */
    bool ParseOffset(const FString& InOffsetStr);

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    int32 DMXBreak = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    TArray<int32> Offset;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFDMXValue Default;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFDMXValue Highlight;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    FName Geometry;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFLogicalChannel LogicalChannel;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFRelation
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    FString Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    FString Master;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    FString Follower;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    EDMXImportGDTFType Type = EDMXImportGDTFType::Multiply;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFFTMacro
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    FName Name;
};

USTRUCT(BlueprintType)
struct FDMXImportGDTFDMXMode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    FName Geometry;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFDMXChannel> DMXChannels;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFRelation> Relations;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFFTMacro> FTMacros;
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportGDTFFixtureType
    : public UDMXImportFixtureType
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fixture Type")
    FName Name;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fixture Type")
    FString ShortName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fixture Type")
    FString LongName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fixture Type")
    FString Manufacturer;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fixture Type")
    FString Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fixture Type")
    FString FixtureTypeID;

    UPROPERTY(VisibleAnywhere, Category = "Fixture Type")
    TObjectPtr<UTexture2D> Thumbnail = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fixture Type")
    FString RefFT;
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportGDTFAttributeDefinitions
    : public UDMXImportAttributeDefinitions
{
    GENERATED_BODY()

public:
    bool FindFeature(const FString& InQuery, FDMXImportGDTFFeature& OutFeature) const;

    bool FindAtributeByName(const FName& InName, FDMXImportGDTFAttribute& OutAttribute) const;

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFActivationGroup> ActivationGroups;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFFeatureGroup> FeatureGroups;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFAttribute> Attributes;
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportGDTFWheels
    : public UDMXImportWheels
{
    GENERATED_BODY()

public:
    bool FindWeelByName(const FName& InName, FDMXImportGDTFWheel& OutWheel) const;

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFWheel> Wheels;
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportGDTFPhysicalDescriptions
    : public UDMXImportPhysicalDescriptions
{
    GENERATED_BODY()

public:
    bool FindEmitterByName(const FName& InName, FDMXImportGDTFEmitter& OutEmitter) const;

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFEmitter> Emitters;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFColorSpace ColorSpace;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFDMXProfiles DMXProfiles;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    FDMXImportGDTFCRIs CRIs;
};


UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportGDTFModels
    : public UDMXImportModels
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFModel> Models;
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportGDTFGeometries
    : public UDMXImportGeometries
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFGeneralGeometry> GeneralGeometry;
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportGDTFDMXModes
    : public UDMXImportDMXModes
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FDMXImportGDTFDMXMode> DMXModes;

public:
	UFUNCTION(BlueprintPure, Category = "DMXGDTF|Import Data")
	TArray<FDMXImportGDTFChannelFunction> GetDMXChannelFunctions(const FDMXImportGDTFDMXMode& InMode);
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportGDTFProtocols
    : public UDMXImportProtocols
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DMX")
    TArray<FName> Protocols;
};

UCLASS(BlueprintType, Blueprintable)
class DMXRUNTIME_API UDMXImportGDTF
	: public UDMXImport
{
    GENERATED_BODY()

public:
	/** Constructor */
	UDMXImportGDTF();

	//~ Begin UObject interface
	virtual void PostLoad() override;
	//~ End UObject interface

	UFUNCTION(BlueprintPure, Category = "DMXGDTF|Import Data")
	UDMXImportGDTFDMXModes* GetDMXModes() const { return Cast<UDMXImportGDTFDMXModes>(DMXModes); }

	/** DEPRECATED 5.1 in favor of AssetImportData */
	UPROPERTY(Meta = (DeprecatedProperty, DeprecationMessage = "Deprecated in favor of GDTFAssetImportData, see UDMXImportGDTF::GetGDTFAssetImportData."))
	FString SourceFilename_DEPRECATED;

#if WITH_EDITORONLY_DATA
	/** Returns GDTF Asset Import Data for this GDTF */
	FORCEINLINE UDMXGDTFAssetImportData* GetGDTFAssetImportData() const { return GDTFAssetImportData; }
#endif 

private:
#if WITH_EDITORONLY_DATA
	/** The Asset Import Data used to generate the GDTF asset or nullptr, if not generated from a GDTF file */
	UPROPERTY()
	TObjectPtr<UDMXGDTFAssetImportData> GDTFAssetImportData;
#endif
};
