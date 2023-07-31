// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "LiveLinkFaceImporterFactory.generated.h"

// Imports a CSV file created by the Live Link Face iOS app when recordin facial animation.
UCLASS()
class ULiveLinkFaceImporterFactory : public UFactory
{
	GENERATED_UCLASS_BODY()
		
	// UFactory interface
	virtual FText GetToolTip() const override;
	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn) override;
	// End of UFactory interface

protected:
	bool LoadCSV(const FString& FileContent, TArray<FString>& KeyArray, TArray<FString>& LineArray, FString& OutLogMessage);
	FString CreateSubjectString(const FString& InName);
};

