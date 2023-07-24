// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundBuilderInterface.h"
#include "MetasoundDataFactory.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFacade.h"
#include "MetasoundLog.h"
#include "MetasoundNode.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundParamHelper.h"
#include "MetasoundVariable.h"
#include "MetasoundVertex.h"

#define LOCTEXT_NAMESPACE "MetasoundGraphCore"


namespace Metasound
{
	namespace VariableNames
	{
		METASOUND_PARAM(InputData, "Value", "Value")
		METASOUND_PARAM(OutputData, "Value", "Value")
		METASOUND_PARAM(InputVariable, "Variable", "Variable")
		METASOUND_PARAM(OutputVariable, "Variable", "Variable")

		static const FDataVertexMetadata InputDataMetadata
		{
			  FText::GetEmpty() // description
			, METASOUND_GET_PARAM_DISPLAYNAME(InputData) // display name
		};

		static const FDataVertexMetadata OutputDataMetadata
		{
			  FText::GetEmpty() // description
			, METASOUND_GET_PARAM_DISPLAYNAME(OutputData) // display name
		};

		static const FDataVertexMetadata InputVariableMetadata
		{
			  FText::GetEmpty() // description
			, METASOUND_GET_PARAM_DISPLAYNAME(InputVariable) // display name
		};

		static const FDataVertexMetadata OutputVariableMetadata
		{
			  FText::GetEmpty() // description
			, METASOUND_GET_PARAM_DISPLAYNAME(OutputVariable) // display name
		};

		/** Class name for variable node. */
		METASOUNDGRAPHCORE_API FNodeClassName GetVariableNodeClassName(const FName& InDataTypeName);

		/** Class name for variable mutator node. */
		METASOUNDGRAPHCORE_API FNodeClassName GetVariableMutatorNodeClassName(const FName& InDataTypeName);

		/** Class name for variable accessor node */
		METASOUNDGRAPHCORE_API FNodeClassName GetVariableAccessorNodeClassName(const FName& InDataTypeName);

		/** Class name for variable deferred accessor node. */
		METASOUNDGRAPHCORE_API FNodeClassName GetVariableDeferredAccessorNodeClassName(const FName& InDataTypeName);

		/** Class name for variable node. */
		template<typename DataType>
		const FNodeClassName& GetVariableNodeClassName()
		{
			static const FNodeClassName ClassName = GetVariableNodeClassName(GetMetasoundDataTypeName<DataType>());
			return ClassName;
		}

		/** Class name for variable mutator node. */
		template<typename DataType>
		const FNodeClassName& GetVariableMutatorNodeClassName()
		{
			static const FNodeClassName ClassName = GetVariableMutatorNodeClassName(GetMetasoundDataTypeName<DataType>());
			return ClassName;
		}

		/** Class name for variable accessor node */
		template<typename DataType>
		const FNodeClassName& GetVariableAccessorNodeClassName()
		{
			static const FNodeClassName ClassName = GetVariableAccessorNodeClassName(GetMetasoundDataTypeName<DataType>());
			return ClassName;
		}

		/** Class name for variable deferred accessor node. */
		template<typename DataType>
		const FNodeClassName& GetVariableDeferredAccessorNodeClassName()
		{
			static const FNodeClassName ClassName = GetVariableDeferredAccessorNodeClassName(GetMetasoundDataTypeName<DataType>());
			return ClassName;
		}

	}

	/** Variable nodes initialize variable values. The output of a VariableNode
	 * is a TVariable.  */
	template<typename DataType>
	class TVariableNode : public FNode
	{
		using FVariable = TVariable<DataType>;

		class FOperator : public TExecutableOperator<FOperator>
		{
			using Super = TExecutableOperator<FOperator>;

		public:
			FOperator(TDataWriteReference<FVariable> InVariable)
			: Variable(InVariable)
			, bCopyReferenceDataOnExecute(false)
			{
			}

			virtual ~FOperator() = default;

			virtual FDataReferenceCollection GetInputs() const override
			{
				FDataReferenceCollection Collection;
				return Collection;
			}

