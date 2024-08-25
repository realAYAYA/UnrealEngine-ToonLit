// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundBuilderInterface.h"
#include "MetasoundBuildError.h"
#include "MetasoundDataReference.h"
#include "MetasoundExecutableOperator.h"
#include "MetasoundFrontendDataTypeTraits.h"
#include "MetasoundNodeConstructorParams.h"
#include "MetasoundLiteral.h"
#include "MetasoundNode.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundVertexData.h"
#include "UObject/NameTypes.h"

namespace Metasound
{
	namespace MetasoundInputNodePrivate
	{
		class METASOUNDFRONTEND_API FNonExecutableInputOperatorBase : public IOperator
		{	
		public:
			virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override;
			virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override;

			virtual IOperator::FExecuteFunction GetExecuteFunction() override;
			virtual IOperator::FPostExecuteFunction GetPostExecuteFunction() override;
			virtual IOperator::FResetFunction GetResetFunction() override;

		protected:
			FNonExecutableInputOperatorBase(const FVertexName& InVertexName, FAnyDataReference&& InDataRef);

			FVertexName VertexName;
			FAnyDataReference DataRef;
		};


		class METASOUNDFRONTEND_API FNonExecutableInputPassThroughOperator : public FNonExecutableInputOperatorBase
		{
		public:
			template<typename DataType>
			FNonExecutableInputPassThroughOperator(const FVertexName& InVertexName, const TDataReadReference<DataType>& InDataRef)
			: FNonExecutableInputOperatorBase(InVertexName, FAnyDataReference{InDataRef})
			{
			}

			template<typename DataType>
			FNonExecutableInputPassThroughOperator(const FVertexName& InVertexName, const TDataWriteReference<DataType>& InDataRef)
			: FNonExecutableInputPassThroughOperator(InVertexName, TDataReadReference<DataType>(InDataRef))
			{
			}
		};


		/** TInputValueOperator provides an input for value references. */
		template<typename DataType>
		class TInputValueOperator : public FNonExecutableInputOperatorBase
		{
		public:
			/** Construct an TInputValueOperator with the name of the vertex and the 
			 * value reference associated with input. 
			 */
			explicit TInputValueOperator(const FName& InVertexName, const TDataValueReference<DataType>& InValueRef)
			: FNonExecutableInputOperatorBase(InVertexName, FAnyDataReference{InValueRef})
			{
			}

			TInputValueOperator(const FVertexName& InVertexName, const FOperatorSettings& InSettings, const FLiteral& InLiteral)
			: FNonExecutableInputOperatorBase(InVertexName, FAnyDataReference{TDataValueReferenceLiteralFactory<DataType>::CreateExplicitArgs(InSettings, InLiteral)})
			{
			}
		};

		template<typename DataType>
		class TExecutableInputOperator : public IOperator
		{
			static_assert(TExecutableDataType<DataType>::bIsExecutable, "TExecutableInputOperatorBase should only be used with executable data types");
		public:
			using FDataWriteReference = TDataWriteReference<DataType>;

			UE_DEPRECATED(5.4, "The Executable data types will no longer be supported. Please use PostExecutable data types.") 
			TExecutableInputOperator(const FVertexName& InDataReferenceName, TDataWriteReference<DataType> InValue)
				: DataReferenceName(InDataReferenceName)
				, InputValue(InValue)
				, OutputValue(FDataWriteReference::CreateNew(*InValue))
			{
			}

			virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
			{
				InOutVertexData.BindWriteVertex(DataReferenceName, InputValue);
			}

			virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
			{
				InOutVertexData.BindReadVertex(DataReferenceName, OutputValue);
			}

			virtual FExecuteFunction GetExecuteFunction() override
			{
				return &Execute;
			}

			virtual FExecuteFunction GetPostExecuteFunction() override
			{
				return nullptr;
			}

			virtual FResetFunction GetResetFunction() override
			{
				return nullptr;
			}


		protected:

			static void Execute(IOperator* InOperator)
			{
				using FExecutableInputOperator = TExecutableInputOperator<DataType>;

				FExecutableInputOperator* DerivedOperator = static_cast<FExecutableInputOperator*>(InOperator);
				check(nullptr != DerivedOperator);

				TExecutableDataType<DataType>::Execute(*(DerivedOperator->InputValue), *(DerivedOperator->OutputValue));
			}

			FVertexName DataReferenceName;

			FDataWriteReference InputValue;
			FDataWriteReference OutputValue;

		};

		template<typename DataType>
		class TResetableExecutableInputOperator : public TExecutableInputOperator<DataType>
		{
		public:
			using FDataWriteReference = TDataWriteReference<DataType>;
			using FDataWriteReferenceFactory = TDataWriteReferenceLiteralFactory<DataType>;

