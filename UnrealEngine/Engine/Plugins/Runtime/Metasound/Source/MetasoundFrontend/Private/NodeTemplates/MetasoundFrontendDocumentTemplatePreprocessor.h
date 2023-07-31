// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendTransform.h"


namespace Metasound
{
	namespace Frontend
	{
		// Preprocesses all template node classes and respective node instances within
		// a given document and removes template classes from document dependencies list.
		class FDocumentTemplatePreprocessTransform : public IDocumentTransform
		{
		public:
			UE_DEPRECATED(5.1, "Transforms acting on handles/controllers are deprecated.")
			virtual bool Transform(FDocumentHandle InDocument) const override;

			virtual bool Transform(FMetasoundFrontendDocument& InOutDocument) const override;
		};

		// Base implementation for preprocessing a given template node
		class FNodeTemplatePreprocessTransformBase : public INodeTransform
		{
		protected:
			FMetasoundFrontendDocument& Document;
			FMetasoundFrontendGraph& Graph;

		public:
			FNodeTemplatePreprocessTransformBase(FMetasoundFrontendDocument& InDocument)
				: Document(InDocument)
				, Graph(InDocument.RootGraph.Graph)
			{
			}

			virtual ~FNodeTemplatePreprocessTransformBase() = default;

			virtual FMetasoundFrontendDocument& GetOwningDocument() const override { return Document; };
			virtual FMetasoundFrontendGraph& GetOwningGraph() const override { return Graph; };
		};

	} // namespace Frontend
} // namespace Metasound
