// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/PathViews.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPtr.h"

#include "NiagaraBakerOutput.generated.h"

USTRUCT()
struct FNiagaraBakerTextureSource
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Source")
	FString DisplayString;

	UPROPERTY(EditAnywhere, Category = "Source")
	FName SourceName;
};

UCLASS(Abstract)
class NIAGARA_API UNiagaraBakerOutput : public UObject
{
	GENERATED_BODY()
public:
	/** Optional output name, useful when you have multiple outputs */
	UPROPERTY(EditAnywhere, Category = "Settings", meta = (DisplayPriority = "0"))
	FString OutputName;

	/** Used to determine if the bake is out of date or not. */
	virtual bool Equals(const UNiagaraBakerOutput& Other) const { return OutputName == Other.OutputName; }

	/** Make an OutputName used when creating a new output */
	virtual FString MakeOutputName() const;

	static FString SanitizeOutputName(FString Name);

#if WITH_EDITOR
	/** Find any warnings about the output. */
	virtual void FindWarnings(TArray<FText>& OutWarnings) const;

	FString GetAssetPath(FString PathFormat, int32 FrameIndex) const;
	FString GetAssetFolder(FString PathFormat, int32 FrameIndex) const;
	FString GetExportPath(FString PathFormat, int32 FrameIndex) const;
	FString GetExportFolder(FString PathFormat, int32 FrameIndex) const;

	template<class TObjectClass>
	TObjectClass* GetAsset(FString PathFormat, int32 FrameIndex)
	{
		const FString AssetFullName = GetAssetPath(PathFormat, FrameIndex);
		const FString AssetName = FString(FPathViews::GetCleanFilename(AssetFullName));
		const FString AssetPath = AssetFullName + "." + AssetName;
		return TSoftObjectPtr<TObjectClass>(AssetPath).LoadSynchronous();
	}

	static UObject* GetOrCreateAsset(const FString& PackagePath, UClass* ObjectClass, UClass* FactoryClass);

	template<typename TObjectClass, typename TFactoryClass>
	static TObjectClass* GetOrCreateAsset(const FString& PackagePath)
	{
		UObject* FoundObject = GetOrCreateAsset(PackagePath, TObjectClass::StaticClass(), TFactoryClass::StaticClass());
		return FoundObject ? CastChecked<TObjectClass>(FoundObject) : nullptr;
	}
#endif

	virtual void PostInitProperties() override;
};
