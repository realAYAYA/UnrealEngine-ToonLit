// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "ISourceControlProvider.h"
#include "ConvertLevelsToExternalActorsCommandlet.generated.h"

UCLASS()
class UNREALED_API UConvertLevelsToExternalActorsCommandlet : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& Params) override;
	//~ End UCommandlet Interface

protected:
	ULevel* LoadLevel(const FString& LevelToLoad) const;
	void GetSubLevelsToConvert(ULevel* MainLevel, TSet<ULevel*>& SubLevels, bool bRecursive);

	bool CheckExternalActors(const FString& Level, bool bRepair);

	bool UseSourceControl() const { return SourceControlProvider != nullptr; }
	ISourceControlProvider& GetSourceControlProvider() { check(UseSourceControl()); return *SourceControlProvider; }
	bool AddPackageToSourceControl(UPackage* Package);
	bool CheckoutPackage(UPackage* Package);
	bool SavePackage(UPackage* Package);
	bool DeleteFile(const FString& Filename);

protected:
	ISourceControlProvider* SourceControlProvider;
};
