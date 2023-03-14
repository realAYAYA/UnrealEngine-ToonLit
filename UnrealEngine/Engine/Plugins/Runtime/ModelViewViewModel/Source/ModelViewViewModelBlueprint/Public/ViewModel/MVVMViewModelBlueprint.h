// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Engine/Blueprint.h"

#include "MVVMViewModelBlueprint.generated.h"

class FCompilerResultsLog;
class UEdGraph;
class UMVVMViewModelBlueprintGeneratedClass;


/**
 * 
 */
UCLASS(BlueprintType, meta=(IgnoreClassThumbnail))
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMViewModelBlueprint : public UBlueprint
{
	GENERATED_BODY()

public:
	/** */
	static TSharedPtr<FKismetCompilerContext> GetCompilerForViewModelBlueprint(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions);

	/** Get the (full) generated class for this viewmodel blueprint */
	UMVVMViewModelBlueprintGeneratedClass* GetViewModelBlueprintGeneratedClass() const;

	/** Get the (skeleton) generated class for this viewmodel blueprint */
	UMVVMViewModelBlueprintGeneratedClass* GetViewModelBlueprintSkeletonClass() const;

protected:
	virtual bool SupportsMacros() const override;
	virtual bool SupportsEventGraphs() const override;
	virtual bool SupportsDelegates() const override;
	virtual bool SupportsAnimLayers() const override;

public:
	// Use during compilation to clean the automatically generated graph.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UEdGraph>> TemporaryGraph;
};
