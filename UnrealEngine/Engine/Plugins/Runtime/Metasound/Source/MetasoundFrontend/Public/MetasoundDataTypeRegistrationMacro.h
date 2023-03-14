// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/Casts.h"

#include "IAudioProxyInitializer.h"
#include "MetasoundArrayNodesRegistration.h"
#include "MetasoundAutoConverterNode.h"
#include "MetasoundConverterNodeRegistrationMacro.h"
#include "MetasoundDataFactory.h"
#include "MetasoundDataReference.h"
#include "MetasoundDataReferenceMacro.h"
#include "MetasoundEnum.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDataTypeTraits.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundInputNode.h"
#include "MetasoundLiteral.h"
#include "MetasoundLiteralNode.h"
#include "MetasoundLog.h"
#include "MetasoundNodeRegistrationMacro.h"
#include "MetasoundOutputNode.h"
#include "MetasoundReceiveNode.h"
#include "MetasoundSendNode.h"
#include "MetasoundTransmissionRegistration.h"
#include "MetasoundVariableNodes.h"

#include <type_traits>

namespace Metasound
{
	namespace MetasoundDataTypeRegistrationPrivate
	{
		// Returns the Array version of a literal type if it exists.
		template<ELiteralType LiteralType>
		struct TLiteralArrayEnum 
		{
			// Default to TArray default constructor by using
			// ELiteralType::None
			static constexpr ELiteralType Value = ELiteralType::None;
		};

		// Specialization for None->NoneArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::None>
		{
			static constexpr ELiteralType Value = ELiteralType::NoneArray;
		};
		 
		// Specialization for Boolean->BooleanArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::Boolean>
		{
			static constexpr ELiteralType Value = ELiteralType::BooleanArray;
		};
		
		// Specialization for Integer->IntegerArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::Integer>
		{
			static constexpr ELiteralType Value = ELiteralType::IntegerArray;
		};
		
		// Specialization for Float->FloatArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::Float>
		{
			static constexpr ELiteralType Value = ELiteralType::FloatArray;
		};
		
		// Specialization for String->StringArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::String>
		{
			static constexpr ELiteralType Value = ELiteralType::StringArray;
		};
		
		// Specialization for UObjectProxy->UObjectProxyArray
		template<>
		struct TLiteralArrayEnum<ELiteralType::UObjectProxy>
		{
			static constexpr ELiteralType Value = ELiteralType::UObjectProxyArray;
		};

		// This utility function can be used to optionally check to see if we can transmit a data type, and autogenerate send and receive nodes for that datatype.
		template<typename TDataType, typename TEnableIf<TIsTransmittable<TDataType>::Value, bool>::Type = true>
		void AttemptToRegisterSendAndReceiveNodes()
		{
			if (TEnableTransmissionNodeRegistration<TDataType>::Value)
			{
				ensureAlways(RegisterNodeWithFrontend<Metasound::TSendNode<TDataType>>());
				ensureAlways(RegisterNodeWithFrontend<Metasound::TReceiveNode<TDataType>>());
			}
		}

		template<typename TDataType, typename TEnableIf<!TIsTransmittable<TDataType>::Value, bool>::Type = true>
		void AttemptToRegisterSendAndReceiveNodes()
		{
			// This implementation intentionally noops, because Metasound::TIsTransmittable is false for this datatype.
			// This is either because the datatype is not trivially copyable, and thus can't be buffered between threads,
			// or it's not an audio buffer type, which we use Audio::FPatchMixerSplitter instances for.
		}

