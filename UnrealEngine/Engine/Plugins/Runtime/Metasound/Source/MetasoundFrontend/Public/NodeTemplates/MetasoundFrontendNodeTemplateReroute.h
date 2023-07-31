// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendNodeTemplateRegistry.h"
#include "MetasoundFrontendTransform.h"
#include "Templates/SharedPointer.h"
#include "UObject/NoExportTypes.h"


namespace Metasound
{
	namespace Frontend
	{
		class METASOUNDFRONTEND_API FRerouteNodeTemplate : public INodeTemplate
		{
		public:
			static const FMetasoundFrontendClassName ClassName;
			static const FMetasoundFrontendVersion Version;

			static const FNodeRegistryKey& GetRegistryKey();

			static FMetasoundFrontendNodeInterface CreateNodeInterfaceFromDataType(FName InDataType);

			virtual ~FRerouteNodeTemplate() = default;

			virtual TUniquePtr<INodeTransform> GenerateNodeTransform(FMetasoundFrontendDocument& InPreprocessedDocument) const override;
			virtual const FMetasoundFrontendClass& GetFrontendClass() const override;
			virtual const FMetasoundFrontendVersion& GetVersion() const override;
			virtual bool IsValidNodeInterface(const FMetasoundFrontendNodeInterface& InNodeInterface) const override;

#if WITH_EDITOR
			virtual bool HasRequiredConnections(FConstNodeHandle InNodeHandle) const override;
#endif // WITH_EDITOR
		};
	} // namespace Frontend
} // namespace Metasound
