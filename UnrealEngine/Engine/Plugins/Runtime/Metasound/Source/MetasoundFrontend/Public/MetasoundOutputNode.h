// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Internationalization/Text.h"
#include "MetasoundBuildError.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFrontendDataTypeTraits.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundNode.h"
#include "MetasoundVertex.h"
#include "Templates/SharedPointer.h"
#include "UObject/NameTypes.h"

#define LOCTEXT_NAMESPACE "MetasoundGraphCore"

namespace Metasound
{
	/** FMissingOutputNodeInputReferenceError
	 *
	 * Caused by Output not being able to generate an IOperator instance due to
	 * the type requiring an input reference (i.e. it is not default constructable).
	 */
	class FMissingOutputNodeInputReferenceError : public FBuildErrorBase
	{
	public:
		FMissingOutputNodeInputReferenceError(const INode& InNode, const FText& InDataType)
			: FBuildErrorBase(
				"MetasoundMissingOutputDataReferenceError",
				METASOUND_LOCTEXT_FORMAT("MissingOutputNodeInputReferenceError", "Missing required output node input reference for type {0}.", InDataType))
		{
			AddNode(InNode);
		}

		FMissingOutputNodeInputReferenceError(const INode& InNode)
			: FBuildErrorBase(
				"MetasoundMissingOutputDataReferenceError",
				METASOUND_LOCTEXT("MissingOutputNodeInputReferenceError", "Missing required output node input reference."))
		{
			AddNode(InNode);
		}

		virtual ~FMissingOutputNodeInputReferenceError() = default;
	};

	namespace OutputNodePrivate
	{
		class METASOUNDFRONTEND_API FOutputOperator : public FNoOpOperator
		{
			public:
				FOutputOperator(const FVertexName& InVertexName, const FAnyDataReference& InDataReference);
				virtual ~FOutputOperator() = default;

				virtual FDataReferenceCollection GetInputs() const override;
				virtual FDataReferenceCollection GetOutputs() const override;
				virtual void BindInputs(FInputVertexInterfaceData& InVertexData) override;
				virtual void BindOutputs(FOutputVertexInterfaceData& InVertexData) override;

			private:
				FVertexName VertexName;
				FAnyDataReference DataReference;
		};

		class METASOUNDFRONTEND_API FOutputNode : public FNode
		{
		private:
			class FOutputOperatorFactory : public IOperatorFactory
			{
			public:

				FOutputOperatorFactory(const FVertexName& InVertexName, const FName& InDataTypeName, EVertexAccessType InVertexAccessType);
				virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override;

			private:

				FVertexName VertexName;
				FName DataTypeName;
				EVertexAccessType VertexAccessType;
			};

			static FName GetVariantName(EVertexAccessType InVertexAccessType);
			static FVertexInterface GetVertexInterface(const FVertexName& InVertexName, const FName& InDataTypeName, EVertexAccessType InVertexAccessType);
			static FNodeClassMetadata GetNodeInfo(const FVertexName& InVertexName, const FName& InDataTypeName, EVertexAccessType InVertexAccessType);

			public:
				FOutputNode(const FName& InInstanceName, const FGuid& InInstanceID, const FVertexName& InVertexName, const FName& InDataType, EVertexAccessType InVertexAccessType);

				/** Return the current vertex interface. */
				virtual const FVertexInterface& GetVertexInterface() const override;

				/** Set the vertex interface. If the vertex was successfully changed, returns true. 
				 *
				 * @param InInterface - New interface for node. 
				 *
				 * @return True on success, false otherwise.
				 */
				virtual bool SetVertexInterface(const FVertexInterface& InInterface) override;

				/** Expresses whether a specific vertex interface is supported.
				 *
				 * @param InInterface - New interface. 
				 *
				 * @return True if the interface is supported, false otherwise. 
				 */
				virtual bool IsVertexInterfaceSupported(const FVertexInterface& InInterface) const;

				virtual TSharedRef<IOperatorFactory, ESPMode::ThreadSafe> GetDefaultOperatorFactory() const override;

			private:
				TSharedRef<FOutputOperatorFactory, ESPMode::ThreadSafe> Factory;
		};
	}

	/** Output nodes are used to expose graph data to external entities. */
	template<typename DataType, EVertexAccessType VertexAccess=EVertexAccessType::Reference>
	class TOutputNode : public OutputNodePrivate::FOutputNode
	{
	public:
		TOutputNode(const FVertexName& InInstanceName, const FGuid& InInstanceID, const FVertexName& InVertexName)
		:	FOutputNode(InInstanceName, InInstanceID, InVertexName, GetMetasoundDataTypeName<DataType>(), VertexAccess)
		{
		}
	};
}
#undef LOCTEXT_NAMESPACE // MetasoundOutputNode
