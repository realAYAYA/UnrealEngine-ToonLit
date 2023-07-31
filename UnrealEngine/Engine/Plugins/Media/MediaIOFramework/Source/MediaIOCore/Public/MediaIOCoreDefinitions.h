// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameRate.h"
#include "UObject/ObjectMacros.h"
#include "MediaIOCoreDefinitions.generated.h"


/**
 * Media transport type.
 */
UENUM()
enum class EMediaIOTransportType
{
	SingleLink,
	DualLink,
	QuadLink,
	HDMI,
};


/**
 * Quad link transport type.
 */
UENUM()
enum class EMediaIOQuadLinkTransportType
{
	SquareDivision,
	TwoSampleInterleave,
};


/**
 * SDI transport type.
 */
UENUM()
enum class EMediaIOStandardType
{
	Progressive,
	Interlaced,
	ProgressiveSegmentedFrame,
};

/**
 * Timecode formats.
 */
UENUM()
enum class EMediaIOTimecodeFormat
{
	None,
	LTC,
	VITC,
};

/**
 * Timecode formats.
 */
UENUM()
enum class EMediaIOAutoDetectableTimecodeFormat : uint8
{
	None = 0,
	LTC,
	VITC,
	Auto = 255,
};

namespace UE::MediaIO
{
	static EMediaIOAutoDetectableTimecodeFormat ToAutoDetectableTimecodeFormat(EMediaIOTimecodeFormat TimecodeFormat)
	{
		switch (TimecodeFormat)
		{
		case EMediaIOTimecodeFormat::None:
			return EMediaIOAutoDetectableTimecodeFormat::None;
		case EMediaIOTimecodeFormat::LTC:
			return EMediaIOAutoDetectableTimecodeFormat::LTC;
		case EMediaIOTimecodeFormat::VITC:
			return EMediaIOAutoDetectableTimecodeFormat::VITC;
		default:
			ensureMsgf(false, TEXT("Unknown timecode format encountered!"));
			return EMediaIOAutoDetectableTimecodeFormat::None;
		}
	}

	static EMediaIOTimecodeFormat FromAutoDetectableTimecodeFormat(EMediaIOAutoDetectableTimecodeFormat TimecodeFormat)
	{
		switch (TimecodeFormat)
		{
		case EMediaIOAutoDetectableTimecodeFormat::LTC:
			return EMediaIOTimecodeFormat::LTC;
		case EMediaIOAutoDetectableTimecodeFormat::VITC:
			return EMediaIOTimecodeFormat::VITC;
		case EMediaIOAutoDetectableTimecodeFormat::None:
			return EMediaIOTimecodeFormat::None;
		default:
			ensureMsgf(false, TEXT("Unknown timecode format encountered!"));
			return EMediaIOTimecodeFormat::None;
		}
	}
}

USTRUCT()
struct MEDIAIOCORE_API FMediaIOAutoDetectableTimecodeFormat_Backup
{
	GENERATED_BODY()

	/** The timecode format if not autodetected. */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	EMediaIOTimecodeFormat TimecodeFormat = EMediaIOTimecodeFormat::None;
	/** Whether the timecode should be autodetected. */
	UPROPERTY(EditAnywhere, Category = "Configuration")
	bool bAutoDetect = false;
};

/**
 * SDI Input type.
 */
UENUM()
enum class EMediaIOInputType
{
	Fill			UMETA(DisplayName="Fill"),
	FillAndKey		UMETA(DisplayName="Fill & Key"),
};

/**
 * SDI Output type.
 */
UENUM()
enum class EMediaIOOutputType
{
	Fill			UMETA(DisplayName="Fill"),
	FillAndKey		UMETA(DisplayName="Fill & Key"),
};


/**
 * SDI Output type.
 */
UENUM()
enum class EMediaIOReferenceType
{
	FreeRun,
	External,
	Input
};


/**
 * Identifies a device.
 */
USTRUCT()
struct MEDIAIOCORE_API FMediaIODevice
{
	GENERATED_BODY()

public:
	FMediaIODevice();

	/** The retail/display name of the Device. */
	UPROPERTY(EditAnywhere, Category=Device)
	FName DeviceName;

	/** The device identifier. */
	UPROPERTY(EditAnywhere, Category=Device)
	int32 DeviceIdentifier;

public:
	bool operator==(const FMediaIODevice& Other) const;

	/** Return true if the device has been set properly */
	bool IsValid() const;
};


/**
 * Identifies an media connection.
 */
USTRUCT()
struct MEDIAIOCORE_API FMediaIOConnection
{
	GENERATED_BODY()

public:
	FMediaIOConnection();

	/** The device identifier. */
	UPROPERTY(EditAnywhere, Category=Connection)
	FMediaIODevice Device;
	
	/** The protocol used by the MediaFramework. */
	UPROPERTY(EditAnywhere, Category=Connection)
	FName Protocol;

