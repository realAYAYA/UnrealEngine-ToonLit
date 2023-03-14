// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Internationalization/Text.h"
#include "MetasoundBuildError.h"
#include "MetasoundBuilderInterface.h"
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

		virtual ~FMissingOutputNodeInputReferenceError() = default;
	};

	namespace OutputNodePrivate
	{
		class METASOUNDFRONTEND_API FOutputOperator : public IOperator
		{
			public:
				FOutputOperator(const FVertexName& InVertexName, const FAnyDataReference& InDataReference);

				virtual ~FOutputOperator() = default;

				virtual FDataReferenceCollection GetInputs() const override;
				virtual FDataReferenceCollection GetOutputs() const override;
				virtual void Bind(FVertexInterfaceData& InVertexData) const;
				virtual FExecuteFunction GetExecuteFunction() override;

			private:
				FVertexName VertexName;
				FAnyDataReference DataReference;
		};
	}

	template<typename DataType, EVertexAccessType VertexAccess=EVertexAccessType::Reference>
	class TOutputNode : public FNode
	{
	private:
		static FName GetVariantName()
		{
			if constexpr (EVertexAccessType::Value == VertexAccess)
			{
				return FName("Constructor");
			}
			else
			{
				return FName();
			}
		}

		class FOutputOperatorFactory : public IOperatorFactory
		{
		public:
			FOutputOperatorFactory(const FVertexName& InDataReferenceName)
			:	DataReferenceName(InDataReferenceName)
			{
			}

			virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override
			{
				using namespace OutputNodePrivate;

				if (const FAnyDataReference* Ref = InParams.InputData.FindDataReference(DataReferenceName))
				{
					return MakeUnique<FOutputOperator>(DataReferenceName, *Ref);
				}

				// Only construct default if default construction is supported
				if constexpr (TIsParsable<DataType>::Value)
				{
					if constexpr (TIsConstructorVertexSupported<DataType>::Value)
					{
						// Prefer to make a constant value
						TDataValueReference<DataType> DefaultValueRef = TDataValueReferenceFactory<DataType>::CreateAny(InParams.OperatorSettings);
						return MakeUnique<FOutputOperator>(DataReferenceName, DefaultValueRef);
					}
					else
					{
						TDataReadReference<DataType> DefaultReadRef = TDataReadReferenceFactory<DataType>::CreateAny(InParams.OperatorSettings);
						return MakeUnique<FOutputOperator>(DataReferenceName, DefaultReadRef);
					}
				}

				OutResults.Errors.Emplace(MakeUnique<FMissingOutputNodeInputReferenceError>(InParams.Node, GetMetasoundDataTypeDisplayText<DataType>()));
				return TUniquePtr<IOperator>(nullptr);
			}

		private:
			FVertexName DataReferenceName;
		};

		static FVertexInterface GetVertexInterface(const FVertexName& InVertexName)
		{
			static const FText VertexDescription = METASOUND_LOCTEXT("Metasound_OutputVertexDescription", "Output from the parent Metasound graph.");

			return FVertexInterface(
				FInputVertexInterface(
					FInputDataVertex(InVertexName, GetMetasoundDataTypeName<DataType>(), FDataVertexMetadata{VertexDescription}, VertexAccess)
				),
				FOutputVertexInterface(
					FOutputDataVertex(InVertexName, GetMetasoundDataTypeName<DataType>(), FDataVertexMetadata{VertexDescription}, VertexAccess)
				)
			);
		}

		static FNodeClassMetadata GetNodeInfo(const FVertexName& InVertexName)
		{
			FNodeClassMetadata Info;

			Info.ClassName = { "Output", GetMetasoundDataTypeName<DataType>(), GetVariantName() };
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.Description = METASOUND_LOCTEXT("Metasound_OutputNodeDescription", "Output from the parent Metasound graph.");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = GetVertexInterface(InVertexName);

			return Info;
		};

		public:
			TOutputNode(const FVertexName& InInstanceName, const FGuid& InInstanceID, const FVertexName& InVertexName)
			:	FNode(InInstanceName, InInstanceID, GetNodeInfo(InVertexName))
			,	VertexInterface(GetVertexInterface(InVertexName))
			,	Factory(MakeShared<FOutputOperatorFactory, ESPMode::ThreadSafe>(InVertexName))
			{
			}

			/** Return the current vertex interface. */
			virtual const FVertexInterface& GetVertexInterface() const override
			{
				return VertexInterface;
			}

			/** Set the vertex interface. If the vertex was successfully changed, returns true. 
			 *
			 * @param InInterface - New interface for node. 
			 *
			 * @return True on success, false otherwise.
			 */
			virtual bool SetVertexInterface(const FVertexInterface& InInterface) override
			{
				return VertexInterface == InInterface;
			}

			/** Expresses whether a specific vertex interface is supported.
			 *
			 * @param InInterface - New interface. 
			 *
			 * @return True if the interface is supported, false otherwise. 
			 */
			virtual bool IsVertexInterfaceSupported(const FVertexInterface& InInterface) const 
			{
				return VertexInterface == InInterface;
			}

			virtual TSharedRef<IOperatorFactory, ESPMode::ThreadSafe> GetDefaultOperatorFactory() const override
			{
				return Factory;
			}

		private:
			FVertexInterface VertexInterface;

			TSharedRef<FOutputOperatorFactory, ESPMode::ThreadSafe> Factory;

	};
}
#undef LOCTEXT_NAMESPACE // MetasoundOutputNode
