// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithGLTFImporter.h"
#include "DatasmithGLTFImportOptions.h"

#include "CoreMinimal.h"
#include "DatasmithTranslator.h"

class FDatasmithGLTFTranslator : public IDatasmithTranslator
{
public:
	// IDatasmithTranslator interface
	virtual FName GetFName() const override { return "DatasmithGLTFTranslator"; };
	virtual void Initialize(FDatasmithTranslatorCapabilities& OutCapabilities) override;
	virtual bool LoadScene(TSharedRef<IDatasmithScene> OutScene) override;
	virtual void UnloadScene() override;

	virtual bool LoadStaticMesh(const TSharedRef<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload) override;
	virtual bool LoadLevelSequence(const TSharedRef<IDatasmithLevelSequenceElement> LevelSequenceElement, FDatasmithLevelSequencePayload& OutLevelSequencePayload) override;

	virtual void GetSceneImportOptions(TArray<TObjectPtr<UDatasmithOptionsBase>>& Options) override;
	virtual void SetSceneImportOptions(const TArray<TObjectPtr<UDatasmithOptionsBase>>& Options) override;
	//~ End IDatasmithTranslator interface

private:
	TStrongObjectPtr<UDatasmithGLTFImportOptions>& GetOrCreateGLTFImportOptions();

private:
    TStrongObjectPtr<UDatasmithGLTFImportOptions> ImportOptions;
    TSharedPtr<class FDatasmithGLTFImporter> Importer;
};
