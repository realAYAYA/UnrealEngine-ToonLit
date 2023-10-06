// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "IWaveformTransformation.h"

#include "WaveformTransformationTrimFade.generated.h"

class WAVEFORMTRANSFORMATIONS_API FWaveTransformationTrimFade : public Audio::IWaveTransformation
{
public:
	explicit FWaveTransformationTrimFade(double InStartTime, double InEndTime, float InStartFadeTime, float InStartFadeCurve, float InEndFadeTime, float InEndFadeCurve);
	virtual void ProcessAudio(Audio::FWaveformTransformationWaveInfo& InOutWaveInfo) const override;

	virtual bool CanChangeFileLength() const override { return true; }

private:
	double StartTime = 0.0;
	double EndTime = 0.0;

	float StartFadeTime = 0.f;
	float StartFadeCurve = 0.f;

	float EndFadeTime = 0.f;
	float EndFadeCurve = 0.f;
};

UCLASS()
class WAVEFORMTRANSFORMATIONS_API UWaveformTransformationTrimFade : public UWaveformTransformationBase
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = "Trim", meta=(ClampMin = 0.0))
	double StartTime = 0.0;

	UPROPERTY(EditAnywhere, Category = "Trim")
	double EndTime = -1.0;

	UPROPERTY(EditAnywhere, Category = "Fade", meta=(ClampMin = 0.0, DisplayName = "Fade-In Duration"))
	float StartFadeTime = 0.f;

	UPROPERTY(EditAnywhere, Category = "Fade", meta=(ClampMin = 0.0, ClampMax = 10.0, DisplayName = "Fade-In Curve"))
	float StartFadeCurve = 1.f;

	UPROPERTY(EditAnywhere, Category = "Fade", meta=(ClampMin = 0.0, DisplayName = "Fade-Out Duration"))
	float EndFadeTime = 0.f;

	UPROPERTY(EditAnywhere, Category = "Fade", meta=(ClampMin = 0.0, ClampMax = 10.0, DisplayName = "Fade-Out Curve"))
	float EndFadeCurve = 1.f;

	virtual Audio::FTransformationPtr CreateTransformation() const override;

	void UpdateConfiguration(FWaveTransformUObjectConfiguration& InOutConfiguration) override;
	
private:	
	void UpdateDurationProperties(const float InAvailableDuration);
	float AvailableWaveformDuration = -1.f;
};

// class UBaseClass;
//
// namespace  MyNamespace
// {
// 	class IBaseInterface;
// }



// // no good
// #include "IBaseInterface.h" // in other module but that module is listed in the build.cs so we good there
// #include "DerivedClass.generated.h"
//
// class PLUGIN_API FDerivedClass : public MyNamespace::IBaseInterface // error: Class 'MyNamespace' Not Found
// {
// public:
// 	virtual void DoThing() const;
// };
//
// UCLASS()
// class PLUGIN_API UDerivedClass : public UBaseClass
// {
// 	GENERATED_BODY()
// public:
// 	TSharedPtr<MyNamespace::IBaseInterface> CreateFObject();
// };
//
// // fine
// #include "IBaseInterface.h"
// #include "DerivedClass.generated.h"
//
// namespace MyNamespace
// {
// 	class PLUGIN_API FDerivedClass : public MyNamespace::IBaseInterface
// 	{
// 	public:
// 		virtual void DoThing() const;
// 	};
// }
//
// UCLASS()
// class PLUGIN_API UDerivedClass : public UBaseClass
// {
// 	GENERATED_BODY()
// public:
// 	TSharedPtr<MyNamespace::IBaseInterface> CreateFObject();
// };