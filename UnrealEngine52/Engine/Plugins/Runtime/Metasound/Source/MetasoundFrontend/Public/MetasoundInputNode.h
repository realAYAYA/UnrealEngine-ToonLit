// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Internationalization/Text.h"
#include "MetasoundBuilderInterface.h"
#include "MetasoundBuildError.h"
#include "MetasoundDataReference.h"
#include "MetasoundExecutableOperator.h"
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
	namespace MetasoundInputNodePrivate
	{
		class METASOUNDFRONTEND_API FInputOperatorBase : public IOperator
		{
		public:
			virtual FDataReferenceCollection GetInputs() const override;
			virtual FDataReferenceCollection GetOutputs() const override;
		};

		class METASOUNDFRONTEND_API FNonExecutableInputOperatorBase : public FInputOperatorBase
		{	
		public:
			virtual void Bind(FVertexInterfaceData& InVertexData) const override;
			virtual IOperator::FExecuteFunction GetExecuteFunction() override;

		protected:
			FNonExecutableInputOperatorBase(const FVertexName& InVertexName, FAnyDataReference&& InDataRef);

		private:
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
			: FNonExecutableInputOperatorBase(InVertexName, FAnyDataReference{InDataRef})
			{
			}
		};

		/** FInputValueOperator provides an input for value references. */
		class METASOUNDFRONTEND_API FInputValueOperator : public FNonExecutableInputOperatorBase
		{
		public:
			/** Construct an FInputValueOperator with the name of the vertex and the 
			 * value reference associated with input. 
			 */
			template<typename DataType>
			explicit FInputValueOperator(const FName& InVertexName, const TDataValueReference<DataType>& InValueRef)
			: FNonExecutableInputOperatorBase(InVertexName, FAnyDataReference{InValueRef})
			{
			}
		};

		/** A writable input and a readable output. */
		template<typename DataType>
		class TExecutableInputOperator : public FInputOperatorBase
		{
			static_assert(TExecutableDataType<DataType>::bIsExecutable, "TExecutableInputOperatorBase should only be used with executable data types");
		public:
			using FDataWriteReference = TDataWriteReference<DataType>;

			TExecutableInputOperator(const FVertexName& InDataReferenceName, FDataWriteReference InDataReference)
				: DataReferenceName(InDataReferenceName)
				// Executable DataTypes require a copy of the output to operate on whereas non-executable
				// types do not. Avoid copy by assigning to reference for non-executable types.
				, InputValue(InDataReference)
				, OutputValue(FDataWriteReference::CreateNew(*InDataReference))
			{
			}

			virtual void Bind(FVertexInterfaceData& InOutVertexData) const override
			{
				InOutVertexData.GetInputs().BindWriteVertex(DataReferenceName, InputValue);
				InOutVertexData.GetOutputs().BindReadVertex(DataReferenceName, OutputValue);
			}

			virtual FExecuteFunction GetExecuteFunction() override
			{
				return &Execute;
			}

		private:

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


		/** FInputValueOperator provides support for transmittable inputs. */
		template<typename DataType>
		class TInputReceiverOperator : public FInputOperatorBase
		{
		public:
			using FDataReadReference = TDataReadReference<DataType>;
			using FDataWriteReference = TDataWriteReference<DataType>;
			using FInputReceiverOperator = TInputReceiverOperator<DataType>;

			/** Construct an TInputReceiverOperator with the name of the vertex, value reference associated with input, SendAddress, & Receiver
			 */
			TInputReceiverOperator(const FVertexName& InDataReferenceName, FDataReadReference InDataReference, FSendAddress&& InSendAddress, TReceiverPtr<DataType>&& InReceiver)
				: DataReferenceName(InDataReferenceName) 
				, InputValue(InDataReference)
				, OutputValue(FDataWriteReference::CreateNew(*InDataReference))
				, SendAddress(MoveTemp(InSendAddress))
				, Receiver(MoveTemp(InReceiver))
			{
			}

			virtual ~TInputReceiverOperator()
			{
				Receiver.Reset();
				FDataTransmissionCenter::Get().UnregisterDataChannelIfUnconnected(SendAddress);
			}

			virtual void Bind(FVertexInterfaceData& InOutVertexData) const override
			{
				InOutVertexData.GetInputs().BindReadVertex(DataReferenceName, InputValue);
				InOutVertexData.GetOutputs().BindReadVertex(DataReferenceName, OutputValue);
			}

			virtual FExecuteFunction GetExecuteFunction() override
			{
				return &StaticExecute;
			}

		private:
			void Execute()
			{
				DataType& OutputData = *OutputValue;

				bool bHasNewData = Receiver->CanPop();
				if (bHasNewData)
				{
					Receiver->Pop(OutputData);
					bHasNotReceivedData = false;
				}

				if (bHasNotReceivedData)
				{
					OutputData = *InputValue;
					bHasNewData = true;
				}

				if constexpr (TExecutableDataType<DataType>::bIsExecutable)
				{
					TExecutableDataType<DataType>::ExecuteInline(OutputData, bHasNewData);
				}
			}

			static void StaticExecute(IOperator* InOperator)
			{
				using FOperator = TInputReceiverOperator<DataType>;

				FOperator* Operator = static_cast<FOperator*>(InOperator);
				check(nullptr != Operator);
				Operator->Execute();
			}

			FVertexName DataReferenceName;
			FDataReadReference InputValue;
			FDataWriteReference OutputValue;
			FSendAddress SendAddress;
			TReceiverPtr<DataType> Receiver;
			bool bHasNotReceivedData = true;
		};
	}

	/** Choose input operator based upon data type and access type */
	template<typename DataType, EVertexAccessType VertexAccess=EVertexAccessType::Reference>
	using TInputOperator = std::conditional_t<
		VertexAccess == EVertexAccessType::Value,
		MetasoundInputNodePrivate::FInputValueOperator,
		std::conditional_t<
			TExecutableDataType<DataType>::bIsExecutable,
			MetasoundInputNodePrivate::TExecutableInputOperator<DataType>,
			MetasoundInputNodePrivate::FNonExecutableInputPassThroughOperator
		>
	>;

	/** Choose pass through operator based upon data type and access type */
	template<typename DataType, EVertexAccessType VertexAccess=EVertexAccessType::Reference>
	using TPassThroughOperator = std::conditional_t<
		VertexAccess == EVertexAccessType::Value,
		MetasoundInputNodePrivate::FInputValueOperator,
		MetasoundInputNodePrivate::FNonExecutableInputPassThroughOperator
	>;

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
				using namespace MetasoundInputNodePrivate;

				using FInputNodeType = TInputNode<DataType, VertexAccess>;

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
					return MakeUnique<TPassThroughOperator<DataType, VertexAccess>>(VertexKey, ReferenceCreator->CreateDataReference(*Ref));
				}

				// Owned input value
				FDataReference DataRef = ReferenceCreator->CreateDataReference(InParams.OperatorSettings);
				return MakeUnique<TInputOperator<DataType, VertexAccess>>(VertexKey, DataRef);
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
