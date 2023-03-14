// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Internationalization/Text.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundBuildError.h"
#include "MetasoundDataReference.h"
#include "MetasoundFrontendDataTypeTraits.h"
#include "MetasoundNodeConstructorParams.h"
#include "MetasoundLiteral.h"
#include "MetasoundNode.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundOperatorInterface.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundRouter.h"
#include "MetasoundTrigger.h"
#include "MetasoundVertexData.h"
#include "UObject/NameTypes.h"

#define LOCTEXT_NAMESPACE "MetasoundFrontend"


namespace Metasound
{
	/** A writable input and a readable output. */
	template<typename DataType>
	class TInputOperator : public IOperator
	{
		public:
			using FDataWriteReference = TDataWriteReference<DataType>;

			TInputOperator(const FVertexName& InDataReferenceName, FDataWriteReference InDataReference)
				: DataReferenceName(InDataReferenceName)
				// Executable DataTypes require a copy of the output to operate on whereas non-executable
				// types do not. Avoid copy by assigning to reference for non-executable types.
				, InputValue(InDataReference)
				, OutputValue(TExecutableDataType<DataType>::bIsExecutable ? FDataWriteReference::CreateNew(*InDataReference) : InDataReference)
			{
			}

			virtual ~TInputOperator() = default;

			virtual FDataReferenceCollection GetInputs() const override
			{
				// This is slated to be deprecated and removed.
				checkNoEntry();
				return {};
			}

			virtual FDataReferenceCollection GetOutputs() const override
			{
				// This is slated to be deprecated and removed.
				checkNoEntry();
				return {};
			}

			virtual void Bind(FVertexInterfaceData& InOutVertexData) const override
			{
				// TODO: Expose a readable reference instead of a writable reference.
				//
				// If data needs to be written to, outside entities should create
				// it and pass it in as a readable reference. Currently, the workflow
				// is to have the input node create a writable reference which is then
				// queried by the outside world. Exposing writable references causes 
				// code maintainability issues where TInputNode<> specializations need
				// to handle multiple situations which can happen in an input node.
				//
				// The only reason that this code is not removed immediately is because
				// of the `TExecutableDataType<>` which primarily supports the FTrigger.
				// The TExecutableDataType<> advances the trigger within the graph. But,
				// with graph composition, the owner of the data type becomes more 
				// complicated and hence triggers advancing should be managed by a 
				// different object. Preferably the graph operator itself, or an
				// explicit trigger manager tied to the environment.
				InOutVertexData.GetInputs().BindWriteVertex(DataReferenceName, InputValue);

				InOutVertexData.GetOutputs().BindReadVertex(DataReferenceName, OutputValue);
			}

			void Execute()
			{
				TExecutableDataType<DataType>::Execute(*InputValue, *OutputValue);
			}

			static void ExecuteFunction(IOperator* InOperator)
			{
				static_cast<TInputOperator<DataType>*>(InOperator)->Execute();
			}

			virtual FExecuteFunction GetExecuteFunction() override
			{
				if (TExecutableDataType<DataType>::bIsExecutable)
				{
					return &TInputOperator<DataType>::ExecuteFunction;
				}
				return nullptr;
			}

		protected:
			FVertexName DataReferenceName;

			FDataWriteReference InputValue;
			FDataWriteReference OutputValue;
	};

	/** TPassThroughOperator supplies a readable input and a readable output. 
	 *
	 * It does *not* invoke executable data types (see `TExecutableDataType<>`).
	 */
	template<typename DataType>
	class TPassThroughOperator : public TInputOperator<DataType>
	{
	public:
		using FDataReadReference = TDataReadReference<DataType>;
		using Super = TInputOperator<DataType>;

		TPassThroughOperator(const FVertexName& InDataReferenceName, FDataReadReference InDataReference)
		: TInputOperator<DataType>(InDataReferenceName, WriteCast(InDataReference)) // Write cast is safe because `GetExecuteFunction() and Bind() are overridden, ensuring that data is not written.
		{
		}

