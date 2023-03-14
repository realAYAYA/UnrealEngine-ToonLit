// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CineCameraComponent.h"
#include "Engine/Scene.h"
#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "DatasmithCineCameraComponentTemplate.generated.h"

struct FCameraFilmbackSettings;
struct FCameraFocusSettings;
struct FCameraLensSettings;

USTRUCT()
struct FDatasmithCameraFilmbackSettingsTemplate
{
	GENERATED_BODY()

	UPROPERTY()
	float SensorWidth = 0.0f;

	UPROPERTY()
	float SensorHeight = 0.0f;

	void Apply( FCameraFilmbackSettings* Destination, const FDatasmithCameraFilmbackSettingsTemplate* PreviousTemplate );
	void Load( const FCameraFilmbackSettings& Source );
	bool Equals( const FDatasmithCameraFilmbackSettingsTemplate& Other ) const;
};

USTRUCT()
struct FDatasmithCameraLensSettingsTemplate
{
	GENERATED_BODY()

	UPROPERTY()
	float MaxFStop = 0.0f;

	void Apply( FCameraLensSettings* Destination, const FDatasmithCameraLensSettingsTemplate* PreviousTemplate );
	void Load( const FCameraLensSettings& Source );
	bool Equals( const FDatasmithCameraLensSettingsTemplate& Other ) const;
};

USTRUCT()
struct FDatasmithCameraFocusSettingsTemplate
{
	GENERATED_BODY()

	UPROPERTY()
	ECameraFocusMethod FocusMethod = ECameraFocusMethod::DoNotOverride;

	UPROPERTY()
	float ManualFocusDistance = 0.0f;

	void Apply( FCameraFocusSettings* Destination, const FDatasmithCameraFocusSettingsTemplate* PreviousTemplate );
	void Load( const FCameraFocusSettings& Source );
	bool Equals( const FDatasmithCameraFocusSettingsTemplate& Other ) const;
};

USTRUCT()
struct DATASMITHCONTENT_API FDatasmithPostProcessSettingsTemplate
{
	GENERATED_BODY()

public:
	UPROPERTY()
	uint32 bOverride_WhiteTemp:1;

	UPROPERTY()
	uint32 bOverride_ColorSaturation:1;

	UPROPERTY()
	uint32 bOverride_VignetteIntensity:1;

	UPROPERTY()
	uint32 bOverride_AutoExposureMethod:1;

	UPROPERTY()
	uint32 bOverride_CameraISO:1;

	UPROPERTY()
	uint32 bOverride_CameraShutterSpeed:1;

	UPROPERTY()
	uint8 bOverride_DepthOfFieldFstop:1;

	UPROPERTY()
	float WhiteTemp = 0.0f;

	UPROPERTY()
	float VignetteIntensity = 0.0f;

	UPROPERTY()
	FVector4 ColorSaturation;

	UPROPERTY()
	TEnumAsByte< enum EAutoExposureMethod > AutoExposureMethod;

	UPROPERTY()
	float CameraISO = 0.0f;

	UPROPERTY()
	float CameraShutterSpeed = 0.0f;

	UPROPERTY()
	float DepthOfFieldFstop = 0.0f;

public:
	FDatasmithPostProcessSettingsTemplate();

	void Apply( struct FPostProcessSettings* Destination, const FDatasmithPostProcessSettingsTemplate* PreviousTemplate );
	void Load( const FPostProcessSettings& Source );
	bool Equals( const FDatasmithPostProcessSettingsTemplate& Other ) const;
};

UCLASS()
class DATASMITHCONTENT_API UDatasmithCineCameraComponentTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FDatasmithCameraFilmbackSettingsTemplate FilmbackSettings;

	UPROPERTY()
	FDatasmithCameraLensSettingsTemplate LensSettings;

	UPROPERTY()
	FDatasmithCameraFocusSettingsTemplate FocusSettings;

	UPROPERTY()
	float CurrentFocalLength = 0.0f;

	UPROPERTY()
	float CurrentAperture = 0.0f;

	UPROPERTY()
	FDatasmithPostProcessSettingsTemplate PostProcessSettings;

	virtual UObject* UpdateObject( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;
};
