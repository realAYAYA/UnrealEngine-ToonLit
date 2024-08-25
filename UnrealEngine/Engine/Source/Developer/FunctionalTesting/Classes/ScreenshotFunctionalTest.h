// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "ScreenshotFunctionalTestBase.h"
#include "AutomationScreenshotOptions.h"

#include "ScreenshotFunctionalTest.generated.h"

class FAutomationTestScreenshotEnvSetup;

/**
 * No UI
 */
UCLASS(Blueprintable)
class FUNCTIONALTESTING_API AScreenshotFunctionalTest : public AScreenshotFunctionalTestBase
{
	GENERATED_BODY()

public:
	AScreenshotFunctionalTest(const FObjectInitializer& ObjectInitializer);

	virtual void Serialize(FArchive& Ar) override;

	// Tests not relying on temporal effects can force a camera cut to flush stale data
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera", SimpleDisplay)
	bool bCameraCutOnScreenshotPrep;


protected:
	virtual bool RunTest(const TArray<FString>& Params = TArray<FString>()) override;
	virtual void PrepareTest() override;
	virtual bool IsReady_Implementation() override;
	virtual void Tick(float DeltaSeconds) override;

	virtual void RequestScreenshot() override;
	virtual void OnScreenShotCaptured(int32 InSizeX, int32 InSizeY, const TArray<FColor>& InImageData) override;
	virtual void OnScreenshotTakenAndCompared() override;
	

private:

	enum EVariantType
	{
		Baseline = 0,
		ViewRectOffset,
		Num
	};

	struct FVariantInfo
	{
		const TCHAR* Name;
		const TCHAR* SetupCommand;
		const TCHAR* RestoreCommand;
	};

	bool SetupNextVariant();
	void SetupVariant(EVariantType VariantType);
	
	TBitArray<> RequestedVariants;
	const TCHAR* CurrentVariantName;
	const TCHAR* VariantRestoreCommand;

	// Used for subsequent variants after the first is done
	void ReprepareTest();

	uint32 VariantFrame;
	float VariantTime;
	bool bVariantQueued;
};