		virtual ~TPassThroughOperator() = default;

		static void ExecuteFunction(IOperator* InOperator)
		{
			using FPassThroughOperator = TPassThroughOperator<DataType>;
			FPassThroughOperator* PassThroughOperator = static_cast<FPassThroughOperator*>(InOperator);
			*(PassThroughOperator->OutputValue) = *(PassThroughOperator->InputValue);
		}

		virtual IOperator::FExecuteFunction GetExecuteFunction() override
		{
			// TODO: this is a hack until we can remove TExecutableOperator<>.
			//
			// The primary contention is that we would like to allow developers 
			// to specialize `TInputNode<>` as in `TInputNode<FStereoAudioFormat>`.
			// `TExecutableOperator<>` adds in a level of complexity that makes it 
			// difficult to allow specialization of TInputNode and to derive from
			// TInputNode to create the TPassThroughOperator. Particularly because
			// TExecutableOperator<> alters which output data reference is used. 
			// Specializations of TInputNode also tend to alter the output data
			// references. Supporting both is likely to cause issues. 
			//
			// We may need to ensure that input nodes do not provide execution
			// functions. Or we may need a more explicit way of only allowing
			// outputs to be modified. Likely a mix of the `final` keyword
			// and disabling template specialization of a base class. 
			//
			// namespace Private
			// {
			//     class TInputNodePrivate<>
			//     {
			//         GetInputs() final
			//         GetExecutionFunction() final
			//         GetOutputs()
			//     }
			// }
			// 
			// template<DataType>
			// using TInputNodeBase<DataType> = TInputNodePrivate<DataType>; // Do not allow specialization of TInputNodePrivate<> or TInputNodeBase<> (this works because you can't specialize a template alias)
			//
			// // DO ALLOW specialization of TInputNode
			// template<DataType>
			// class TInputNode<DataType> : public TInputNodeBase<DataType>
			// {
			// };
			// 
			// template<>
			// class TInputNode<MyType> : public TInputNodeBase<MyType>
			// {
			// 	 GetOutputs() <-- OK to override
			// }
			//
			if (TExecutableDataType<DataType>::bIsExecutable)
			{
				return &TPassThroughOperator<DataType>::ExecuteFunction;
			}

			return nullptr;
		}
	};

	/** FInputValueOperator provides support for transmittable inputs. */
	template<typename DataType>
	class TInputReceiverOperator : public TInputOperator<DataType>
	{
	public:
		using FDataReadReference = TDataReadReference<DataType>;
		using FInputReceiverOperator = TInputReceiverOperator<DataType>;
		using Super = TInputOperator<DataType>;

		/** Construct an TInputReceiverOperator with the name of the vertex, value reference associated with input, SendAddress, & Receiver
		 */
		TInputReceiverOperator(const FVertexName& InDataReferenceName, FDataReadReference InDataReference, FSendAddress&& InSendAddress, TReceiverPtr<DataType>&& InReceiver)
			: Super(InDataReferenceName, WriteCast(InDataReference)) // Write cast is safe because `GetExecuteFunction() and Bind() are overridden, ensuring that data is not written.
			, SendAddress(MoveTemp(InSendAddress))
			, Receiver(MoveTemp(InReceiver))
		{
		}

		virtual ~TInputReceiverOperator()
		{
			Receiver.Reset();
			FDataTransmissionCenter::Get().UnregisterDataChannelIfUnconnected(SendAddress);
		}

		void Execute()
		{
			DataType& OutputData = *Super::OutputValue;

			bool bHasNewData = Receiver->CanPop();
			if (bHasNewData)
			{
				Receiver->Pop(OutputData);
				bHasNotReceivedData = false;
			}

			if (bHasNotReceivedData)
			{
				OutputData = *Super::InputValue;
				bHasNewData = true;
			}

			if constexpr (TExecutableDataType<DataType>::bIsExecutable)
			{
				TExecutableDataType<DataType>::ExecuteInline(OutputData, bHasNewData);
			}
		}

