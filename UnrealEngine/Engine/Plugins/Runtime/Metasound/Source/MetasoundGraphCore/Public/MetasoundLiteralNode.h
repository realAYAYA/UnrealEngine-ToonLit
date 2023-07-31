// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Internationalization/Text.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundNode.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundVertex.h"

#define LOCTEXT_NAMESPACE "MetasoundGraphCore"


namespace Metasound
{
	namespace LiteralNodeNames
	{
		METASOUND_PARAM(OutputValue, "Value", "Value")
	}

	template<typename DataType>
	class TLiteralOperator : public IOperator
	{
		static_assert(!TExecutableDataType<DataType>::bIsExecutable, "TLiteralOperator is only suitable for non-executable data types");

	public:
		TLiteralOperator(TDataValueReference<DataType> InValue)
		: Value (InValue)
		{
		}

		virtual ~TLiteralOperator() = default;

		virtual void Bind(FVertexInterfaceData& InVertexData) const
		{
			using namespace LiteralNodeNames;
			InVertexData.GetOutputs().BindValueVertex(METASOUND_GET_PARAM_NAME(OutputValue), Value);
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			FDataReferenceCollection Inputs;
			return Inputs;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace LiteralNodeNames;
			FDataReferenceCollection Outputs;
			Outputs.AddDataReference(METASOUND_GET_PARAM_NAME(OutputValue), FAnyDataReference{Value});
			return Outputs;
		}

		virtual FExecuteFunction GetExecuteFunction() override
		{
			return nullptr;
		}

	private:
		TDataValueReference<DataType> Value;
	};

	/** TExecutableLiteralOperator is used for executable types. The majority of literals
	 * are constant values, but executable data types cannot be constant values.
	 * 
	 * Note: this is supporting for FTrigger literals which are deprecated.
	 */
	template<typename DataType>
	class TExecutableLiteralOperator : public IOperator
	{
		static_assert(TExecutableDataType<DataType>::bIsExecutable, "TExecutableLiteralOperator is only suitable for executable data types");

	public:
		using FDataWriteReference = TDataWriteReference<DataType>;

		TExecutableLiteralOperator(FDataWriteReference InDataReference)
			// Executable DataTypes require a copy of the output to operate on whereas non-executable
			// types do not. Avoid copy by assigning to reference for non-executable types.
			: InputValue(InDataReference)
			, OutputValue(FDataWriteReference::CreateNew(*InDataReference))
		{
		}

		virtual ~TExecutableLiteralOperator() = default;

		virtual void Bind(FVertexInterfaceData& InVertexData) const
		{
			using namespace LiteralNodeNames;
			InVertexData.GetOutputs().BindReadVertex(METASOUND_GET_PARAM_NAME(OutputValue), OutputValue);
		}

		virtual FDataReferenceCollection GetInputs() const override
		{
			FDataReferenceCollection Inputs;
			return Inputs;
		}

		virtual FDataReferenceCollection GetOutputs() const override
		{
			using namespace LiteralNodeNames;
			FDataReferenceCollection Outputs;
			Outputs.AddDataReadReference<DataType>(METASOUND_GET_PARAM_NAME(OutputValue), OutputValue);
			return Outputs;
		}

		void Execute()
		{
			TExecutableDataType<DataType>::Execute(*InputValue, *OutputValue);
		}

		static void ExecuteFunction(IOperator* InOperator)
		{
			static_cast<TExecutableLiteralOperator<DataType>*>(InOperator)->Execute();
		}

		virtual FExecuteFunction GetExecuteFunction() override
		{
			return &TExecutableLiteralOperator<DataType>::ExecuteFunction;
		}

	private:
		FDataWriteReference InputValue;
		FDataWriteReference OutputValue;
	};


	/** TLiteralOperatorLiteralFactory creates an input by passing it a literal. */
	template<typename DataType>
	class TLiteralOperatorLiteralFactory : public IOperatorFactory
	{
	public:

		using FDataWriteReference = TDataWriteReference<DataType>;

		TLiteralOperatorLiteralFactory(FLiteral&& InInitParam)
			: InitParam(MoveTemp(InInitParam))
		{
		}

		virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override;

	private:
		FLiteral InitParam;
	};

