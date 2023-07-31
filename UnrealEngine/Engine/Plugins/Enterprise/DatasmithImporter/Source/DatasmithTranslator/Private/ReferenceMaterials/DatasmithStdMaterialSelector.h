// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DatasmithDefinitions.h"
#include "ReferenceMaterials/DatasmithReferenceMaterial.h"
#include "ReferenceMaterials/DatasmithReferenceMaterialSelector.h"

#include "Templates/SharedPointer.h"

class IDatasmithMaterialInstanceElement;

class FDatasmithStdMaterialSelector : public FDatasmithReferenceMaterialSelector
{
public:
	FDatasmithStdMaterialSelector();
	virtual ~FDatasmithStdMaterialSelector() = default;

	virtual bool IsValid() const override;
	virtual const FDatasmithReferenceMaterial& GetReferenceMaterial( const TSharedPtr< IDatasmithMaterialInstanceElement >& InDatasmithMaterial ) const override;
	virtual void FinalizeMaterialInstance(const TSharedPtr< IDatasmithMaterialInstanceElement >& InDatasmithMaterial, UMaterialInstanceConstant* MaterialInstance) const override {}

private:
	FDatasmithReferenceMaterial ReferenceMaterialOpaque;
	FDatasmithReferenceMaterial ReferenceMaterialTranslucent;
	FDatasmithReferenceMaterial ReferenceMaterialEmissive;
};
