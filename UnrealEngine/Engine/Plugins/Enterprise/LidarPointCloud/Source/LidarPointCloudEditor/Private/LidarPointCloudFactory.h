// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EditorReimportHandler.h"
#include "AssetTypeCategories.h"
#include "AssetTypeActions_Base.h"
#include "Factories/Factory.h"
#include "LidarPointCloud.h"
#include "LidarPointCloudFactory.generated.h"

class FAssetTypeActions_LidarPointCloud : public FAssetTypeActions_Base
{
public:
	FAssetTypeActions_LidarPointCloud() {}

	// Begin IAssetTypeActions Interface
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override { return FColor(0, 128, 128); }
	virtual UClass* GetSupportedClass() const override { return ULidarPointCloud::StaticClass(); }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual bool IsImportedAsset() const override { return true; }
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	// End IAssetTypeActions Interface

private:
	void ExecuteMerge(TArray<ULidarPointCloud*> PointClouds);
	void ExecuteAlign(TArray<ULidarPointCloud*> PointClouds);
	void ExecuteCollision(TArray<ULidarPointCloud*> PointClouds);
	void ExecuteNormals(TArray<ULidarPointCloud*> PointClouds);
};

UCLASS()
class ULidarPointCloudFactory : public UFactory, public FReimportHandler
{
	GENERATED_BODY()

private:
	TSharedPtr<struct FLidarPointCloudImportSettings> ImportSettings;
	bool bImportingAll;

public:
	ULidarPointCloudFactory();

	// Begin UFactory Interface
	virtual UObject* ImportObject(UClass* InClass, UObject* InOuter, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, bool& OutCanceled) override;
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;
	virtual UObject* FactoryCreateNew(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	virtual bool DoesSupportClass(UClass* Class) override;
	virtual bool FactoryCanImport(const FString& Filename) override { return true; }
	virtual FText GetDisplayName() const override { return FText::FromString("LiDAR Point Cloud"); }
	// End UFactory Interface

	// Begin FReimportHandler Interface
	virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
	virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
	virtual EReimportResult::Type Reimport(UObject* Obj) override;
	// End FReimportHandler Interface
};