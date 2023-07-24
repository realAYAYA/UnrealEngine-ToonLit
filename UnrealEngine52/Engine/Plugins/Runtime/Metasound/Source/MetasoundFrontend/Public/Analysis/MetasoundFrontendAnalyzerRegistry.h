// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UObject/NameTypes.h"
#include "Templates/UniquePtr.h"

namespace Metasound
{
	// Forward Declarations
	class FDataReferenceCollection;

	namespace Frontend
	{
		// Forward Declarations
		class IVertexAnalyzerFactory;

		class METASOUNDFRONTEND_API IVertexAnalyzerRegistry
		{
		public:
			static IVertexAnalyzerRegistry& Get();

			virtual const IVertexAnalyzerFactory* FindAnalyzerFactory(FName InAnalyzerName) const = 0;
			virtual void RegisterAnalyzerFactories() = 0;
		};
	} // namespace Frontend
} // namespace Metasound
