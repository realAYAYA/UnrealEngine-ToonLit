// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"

#include "XRCreativeSubsystem.generated.h"


UCLASS(Abstract, Blueprintable)
class XRCREATIVE_API UXRCreativeSubsystemHelpers : public UObject
{
	GENERATED_BODY()
};


UCLASS()
class XRCREATIVE_API UXRCreativeSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

#if WITH_EDITOR
	/** Enter VR Mode */
	UFUNCTION(BlueprintCallable, Category="XR Creative")
	static bool EnterVRMode();

	/** Exit VR Mode */
	UFUNCTION(BlueprintCallable, Category="XR Creative")
	static void ExitVRMode();
#endif // #if WITH_EDITOR

protected:
	void OnEngineInitComplete();

protected:
	UPROPERTY(BlueprintReadOnly, Category="XR Creative")
	TObjectPtr<UXRCreativeSubsystemHelpers> Helpers;

	FDelegateHandle EngineInitCompleteDelegate;
};