		// This utility function can be used to check to see if we can static cast between two types, and autogenerate a node for that static cast.
		template<typename TFromDataType, typename TToDataType, typename std::enable_if<TIsAutoConvertible<TFromDataType, TToDataType>::Value, bool>::type = true>
		void AttemptToRegisterConverter()
		{
			using FConverterNode = Metasound::TAutoConverterNode<TFromDataType, TToDataType>;

			if (TEnableAutoConverterNodeRegistration<TFromDataType, TToDataType>::Value)
			{
				const FNodeClassMetadata& Metadata = FConverterNode::GetAutoConverterNodeMetadata();
				const Metasound::Frontend::FNodeRegistryKey Key = FMetasoundFrontendRegistryContainer::GetRegistryKey(Metadata);

				if (!std::is_same<TFromDataType, TToDataType>::value && !FMetasoundFrontendRegistryContainer::Get()->IsNodeRegistered(Key))
				{
					ensureAlways(RegisterNodeWithFrontend<FConverterNode>(Metadata));
					
					bool bSucessfullyRegisteredConversionNode = RegisterConversionNode<FConverterNode, TFromDataType, TToDataType>(FConverterNode::GetInputName(), FConverterNode::GetOutputName(), Metadata);
					ensureAlways(bSucessfullyRegisteredConversionNode);
				}
			}
		}

		template<typename TFromDataType, typename TToDataType, typename std::enable_if<!TIsAutoConvertible<TFromDataType, TToDataType>::Value, int>::type = 0>
		void AttemptToRegisterConverter()
		{
			// This implementation intentionally noops, because static_cast<TFromDataType>(TToDataType&) is invalid.
		}

		// Here we attempt to infer and autogenerate conversions for basic datatypes.
		template<typename TDataType>
		void RegisterConverterNodes()
		{
			// Conversions to this data type:
			AttemptToRegisterConverter<bool, TDataType>();
			AttemptToRegisterConverter<int32, TDataType>();
			AttemptToRegisterConverter<float, TDataType>();
			AttemptToRegisterConverter<FString, TDataType>();

			// Conversions from this data type:
			AttemptToRegisterConverter<TDataType, bool>();
			AttemptToRegisterConverter<TDataType, int32>();
			AttemptToRegisterConverter<TDataType, float>();
			AttemptToRegisterConverter<TDataType, FString>();
		}

		/** Creates the FDataTypeRegistryInfo for a data type.
		 * 
		 * @tparam TDataType - The data type to create info for.
		 * @tparam PreferredArgType - The preferred constructor argument type to use when creating an instance of the data type.
		 * @tparam UClassToUse - The preferred UObject class to use when constructing from an Audio::IProxyData.
		 */
		template<typename TDataType, ELiteralType PreferredArgType, typename UClassToUse>
		Frontend::FDataTypeRegistryInfo CreateDataTypeInfo()
		{
			Frontend::FDataTypeRegistryInfo RegistryInfo;

			RegistryInfo.DataTypeName = GetMetasoundDataTypeName<TDataType>();
			RegistryInfo.DataTypeDisplayText = GetMetasoundDataTypeDisplayText<TDataType>();
			RegistryInfo.PreferredLiteralType = PreferredArgType;

			RegistryInfo.bIsParsable = TLiteralTraits<TDataType>::bIsParsableFromAnyLiteralType;
			RegistryInfo.bIsArrayParseable = TLiteralTraits<TDataType>::bIsParseableFromAnyArrayLiteralType;

			RegistryInfo.bIsArrayType = TIsArrayType<TDataType>::Value;

			RegistryInfo.bIsDefaultParsable = TIsParsable<TDataType, FLiteral::FNone>::Value;
			RegistryInfo.bIsBoolParsable = TIsParsable<TDataType, bool>::Value;
			RegistryInfo.bIsIntParsable = TIsParsable<TDataType, int32>::Value;
			RegistryInfo.bIsFloatParsable = TIsParsable<TDataType, float>::Value;
			RegistryInfo.bIsStringParsable = TIsParsable<TDataType, FString>::Value;
			RegistryInfo.bIsProxyParsable = TIsParsable<TDataType, const Audio::IProxyDataPtr&>::Value;

			RegistryInfo.bIsDefaultArrayParsable = TIsParsable<TDataType, TArray<FLiteral::FNone>>::Value;
			RegistryInfo.bIsBoolArrayParsable = TIsParsable<TDataType, TArray<bool>>::Value;
			RegistryInfo.bIsIntArrayParsable = TIsParsable<TDataType, TArray<int32>>::Value;
			RegistryInfo.bIsFloatArrayParsable = TIsParsable<TDataType, TArray<float>>::Value;
			RegistryInfo.bIsStringArrayParsable = TIsParsable<TDataType, TArray<FString>>::Value;
			RegistryInfo.bIsProxyArrayParsable = TIsParsable<TDataType, const TArray<Audio::IProxyDataPtr>& >::Value;

			RegistryInfo.bIsEnum = TEnumTraits<TDataType>::bIsEnum;
			RegistryInfo.bIsVariable = TIsVariable<TDataType>::Value;
			RegistryInfo.bIsTransmittable = TIsTransmittable<TDataType>::Value;
			RegistryInfo.bIsConstructorType = TIsConstructorVertexSupported<TDataType>::Value;
			
			if constexpr (std::is_base_of<UObject, UClassToUse>::value)
			{
				RegistryInfo.ProxyGeneratorClass = UClassToUse::StaticClass();
			}
			else
			{
				static_assert(std::is_same<UClassToUse, void>::value, "Only UObject derived classes can supply proxy interfaces.");
				RegistryInfo.ProxyGeneratorClass = nullptr;
			}

			return RegistryInfo;
		}

