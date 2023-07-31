// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ParametricSurfaceTranslator.h"
#include "UObject/ObjectMacros.h"


namespace UE_DATASMITHWIRETRANSLATOR_NAMESPACE
{

class FWireTranslatorImpl;

class FDatasmithWireTranslator : public FParametricSurfaceTranslator
{
public:
	FDatasmithWireTranslator();

	// Begin IDatasmithTranslator overrides
	virtual FName GetFName() const override { return "DatasmithWireTranslator"; };

	virtual void Initialize(FDatasmithTranslatorCapabilities& OutCapabilities) override;

	virtual bool IsSourceSupported(const FDatasmithSceneSource& Source) override;

	virtual bool LoadScene(TSharedRef<IDatasmithScene> OutScene) override;
	virtual void UnloadScene() override;

	virtual bool LoadStaticMesh(const TSharedRef<IDatasmithMeshElement> MeshElement, FDatasmithMeshElementPayload& OutMeshPayload) override;
	// End IDatasmithTranslator overrides

	// Begin ADatasmithCoreTechTranslator overrides
	virtual void SetSceneImportOptions(const TArray<TObjectPtr<UDatasmithOptionsBase>>& Options) override;

protected:
	virtual void InitCommonTessellationOptions(FDatasmithTessellationOptions& TessellationOptions) override
	{
		TessellationOptions.StitchingTechnique = EDatasmithCADStitchingTechnique::StitchingNone;
	}
	// End ADatasmithCoreTechTranslator overrides

private:
	TSharedPtr<FWireTranslatorImpl> Translator;
};

}