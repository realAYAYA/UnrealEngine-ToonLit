// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"

#include "VCamInputSettings.generated.h"

USTRUCT(BlueprintType)
struct VCAMINPUT_API FVCamInputProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Input")
	TMap<FName, FKey> MappableKeyOverrides;

	bool operator==(const FVCamInputProfile& OtherProfile) const;
};

UCLASS(config=Game)
class VCAMINPUT_API UVCamInputSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, BlueprintSetter=SetDefaultInputProfile, Category="Input", meta=(GetOptions=GetInputProfileNames))
	FName DefaultInputProfile;

	UFUNCTION(BlueprintCallable, Category="VCam Input")
	void SetDefaultInputProfile(const FName NewDefaultInputProfile);
	
	UPROPERTY(EditAnywhere, Config, BlueprintReadWrite, BlueprintSetter=SetInputProfiles, Category="Input")
	TMap<FName, FVCamInputProfile> InputProfiles;

	UFUNCTION(BlueprintCallable, Category="VCam Input")
	void SetInputProfiles(const TMap<FName, FVCamInputProfile>& NewInputProfiles);

	UFUNCTION(BlueprintCallable, Category="VCam Input")
	TArray<FName> GetInputProfileNames() const;
	
private:
	UFUNCTION(BlueprintCallable, Category="VCam")
	static UVCamInputSettings* GetVCamInputSettings();
};