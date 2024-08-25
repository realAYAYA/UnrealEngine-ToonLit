// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "MetasoundFrontendDocument.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Guid.h"

namespace Metasound
{
	namespace Frontend
	{
		extern METASOUNDFRONTEND_API int32 MetaSoundEnableCookDeterministicIDGeneration;

		/*** 
		 * For generating IDs using a given document. 
		 * USAGE:
		 *
		 * If you want everything within the calling scope to be deterministic, use
		 * the scope determinism lock like you would a `FScopeLock`
		 *
		 * {
		 * 		constexpr bool bIsDeterministic = true;
		 * 		FDocumentIDGenerator::FScopeDeterminism DeterminismScope(bIsDeterministic);
		 *
		 *      // Anything called in this scope will use a deterministic ID generator.
		 * 		// Once the `DeterminismScope` variable is destroyed, it will return to
		 * 		// whatever the prior behavior was
		 * 		MetaSoundAsset->UpdateOrWhatever();
		 * }
		 *
		 * void FMetaSoundAsset::UpdateOrWhatever()
		 * {
		 *      // Anytime you call IDGen::Create*ID(..), it will obey whatever the outside scope set it to be.
		 *      AddNewNodeOrWhatever(...)
		 * }
		 *
		 * void FMetaSoundAsset::AddNodeOrWhatever(...)
		 * {
		 * 		FDocumentIDGenerator& IDGen = FDocumentIDGenerator::Get();
		 *
		 * 		FGuid NewNodeID = IDGen::CreateNewNodeID();
		 * 		...
		 * }
		 *
		 */
		class METASOUNDFRONTEND_API FDocumentIDGenerator
		{
		public:
			class METASOUNDFRONTEND_API FScopeDeterminism final
			{
			public:
				FScopeDeterminism(bool bInIsDeterministic);
				bool GetDeterminism() const;
				~FScopeDeterminism();

			private:
				bool bOriginalValue = false;
			};

			static FDocumentIDGenerator& Get();

			FGuid CreateNodeID(const FMetasoundFrontendDocument& InDocument) const;
			FGuid CreateVertexID(const FMetasoundFrontendDocument& InDocument) const;
			FGuid CreateClassID(const FMetasoundFrontendDocument& InDocument) const;

			FGuid CreateIDFromDocument(const FMetasoundFrontendDocument& InDocument) const;

		private:
			FDocumentIDGenerator() = default;

			friend class FScopeDeterminism;
			void SetDeterminism(bool bInIsDeterministic);
			bool GetDeterminism() const;

			bool bIsDeterministic = false;
		};

		// For generating IDs that are not derived from a given document. 
		class FClassIDGenerator
		{
		public: 
			static FClassIDGenerator& Get();

			FGuid CreateInputID(const FMetasoundFrontendClassInput& Input) const;
			FGuid CreateInputID(const Audio::FParameterInterface::FInput& Input) const;
			FGuid CreateOutputID(const FMetasoundFrontendClassOutput& Output) const;
			FGuid CreateOutputID(const Audio::FParameterInterface::FOutput& Output) const;

			FGuid CreateNamespacedIDFromString(const FGuid NamespaceGuid, const FString& StringToHash) const;
		};

		METASOUNDFRONTEND_API FGuid CreateLocallyUniqueId();
	}
}
