// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DatasmithDefinitions.h"

class UMaterialInstanceConstant;

namespace UE
{
	namespace DatasmithInterchange
	{
		class FDatasmithReferenceMaterial;

		class DATASMITHINTERCHANGE_API FDatasmithReferenceMaterialSelector
		{
		public:
			virtual ~FDatasmithReferenceMaterialSelector() = default;

			virtual bool IsValid() const { return bIsValid; }
			virtual const TCHAR* GetMaterialPath(EDatasmithReferenceMaterialType MaterialType) const { return nullptr; }
#if WITH_EDITOR
			virtual void PostImportProcess(EDatasmithReferenceMaterialType MaterialType, EDatasmithReferenceMaterialQuality MaterialQuality, UMaterialInstanceConstant* MaterialInstance) const {}
#endif

		protected:
			bool bIsValid = false;
		};
	}
}