		static void ExecuteFunction(IOperator* InOperator)
		{
			static_cast<FInputReceiverOperator*>(InOperator)->Execute();
		}

		IOperator::FExecuteFunction GetExecuteFunction()
		{
			return &FInputReceiverOperator::ExecuteFunction;
		}

	private:
		FSendAddress SendAddress;
		TReceiverPtr<DataType> Receiver;
		bool bHasNotReceivedData = true;
	};

	/** FInputValueOperator provides an input for value references. */
	class METASOUNDFRONTEND_API FInputValueOperator : public IOperator
	{
	public:
		/** Construct an FInputValueOperator with the name of the vertex and the 
		 * value reference associated with input. 
		 */
		template<typename DataType>
		explicit FInputValueOperator(const FName& InVertexName, const TDataValueReference<DataType>& InValueRef)
		: VertexName(InVertexName)
		, Default(InValueRef)
		{
		}

		virtual ~FInputValueOperator() = default;

		virtual FDataReferenceCollection GetInputs() const override;
		virtual FDataReferenceCollection GetOutputs() const override;
		virtual void Bind(FVertexInterfaceData& InVertexData) const override;
		virtual FExecuteFunction GetExecuteFunction() override;

	private:
		FName VertexName;
		FAnyDataReference Default;
	};

	/** Data type creation policy to create by copy construction. */
	template<typename DataType>
	struct UE_DEPRECATED(5.1, "Moved to private implementation.") FCreateDataReferenceWithCopy
	{
		template<typename... ArgTypes>
		FCreateDataReferenceWithCopy(ArgTypes&&... Args)
		:	Data(Forward<ArgTypes>(Args)...)
		{
		}

		TDataWriteReference<DataType> CreateDataReference(const FOperatorSettings& InOperatorSettings) const
		{
			return TDataWriteReferenceFactory<DataType>::CreateExplicitArgs(InOperatorSettings, Data);
		}

	private:
		DataType Data;
	};


	/** Data type creation policy to create by literal construction. */
	template<typename DataType>
	struct UE_DEPRECATED(5.1, "Moved to private implementation.") FCreateDataReferenceWithLiteral
	{
		// If the data type is parsable from a literal type, then the data type 
		// can be registered as an input type with the frontend.  To make a 
		// DataType registrable, either create a constructor for the data type
		// which accepts the one of the supported literal types with an optional 
		// FOperatorSettings argument, or create a default constructor, or specialize
		// this factory with an implementation for that specific data type.
		static constexpr bool bCanCreateWithLiteral = TLiteralTraits<DataType>::bIsParsableFromAnyLiteralType;

		FCreateDataReferenceWithLiteral(FLiteral&& InLiteral)
		:	Literal(MoveTemp(InLiteral))
		{
		}

		TDataWriteReference<DataType> CreateDataReference(const FOperatorSettings& InOperatorSettings) const
		{
			return TDataWriteReferenceLiteralFactory<DataType>::CreateExplicitArgs(InOperatorSettings, Literal);
		}

	private:
		FLiteral Literal;
	};


	/** TInputOperatorFactory initializes the DataType at construction. It uses
	 * the ReferenceCreatorType to create a data reference if one is not passed in.
	 */
	template<typename DataType, typename ReferenceCreatorType>
	class UE_DEPRECATED(5.1, "Moved to private implementation") TInputOperatorFactory : public IOperatorFactory
	{
		public:

			using FDataWriteReference = TDataWriteReference<DataType>;
			using FDataReadReference = TDataReadReference<DataType>;

			TInputOperatorFactory(ReferenceCreatorType&& InReferenceCreator)
			:	ReferenceCreator(MoveTemp(InReferenceCreator))
			{
			}

			virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override;

		private:
			ReferenceCreatorType ReferenceCreator;
	};

	/** TInputNode represents an input to a metasound graph. */
	template<typename DataType, EVertexAccessType VertexAccess=EVertexAccessType::Reference>
	class TInputNode : public FNode
	{
		static constexpr bool bIsConstructorInput = VertexAccess == EVertexAccessType::Value;
		static constexpr bool bIsSupportedConstructorInput = TIsConstructorVertexSupported<DataType>::Value && bIsConstructorInput;
		static constexpr bool bIsReferenceInput = VertexAccess == EVertexAccessType::Reference;
		static constexpr bool bIsSupportedReferenceInput = TLiteralTraits<DataType>::bIsParsableFromAnyLiteralType && bIsReferenceInput;

		static constexpr bool bIsSupportedInput = bIsSupportedConstructorInput || bIsSupportedReferenceInput;

		// Use Variant names to differentiate between normal input nodes and constructor 
		// input nodes.
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

		// Factory for creating input operators. 
		class FInputNodeOperatorFactory : public IOperatorFactory
		{
			static constexpr bool bIsReferenceVertexAccess = VertexAccess == EVertexAccessType::Reference;
			static constexpr bool bIsValueVertexAccess = VertexAccess == EVertexAccessType::Value;

			static_assert(bIsValueVertexAccess || bIsReferenceVertexAccess, "Unsupported EVertexAccessType");

			// Choose which data reference type is created based on template parameters
			using FDataReference = std::conditional_t<bIsReferenceVertexAccess, TDataWriteReference<DataType>, TDataValueReference<DataType>>;
			using FPassThroughDataReference = std::conditional_t<bIsReferenceVertexAccess, TDataReadReference<DataType>, TDataValueReference<DataType>>;

			// Utility struct for creating data references for varying flavors of 
			// runtime scenarios and vertex access types.
			struct FDataReferenceCreatorBase
			{
				virtual ~FDataReferenceCreatorBase() = default;

				// Create a a new data reference for constructing operators 
				virtual FDataReference CreateDataReference(const FOperatorSettings& InOperatorSettings) const = 0;

				// Create a data reference for constructing operators from a given data reference
				virtual FPassThroughDataReference CreateDataReference(const FAnyDataReference& InRef) const
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
			};

			// Create data references using a literal 
			struct FCreateWithLiteral : FDataReferenceCreatorBase
			{
				using FDataFactory = std::conditional_t<bIsReferenceVertexAccess, TDataWriteReferenceLiteralFactory<DataType>, TDataValueReferenceLiteralFactory<DataType>>;

				FLiteral Literal;

				FCreateWithLiteral(FLiteral&& InLiteral)
				: Literal(MoveTemp(InLiteral))
				{
				}

				virtual FDataReference CreateDataReference(const FOperatorSettings& InOperatorSettings) const override
				{
					return FDataFactory::CreateExplicitArgs(InOperatorSettings, Literal);
				}
			};

			// Create data references using a copy 
			struct FCreateWithCopy : FDataReferenceCreatorBase
			{
				using FDataFactory = std::conditional_t<bIsReferenceVertexAccess, TDataWriteReferenceFactory<DataType>, TDataValueReferenceFactory<DataType>>;

				DataType Value;

				virtual FDataReference CreateDataReference(const FOperatorSettings& InOperatorSettings) const override
				{
					return FDataFactory::CreateExplicitArgs(InOperatorSettings, Value);
				}
			};

		public:
			explicit FInputNodeOperatorFactory(FLiteral&& InLiteral, bool bInEnableTransmission)
			: ReferenceCreator(MakeUnique<FCreateWithLiteral>(MoveTemp(InLiteral)))
			, bEnableTransmission(bInEnableTransmission)
			{
			}

			virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override
			{
				using FInputNodeType = TInputNode<DataType, VertexAccess>;
				using FOwnedInputOperatorType = std::conditional_t<bIsReferenceVertexAccess, TInputOperator<DataType>, FInputValueOperator>;
				using FPassThroughInputOperatorType = std::conditional_t<bIsReferenceVertexAccess, TPassThroughOperator<DataType>, FInputValueOperator>;

				checkf(!(bEnableTransmission && bIsValueVertexAccess), TEXT("Input cannot enable transmission for vertex with access 'EVertexAccessType::Reference'"));

				const FInputNodeType& InputNode = static_cast<const FInputNodeType&>(InParams.Node);
				const FVertexName& VertexKey = InputNode.GetVertexName();

				if (bEnableTransmission)
				{
					const FName DataTypeName = GetMetasoundDataTypeName<DataType>();
					FSendAddress SendAddress = FMetaSoundParameterTransmitter::CreateSendAddressFromEnvironment(InParams.Environment, VertexKey, DataTypeName);
					TReceiverPtr<DataType> Receiver = FDataTransmissionCenter::Get().RegisterNewReceiver<DataType>(SendAddress, FReceiverInitParams{ InParams.OperatorSettings });
					if (Receiver.IsValid())
					{
						// Transmittable input value
						if (const FAnyDataReference* Ref = InParams.InputData.FindDataReference(VertexKey))
						{
							return MakeUnique<TInputReceiverOperator<DataType>>(VertexKey, ReferenceCreator->CreateDataReference(*Ref), MoveTemp(SendAddress), MoveTemp(Receiver));
						}
						else
						{
							FDataReference DataRef = ReferenceCreator->CreateDataReference(InParams.OperatorSettings);
							return MakeUnique<TInputReceiverOperator<DataType>>(VertexKey, DataRef, MoveTemp(SendAddress), MoveTemp(Receiver));
						}
					}

					AddBuildError<FInputReceiverInitializationError>(OutResults.Errors, InParams.Node, VertexKey, DataTypeName);
					return nullptr;
				}

				if (const FAnyDataReference* Ref = InParams.InputData.FindDataReference(VertexKey))
				{
					// Pass through input value
					return MakeUnique<FPassThroughInputOperatorType>(VertexKey, ReferenceCreator->CreateDataReference(*Ref));
				}


				// Owned input value
				FDataReference DataRef = ReferenceCreator->CreateDataReference(InParams.OperatorSettings);
				return MakeUnique<FOwnedInputOperatorType>(VertexKey, DataRef);
			}

		private:
			TUniquePtr<FDataReferenceCreatorBase> ReferenceCreator;
			bool bEnableTransmission = false;
		};


	public:
		// If true, this node can be instantiated by the Frontend.
		static constexpr bool bCanRegister = bIsSupportedInput;

		static FVertexInterface DeclareVertexInterface(const FVertexName& InVertexName)
		{
			return FVertexInterface(
				FInputVertexInterface(
					FInputDataVertex(InVertexName, GetMetasoundDataTypeName<DataType>(), FDataVertexMetadata{ FText::GetEmpty() }, VertexAccess)
				),
				FOutputVertexInterface(
					FOutputDataVertex(InVertexName, GetMetasoundDataTypeName<DataType>(), FDataVertexMetadata{ FText::GetEmpty() }, VertexAccess)
				)
			);
		}

		static FNodeClassMetadata GetNodeInfo(const FVertexName& InVertexName)
		{
			FNodeClassMetadata Info;

			Info.ClassName = { "Input", GetMetasoundDataTypeName<DataType>(), GetVariantName() };
			Info.MajorVersion = 1;
			Info.MinorVersion = 0;
			Info.Description = METASOUND_LOCTEXT("Metasound_InputNodeDescription", "Input into the parent Metasound graph.");
			Info.Author = PluginAuthor;
			Info.PromptIfMissing = PluginNodeMissingPrompt;
			Info.DefaultInterface = DeclareVertexInterface(InVertexName);

			return Info;
		}

