// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithTranslator.h"
#include "DatasmithCloImportOptions.h"



class DATASMITHCLOTRANSLATOR_API FDatasmithCloTranslator : public IDatasmithTranslator
{
public:
	virtual FName GetFName() const override { return "DatasmithCloTranslator"; };
	virtual void Initialize(FDatasmithTranslatorCapabilities& OutCapabilities) override;

	virtual void SetSceneImportOptions(const TArray<TObjectPtr<UDatasmithOptionsBase>>& AllOptions) override;
	virtual void GetSceneImportOptions(TArray<TObjectPtr<UDatasmithOptionsBase>>& AllOptions) override;

	virtual bool LoadScene(TSharedRef<IDatasmithScene> OutScene) override;
	virtual void UnloadScene() override;

	virtual bool LoadCloth(const TSharedRef<IDatasmithClothElement> ClothElement, FDatasmithClothElementPayload& OutClothPayload) override;
	virtual bool LoadStaticMesh(const TSharedRef<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload) override;

private:
	bool ParseFromJson();

private:
	TSharedPtr<struct FCloCloth> CloClothPtr;
	FCloOptions ImportOptions;
	TArray<FString> PanelMeshNames;
};