			virtual FDataReferenceCollection GetOutputs() const override
			{
				using namespace VariableNames; 

				FDataReferenceCollection Collection;
				Collection.AddDataWriteReference(METASOUND_GET_PARAM_NAME(OutputVariable), Variable);
				return Collection;
			}

			void Execute()
			{
				if (bCopyReferenceDataOnExecute)
				{
					Variable->CopyReferencedData();
				}
				else
				{
					// The first time a variable node is run, it should not copy
					// reference data in the variable, but instead use the original
					// initial value of the variable. 
					//
					// The TVariableNode is currently executed before the TVariableDeferredAccessor
					// nodes. This execution order is managed in the Metasound::Frontend::FGraphController.
					// Because the TVariableNode is executed before the TVariableDeferredAccessor node
					// it can undesireably override the "init" value of the variable with the "init" 
					// value of the data reference set in the TVariableMutatorNode. 
					// This would mean that the first call to execute on TVariableDeferredAccessor
					// would result in reading the "init" value of the data reference
					// set in the TVariableMutatorNode as opposed to the "init" value
					// of the data reference set in the TVariableNode. This boolean
					// protects against that situation so that on first call to execute, the 
					// TVariableDeferredAccessor always reads the "init" value provided
					// by the TVariableNode. 
					bCopyReferenceDataOnExecute = true;
				}
			}

		private:

			TDataWriteReference<FVariable> Variable;
			bool bCopyReferenceDataOnExecute;
		};

		class FFactory : public IOperatorFactory
		{
		public:
			FFactory(FLiteral&& InLiteral)
			: Literal(MoveTemp(InLiteral))
			{
			}

			virtual ~FFactory() = default;

			virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override
			{
				TDataWriteReference<DataType> Data = TDataWriteReferenceLiteralFactory<DataType>::CreateExplicitArgs(InParams.OperatorSettings, Literal);
				return MakeUnique<FOperator>(TDataWriteReference<FVariable>::CreateNew(Data));
			}
		private:
			FLiteral Literal;
		};


		static FVertexInterface DeclareVertexInterface()
		{
			using namespace VariableNames; 

			return FVertexInterface(
				FInputVertexInterface(
				),
				FOutputVertexInterface(
					TOutputDataVertex<FVariable>(METASOUND_GET_PARAM_NAME(OutputVariable), OutputVariableMetadata)
				)
			);
		}

		static FNodeClassMetadata GetNodeMetadata()
		{
			FNodeClassMetadata Info;

			Info.ClassName = VariableNames::GetVariableNodeClassName<DataType>();
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;

#if WITH_EDITOR
			Info.DisplayStyle.bShowName = false;
			Info.Description = LOCTEXT("Metasound_InitVariableNodeDescription", "Initialize a variable of a MetaSound graph.");
			Info.CategoryHierarchy = { LOCTEXT("Metasound_VariableCategory", "Variable") };
#endif // WITH_EDITOR
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = DeclareVertexInterface();

			return Info;
		}

	public:

		TVariableNode(const FVertexName& InNodeName, const FGuid& InInstanceID, FLiteral&& InLiteral)
		: FNode(InNodeName, InInstanceID, GetNodeMetadata())
		, Interface(DeclareVertexInterface())
		, Factory(MakeShared<FFactory>(MoveTemp(InLiteral)))
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

	/** TVariableMutatorNode allows variable values to be set.  */
	template<typename DataType>
	class TVariableMutatorNode : public FNodeFacade
	{
		using FVariable = TVariable<DataType>;

		class FOperator : public FNoOpOperator 
		{
		public:
			static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);
			static FVertexInterface DeclareVertexInterface();
			static const FNodeClassMetadata& GetNodeInfo();

			FOperator(TDataReadReference<FVariable> InVariable)
			: Variable(InVariable)
			{
			}

			virtual ~FOperator() = default;

			virtual FDataReferenceCollection GetOutputs() const override
			{
				using namespace VariableNames; 

				FDataReferenceCollection Collection;
				Collection.AddDataReadReference<FVariable>(METASOUND_GET_PARAM_NAME(OutputVariable), Variable);
				return Collection;
			}

		private:
			TDataReadReference<FVariable> Variable;
		};