		template<typename... ArgTypes>
		UE_DEPRECATED(5.1, "Moved to internal implementation.")
		static FOperatorFactorySharedRef CreateOperatorFactoryWithArgs(ArgTypes&&... Args)
		{
			using FCreatorType = FCreateDataReferenceWithCopy<DataType>;
			using FFactoryType = TInputOperatorFactory<DataType, FCreatorType>;

			return MakeOperatorFactoryRef<FFactoryType>(FCreatorType(Forward<ArgTypes>(Args)...));
		}

		UE_DEPRECATED(5.1, "Moved to internal implementation.")
		static FOperatorFactorySharedRef CreateOperatorFactoryWithLiteral(FLiteral&& InLiteral)
		{
			using FCreatorType = FCreateDataReferenceWithLiteral<DataType>;
			using FFactoryType = TInputOperatorFactory<DataType, FCreatorType>;

			return MakeOperatorFactoryRef<FFactoryType>(FCreatorType(MoveTemp(InLiteral)));
		}

		/* Construct a TInputNode using the TInputOperatorFactory<> and forwarding 
		 * Args to the TInputOperatorFactory constructor.*/
		template<typename... ArgTypes>
		UE_DEPRECATED(5.1, "Constructing an TInputNode with args will no longer be supported.")
		TInputNode(const FVertexName& InInstanceName, const FGuid& InInstanceID, const FVertexName& InVertexName, ArgTypes&&... Args)
		:	FNode(InInstanceName, InInstanceID, GetNodeInfo(InVertexName))
		,	VertexName(InVertexName)
		,	Interface(DeclareVertexInterface(InVertexName))
		,	Factory(MakeShared<FInputNodeOperatorFactory>(Forward<ArgTypes>(Args)...))
		{
		}

		/* Construct a TInputNode using the TInputOperatorLiteralFactory<> and moving
		 * InParam to the TInputOperatorLiteralFactory constructor.*/
		explicit TInputNode(FInputNodeConstructorParams&& InParams)
		:	FNode(InParams.NodeName, InParams.InstanceID, GetNodeInfo(InParams.VertexName))
		,	VertexName(InParams.VertexName)
		,	Interface(DeclareVertexInterface(InParams.VertexName))
		,	Factory(MakeShared<FInputNodeOperatorFactory>(MoveTemp(InParams.InitParam), InParams.bEnableTransmission))
		{
		}

		const FVertexName& GetVertexName() const
		{
			return VertexName;
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
		FVertexName VertexName;

		FVertexInterface Interface;
		FOperatorFactorySharedRef Factory;
	};


	template<typename DataType, typename ReferenceCreatorType>
	TUniquePtr<IOperator> TInputOperatorFactory<DataType, ReferenceCreatorType>::CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults)
	{
		using FInputNodeType = TInputNode<DataType>;

		const FInputNodeType& InputNode = static_cast<const FInputNodeType&>(InParams.Node);
		const FVertexName& VertexKey = InputNode.GetVertexName();

		if (InParams.InputData.IsVertexBound(VertexKey))
		{
			// Data is externally owned. Use pass through operator
			TDataReadReference<DataType> DataRef = InParams.InputData.GetDataReadReference<DataType>(VertexKey);
			return MakeUnique<TPassThroughOperator<DataType>>(InputNode.GetVertexName(), DataRef);
		}
		else
		{
			// Create write reference by calling compatible constructor with literal.
			TDataWriteReference<DataType> DataRef = ReferenceCreator.CreateDataReference(InParams.OperatorSettings);
			return MakeUnique<TInputOperator<DataType>>(InputNode.GetVertexName(), DataRef);
		}
	}
} // namespace Metasound
#undef LOCTEXT_NAMESPACE // MetasoundFrontend