	/** TLiteralNode represents a variable within a metasound graph. */
	template<typename DataType>
	class TLiteralNode : public FNode
	{
		// Executable data types handle Triggers which need to be advanced every
		// block.
		static constexpr bool bIsExecutableDataType = TExecutableDataType<DataType>::bIsExecutable;

	public:

		static FVertexInterface DeclareVertexInterface()
		{
			using namespace LiteralNodeNames; 
			static const FDataVertexMetadata OutputMetadata
			{
				  FText::GetEmpty() // description
				, METASOUND_GET_PARAM_DISPLAYNAME(OutputValue) // display name
			};

			if constexpr (bIsExecutableDataType)
			{
				return FVertexInterface(
					FInputVertexInterface(),
					FOutputVertexInterface(
						TOutputDataVertex<DataType>(METASOUND_GET_PARAM_NAME(OutputValue), OutputMetadata)
					)
				);
			}
			else
			{
				// If the data type is not executable, we treat it as a constructor
				// vertex to produce a constant value.
				return FVertexInterface(
					FInputVertexInterface(),
					FOutputVertexInterface(
						TOutputConstructorVertex<DataType>(METASOUND_GET_PARAM_NAME(OutputValue), OutputMetadata)
					)
				);
			}
		}

		static FNodeClassMetadata GetNodeInfo()
		{
			FNodeClassMetadata Info;

			Info.ClassName = {"Literal", GetMetasoundDataTypeName<DataType>(), ""};
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
#if WITH_EDITOR
			Info.DisplayName = FText::Format(LOCTEXT("Metasound_LiteralNodeDisplayNameFormat", "Literal {0}"), FText::FromName(GetMetasoundDataTypeName<DataType>()));
			Info.Description = LOCTEXT("Metasound_LiteralNodeDescription", "Literal accessible within a parent Metasound graph.");
#endif // WITH_EDITOR
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = DeclareVertexInterface();

			return Info;
		}

		/* Construct a TLiteralNode using the TLiteralOperatorLiteralFactory<> and moving
		 * InParam to the TLiteralOperatorLiteralFactory constructor.*/
		explicit TLiteralNode(const FVertexName& InInstanceName, const FGuid& InInstanceID, FLiteral&& InParam)
		:	FNode(InInstanceName, InInstanceID, GetNodeInfo())
		,	Interface(DeclareVertexInterface())
		, 	Factory(MakeOperatorFactoryRef<TLiteralOperatorLiteralFactory<DataType>>(MoveTemp(InParam)))
		{
		}

		virtual const FVertexInterface& GetVertexInterface() const override
		{
			return Interface;
		}

		virtual bool SetVertexInterface(const FVertexInterface& InInterface) override
		{
			return Interface == InInterface;
		}

		virtual bool IsVertexInterfaceSupported(const FVertexInterface& InInterface) const override
		{
			return Interface == InInterface;
		}

		virtual TSharedRef<IOperatorFactory, ESPMode::ThreadSafe> GetDefaultOperatorFactory() const override
		{
			return Factory;
		}

	private:
		FVertexInterface Interface;
		FOperatorFactorySharedRef Factory;
	};

	template<typename DataType>
	TUniquePtr<IOperator> TLiteralOperatorLiteralFactory<DataType>::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using FLiteralNodeType = TLiteralNode<DataType>;

		static constexpr bool bIsExecutableDataType = TExecutableDataType<DataType>::bIsExecutable;

		if constexpr (bIsExecutableDataType)
		{
			// Create write reference by calling compatible constructor with literal.
			TDataWriteReference<DataType> DataRef = TDataWriteReferenceLiteralFactory<DataType>::CreateExplicitArgs(InParams.OperatorSettings, InitParam);
			return MakeUnique<TExecutableLiteralOperator<DataType>>(DataRef);
		}
		else
		{
			// Create value reference by calling compatible constructor with literal.
			TDataValueReference DataRef = TDataValueReferenceLiteralFactory<DataType>::CreateExplicitArgs(InParams.OperatorSettings, InitParam);
			return MakeUnique<TLiteralOperator<DataType>>(DataRef);
		}
	}

} // namespace Metasound
#undef LOCTEXT_NAMESPACE // MetasoundGraphCore