			TResetableExecutableInputOperator(const FVertexName& InDataReferenceName, const FOperatorSettings& InSettings, const FLiteral& InLiteral)
			: TExecutableInputOperator<DataType>(InDataReferenceName, FDataWriteReferenceFactory::CreateExplicitArgs(InSettings, InLiteral))
			, Literal(InLiteral)
			{
			}

			virtual IOperator::FResetFunction GetResetFunction() override
			{
				return &Reset;
			}

		private:

			static void Reset(IOperator* InOperator, const IOperator::FResetParams& InParams)
			{
				using FResetableExecutableInputOperator = TResetableExecutableInputOperator<DataType>;

				FResetableExecutableInputOperator* Operator = static_cast<FResetableExecutableInputOperator*>(InOperator);
				check(nullptr != Operator);

				*Operator->InputValue = TDataTypeLiteralFactory<DataType>::CreateExplicitArgs(InParams.OperatorSettings, Operator->Literal);
				*Operator->OutputValue = *Operator->InputValue;
			}

			FLiteral Literal;
		};

		template<typename DataType>
		class TPostExecutableInputOperator : public IOperator
		{
			static_assert(TPostExecutableDataType<DataType>::bIsPostExecutable, "TPostExecutableInputOperator should only be used with post executable data types");
			static_assert(!TExecutableDataType<DataType>::bIsExecutable, "A data type cannot be Executable and PostExecutable");

		public:
			using FDataWriteReference = TDataWriteReference<DataType>;

			TPostExecutableInputOperator(const FVertexName& InDataReferenceName, TDataWriteReference<DataType> InValue)
				: DataReferenceName(InDataReferenceName)
				, DataRef(InValue)
			{
			}

			virtual void BindInputs(FInputVertexInterfaceData& InOutVertexData) override
			{
				InOutVertexData.BindVertex(DataReferenceName, DataRef);
			}

			virtual void BindOutputs(FOutputVertexInterfaceData& InOutVertexData) override
			{
				InOutVertexData.BindVertex(DataReferenceName, DataRef.GetDataReadReference<DataType>());
			}

			virtual FExecuteFunction GetExecuteFunction() override
			{
				return nullptr;
			}

			virtual FPostExecuteFunction GetPostExecuteFunction() override
			{
				// This condition is checked at runtime as its possible dynamic graphs may reassign ownership
				// of underlying data to operate on in post execute. In this case, the expectation is that the
				// data reference is now owned by another provider/operator.
				if (DataRef.GetAccessType() == EDataReferenceAccessType::Write)
				{
					return &PostExecute;
				}
				else
				{
					return nullptr;
				}
			}

			virtual FResetFunction GetResetFunction() override
			{
				// This condition is checked at runtime as its possible dynamic graphs may reassign ownership
				// of underlying data to operate on in post execute. In this case, the expectation is that the
				// data reference is now owned by another provider/operator.
				if (DataRef.GetAccessType() == EDataReferenceAccessType::Write)
				{
					return &NoOpReset;
				}
				else
				{
					return nullptr;
				}
			}

		protected:
			static void NoOpReset(IOperator* InOperator, const IOperator::FResetParams& InParams)
			{
				// All post executable nodes must have a reset.  This is a special
				// case of a non-owning node performing post execute on a data type
				// owned by an external system.
			}

			static void PostExecute(IOperator* InOperator)
			{
				using FPostExecutableInputOperator = TPostExecutableInputOperator<DataType>;

				FPostExecutableInputOperator* DerivedOperator = static_cast<FPostExecutableInputOperator*>(InOperator);
				check(nullptr != DerivedOperator);

				DataType* Value = DerivedOperator->DataRef.template GetWritableValue<DataType>();
				if (ensure(Value != nullptr))
				{
					TPostExecutableDataType<DataType>::PostExecute(*Value);
				}
			}

			FVertexName DataReferenceName;
			FAnyDataReference DataRef;
		};

		template<typename DataType>
		class TResetablePostExecutableInputOperator : public TPostExecutableInputOperator<DataType>
		{
		public:
			using FDataWriteReference = TDataWriteReference<DataType>;
			using FDataWriteReferenceFactory = TDataWriteReferenceLiteralFactory<DataType>;
			using TPostExecutableInputOperator<DataType>::DataRef;

			TResetablePostExecutableInputOperator(const FVertexName& InDataReferenceName, const FOperatorSettings& InSettings, const FLiteral& InLiteral)
			: TPostExecutableInputOperator<DataType>(InDataReferenceName, FDataWriteReferenceFactory::CreateExplicitArgs(InSettings, InLiteral))
			, Literal(InLiteral)
			{
			}

