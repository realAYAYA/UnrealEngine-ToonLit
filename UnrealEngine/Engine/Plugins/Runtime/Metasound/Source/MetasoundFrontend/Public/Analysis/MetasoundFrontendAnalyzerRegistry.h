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
			virtual ~IVertexAnalyzerRegistry() = default;
			
			static IVertexAnalyzerRegistry& Get();

			virtual const IVertexAnalyzerFactory* FindAnalyzerFactory(FName InAnalyzerName) const = 0;
			virtual void RegisterAnalyzerFactory(FName AnalyzerName, TUniquePtr<IVertexAnalyzerFactory>&& Factory) = 0;
		};
	} // namespace Frontend
} // namespace Metasound

#define METASOUND_REGISTER_VERTEX_ANALYZER_FACTORY(ANALYZER_CLASS) \
	{ \
	Frontend::IVertexAnalyzerRegistry& AnalyzerRegistry = Frontend::IVertexAnalyzerRegistry::Get(); \
	AnalyzerRegistry.RegisterAnalyzerFactory( \
		ANALYZER_CLASS::GetAnalyzerName(), \
		MakeUnique<ANALYZER_CLASS::FFactory>()); \
	}