	/** The type of cable link used for that configuration */
	UPROPERTY(EditAnywhere, Category=Connection)
	EMediaIOTransportType TransportType;

	/** The type of cable link used for that configuration */
	UPROPERTY(EditAnywhere, Category=Connection)
	EMediaIOQuadLinkTransportType QuadTransportType;

	/** The port of the video channel on the device. */
	UPROPERTY(EditAnywhere, Category=Connection)
	int32 PortIdentifier;

public:
	bool operator==(const FMediaIOConnection& Other) const;

	/**
	 * Get a url used by the media framework
	 * @return Url representation, "protocol://device0/single1"
	 */
	FString ToUrl() const;

	/** Return true if the connection has been set properly */
	bool IsValid() const;
};


/**
 * Identifies a media mode.
 */
USTRUCT()
struct MEDIAIOCORE_API FMediaIOMode
{
	GENERATED_BODY()

public:
	FMediaIOMode();

public:
	/** The mode's frame rate. */
	UPROPERTY(VisibleAnywhere, Category=Mode)
	FFrameRate FrameRate;

	/** The mode's image resolution. */
	UPROPERTY(VisibleAnywhere, Category=Mode)
	FIntPoint Resolution;

	/** The mode's scanning type. */
	UPROPERTY(VisibleAnywhere, Category=Mode)
	EMediaIOStandardType Standard;

	/** The mode's identifier for the device. */
	UPROPERTY(VisibleAnywhere, Category=Mode)
	int32 DeviceModeIdentifier;

public:
	bool operator==(const FMediaIOMode& Other) const;

	/** Return the name of this mode from MediaIOCommonDisplayModes. */
	FText GetModeName() const;

	/** Return true if the mode has been set properly. */
	bool IsValid() const;
};


/**
 * Configuration of a device input / output.
 */
USTRUCT()
struct MEDIAIOCORE_API FMediaIOConfiguration
{
	GENERATED_BODY()
	
public:
	FMediaIOConfiguration();

public:
	/** Configured as an input or output. */
	UPROPERTY()
	bool bIsInput;

	/** The configuration's device and transport type. */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	FMediaIOConnection MediaConnection;

	/** The configuration's video mode. */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	FMediaIOMode MediaMode;

public:
	bool operator== (const FMediaIOConfiguration& Other) const;

	/** Return true if the configuration has been set properly */
	bool IsValid() const;
};

/**
 * Configuration of a device input.
 */
USTRUCT()
struct MEDIAIOCORE_API FMediaIOInputConfiguration
{
	GENERATED_BODY()

public:
	FMediaIOInputConfiguration();

	/** The signal input format. */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	FMediaIOConfiguration MediaConfiguration;

	/** Whether to input the fill or the fill and key. */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	EMediaIOInputType InputType;

	/** The port of the video channel on the device to input the key from. */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	int32 KeyPortIdentifier;

public:
	bool operator== (const FMediaIOInputConfiguration& Other) const;

	/** Return true if the configuration has been set properly */
	bool IsValid() const;
};

/**
 * Configuration of a device output.
 */
USTRUCT()
struct MEDIAIOCORE_API FMediaIOOutputConfiguration
{
	GENERATED_BODY()

public:
	FMediaIOOutputConfiguration();

	/** The signal output format. */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	FMediaIOConfiguration MediaConfiguration;

	/** Whether to output the fill or the fill and key. */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	EMediaIOOutputType OutputType;

	/**
	 * The port of the video channel on the device to output the key to.
	 * @note	'Frame Buffer Pixel Format' must be set to at least 8 bits of alpha.
	 * @note	'Enable alpha channel support in post-processing' must be set to 'Allow through tonemapper'.
	 */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	int32 KeyPortIdentifier;

	/** The Device output sync with either its internal clock, an external reference, or an other input. */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	EMediaIOReferenceType OutputReference;

	/** The port of the video channel on the device to output the synchronize to. */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	int32 ReferencePortIdentifier;

public:
	bool operator== (const FMediaIOOutputConfiguration& Other) const;

	/** Return true if the configuration has been set properly */
	bool IsValid() const;
};

/**
 * Configuration of a Timecode from Video
 */
USTRUCT()
struct MEDIAIOCORE_API FMediaIOVideoTimecodeConfiguration
{
	GENERATED_BODY()
	
	/** Read the timecode from a video signal. */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	FMediaIOConfiguration MediaConfiguration;

	/** Timecode format to read from a video signal. */
	UPROPERTY(VisibleAnywhere, Category=Configuration)
	EMediaIOAutoDetectableTimecodeFormat TimecodeFormat = EMediaIOAutoDetectableTimecodeFormat::Auto;

public:
	/** Return true if the configuration has been set properly */
	bool IsValid() const;

	bool operator== (const FMediaIOVideoTimecodeConfiguration& Other) const;

	/**
	 * Get the configuration text representation.
	 * @return String representation, i.e. "Video/Single1/1080p30fps/LTC".
	 */
	FText ToText(bool bAutoDetected = false) const;
};