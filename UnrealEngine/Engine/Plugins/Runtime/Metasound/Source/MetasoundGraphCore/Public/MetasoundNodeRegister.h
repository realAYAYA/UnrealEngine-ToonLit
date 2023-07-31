// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace Metasound
{
	class METASOUNDGRAPHCORE_API FNodeFactoryRegister
	{
		public:
			void RegisterNodeFactory(const FString& InNodeTypeName, TUniquePtr<INodeFactory> InNodeFactory);

		private:
			TMap<FString, TUniquePtr<INodeFactory>> NodeFactories;
	};
}
