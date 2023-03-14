// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeKernelSource.h"
#include "Engine/EngineTypes.h"
#include "ComputeKernelFromText.generated.h"

/**
 * Class responsible for loading HLSL text and parsing the options available.
 */
UCLASS(BlueprintType)
class COMPUTEFRAMEWORK_API UComputeKernelFromText : public UComputeKernelSource
{
	GENERATED_BODY()

public:
	/** Filepath to the source file containing the kernel entry points and all options for parsing. */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, meta = (ContentDir, RelativeToGameContentDir, FilePathFilter = "Unreal Shader File (*.usf)|*.usf"), Category = "Kernel")
	FFilePath SourceFile;

#if WITH_EDITOR
	/** Parse the kernel source to get the kernel external functions and other data. */
	void ReparseKernelSourceText();
#endif

protected:
	//~ Begin UComputeKernelSource Interface.
	FString GetSource() const override
	{
#if WITH_EDITOR
		return KernelSourceText;
#else
		return {};
#endif
	}
	//~ End UComputeKernelSource Interface.

#if WITH_EDITOR
	//~ Begin UObject Interface.
	void PostLoad() override;
	void PreEditChange(FEditPropertyChain& PropertyAboutToChange) override;
	void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	//~ End UObject Interface.
#endif

private:
#if WITH_EDITOR
	FString KernelSourceText;
	FFilePath PrevSourceFile;
#endif
};
