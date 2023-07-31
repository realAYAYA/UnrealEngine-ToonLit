// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/ContainersFwd.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"

namespace UE
{
	namespace DatasmithInterchange
	{
		class FDatasmithReferenceMaterialSelector;

		class DATASMITHINTERCHANGE_API FDatasmithReferenceMaterialManager
		{
		public:
			static void Create();
			static void Destroy();
			static FDatasmithReferenceMaterialManager& Get();

			FString GetHostFromString(const FString& HostString);

			void RegisterSelector(const TCHAR* Host, TSharedPtr< FDatasmithReferenceMaterialSelector > Selector);
			void UnregisterSelector(const TCHAR* Host);

			const TSharedPtr< FDatasmithReferenceMaterialSelector > GetSelector(const TCHAR* Host) const;

		private:
			static TUniquePtr< FDatasmithReferenceMaterialManager > Instance;

			TMap< FString, TSharedPtr< FDatasmithReferenceMaterialSelector > > Selectors;
		};
	}
}