			virtual IOperator::FResetFunction GetResetFunction() override
			{
				if (DataRef.GetAccessType() == EDataReferenceAccessType::Write)
				{
					return &Reset;
				}
				else
				{
					// If DataRef is not writable, reference is assumed to be reset by another owning operator.
					return nullptr;
				}
			}

		private:

			static void Reset(IOperator* InOperator, const IOperator::FResetParams& InParams)
			{
				using FResetablePostExecutableInputOperator = TResetablePostExecutableInputOperator<DataType>;

				FResetablePostExecutableInputOperator* Operator = static_cast<FResetablePostExecutableInputOperator*>(InOperator);
				check(nullptr != Operator);

				DataType* Value = Operator->DataRef.template GetWritableValue<DataType>();
				if (ensure(Value != nullptr))
				{
					*Value = TDataTypeLiteralFactory<DataType>::CreateExplicitArgs(InParams.OperatorSettings, Operator->Literal);
				}
			}

			FLiteral Literal;
		};

		/** Non owning input operator that may need execution. */
		template<typename DataType, EVertexAccessType VertexAccess=EVertexAccessType::Reference>
		using TNonOwningInputOperator = std::conditional_t<
			TExecutableDataType<DataType>::bIsExecutable,
			TExecutableInputOperator<DataType>, // Use this input operator if the data type is not owned by the input node but needs execution.
			std::conditional_t<
				TPostExecutableDataType<DataType>::bIsPostExecutable,
				TPostExecutableInputOperator<DataType>, // Use this input operator if the data type is not owned by the input node but needs post execution.
				MetasoundInputNodePrivate::FNonExecutableInputPassThroughOperator // Use this input operator if the data type is not owned by the input node and is not executable, nor post executable.
			>
		>;
	}

	/** Owning input operator that may need execution. */
	template<typename DataType, EVertexAccessType VertexAccess=EVertexAccessType::Reference>
	using TInputOperator = std::conditional_t<
		VertexAccess == EVertexAccessType::Value || (!TExecutableDataType<DataType>::bIsExecutable && !TPostExecutableDataType<DataType>::bIsPostExecutable),
		MetasoundInputNodePrivate::TInputValueOperator<DataType>, // Use this input operator if the data type is owned by the input node and is not executable, nor post executable.
		std::conditional_t<
			TExecutableDataType<DataType>::bIsExecutable,
			MetasoundInputNodePrivate::TResetableExecutableInputOperator<DataType>, // Use this input operator if the data type is owned by the input node and is executable.
			MetasoundInputNodePrivate::TResetablePostExecutableInputOperator<DataType> // Use this input operator if the data type is owned by the input node and is post executable.

		>
	>;

	/** Choose pass through operator based upon data type and access type */
	template<typename DataType, EVertexAccessType VertexAccess=EVertexAccessType::Reference>
	using TPassThroughOperator = std::conditional_t<
		VertexAccess == EVertexAccessType::Value,
		MetasoundInputNodePrivate::TInputValueOperator<DataType>,
		MetasoundInputNodePrivate::FNonExecutableInputPassThroughOperator
	>;

	/** FInputNode represents an input to a metasound graph. */
	class METASOUNDFRONTEND_API FInputNode : public FNode
	{
		static FLazyName ConstructorVariant;
		// Use Variant names to differentiate between normal input nodes and constructor 
		// input nodes.
		static FName GetVariantName(EVertexAccessType InVertexAccess);

		static FVertexInterface CreateVertexInterface(const FVertexName& InVertexName, const FName& InDataTypeName, EVertexAccessType InVertexAccess, const FLiteral& InLiteral);

	protected:

		static FVertexInterface CreateDefaultVertexInterface(const FVertexName& InVertexName, const FName& InDataTypeName, EVertexAccessType InVertexAccess);

	public:

		static FNodeClassMetadata GetNodeMetadata(const FVertexName& InVertexName, const FName& InDataTypeName, EVertexAccessType InVertexAccess);

		/* Construct a TInputNode using the TInputOperatorLiteralFactory<> and moving
		 * InParam to the TInputOperatorLiteralFactory constructor.*/
		explicit FInputNode(FInputNodeConstructorParams&& InParams, const FName& InDataTypeName, EVertexAccessType InVertexAccess, FOperatorFactorySharedRef InFactory);

		const FVertexName& GetVertexName() const;

		virtual const FVertexInterface& GetVertexInterface() const override;
		virtual bool SetVertexInterface(const FVertexInterface& InInterface) override;
		virtual bool IsVertexInterfaceSupported(const FVertexInterface& InInterface) const override;
		virtual TSharedRef<IOperatorFactory, ESPMode::ThreadSafe> GetDefaultOperatorFactory() const override;

	private:
		FVertexName VertexName;
		FVertexInterface Interface;
		FOperatorFactorySharedRef Factory;
	};