		/** Returns an IEnumDataTypeInterface pointer for the data type. If the 
		 * data type has no IEnumDataTypeInterface, the returned pointer will be
		 * invalid.
		 *
		 * @tparam TDataType - The data type to create the interface for.
		 *
		 * @return A shared pointer to the IEnumDataTypeInterface. If the TDataType
		 *         does not have an IEnumDataTypeInterface, returns an invalid pointer.
		 */
		template<typename TDataType>
		TSharedPtr<Frontend::IEnumDataTypeInterface> GetEnumDataTypeInterface()
		{
			TSharedPtr<Frontend::IEnumDataTypeInterface> EnumInterfacePtr;

			using FEnumTraits = TEnumTraits<TDataType>;

			// Check if data type is an enum.
			if constexpr (FEnumTraits::bIsEnum)
			{
				using InnerType = typename FEnumTraits::InnerType;
				using FStringHelper = TEnumStringHelper<InnerType>;

				struct FEnumHandler : Metasound::Frontend::IEnumDataTypeInterface
				{
					FName GetNamespace() const override
					{
						return FStringHelper::GetNamespace();
					}

					int32 GetDefaultValue() const override
					{
						return static_cast<int32>(TEnumTraits<TDataType>::DefaultValue);
					}

					const TArray<FGenericInt32Entry>& GetAllEntries() const override
					{
						auto BuildIntEntries = []()
						{
							// Convert to int32 representation 
							TArray<FGenericInt32Entry> IntEntries;
							IntEntries.Reserve(FStringHelper::GetAllEntries().Num());
							for (const TEnumEntry<InnerType>& i : FStringHelper::GetAllEntries())
							{
								IntEntries.Emplace(i);
							}
							return IntEntries;
						};
						static const TArray<FGenericInt32Entry> IntEntries = BuildIntEntries();
						return IntEntries;
					}
				};

				EnumInterfacePtr = MakeShared<FEnumHandler>();
			}

			return EnumInterfacePtr;
		}

