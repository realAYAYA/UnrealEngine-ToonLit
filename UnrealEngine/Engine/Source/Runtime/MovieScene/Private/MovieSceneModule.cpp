// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IMovieSceneModule.h"
#include "MovieScene.h"
#include "Compilation/IMovieSceneTemplateGenerator.h"
#include "HAL/IConsoleManager.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"
#include "EntitySystem/BuiltInComponentTypes.h"

DEFINE_LOG_CATEGORY(LogMovieScene);
DEFINE_LOG_CATEGORY(LogMovieSceneECS);

TAutoConsoleVariable<FString> CVarLegacyConversionFrameRate(
	TEXT("MovieScene.LegacyConversionFrameRate"),
	TEXT("60000fps"),
	TEXT("Specifies default tick resolution for UMovieScene data saved before 4.20 (default: 60000fps). Examples: 60000 fps, 120/1 (120 fps), 30000/1001 (29.97), 0.01s (10ms)."),
	ECVF_Default);

struct FCachedLegacyConversionFrameRate
{
	FCachedLegacyConversionFrameRate()
		: FrameRate(60000, 1)
	{
		DelegateHandle = IConsoleManager::Get().RegisterConsoleVariableSink_Handle(FConsoleCommandDelegate::CreateRaw(this, &FCachedLegacyConversionFrameRate::OnChanged));

		OnChanged();
	}
	~FCachedLegacyConversionFrameRate()
	{
		IConsoleManager::Get().UnregisterConsoleVariableSink_Handle(DelegateHandle);
	}

	void OnChanged()
	{
		TryParseString(FrameRate, *CVarLegacyConversionFrameRate.GetValueOnGameThread());
	}

	FFrameRate FrameRate;
	FConsoleVariableSinkHandle DelegateHandle;
};

FFrameRate GetLegacyConversionFrameRate()
{
	static FCachedLegacyConversionFrameRate CachedRate;
	return CachedRate.FrameRate;
}

void EmitLegacyOutOfBoundsError(UObject* Object, FFrameRate InFrameRate, double InTime)
{
#if WITH_EDITOR
	static const FName NAME_AssetCheck("AssetCheck");

	FMessageLog AssetCheckLog(NAME_AssetCheck);

	const FText Message = FText::Format(
		NSLOCTEXT("MovieScene", "LegacyOutOfBoundsError", "Encountered time ({0} seconds) that is out of the supported range with a resolution of {1}fps. Saving this asset will cause loss of data. Please reduce MovieScene.LegacyConversionFrameRate and re-load this asset."),
		InTime, InFrameRate.AsDecimal()
	);
	AssetCheckLog.Error()
		->AddToken(FUObjectToken::Create(Object))
		->AddToken(FTextToken::Create(Message));

	AssetCheckLog.Open(EMessageSeverity::Warning);
#endif
}

FFrameNumber UpgradeLegacyMovieSceneTime(UObject* ErrorContext, FFrameRate InFrameRate, double InTime)
{
	double ClampedKeyTime = FMath::Clamp(InTime, -InFrameRate.MaxSeconds(), InFrameRate.MaxSeconds());
	if (InTime != ClampedKeyTime)
	{
		EmitLegacyOutOfBoundsError(ErrorContext, InFrameRate, InTime);
	}
	return InFrameRate.AsFrameNumber(ClampedKeyTime);
}

/**
 * MovieScene module implementation.
 */
class FMovieSceneModule
	: public IMovieSceneModule
	, public TSharedFromThis<FMovieSceneModule>
{
public:

	// IModuleInterface interface
	~FMovieSceneModule()
	{
		ensure(ModuleHandle.IsUnique());
	}

	virtual void StartupModule() override
	{
		FModuleManager::Get().LoadModuleChecked("UniversalObjectLocator");

		struct FNoopDefaultDeleter
		{
			void operator()(FMovieSceneModule* Object) const {}
		};
		ModuleHandle = MakeShareable(this, FNoopDefaultDeleter());

		UE::MovieScene::FBuiltInComponentTypes::Get();
	}

	virtual void ShutdownModule() override
	{
		UE::MovieScene::FBuiltInComponentTypes::Destroy();
	}

	virtual void RegisterEvaluationGroupParameters(FName GroupName, const FMovieSceneEvaluationGroupParameters& GroupParameters) override
	{
		check(!GroupName.IsNone() && GroupParameters.EvaluationPriority != 0);

		for (auto& Pair : EvaluationGroupParameters)
		{
			checkf(Pair.Key != GroupName, TEXT("Cannot add 2 groups of the same name"));
			checkf(Pair.Value.EvaluationPriority != GroupParameters.EvaluationPriority, TEXT("Cannot add 2 groups of the same priority"));
		}

		EvaluationGroupParameters.Add(GroupName, GroupParameters);
	}

	virtual FMovieSceneEvaluationGroupParameters GetEvaluationGroupParameters(FName GroupName) const override
	{
		return EvaluationGroupParameters.FindRef(GroupName);
	}

	virtual TWeakPtr<IMovieSceneModule> GetWeakPtr() override
	{
		return ModuleHandle;
	}

private:
	TSharedPtr<FMovieSceneModule> ModuleHandle;
	TMap<FName, FMovieSceneEvaluationGroupParameters> EvaluationGroupParameters;
};


IMPLEMENT_MODULE(FMovieSceneModule, MovieScene);
