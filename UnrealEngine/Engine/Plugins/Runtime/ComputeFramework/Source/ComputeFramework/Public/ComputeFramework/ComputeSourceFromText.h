// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ComputeSource.h"
#include "Engine/EngineTypes.h"
#include "ComputeSourceFromText.generated.h"

/**
 * Class responsible for loading HLSL text and parsing any metadata available.
 */
UCLASS(BlueprintType)
class COMPUTEFRAMEWORK_API UComputeSourceFromText : public UComputeSource
{
	GENERATED_BODY()

public:
	/** Filepath to the source file containing the kernel entry points and all options for parsing. */
	UPROPERTY(EditDefaultsOnly, AssetRegistrySearchable, meta = (ContentDir, RelativeToGameContentDir, FilePathFilter = "Unreal Shader File (*.usf)|*.usf"), Category = "Kernel")
	FFilePath SourceFile;

#if WITH_EDITOR
	/** Parse the source to get metadata. */
	void ReparseSourceText();
#endif

protected:
	//~ Begin UComputeSource Interface.
	FString GetSource() const override
	{
#if WITH_EDITOR
		return SourceText;
#else
		return {};
#endif
	}
	//~ End UComputeSource Interface.

#if WITH_EDITOR
	//~ Begin UObject Interface.
	void PostLoad() override;
	void PreEditChange(FEditPropertyChain& PropertyAboutToChange) override;
	void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent) override;
	//~ End UObject Interface.
#endif

private:
#if WITH_EDITOR
	FString SourceText;
	FFilePath PrevSourceFile;
#endif
};
