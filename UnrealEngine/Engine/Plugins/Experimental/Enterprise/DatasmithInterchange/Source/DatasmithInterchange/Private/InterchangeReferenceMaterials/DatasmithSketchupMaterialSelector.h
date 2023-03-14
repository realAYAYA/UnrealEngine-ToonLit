// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DatasmithDefinitions.h"
#include "InterchangeReferenceMaterials/DatasmithReferenceMaterialSelector.h"

namespace UE
{
	namespace DatasmithInterchange
	{
		class FDatasmithSketchUpMaterialSelector : public FDatasmithReferenceMaterialSelector
		{
		public:
			FDatasmithSketchUpMaterialSelector();
			virtual ~FDatasmithSketchUpMaterialSelector() = default;

			virtual const TCHAR* GetMaterialPath(EDatasmithReferenceMaterialType MaterialType) const override;
#if WITH_EDITOR
			virtual void PostImportProcess(EDatasmithReferenceMaterialType MaterialType, EDatasmithReferenceMaterialQuality MaterialQuality, UMaterialInstanceConstant* MaterialInstance) const override;
#endif
		};
	}
}