	public:
		TVariableMutatorNode(const FVertexName& InNodeName, const FGuid& InInstanceID)
		: FNodeFacade(InNodeName, InInstanceID, TFacadeOperatorClass<FOperator>())
		{
		}

		TVariableMutatorNode(const FNodeInitData& InInitData)
		: TVariableMutatorNode(InInitData.InstanceName, InInitData.InstanceID)
		{}
	};

	template<typename DataType>
	TUniquePtr<IOperator> TVariableMutatorNode<DataType>::FOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors) 
	{
		using namespace VariableNames;

		const FDataReferenceCollection& Inputs = InParams.InputDataReferences;

		if (Inputs.ContainsDataWriteReference<FVariable>(METASOUND_GET_PARAM_NAME(InputVariable)))
		{
			// If a variable is provided, set the reference to read.
			TDataWriteReference<FVariable> Variable = Inputs.GetDataWriteReference<FVariable>(METASOUND_GET_PARAM_NAME(InputVariable));
			if (Inputs.ContainsDataReadReference<DataType>(METASOUND_GET_PARAM_NAME(InputData)))
			{
				// Update the input variable with the data reference to copy from.
				TDataReadReference InputData = Inputs.GetDataReadReference<DataType>(METASOUND_GET_PARAM_NAME(InputData));
				Variable->SetDataReference(InputData);
			}

			return MakeUnique<FOperator>(Variable);
		}
		else if (Inputs.ContainsDataReadReference<DataType>(METASOUND_GET_PARAM_NAME(InputData)))
		{
			// If no input variable is provided create Variable with input variable
			TDataReadReference<DataType> InputData = Inputs.GetDataReadReference<DataType>(METASOUND_GET_PARAM_NAME(InputData));
			TDataWriteReference<DataType> InitData = TDataWriteReference<DataType>::CreateNew(*InputData);
			TDataWriteReference<FVariable> Variable = TDataWriteReference<FVariable>::CreateNew(InitData);
			Variable->SetDataReference(InputData);
			return MakeUnique<FOperator>(Variable);
		}
		else 
		{
			// Nothing to do if there's no input data.
			return MakeUnique<FNoOpOperator>();
		}
	}

	template<typename DataType>
	FVertexInterface TVariableMutatorNode<DataType>::FOperator::DeclareVertexInterface()
	{
		using namespace VariableNames;

		return FVertexInterface(
			FInputVertexInterface(
				TInputDataVertex<DataType>(METASOUND_GET_PARAM_NAME(InputData), InputDataMetadata),
				TInputDataVertex<FVariable>(METASOUND_GET_PARAM_NAME(InputVariable), InputVariableMetadata)
			),
			FOutputVertexInterface(
				TOutputDataVertex<FVariable>(METASOUND_GET_PARAM_NAME(OutputVariable), OutputVariableMetadata)
			)
		);
	}

	template<typename DataType>
	const FNodeClassMetadata& TVariableMutatorNode<DataType>::FOperator::GetNodeInfo()
	{
		auto CreateNodeClassMetadata = []()
		{
			FNodeClassMetadata Info;

			Info.ClassName = VariableNames::GetVariableMutatorNodeClassName<DataType>();
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;

#if WITH_EDITOR
			Info.DisplayStyle.bShowName = false;
			Info.DisplayName = LOCTEXT("Metasound_VariableMutatorNodeDisplayName", "Set");
			Info.Description = LOCTEXT("Metasound_VariableMutatorNodeDescription", "Set variable on MetaSound graph.");
			Info.CategoryHierarchy = { LOCTEXT("Metasound_VariableCategory", "Variable") };
#endif // WITH_EDITOR
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = DeclareVertexInterface();

			return Info;
		};

		static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
		return Metadata;
	}

	/** TVariableDeferredAccessorNode provides access to the prior executions variable value.
	 * TVariableDeferredAccessorNodes must always be before TVariableMutatorNodes in the dependency
	 * order.
	 */
	template<typename DataType>
	class TVariableDeferredAccessorNode : public FNodeFacade
	{
		using FVariable = TVariable<DataType>;

		class FOperator : public FNoOpOperator 
		{
		public:
			static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);
			static FVertexInterface DeclareVertexInterface();
			static const FNodeClassMetadata& GetNodeInfo();

			FOperator(TDataWriteReference<FVariable> InDelayedVariable)
			: DelayedVariable(InDelayedVariable)
			, DelayedData(DelayedVariable->GetDelayedDataReference())
			{
			}

			virtual ~FOperator() = default;

			virtual FDataReferenceCollection GetOutputs() const override
			{
				using namespace VariableNames; 

				FDataReferenceCollection Collection;
				Collection.AddDataWriteReference<FVariable>(METASOUND_GET_PARAM_NAME(OutputVariable), DelayedVariable);
				Collection.AddDataReadReference<DataType>(METASOUND_GET_PARAM_NAME(OutputData), DelayedVariable->GetDelayedDataReference());
				return Collection;
			}

		private:
			TDataWriteReference<FVariable> DelayedVariable;
			TDataReadReference<DataType> DelayedData;
		};

	public:

		TVariableDeferredAccessorNode(const FVertexName& InNodeName, const FGuid& InInstanceID)
		: FNodeFacade(InNodeName, InInstanceID, TFacadeOperatorClass<FOperator>())
		{
		}

		TVariableDeferredAccessorNode(const FNodeInitData& InInitData)
		: TVariableDeferredAccessorNode(InInitData.InstanceName, InInitData.InstanceID)
		{}
	};

	template<typename DataType>
	TUniquePtr<IOperator> TVariableDeferredAccessorNode<DataType>::FOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors) 
	{
		using namespace VariableNames;

		const FDataReferenceCollection& Inputs = InParams.InputDataReferences;

		if (ensure(Inputs.ContainsDataWriteReference<FVariable>(METASOUND_GET_PARAM_NAME(InputVariable))))
		{
			TDataWriteReference<FVariable> DelayedVariable = Inputs.GetDataWriteReference<FVariable>(METASOUND_GET_PARAM_NAME(InputVariable));

			return MakeUnique<FOperator>(DelayedVariable);
		}
		else 
		{
			// Nothing to do if there's no input data.
			UE_LOG(LogMetaSound, Warning, TEXT("Missing internal variable connection. Failed to create valid \"GetDelayedVariable\" operator"));
			return MakeUnique<FNoOpOperator>();
		}
	}

	template<typename DataType>
	FVertexInterface TVariableDeferredAccessorNode<DataType>::FOperator::DeclareVertexInterface()
	{
		using namespace VariableNames; 

		return FVertexInterface(
			FInputVertexInterface(
				TInputDataVertex<FVariable>(METASOUND_GET_PARAM_NAME(InputVariable), InputVariableMetadata)
			),
			FOutputVertexInterface(
				TOutputDataVertex<FVariable>(METASOUND_GET_PARAM_NAME(OutputVariable), OutputVariableMetadata),
				TOutputDataVertex<DataType>(METASOUND_GET_PARAM_NAME(OutputData), OutputDataMetadata)
			)
		);
	}

	template<typename DataType>
	const FNodeClassMetadata& TVariableDeferredAccessorNode<DataType>::FOperator::GetNodeInfo()
	{
		auto CreateNodeClassMetadata = []()
		{
			FNodeClassMetadata Info;

			Info.ClassName = VariableNames::GetVariableDeferredAccessorNodeClassName<DataType>();
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
#if WITH_EDITOR
			Info.DisplayStyle.bShowName = false;
			Info.DisplayName = LOCTEXT("Metasound_VariableDeferredAccessorNodeDisplayName", "Get Delayed");
			Info.Description = LOCTEXT("Metasound_VariableDeferredAccessorNodeDescription", "Get a delayed variable on MetaSound graph.");
			Info.CategoryHierarchy = { LOCTEXT("Metasound_VariableCategory", "Variable") };
#endif // WITH_EDITOR
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = DeclareVertexInterface();

			return Info;
		};

		static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
		return Metadata;
	}

	/** FGetVariable node provides delay free, cpu free access to a set variable. */
	template<typename DataType>
	class TVariableAccessorNode : public FNodeFacade
	{
		using FVariable = TVariable<DataType>;

		class FOperator : public FNoOpOperator 
		{
		public:
			static TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors);
			static FVertexInterface DeclareVertexInterface();
			static const FNodeClassMetadata& GetNodeInfo();

			FOperator(TDataReadReference<FVariable> InVariable)
			: Variable(InVariable)
			{
			}

			virtual ~FOperator() = default;

			virtual FDataReferenceCollection GetOutputs() const override
			{
				using namespace VariableNames; 

				FDataReferenceCollection Collection;
				Collection.AddDataReadReference<DataType>(METASOUND_GET_PARAM_NAME(OutputData), Variable->GetDataReference());
				Collection.AddDataReadReference<FVariable>(METASOUND_GET_PARAM_NAME(OutputVariable), Variable);
				return Collection;
			}

		private:
			TDataReadReference<FVariable> Variable;
		};
	public:

		TVariableAccessorNode(const FVertexName& InNodeName, const FGuid& InInstanceID)
		: FNodeFacade(InNodeName, InInstanceID, TFacadeOperatorClass<FOperator>())
		{
		}

		TVariableAccessorNode(const FNodeInitData& InInitData)
		: TVariableAccessorNode(InInitData.InstanceName, InInitData.InstanceID)
		{}
	};

	template<typename DataType>
	TUniquePtr<IOperator> TVariableAccessorNode<DataType>::FOperator::CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors) 
	{
		using namespace VariableNames;

		const FDataReferenceCollection& Inputs = InParams.InputDataReferences;

		// Update delayed variable.
		if (Inputs.ContainsDataReadReference<FVariable>(METASOUND_GET_PARAM_NAME(InputVariable)))
		{
			TDataReadReference<FVariable> Variable = Inputs.GetDataReadReference<FVariable>(METASOUND_GET_PARAM_NAME(InputVariable));
			return MakeUnique<FOperator>(Variable);
		}
		else if (Inputs.ContainsDataWriteReference<FVariable>(METASOUND_GET_PARAM_NAME(InputVariable)))
		{
			TDataReadReference<FVariable> Variable = Inputs.GetDataWriteReference<FVariable>(METASOUND_GET_PARAM_NAME(InputVariable));
			return MakeUnique<FOperator>(Variable);
		}
		else 
		{
			// Nothing to do if there's no input data.
			UE_LOG(LogMetaSound, Warning, TEXT("Missing internal variable connection. Failed to create valid \"GetVariable\" operator"));
			return MakeUnique<FNoOpOperator>();
		}
	}

	template<typename DataType>
	FVertexInterface TVariableAccessorNode<DataType>::FOperator::DeclareVertexInterface()
	{
		using namespace VariableNames; 

		return FVertexInterface(
			FInputVertexInterface(
				TInputDataVertex<FVariable>(METASOUND_GET_PARAM_NAME(InputVariable), InputDataMetadata)
			),
			FOutputVertexInterface(
				TOutputDataVertex<DataType>(METASOUND_GET_PARAM_NAME(OutputData), OutputDataMetadata),
				TOutputDataVertex<FVariable>(METASOUND_GET_PARAM_NAME(OutputVariable), OutputVariableMetadata)
			)
		);
	}

	template<typename DataType>
	const FNodeClassMetadata& TVariableAccessorNode<DataType>::FOperator::GetNodeInfo()
	{
		auto CreateNodeClassMetadata = []()
		{
			FNodeClassMetadata Info;

			Info.ClassName = VariableNames::GetVariableAccessorNodeClassName<DataType>();
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
#if WITH_EDITOR
			Info.DisplayStyle.bShowName = false;
			Info.DisplayName = LOCTEXT("Metasound_VariableAccessorNodeDisplayName", "Get");
			Info.Description = LOCTEXT("Metasound_VariableAccessorNodeDescription", "Get variable on MetaSound graph.");
#endif // WITH_EDITOR

			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = DeclareVertexInterface();

			return Info;
		};

		static const FNodeClassMetadata Metadata = CreateNodeClassMetadata();
		return Metadata;
	}
}

#undef LOCTEXT_NAMESPACE // MetasoundGraphCore
