// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "CinePrestreamingRecorderSetting.h"
#include "CinePrestreamingEditorSubsystem.generated.h"

// Forward Declare
class ULevelSequence;
class UMoviePipelineExecutorBase;

USTRUCT(BlueprintType)
struct FCinePrestreamingGenerateAssetArgs
{
	GENERATED_BODY()
	
	FCinePrestreamingGenerateAssetArgs()
	{
		Resolution = FIntPoint(1920, 1080);
	}
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cinematic Prestreaming|Editor")
	FDirectoryPath OutputDirectoryOverride;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cinematic Prestreaming|Editor", meta = (AllowedClasses = "/Script/LevelSequence.LevelSequence"))
	FSoftObjectPath Sequence;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cinematic Prestreaming|Editor", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath Map;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Cinematic Prestreaming|Editor")
	FIntPoint Resolution;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCinePrestreamingGenerateAsset, FCinePrestreamingGenerateAssetArgs, OriginalGenerationArgs);


UCLASS(BlueprintType)
class CINEMATICPRESTREAMINGEDITOR_API UCinePrestreamingEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()
	
public:
	UCinePrestreamingEditorSubsystem() {}
	
	UFUNCTION(BlueprintCallable, Category = "Cinematic Prestreaming|Editor")
	bool IsRendering() const { return ActiveExecutor != nullptr; }
	
	UFUNCTION(BlueprintCallable, Category = "Cinematic Prestreaming|Editor")
	void GeneratePrestreamingAsset(const FCinePrestreamingGenerateAssetArgs& InArgs);

	UFUNCTION(BlueprintCallable, Category = "Cinematic Prestreaming|Editor")
	void CreatePackagesFromGeneratedData(TArray<FMoviePipelineCinePrestreamingGeneratedData>& InOutData);
	
public:
	UPROPERTY(BlueprintAssignable, Category = "Cinematic Prestreaming|Editor")
	FOnCinePrestreamingGenerateAsset OnAssetGenerated;

private:
	UFUNCTION()
	void OnBuildPrestreamingComplete(UMoviePipelineExecutorBase* InPipelineExecutor, bool bSuccess);
	
private:
	// UPROPERTY(Transient)
	// UMoviePipelineQueue* OriginalQueue;

	UPROPERTY(Transient)
	TObjectPtr<UMoviePipelineExecutorBase> ActiveExecutor;

	TOptional<FCinePrestreamingGenerateAssetArgs> ActiveAssetArgs;
};