	/** TInputNode represents an input to a metasound graph. */
	template<typename DataType, EVertexAccessType VertexAccess=EVertexAccessType::Reference>
	class TInputNode : public FInputNode
	{
		static constexpr bool bIsConstructorInput = VertexAccess == EVertexAccessType::Value;
		static constexpr bool bIsSupportedConstructorInput = TIsConstructorVertexSupported<DataType>::Value && bIsConstructorInput;
		static constexpr bool bIsReferenceInput = VertexAccess == EVertexAccessType::Reference;
		static constexpr bool bIsSupportedReferenceInput = TLiteralTraits<DataType>::bIsParsableFromAnyLiteralType && bIsReferenceInput;

		static constexpr bool bIsSupportedInput = bIsSupportedConstructorInput || bIsSupportedReferenceInput;

		// Factory for creating input operators. 
		class FInputNodeOperatorFactory : public IOperatorFactory
		{
			static constexpr bool bIsReferenceVertexAccess = VertexAccess == EVertexAccessType::Reference;
			static constexpr bool bIsValueVertexAccess = VertexAccess == EVertexAccessType::Value;

			static_assert(bIsValueVertexAccess || bIsReferenceVertexAccess, "Unsupported EVertexAccessType");

			// Choose which data reference type is created based on template parameters
			using FDataReference = std::conditional_t<bIsReferenceVertexAccess, TDataReadReference<DataType>, TDataValueReference<DataType>>;
			using FDataReferenceFactory = std::conditional_t<bIsReferenceVertexAccess, TDataReadReferenceLiteralFactory<DataType>, TDataValueReferenceLiteralFactory<DataType>>;
			using FPassThroughDataReference = std::conditional_t<bIsReferenceVertexAccess, TDataReadReference<DataType>, TDataValueReference<DataType>>;

			// Return correct data reference type based on vertex access type for pass through scenario.
			FPassThroughDataReference CreatePassThroughDataReference(const FAnyDataReference& InRef)
			{
				if constexpr (bIsReferenceVertexAccess)
				{
					return InRef.GetDataReadReference<DataType>();
				}
				else if constexpr (bIsValueVertexAccess)
				{
					return InRef.GetDataValueReference<DataType>();
				}
				else
				{
					static_assert("Unsupported EVertexAccessType");
				}
			}

		public:
			explicit FInputNodeOperatorFactory()
			{
			}

			virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override
			{
				using namespace MetasoundInputNodePrivate;

				using FInputNodeType = TInputNode<DataType, VertexAccess>;

				const FInputNodeType& InputNode = static_cast<const FInputNodeType&>(InParams.Node);
				const FVertexName& VertexKey = InputNode.GetVertexName();

				if (const FAnyDataReference* Ref = InParams.InputData.FindDataReference(VertexKey))
				{
					if constexpr (bIsReferenceVertexAccess)
					{
						if (EDataReferenceAccessType::Write == Ref->GetAccessType())
						{
							return MakeUnique<TNonOwningInputOperator<DataType, VertexAccess>>(VertexKey, Ref->GetDataWriteReference<DataType>());
						}
					}
					// Pass through input value
					return MakeUnique<TPassThroughOperator<DataType, VertexAccess>>(VertexKey, CreatePassThroughDataReference(*Ref));
				}
				else
				{
					const FLiteral& Literal = InputNode.GetVertexInterface().GetInputInterface()[VertexKey].GetDefaultLiteral();
					// Owned input value
					return MakeUnique<TInputOperator<DataType, VertexAccess>>(VertexKey, InParams.OperatorSettings, Literal);
				}
			}
		};


	public:
		// If true, this node can be instantiated by the Frontend.
		static constexpr bool bCanRegister = bIsSupportedInput;

		UE_DEPRECATED(5.3, "Access the default vertex interface from the input node metadata.")
		static FVertexInterface DeclareVertexInterface(const FVertexName& InVertexName)
		{
			return CreateDefaultVertexInterface(InVertexName, GetMetasoundDataTypeName<DataType>(), VertexAccess);
		}

		static FNodeClassMetadata GetNodeInfo(const FVertexName& InVertexName)
		{
			return GetNodeMetadata(InVertexName, GetMetasoundDataTypeName<DataType>(), VertexAccess);
		}

		/* Construct a TInputNode using the TInputOperatorLiteralFactory<> and moving
		 * InParam to the TInputOperatorLiteralFactory constructor.*/
		explicit TInputNode(FInputNodeConstructorParams&& InParams)
		:	FInputNode(MoveTemp(InParams), GetMetasoundDataTypeName<DataType>(), VertexAccess, MakeShared<FInputNodeOperatorFactory>())
		{
		}
	};
} // namespace Metasound
