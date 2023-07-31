// Copyright Epic Games, Inc. All Rights Reserved.
#include "Analysis/MetasoundFrontendAnalyzerRegistry.h"

#include "Analysis/MetasoundFrontendAnalyzerFactory.h"
#include "Analysis/MetasoundFrontendVertexAnalyzerEnvelopeFollower.h"
#include "Analysis/MetasoundFrontendVertexAnalyzerForwardValue.h"
#include "Analysis/MetasoundFrontendVertexAnalyzerTriggerDensity.h"
#include "MetasoundPrimitives.h"


namespace Metasound
{
	namespace Frontend
	{
		class FVertexAnalyzerRegistry : public IVertexAnalyzerRegistry
		{
			TMap<FName, TUniquePtr<IVertexAnalyzerFactory>> AnalyzerFactoryRegistry;

			template<typename TAnalyzerFactoryClass>
			void RegisterAnalyzerFactory()
			{
				TUniquePtr<IVertexAnalyzerFactory> Factory(new TAnalyzerFactoryClass());
				AnalyzerFactoryRegistry.Emplace(TAnalyzerFactoryClass::GetAnalyzerName(), MoveTemp(Factory));
			}

		public:
			FVertexAnalyzerRegistry() = default;
			virtual ~FVertexAnalyzerRegistry() = default;

			virtual const IVertexAnalyzerFactory* FindAnalyzerFactory(FName InAnalyzerName) const override
			{
				const TUniquePtr<IVertexAnalyzerFactory>* Factory = AnalyzerFactoryRegistry.Find(InAnalyzerName);
				if (ensureMsgf(Factory, TEXT("Failed to find registered MetaSound Analyzer Factory with name '%s'"), *InAnalyzerName.ToString()))
				{
					check(Factory->IsValid());
					return Factory->Get();
				}

				return nullptr;
			}

			virtual void RegisterAnalyzerFactories() override
			{
				RegisterAnalyzerFactory<FVertexAnalyzerEnvelopeFollower::FFactory>();
				RegisterAnalyzerFactory<FVertexAnalyzerTriggerDensity::FFactory>();

				// Primitives
				RegisterAnalyzerFactory<FVertexAnalyzerForwardBool::FFactory>();
				RegisterAnalyzerFactory<FVertexAnalyzerForwardFloat::FFactory>();
				RegisterAnalyzerFactory<FVertexAnalyzerForwardInt::FFactory>();
				RegisterAnalyzerFactory<FVertexAnalyzerForwardString::FFactory>();
			}
		};

		IVertexAnalyzerRegistry& IVertexAnalyzerRegistry::Get()
		{
			static FVertexAnalyzerRegistry Registry;
			return Registry;
		}
	} // namespace Frontend
} // namespace Metasound