		/** Registers a data type with the MetaSound Frontend. This allows the data type
		 * to be used in Input and Output nodes by informing the Frontend how to 
		 * instantiate an instance. 
		 *
		 * @tparam TDataType - The data type to register.
		 * @tparam PreferredArgType - The preferred constructor argument type to use when creating an instance of the data type.
		 * @tparam UClassToUse - The preferred UObject class to use when constructing from an Audio::IProxyDataPtr. If the type is not
		 *                       constructible with an Audio::IProxyDataPtr, then this should be void.
		 *
		 * @return True on success, false on failure.
		 */
		template<typename TDataType, ELiteralType PreferredArgType = ELiteralType::None, typename UClassToUse = void>
		bool RegisterDataTypeWithFrontendInternal()
		{
			static constexpr bool bIsParsable = TLiteralTraits<TDataType>::bIsParsableFromAnyLiteralType;
			static constexpr bool bIsConstructorType = TIsConstructorVertexSupported<TDataType>::Value;

			static bool bAlreadyRegisteredThisDataType = false;
			if (bAlreadyRegisteredThisDataType)
			{
				UE_LOG(LogMetaSound, Display, TEXT("Tried to call REGISTER_METASOUND_DATATYPE twice with the same class %s. ignoring the second call. Likely because REGISTER_METASOUND_DATATYPE is in a header that's used in multiple modules. Consider moving it to a private header or cpp file."), TDataReferenceTypeInfo<TDataType>::TypeName)
				return false;
			}

			bAlreadyRegisteredThisDataType = true;

			// Define registry entry class for this data type.
			class FDataTypeRegistryEntry : public Frontend::IDataTypeRegistryEntry
			{
			public:

				FDataTypeRegistryEntry()
				: Info(CreateDataTypeInfo<TDataType, PreferredArgType, UClassToUse>())
				, EnumInterface(GetEnumDataTypeInterface<TDataType>())
				{
					// Create class info using prototype node
					// TODO: register nodes with static class info instead of prototype instance.

					if constexpr (bIsParsable)
					{
						TInputNode<TDataType, EVertexAccessType::Reference> InputPrototype(FInputNodeConstructorParams { });
						InputClass = Metasound::Frontend::GenerateClass(InputPrototype.GetMetadata(), EMetasoundFrontendClassType::Input);

						TOutputNode<TDataType, EVertexAccessType::Reference> OutputPrototype(TEXT(""), FGuid(), TEXT(""));
						OutputClass = Metasound::Frontend::GenerateClass(OutputPrototype.GetMetadata(), EMetasoundFrontendClassType::Output);

						TLiteralNode<TDataType> LiteralPrototype(TEXT(""), FGuid(), FLiteral());
						LiteralClass = Metasound::Frontend::GenerateClass(LiteralPrototype.GetMetadata(), EMetasoundFrontendClassType::Literal);

						TVariableNode<TDataType> VariablePrototype(TEXT(""), FGuid(), FLiteral());
						VariableClass = Metasound::Frontend::GenerateClass(VariablePrototype.GetMetadata(), EMetasoundFrontendClassType::Variable);

						TVariableMutatorNode<TDataType> VariableMutatorPrototype(TEXT(""), FGuid());
						VariableMutatorClass = Metasound::Frontend::GenerateClass(VariableMutatorPrototype.GetMetadata(), EMetasoundFrontendClassType::VariableMutator);

						TVariableAccessorNode<TDataType> VariableAccessorPrototype(TEXT(""), FGuid());
						VariableAccessorClass = Metasound::Frontend::GenerateClass(VariableAccessorPrototype.GetMetadata(), EMetasoundFrontendClassType::VariableAccessor);

						TVariableDeferredAccessorNode<TDataType> VariableDeferredAccessorPrototype(TEXT(""), FGuid());
						VariableDeferredAccessorClass = Metasound::Frontend::GenerateClass(VariableDeferredAccessorPrototype.GetMetadata(), EMetasoundFrontendClassType::VariableDeferredAccessor);

						if constexpr (bIsConstructorType)
						{
							TInputNode<TDataType, EVertexAccessType::Value> ConstructorInputPrototype(FInputNodeConstructorParams { });
							ConstructorInputClass = Metasound::Frontend::GenerateClass(ConstructorInputPrototype.GetMetadata(), EMetasoundFrontendClassType::Input);
							TOutputNode<TDataType, EVertexAccessType::Value> ConstructorOutputPrototype(TEXT(""), FGuid(), TEXT(""));
							ConstructorOutputClass = Metasound::Frontend::GenerateClass(ConstructorOutputPrototype.GetMetadata(), EMetasoundFrontendClassType::Output);
						}
					}
				}

				virtual ~FDataTypeRegistryEntry() {}

				virtual const Frontend::FDataTypeRegistryInfo& GetDataTypeInfo() const override
				{
					return Info;
				}

				virtual TSharedPtr<const Frontend::IEnumDataTypeInterface> GetEnumInterface() const override
				{
					return EnumInterface;
				}

				virtual const FMetasoundFrontendClass& GetFrontendInputClass() const override
				{
					return InputClass;
				}

				virtual const FMetasoundFrontendClass& GetFrontendConstructorInputClass() const override
				{
					return ConstructorInputClass;
				}

				virtual const FMetasoundFrontendClass& GetFrontendLiteralClass() const override
				{
					return LiteralClass;
				}

				virtual const FMetasoundFrontendClass& GetFrontendOutputClass() const override
				{
					return OutputClass;
				}

				virtual const FMetasoundFrontendClass& GetFrontendConstructorOutputClass() const override
				{
					return ConstructorOutputClass;
				}

				virtual const FMetasoundFrontendClass& GetFrontendVariableClass() const override
				{
					return VariableClass;
				}

				virtual const FMetasoundFrontendClass& GetFrontendVariableMutatorClass() const override
				{
					return VariableMutatorClass;
				}

				virtual const FMetasoundFrontendClass& GetFrontendVariableAccessorClass() const override
				{
					return VariableAccessorClass;
				}

				virtual const FMetasoundFrontendClass& GetFrontendVariableDeferredAccessorClass() const 
				{
					return VariableDeferredAccessorClass;
				}

				virtual TUniquePtr<INode> CreateInputNode(FInputNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TInputNode<TDataType, EVertexAccessType::Reference>>(MoveTemp(InParams));
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateConstructorInputNode(FInputNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable && bIsConstructorType)
					{
						checkf(!InParams.bEnableTransmission, TEXT("Cannot enable transmission on a constructor input."));
						return MakeUnique<TInputNode<TDataType, EVertexAccessType::Value>>(MoveTemp(InParams));
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateOutputNode(FOutputNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TOutputNode<TDataType, EVertexAccessType::Reference>>(InParams.NodeName, InParams.InstanceID, InParams.VertexName);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateConstructorOutputNode(FOutputNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable && bIsConstructorType)
					{
						return MakeUnique<TOutputNode<TDataType, EVertexAccessType::Value>>(InParams.NodeName, InParams.InstanceID, InParams.VertexName);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateLiteralNode(FLiteralNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TLiteralNode<TDataType>>(InParams.NodeName, InParams.InstanceID, MoveTemp(InParams.Literal));
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateReceiveNode(const FNodeInitData& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TReceiveNode<TDataType>>(InParams);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateVariableNode(FVariableNodeConstructorParams&& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TVariableNode<TDataType>>(InParams.NodeName, InParams.InstanceID, MoveTemp(InParams.Literal));
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateVariableMutatorNode(const FNodeInitData& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TVariableMutatorNode<TDataType>>(InParams);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateVariableAccessorNode(const FNodeInitData& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TVariableAccessorNode<TDataType>>(InParams);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual TUniquePtr<INode> CreateVariableDeferredAccessorNode(const FNodeInitData& InParams) const override
				{
					if constexpr (bIsParsable)
					{
						return MakeUnique<TVariableDeferredAccessorNode<TDataType>>(InParams);
					}
					else
					{
						return TUniquePtr<INode>(nullptr);
					}
				}

				virtual Audio::IProxyDataPtr CreateProxy(UObject* InObject) const override
				{
					// Only attempt to create proxy if the `UClassToUse` is not void.
					if constexpr (!std::is_same<UClassToUse, void>::value)
					{
						static_assert(std::is_base_of<IAudioProxyDataFactory, UClassToUse>::value, "If a Metasound Datatype uses a UObject as a literal, the UClass of that object needs to also derive from Audio::IProxyDataFactory. See USoundWave as an example.");
						if (Frontend::IDataTypeRegistry::Get().IsUObjectProxyFactory(InObject))
						{
							IAudioProxyDataFactory* ObjectAsFactory = Audio::CastToProxyDataFactory<UClassToUse>(InObject);
							if (ensureAlways(ObjectAsFactory))
							{
								Audio::FProxyDataInitParams ProxyInitParams;
								ProxyInitParams.NameOfFeatureRequestingProxy = "MetaSound";

								return ObjectAsFactory->CreateNewProxyData(ProxyInitParams);
							}
						}
					}

					return Audio::IProxyDataPtr(nullptr);
				}

				virtual TOptional<FAnyDataReference> CreateDataReference(EDataReferenceAccessType InAccessType, const FLiteral& InLiteral, const FOperatorSettings& InOperatorSettings) const override
				{
					if constexpr(bIsParsable)
					{
						switch (InAccessType)
						{
							case EDataReferenceAccessType::Read:
								return FAnyDataReference{TDataReadReferenceLiteralFactory<TDataType>::CreateExplicitArgs(InOperatorSettings, InLiteral)};

							case EDataReferenceAccessType::Write:
								return FAnyDataReference{TDataWriteReferenceLiteralFactory<TDataType>::CreateExplicitArgs(InOperatorSettings, InLiteral)};

							case EDataReferenceAccessType::Value:
								return FAnyDataReference{TDataValueReferenceLiteralFactory<TDataType>::CreateExplicitArgs(InOperatorSettings, InLiteral)};

							default:
								break;
						}
					}
					return TOptional<FAnyDataReference>();
				}

				virtual TSharedPtr<IDataChannel, ESPMode::ThreadSafe> CreateDataChannel(const FOperatorSettings& InOperatorSettings) const override
				{
					if constexpr (bIsParsable)
					{
						return FTransmissionDataChannelFactory::CreateDataChannel<TDataType>(InOperatorSettings);
					}
					else
					{
						return TSharedPtr<IDataChannel, ESPMode::ThreadSafe>(nullptr);
					}
				}

				virtual TUniquePtr<IDataTypeRegistryEntry> Clone() const override
				{
					return MakeUnique<FDataTypeRegistryEntry>();
				}

			private:
				Frontend::FDataTypeRegistryInfo Info;
				FMetasoundFrontendClass InputClass;
				FMetasoundFrontendClass ConstructorInputClass;
				FMetasoundFrontendClass OutputClass;
				FMetasoundFrontendClass ConstructorOutputClass;
				FMetasoundFrontendClass LiteralClass;
				FMetasoundFrontendClass VariableClass;
				FMetasoundFrontendClass VariableMutatorClass;
				FMetasoundFrontendClass VariableAccessorClass;
				FMetasoundFrontendClass VariableDeferredAccessorClass;
				TSharedPtr<Frontend::IEnumDataTypeInterface> EnumInterface;
			};

			bool bSucceeded = Frontend::IDataTypeRegistry::Get().RegisterDataType(MakeUnique<FDataTypeRegistryEntry>());
			ensureAlwaysMsgf(bSucceeded, TEXT("Failed to register data type %s in the node registry!"), *GetMetasoundDataTypeString<TDataType>());

			if (bSucceeded)
			{
				RegisterConverterNodes<TDataType>();
				AttemptToRegisterSendAndReceiveNodes<TDataType>();
			}
			
			return bSucceeded;
		}

		/** Registers an array of a data type with the MetaSound Frontend. This allows 
		 * an array of the data type to be used in Input, Output, Send and Receive 
		 * nodes by informing the Frontend how to instantiate an instance. 
		 *
		 * @tparam TDataType - The data type to register.
		 * @tparam PreferredArgType - The preferred constructor argument type to use when creating an instance of the data type.
		 * @tparam UClassToUse - The preferred UObject class to use when constructing from an Audio::IProxyDataPtr. If the type is not
		 *                       constructible with an Audio::IProxyDataPtr, then this should be void.
		 *
		 * @return True on success, false on failure.
		 */
		template<typename TDataType, ELiteralType PreferredArgType>
		bool RegisterDataTypeArrayWithFrontend()
		{
			using namespace MetasoundDataTypeRegistrationPrivate;
			using TArrayType = TArray<TDataType>;

			if (TEnableAutoArrayTypeRegistration<TDataType>::Value)
			{
				constexpr bool bIsArrayType = true;
				bool bSuccess = RegisterDataTypeWithFrontendInternal<TArrayType, TLiteralArrayEnum<PreferredArgType>::Value>();
				bSuccess = bSuccess && RegisterArrayNodes<TArrayType>();
				bSuccess = bSuccess && RegisterDataTypeWithFrontendInternal<TVariable<TArrayType>>();
				return bSuccess;
			}

			return true;
		}
	}
	
	/** Registers a data type with the MetaSound Frontend. This allows the data type 
	 * to be used in Input, Output, Send and Receive  nodes by informing the 
	 * Frontend how to instantiate an instance. 
	 *
	 * @tparam TDataType - The data type to register.
	 * @tparam PreferredArgType - The preferred constructor argument type to use when creating an instance of the data type.
	 * @tparam UClassToUse - The preferred UObject class to use when constructing from an Audio::IProxyDataPtr. If the type is not
	 *                       constructible with an Audio::IProxyDataPtr, then this should be void.
	 *
	 * @return True on success, false on failure.
	 */
	template<typename TDataType, ELiteralType PreferredArgType = ELiteralType::None, typename UClassToUse = void>
	bool RegisterDataTypeWithFrontend()
	{
		using namespace MetasoundDataTypeRegistrationPrivate;

		// Register TDataType as a metasound data type.
		bool bSuccess = RegisterDataTypeWithFrontendInternal<TDataType, PreferredArgType, UClassToUse>();
		ensure(bSuccess);
		bSuccess = bSuccess && RegisterDataTypeWithFrontendInternal<TVariable<TDataType>>();
		ensure(bSuccess);

		// Register TArray<TDataType> as a metasound data type.
		bSuccess = bSuccess && RegisterDataTypeArrayWithFrontend<TDataType, PreferredArgType>();
		ensure(bSuccess);

		return bSuccess;
	}


	/** Registration info for a data type.
	 *
	 * @tparam DataType - The data type to be registered. 
	 */
	template<typename DataType>
	struct TMetasoundDataTypeRegistration
	{
		static_assert(std::is_same<DataType, typename std::decay<DataType>::type>::value, "DataType and decayed DataType must be the same");
		
		// To register a data type, an input node must be able to instantiate it.
		static constexpr bool bCanRegister = TInputNode<DataType, EVertexAccessType::Reference>::bCanRegister;

		// If a data type has been successfully registered, this will be true.
		static const bool bSuccessfullyRegistered;
	};
}

// This should be used to expose a datatype as a potential input or output for a metasound graph.
// The first argument to the macro is the class to expose.
// the second argument is the display name of that type in the Metasound editor.
// Optionally, a Metasound::ELiteralType can be passed in to designate a preferred literal type-
// For example, if Metasound::ELiteralType::Float is passed in, we will default to using a float parameter to create this datatype.
// If no argument is passed in, we will infer a literal type to use.
// If 
// Metasound::ELiteralType::Invalid can be used to enforce that we don't provide space for a literal, in which case you should have a default constructor or a constructor that takes [const FOperatorSettings&] implemented.
// If you pass in a preferred arg type, please make sure that the passed in datatype has a matching constructor, since we won't check this until runtime.

#define CANNOT_REGISTER_METASOUND_DATA_TYPE_ASSERT_STRING(DataType) \
"To register " #DataType " to be used as a Metasounds input or output type, it needs a default constructor or one of the following constructors must be implemented:  " \
#DataType "(), " \
#DataType "(bool InValue), " \
#DataType "(int32 InValue), " \
#DataType "(float InValue), " \
#DataType "(const FString& InString)" \
#DataType "(const Audio::IProxyDataPtr& InData),  or " \
#DataType "(const TArray<Audio::IProxyDataPtr>& InProxyArray)."\
#DataType "(const ::Metasound::FOperatorSettings& InSettings), " \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, bool InValue), " \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, int32 InValue), " \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, float InValue), " \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, const FString& InString)" \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, const Audio::IProxyDataPtr& InData),  or " \
#DataType "(const ::Metasound::FOperatorSettings& InSettings, const TArray<Audio::IProxyDataPtr>& InProxyArray)."

#define REGISTER_METASOUND_DATATYPE(DataType, DataTypeName, ...) \
	DEFINE_METASOUND_DATA_TYPE(DataType, DataTypeName); \
	static_assert(::Metasound::TMetasoundDataTypeRegistration<DataType>::bCanRegister, CANNOT_REGISTER_METASOUND_DATA_TYPE_ASSERT_STRING(DataType)); \
	template<> const bool ::Metasound::TMetasoundDataTypeRegistration<DataType>::bSuccessfullyRegistered = ::FMetasoundFrontendRegistryContainer::Get()->EnqueueInitCommand([](){ ::Metasound::RegisterDataTypeWithFrontend<DataType, ##__VA_ARGS__>(); }); // This static bool is useful for debugging, but also is the only way the compiler will let us call this function outside of an expression.

