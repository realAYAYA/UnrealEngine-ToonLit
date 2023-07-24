// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleInterface.h"
#include "UObject/StrongObjectPtr.h"

class UNaniteDisplacedMesh;
class FAssetTypeActions_NaniteDisplacedMesh;
class UPackage;
class FString;

struct FNaniteDisplacedMeshParams;

class FNaniteDisplacedMeshEditorModule : public IModuleInterface
{
public:

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static FNaniteDisplacedMeshEditorModule& GetModule();

	UPackage* GetNaniteDisplacementMeshTransientPackage() const;

	// Invoked when a nanite displaced mesh is linked
	DECLARE_DELEGATE_RetVal_TwoParams(UNaniteDisplacedMesh*, FOnLinkDisplacedMesh, const FNaniteDisplacedMeshParams& /*InParameters*/, const FString& /*DisplacedMeshFolder*/);
	FOnLinkDisplacedMesh OnLinkDisplacedMeshOverride;

private:
	FAssetTypeActions_NaniteDisplacedMesh* NaniteDisplacedMeshAssetActions;
	TStrongObjectPtr<UPackage> NaniteDisplacedMeshTransientPackage;
};
