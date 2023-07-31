// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GenerateNaniteDisplacedMeshCommandlet.generated.h"

class UNaniteDisplacedMesh;

struct FAssetData;
struct FNaniteDisplacedMeshParams;

/*
 * Commandlet to help keeping up to date generated nanite displacement mesh assets
 * Iterate all the levels and keep track of the linked mesh used.
 */
UCLASS()
class UGenerateNaniteDisplacedMeshCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:

	static bool IsRunning();

	//~ Begin UCommandlet Interface
	virtual int32 Main(const FString& CmdLineParams) override;
	//~ End UCommandlet Interface

private:

	UNaniteDisplacedMesh* OnLinkDisplacedMesh(const FNaniteDisplacedMeshParams& Parameters, const FString& Folder);

	static void LoadLevel(const FAssetData& AssetData);

	static TSet<FString> GetPackagesInFolders(const TSet<FString>& Folders, const FString& NamePrefix);

	TSet<FString> LinkedPackageNames;
	TSet<FString> LinkedPackageFolders;
};
