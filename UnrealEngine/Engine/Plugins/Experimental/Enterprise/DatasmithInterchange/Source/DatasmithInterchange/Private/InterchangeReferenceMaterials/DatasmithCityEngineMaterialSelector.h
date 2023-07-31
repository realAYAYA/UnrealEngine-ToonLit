// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DatasmithDefinitions.h"
#include "InterchangeReferenceMaterials/DatasmithReferenceMaterialSelector.h"

#include "Templates/SharedPointer.h"

namespace UE
{
	namespace DatasmithInterchange
	{
		class FDatasmithCityEngineMaterialSelector : public FDatasmithReferenceMaterialSelector
		{
		public:
			FDatasmithCityEngineMaterialSelector();

			virtual const TCHAR* GetMaterialPath(EDatasmithReferenceMaterialType MaterialType) const override;
#if WITH_EDITOR
			virtual void PostImportProcess(EDatasmithReferenceMaterialType MaterialType, EDatasmithReferenceMaterialQuality MaterialQuality, UMaterialInstanceConstant* MaterialInstance) const override;
#endif
		};
	}
}
