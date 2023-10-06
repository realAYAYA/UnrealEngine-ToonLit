// Copyright Epic Games, Inc. All Rights Reserved.

//~=============================================================================
// FontFactory: Creates a Font Factory
//~=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "EditorReimportHandler.h"
#include "FontFileImportFactory.generated.h"

UCLASS(MinimalAPI)
class UFontFileImportFactory : public UFactory, public FReimportHandler
{
	GENERATED_UCLASS_BODY()

	//~ Begin UFactory Interface
	UNREALED_API virtual bool ConfigureProperties() override;
	UNREALED_API virtual UObject* FactoryCreateBinary(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn) override;
	//~ Begin UFactory Interface

	//~ Begin FReimportHandler Interface
	UNREALED_API virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	UNREALED_API virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	UNREALED_API virtual EReimportResult::Type Reimport(UObject* Obj) override;
	UNREALED_API virtual int32 GetPriority() const override;
	//~ End FReimportHandler Interface

private:
	enum class EBatchCreateFontAsset : uint8
	{
		Unknown,
		Cancel,
		Yes,
		No,
	};
	EBatchCreateFontAsset BatchCreateFontAsset;
};
