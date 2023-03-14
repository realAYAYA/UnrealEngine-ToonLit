// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeFramework/ComputeSource.h"
#include "IOptimusShaderTextProvider.h"
#include "OptimusSource.generated.h"

UCLASS()
class OPTIMUSCORE_API UOptimusSource
	: public UComputeSource
	, public IOptimusShaderTextProvider
{
	GENERATED_BODY()

public:
	void SetSource(const FString& InText);

	// Begin UComputeSource interface.
	FString GetSource() const override { return SourceText; }
	// End UComputeSource interface.

	// Begin IOptimusShaderTextProvider interface.
#if WITH_EDITOR	
	FString GetNameForShaderTextEditor() const override;
	FString GetDeclarations() const override { return FString(); }
	FString GetShaderText() const override { return SourceText; }
	void SetShaderText(const FString& InText) override { SetSource(InText); }
#endif
	// End IOptimusShaderTextProvider interface.

protected:
	/** HLSL Source. */
	UPROPERTY(EditAnywhere, Category = Source, meta = (DisplayAfter = "AdditionalSources"))
	FString SourceText;
};
