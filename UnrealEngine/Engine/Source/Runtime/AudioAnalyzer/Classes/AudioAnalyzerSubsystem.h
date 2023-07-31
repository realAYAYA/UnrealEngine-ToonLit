// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AudioAnalyzer.h"
#include "Containers/Ticker.h"
#include "Subsystems/EngineSubsystem.h"
#include "AudioAnalyzerSubsystem.generated.h"

class UWorld;

/** 
* Class manages querying analysis results from various audio analyzers.
*/
UCLASS()
class AUDIOANALYZER_API UAudioAnalyzerSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	UAudioAnalyzerSubsystem();
	~UAudioAnalyzerSubsystem();

	bool Tick(float DeltaTime);

	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	void RegisterAudioAnalyzer(UAudioAnalyzer* InAnalyzer);
	void UnregisterAudioAnalyzer(UAudioAnalyzer* InAnalyzer);

	static UAudioAnalyzerSubsystem* Get();

private:

	UPROPERTY(Transient);
	TArray<TObjectPtr<UAudioAnalyzer>> AudioAnalyzers;

	FTSTicker::FDelegateHandle TickerHandle;
};
