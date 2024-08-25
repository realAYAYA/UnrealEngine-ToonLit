// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Factories/Factory.h"
#include "EditorReimportHandler.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#include "FusionPatchAssetFactory.generated.h"

class UFusionPatchImportOptions;
class UFusionPatchCreateOptions;

DECLARE_LOG_CATEGORY_EXTERN(LogFusionPatchAssetFactory, Log, All);

UCLASS()
class HARMONIXDSPEDITOR_API UFusionPatchAssetFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

public:

	UFusionPatchAssetFactory();

	virtual bool FactoryCanImport(const FString& Filename) override;
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	virtual void PostImportCleanUp() override;

	virtual UObject* FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn) override;
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;

	const UFusionPatchCreateOptions* CreateOptions = nullptr;
private:

	bool GetReplaceExistingSamplesResponse(const FString& InName);
	
	bool ApplyOptionsToAllImport = false;

	EAppReturnType::Type ReplaceExistingSamplesResponse = EAppReturnType::No;

	void UpdateFusionPatchImportNotificationItem(TSharedPtr<SNotificationItem> InItem, bool bImportSuccessful, FName InName);
};