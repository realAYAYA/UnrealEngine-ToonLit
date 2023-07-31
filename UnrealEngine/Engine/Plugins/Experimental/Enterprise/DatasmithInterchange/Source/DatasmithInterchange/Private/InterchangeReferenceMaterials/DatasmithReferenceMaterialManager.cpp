// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeReferenceMaterials/DatasmithReferenceMaterialManager.h"

#include "InterchangeReferenceMaterials/DatasmithReferenceMaterialSelector.h"

namespace UE
{
	namespace DatasmithInterchange
	{
		TUniquePtr< FDatasmithReferenceMaterialManager > FDatasmithReferenceMaterialManager::Instance;

		void FDatasmithReferenceMaterialManager::Create()
		{
			Instance = MakeUnique< FDatasmithReferenceMaterialManager >();
		}

		void FDatasmithReferenceMaterialManager::Destroy()
		{
			Instance.Reset();
		}

		FDatasmithReferenceMaterialManager& FDatasmithReferenceMaterialManager::Get()
		{
			check(Instance.IsValid());
			return *Instance.Get();
		}

		FString FDatasmithReferenceMaterialManager::GetHostFromString(const FString& HostString)
		{
			if (HostString.Contains(TEXT("CityEngine")))
			{
				return TEXT("CityEngine");
			}
			else if (HostString.Contains(TEXT("Deltagen")))
			{
				return TEXT("Deltagen");
			}
			else if (HostString.Contains(TEXT("VRED")))
			{
				return TEXT("VRED");
			}
			else
			{
				return HostString;
			}
		}

		void FDatasmithReferenceMaterialManager::RegisterSelector(const TCHAR* Host, TSharedPtr< FDatasmithReferenceMaterialSelector > Selector)
		{
			Selectors.FindOrAdd(Host) = Selector;
		}

		void FDatasmithReferenceMaterialManager::UnregisterSelector(const TCHAR* Host)
		{
			Selectors.Remove(Host);
		}

		const TSharedPtr< FDatasmithReferenceMaterialSelector > FDatasmithReferenceMaterialManager::GetSelector(const TCHAR* Host) const
		{
			if (Selectors.Contains(Host))
			{
				return Selectors[Host];
			}
			else
			{
				return MakeShared< FDatasmithReferenceMaterialSelector >();
			}
		}
	}
}