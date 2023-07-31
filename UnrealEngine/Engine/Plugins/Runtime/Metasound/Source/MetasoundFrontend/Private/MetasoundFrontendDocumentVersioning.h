// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/UnrealString.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundFrontendDocument.h"
#include "UObject/NameTypes.h"


namespace Metasound
{
	namespace Frontend
	{
		/** Base class for versioning a document. */
		class METASOUNDFRONTEND_API FVersionDocument : public IDocumentTransform
		{
			const FName Name;
			const FString& Path;

		public:
			static FMetasoundFrontendVersionNumber GetMaxVersion()
			{
				return FMetasoundFrontendVersionNumber { 1, 10 };
			}

			FVersionDocument(FName InName, const FString& InPath);

			bool Transform(FDocumentHandle InDocument) const override;
		};
	} // namespace Frontend
} // namespace Metasound
