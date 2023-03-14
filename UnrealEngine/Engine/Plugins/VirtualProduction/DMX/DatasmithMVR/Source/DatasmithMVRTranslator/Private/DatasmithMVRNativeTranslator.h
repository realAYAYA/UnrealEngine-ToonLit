// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithMVRImportOptions.h"

#include "CoreMinimal.h"
#include "DatasmithNativeTranslator.h"

class UDMXLibrary;


/** An override of the datasmith native translator that considers MVR assets aside or in the *_Assets folder of a .udatasmith file */
class FDatasmithMVRNativeTranslator 
	: public FDatasmithNativeTranslator
{
public:
	//~ Begin DatasmithNativeTranslator interface
	virtual FName GetFName() const override { return "DatasmithMVRNativeTranslator"; };
	virtual bool LoadScene(TSharedRef<IDatasmithScene> InOutScene) override;
	virtual void GetSceneImportOptions(TArray<TObjectPtr<UDatasmithOptionsBase>>& Options) override;
	virtual void SetSceneImportOptions(const TArray<TObjectPtr<UDatasmithOptionsBase>>& Options) override;
	//~ End DatasmithNativeTranslator interface

private:
	/** Tries to find an MVR file next to or inside the datasmith folder */
	bool FindMVRFile(FString& OutMVRFilePathAndName) const;

	/** Creates a DMX Library from an MVR */
	UDMXLibrary* CreateDMXLibraryFromMVR(const FString& MVRFilePathAndName) const;

	/** Replaces actors in the Datasmith Scene that correspond to the MVR with the MVR Scene Actor */
	void ReplaceMVRActorsWithMVRSceneActor(TSharedRef<IDatasmithScene>& InOutScene, UDMXLibrary* DMXLibrary) const;

	/** Datasmith MVR Import Options */
	TStrongObjectPtr<UDatasmithMVRImportOptions> ImportOptions;
};
