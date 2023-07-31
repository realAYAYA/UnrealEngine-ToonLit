// Copyright Epic Games, Inc. All Rights Reserved.
#include "MetasoundFrontendDataTypeRegistry.h"

#include "MetasoundTrace.h"

namespace Metasound
{
	namespace Frontend
	{
		namespace MetasoundFrontendDataTypeRegistryPrivate
		{
			// Return the compatible literal with the most descriptive type.
			// TODO: Currently TIsParsable<> allows for implicit conversion of
			// constructor arguments of integral types which can cause some confusion
			// here when trying to match a literal type to a constructor. For example:
			//
			// struct FBoolConstructibleType
			// {
			// 	FBoolConstructibleType(bool InValue);
			// };
			//
			// static_assert(TIsParsable<FBoolConstructible, double>::Value); 
			//
			// Implicit conversions are currently allowed in TIsParsable because this
			// is perfectly legal syntax.
			//
			// double Value = 10.0;
			// FBoolConstructibleType BoolConstructible = Value;
			//
			// There are some tricks to possibly disable implicit conversions when
			// checking for specific constructors, but they are yet to be implemented 
			// and are untested. Here's the basic idea.
			//
			// template<DataType, DesiredIntegralArgType>
			// struct TOnlyConvertIfIsSame
			// {
			// 		// Implicit conversion only defined if types match.
			// 		template<typename SuppliedIntegralArgType, std::enable_if<std::is_same<std::decay<SuppliedIntegralArgType>::type, DesiredIntegralArgType>::value, int> = 0>
			// 		operator DesiredIntegralArgType()
			// 		{
			// 			return DesiredIntegralArgType{};
			// 		}
			// };
			//
			// static_assert(false == std::is_constructible<FBoolConstructibleType, TOnlyConvertIfSame<double>>::value);
			// static_assert(true == std::is_constructible<FBoolConstructibleType, TOnlyConvertIfSame<bool>>::value);
			ELiteralType GetMostDescriptiveLiteralForDataType(const FDataTypeRegistryInfo& InDataTypeInfo)
			{
				if (InDataTypeInfo.bIsProxyArrayParsable)
				{
					return ELiteralType::UObjectProxyArray;
				}
				else if (InDataTypeInfo.bIsProxyParsable)
				{
					return ELiteralType::UObjectProxy;
				}
				else if (InDataTypeInfo.bIsEnum && InDataTypeInfo.bIsIntParsable)
				{
					return ELiteralType::Integer;
				}
				else if (InDataTypeInfo.bIsStringArrayParsable)
				{
					return ELiteralType::StringArray;
				}
				else if (InDataTypeInfo.bIsFloatArrayParsable)
				{
					return ELiteralType::FloatArray;
				}
				else if (InDataTypeInfo.bIsIntArrayParsable)
				{
					return ELiteralType::IntegerArray;
				}
				else if (InDataTypeInfo.bIsBoolArrayParsable)
				{
					return ELiteralType::BooleanArray;
				}
				else if (InDataTypeInfo.bIsStringParsable)
				{
					return ELiteralType::String;
				}
				else if (InDataTypeInfo.bIsFloatParsable)
				{
					return ELiteralType::Float;
				}
				else if (InDataTypeInfo.bIsIntParsable)
				{
					return ELiteralType::Integer;
				}
				else if (InDataTypeInfo.bIsBoolParsable)
				{
					return ELiteralType::Boolean;
				}
				else if (InDataTypeInfo.bIsDefaultArrayParsable)
				{
					return ELiteralType::NoneArray; 
				}
				else if (InDataTypeInfo.bIsDefaultParsable)
				{
					return ELiteralType::None;
				}
				else
				{
					// if we ever hit this, something has gone wrong with the REGISTER_METASOUND_DATATYPE macro.
					// we should have failed to compile if any of these are false.
					checkNoEntry();
					return ELiteralType::Invalid;
				}
			}

			// Base class for INodeRegistryEntrys that come from an IDataTypeRegistryEntry
			class FDataTypeNodeRegistryEntry : public INodeRegistryEntry
			{
			public:
				FDataTypeNodeRegistryEntry() = default;

				virtual ~FDataTypeNodeRegistryEntry() = default;

				virtual const FNodeClassInfo& GetClassInfo() const override
				{
					return ClassInfo;
				}

				virtual TUniquePtr<INode> CreateNode(const FNodeInitData&) const override
				{
					return nullptr;
				}

				virtual TUniquePtr<INode> CreateNode(FDefaultLiteralNodeConstructorParams&& InParams) const override
				{
					return nullptr;
				}

				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexNodeConstructorParams&&) const override
				{
					return nullptr;
				}

				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexWithLiteralNodeConstructorParams&& InParams) const override
				{
					return nullptr;
				}

				virtual bool IsNative() const override
				{
					return true;
				}

			protected:

				void UpdateNodeClassInfo(const FMetasoundFrontendClass& InClass)
				{
					ClassInfo = FNodeClassInfo(InClass.Metadata);
				}

			private:
				
				FNodeClassInfo ClassInfo;
			};

			// Node registry entry for input nodes created from a data type registry entry.
			class FInputNodeRegistryEntry : public FDataTypeNodeRegistryEntry
			{
			public:
				FInputNodeRegistryEntry() = delete;

				FInputNodeRegistryEntry(const TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>& InDataTypeEntry)
				: DataTypeEntry(InDataTypeEntry)
				{
					UpdateNodeClassInfo(DataTypeEntry->GetFrontendInputClass());
				}

				virtual ~FInputNodeRegistryEntry() = default;

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return DataTypeEntry->GetFrontendInputClass();
				}

				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexWithLiteralNodeConstructorParams&& InParams) const override
				{
					FInputNodeConstructorParams InputParams;
					InputParams.InitParam = MoveTemp(InParams.InitParam);
					InputParams.InstanceID = InParams.InstanceID;
					InputParams.NodeName = InParams.NodeName;
					InputParams.VertexName = InParams.VertexName;
					InputParams.bEnableTransmission = false;

					return DataTypeEntry->CreateInputNode(MoveTemp(InputParams));
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FInputNodeRegistryEntry>(DataTypeEntry);
				}

			private:
				
				TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe> DataTypeEntry;
			};

			// Node registry entry for constructor input nodes created from a data type registry entry.
			class FConstructorInputNodeRegistryEntry : public FDataTypeNodeRegistryEntry
			{
			public:
				FConstructorInputNodeRegistryEntry() = delete;

				FConstructorInputNodeRegistryEntry(const TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>& InDataTypeEntry)
				: DataTypeEntry(InDataTypeEntry)
				{
					UpdateNodeClassInfo(DataTypeEntry->GetFrontendConstructorInputClass());
				}

				virtual ~FConstructorInputNodeRegistryEntry() = default;

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return DataTypeEntry->GetFrontendConstructorInputClass();
				}

				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexWithLiteralNodeConstructorParams&& InParams) const override
				{
					FInputNodeConstructorParams InputParams;
					InputParams.InitParam = MoveTemp(InParams.InitParam);
					InputParams.InstanceID = InParams.InstanceID;
					InputParams.NodeName = InParams.NodeName;
					InputParams.VertexName = InParams.VertexName;
					InputParams.bEnableTransmission = false;

					return DataTypeEntry->CreateConstructorInputNode(MoveTemp(InputParams));
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FConstructorInputNodeRegistryEntry>(DataTypeEntry);
				}

			private:
				
				TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe> DataTypeEntry;
			};

			// Node registry entry for output nodes created from a data type registry entry.
			class FOutputNodeRegistryEntry : public FDataTypeNodeRegistryEntry
			{
			public:
				FOutputNodeRegistryEntry() = delete;

				FOutputNodeRegistryEntry(const TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>& InDataTypeEntry)
				: DataTypeEntry(InDataTypeEntry)
				{
					UpdateNodeClassInfo(DataTypeEntry->GetFrontendOutputClass());
				}

				virtual ~FOutputNodeRegistryEntry() = default;

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return DataTypeEntry->GetFrontendOutputClass();
				}

				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexNodeConstructorParams&& InParams) const override
				{
					return DataTypeEntry->CreateOutputNode(MoveTemp(InParams));
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FOutputNodeRegistryEntry>(DataTypeEntry);
				}

			private:
				
				TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe> DataTypeEntry;
			};

			// Node registry entry for constructor output nodes created from a data type registry entry.
			class FConstructorOutputNodeRegistryEntry : public FDataTypeNodeRegistryEntry
			{
			public:
				FConstructorOutputNodeRegistryEntry() = delete;

				FConstructorOutputNodeRegistryEntry(const TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>& InDataTypeEntry)
				: DataTypeEntry(InDataTypeEntry)
				{
					UpdateNodeClassInfo(DataTypeEntry->GetFrontendConstructorOutputClass());
				}

				virtual ~FConstructorOutputNodeRegistryEntry() = default;

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return DataTypeEntry->GetFrontendConstructorOutputClass();
				}

				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexNodeConstructorParams&& InParams) const override
				{
					return DataTypeEntry->CreateConstructorOutputNode(MoveTemp(InParams));
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FConstructorOutputNodeRegistryEntry>(DataTypeEntry);
				}

			private:
				
				TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe> DataTypeEntry;
			};


			// Node registry entry for literal nodes created from a data type registry entry.
			class FLiteralNodeRegistryEntry : public FDataTypeNodeRegistryEntry
			{
			public:
				FLiteralNodeRegistryEntry() = delete;

				FLiteralNodeRegistryEntry(const TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>& InDataTypeEntry)
				: DataTypeEntry(InDataTypeEntry)
				{
					UpdateNodeClassInfo(DataTypeEntry->GetFrontendLiteralClass());
				}

				virtual ~FLiteralNodeRegistryEntry() = default;

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return DataTypeEntry->GetFrontendLiteralClass();
				}

				virtual TUniquePtr<INode> CreateNode(FDefaultLiteralNodeConstructorParams&& InParams) const override
				{
					return DataTypeEntry->CreateLiteralNode(MoveTemp(InParams));
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FLiteralNodeRegistryEntry>(DataTypeEntry);
				}

			private:
				
				TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe> DataTypeEntry;
			};


			// Node registry entry for init variable nodes created from a data type registry entry.
			class FVariableNodeRegistryEntry : public FDataTypeNodeRegistryEntry
			{
			public:
				FVariableNodeRegistryEntry() = delete;

				FVariableNodeRegistryEntry(const TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>& InDataTypeEntry)
				: DataTypeEntry(InDataTypeEntry)
				{
					UpdateNodeClassInfo(DataTypeEntry->GetFrontendVariableClass());
				}

				virtual ~FVariableNodeRegistryEntry() = default;

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return DataTypeEntry->GetFrontendVariableClass();
				}

				virtual TUniquePtr<INode> CreateNode(FDefaultLiteralNodeConstructorParams&& InParams) const override
				{
					return DataTypeEntry->CreateVariableNode(MoveTemp(InParams));
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FVariableNodeRegistryEntry>(DataTypeEntry);
				}

			private:
				
				TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe> DataTypeEntry;
			};

			// Node registry entry for set variable nodes created from a data type registry entry.
			class FVariableMutatorNodeRegistryEntry : public FDataTypeNodeRegistryEntry
			{
			public:
				FVariableMutatorNodeRegistryEntry() = delete;

				FVariableMutatorNodeRegistryEntry(const TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>& InDataTypeEntry)
				: DataTypeEntry(InDataTypeEntry)
				{
					UpdateNodeClassInfo(DataTypeEntry->GetFrontendVariableMutatorClass());
				}

				virtual ~FVariableMutatorNodeRegistryEntry() = default;

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return DataTypeEntry->GetFrontendVariableMutatorClass();
				}

				virtual TUniquePtr<INode> CreateNode(const FNodeInitData& InParams) const override
				{
					return DataTypeEntry->CreateVariableMutatorNode(InParams);
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FVariableMutatorNodeRegistryEntry>(DataTypeEntry);
				}

			private:
				
				TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe> DataTypeEntry;
			};

			// Node registry entry for get variable nodes created from a data type registry entry.
			class FVariableAccessorNodeRegistryEntry : public FDataTypeNodeRegistryEntry
			{
			public:
				FVariableAccessorNodeRegistryEntry() = delete;

				FVariableAccessorNodeRegistryEntry(const TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>& InDataTypeEntry)
				: DataTypeEntry(InDataTypeEntry)
				{
					UpdateNodeClassInfo(DataTypeEntry->GetFrontendVariableAccessorClass());
				}

				virtual ~FVariableAccessorNodeRegistryEntry() = default;

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return DataTypeEntry->GetFrontendVariableAccessorClass();
				}

				virtual TUniquePtr<INode> CreateNode(const FNodeInitData& InParams) const override
				{
					return DataTypeEntry->CreateVariableAccessorNode(InParams);
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FVariableAccessorNodeRegistryEntry>(DataTypeEntry);
				}

			private:
				
				TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe> DataTypeEntry;
			};

			// Node registry entry for get delayed variable nodes created from a data type registry entry.
			class FVariableDeferredAccessorNodeRegistryEntry : public FDataTypeNodeRegistryEntry
			{
			public:
				FVariableDeferredAccessorNodeRegistryEntry() = delete;

				FVariableDeferredAccessorNodeRegistryEntry(const TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>& InDataTypeEntry)
				: DataTypeEntry(InDataTypeEntry)
				{
					UpdateNodeClassInfo(DataTypeEntry->GetFrontendVariableDeferredAccessorClass());
				}

				virtual ~FVariableDeferredAccessorNodeRegistryEntry() = default;

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return DataTypeEntry->GetFrontendVariableDeferredAccessorClass();
				}

				virtual TUniquePtr<INode> CreateNode(const FNodeInitData& InParams) const override
				{
					return DataTypeEntry->CreateVariableDeferredAccessorNode(InParams);
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FVariableDeferredAccessorNodeRegistryEntry>(DataTypeEntry);
				}

			private:
				
				TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe> DataTypeEntry;
			};

			class FDataTypeRegistry : public IDataTypeRegistry
			{
			public:
				virtual ~FDataTypeRegistry() = default;

				/** Register a data type
				 * @param InName - Name of data type.
				 * @param InEntry - TUniquePtr to data type registry entry.
				 *
				 * @return True on success, false on failure.
				 */
				virtual bool RegisterDataType(TUniquePtr<IDataTypeRegistryEntry>&& InEntry) override;

				virtual void GetRegisteredDataTypeNames(TArray<FName>& OutNames) const override;

				virtual bool GetDataTypeInfo(const UObject* InObject, FDataTypeRegistryInfo& OutInfo) const override;
				virtual bool GetDataTypeInfo(const FName& InDataType, FDataTypeRegistryInfo& OutInfo) const override;

				virtual void IterateDataTypeInfo(TFunctionRef<void(const FDataTypeRegistryInfo&)> InFunction) const override;

				// Return the enum interface for a data type. If the data type does not have 
				// an enum interface, returns a nullptr.
				virtual TSharedPtr<const IEnumDataTypeInterface> GetEnumInterfaceForDataType(const FName& InDataType) const override;

				virtual ELiteralType GetDesiredLiteralType(const FName& InDataType) const override;

				virtual bool IsRegistered(const FName& InDataType) const override;

				virtual bool IsLiteralTypeSupported(const FName& InDataType, ELiteralType InLiteralType) const override;
				virtual bool IsLiteralTypeSupported(const FName& InDataType, EMetasoundFrontendLiteralType InLiteralType) const override;

				virtual UClass* GetUClassForDataType(const FName& InDataType) const override;

				bool IsUObjectProxyFactory(UObject* InObject) const override;
				Audio::IProxyDataPtr CreateProxyFromUObject(const FName& InDataType, UObject* InObject) const override;

				virtual FLiteral CreateDefaultLiteral(const FName& InDataType) const override;
				virtual FLiteral CreateLiteralFromUObject(const FName& InDataType, UObject* InObject) const override;
				virtual FLiteral CreateLiteralFromUObjectArray(const FName& InDataType, const TArray<UObject*>& InObjectArray) const override;

				virtual TOptional<FAnyDataReference> CreateDataReference(const FName& InDataType, EDataReferenceAccessType InAccessType, const FLiteral& InLiteral, const FOperatorSettings& InOperatorSettings) const override;
				virtual TSharedPtr<IDataChannel, ESPMode::ThreadSafe> CreateDataChannel(const FName& InDataType, const FOperatorSettings& InOperatorSettings) const override;

				virtual bool GetFrontendInputClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const override;
				virtual bool GetFrontendConstructorInputClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const override;
				virtual bool GetFrontendLiteralClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const override;
				virtual bool GetFrontendOutputClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const override;
				virtual bool GetFrontendConstructorOutputClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const override;
				virtual bool GetFrontendVariableClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const override;
				virtual bool GetFrontendVariableMutatorClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const override;
				virtual bool GetFrontendVariableAccessorClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const override;
				virtual bool GetFrontendVariableDeferredAccessorClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const override;

				// Create a new instance of a C++ implemented node from the registry.
				virtual TUniquePtr<INode> CreateInputNode(const FName& InInputType, FInputNodeConstructorParams&& InParams) const override;
				virtual TUniquePtr<INode> CreateConstructorInputNode(const FName& InInputType, FInputNodeConstructorParams&& InParams) const override;
				virtual TUniquePtr<INode> CreateLiteralNode(const FName& InLiteralType, FLiteralNodeConstructorParams&& InParams) const override;
				virtual TUniquePtr<INode> CreateOutputNode(const FName& InOutputType, FOutputNodeConstructorParams&& InParams) const override;
				virtual TUniquePtr<INode> CreateConstructorOutputNode(const FName& InOutputType, FOutputNodeConstructorParams&& InParams) const override;
				virtual TUniquePtr<INode> CreateReceiveNode(const FName& InOutputType, const FNodeInitData& InParams) const override;
				virtual TUniquePtr<INode> CreateVariableNode(const FName& InDataType, FVariableNodeConstructorParams&&) const override;
				virtual TUniquePtr<INode> CreateVariableMutatorNode(const FName& InDataType, const FNodeInitData&) const override;
				virtual TUniquePtr<INode> CreateVariableAccessorNode(const FName& InDataType, const FNodeInitData&) const override;
				virtual TUniquePtr<INode> CreateVariableDeferredAccessorNode(const FName& InDataType, const FNodeInitData&) const override;

			private:

				const IDataTypeRegistryEntry* FindDataTypeEntry(const FName& InDataTypeName) const;

				TMap<FName, TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>> RegisteredDataTypes;

				// UObject type names to DataTypeNames
				TMap<const UClass*, FName> RegisteredObjectClasses;
			};

			bool FDataTypeRegistry::RegisterDataType(TUniquePtr<IDataTypeRegistryEntry>&& InEntry)
			{
				METASOUND_LLM_SCOPE;

				if (InEntry.IsValid())
				{
					TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe> Entry(InEntry.Release());

					const FName Name = Entry->GetDataTypeInfo().DataTypeName;

					if (!ensureAlwaysMsgf(!RegisteredDataTypes.Contains(Name),
						TEXT("Name collision when trying to register Metasound Data Type [Name:%s]. DataType must have "
							"unique name and REGISTER_METASOUND_DATATYPE cannot be called in a public header."),
							*Name.ToString()))
					{
						return false;
					}

					// Register nodes associated with data type.
					FMetasoundFrontendRegistryContainer* NodeRegistry = FMetasoundFrontendRegistryContainer::Get();
					if (ensure(nullptr != NodeRegistry))
					{
						if (Entry->GetDataTypeInfo().bIsParsable)
						{
							NodeRegistry->RegisterNode(MakeUnique<FInputNodeRegistryEntry>(Entry));
							
							NodeRegistry->RegisterNode(MakeUnique<FOutputNodeRegistryEntry>(Entry));
							
							NodeRegistry->RegisterNode(MakeUnique<FLiteralNodeRegistryEntry>(Entry));
							NodeRegistry->RegisterNode(MakeUnique<FVariableNodeRegistryEntry>(Entry));
							NodeRegistry->RegisterNode(MakeUnique<FVariableMutatorNodeRegistryEntry>(Entry));
							NodeRegistry->RegisterNode(MakeUnique<FVariableAccessorNodeRegistryEntry>(Entry));
							NodeRegistry->RegisterNode(MakeUnique<FVariableDeferredAccessorNodeRegistryEntry>(Entry));

							if (Entry->GetDataTypeInfo().bIsConstructorType)
							{
								NodeRegistry->RegisterNode(MakeUnique<FConstructorInputNodeRegistryEntry>(Entry));
								NodeRegistry->RegisterNode(MakeUnique<FConstructorOutputNodeRegistryEntry>(Entry));
							}
						}
					}

					const FDataTypeRegistryInfo& RegistryInfo = Entry->GetDataTypeInfo();
					if (const UClass* Class = RegistryInfo.ProxyGeneratorClass)
					{
						RegisteredObjectClasses.Add(Class, Name);
					}

					RegisteredDataTypes.Add(Name, Entry);

					UE_LOG(LogMetaSound, Verbose, TEXT("Registered Metasound Datatype [Name:%s]."), *Name.ToString());
					return true;
				}

				return false;
			}

			void FDataTypeRegistry::GetRegisteredDataTypeNames(TArray<FName>& OutNames) const
			{
				RegisteredDataTypes.GetKeys(OutNames);
			}

			bool FDataTypeRegistry::GetDataTypeInfo(const UObject* InObject, FDataTypeRegistryInfo& OutInfo) const
			{
				if (InObject)
				{
					if (const FName* DataTypeName = RegisteredObjectClasses.Find(InObject->GetClass()))
					{
						if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(*DataTypeName))
						{
							OutInfo = Entry->GetDataTypeInfo();
							return true;
						}
					}
				}

				return false;
			}

			bool FDataTypeRegistry::GetDataTypeInfo(const FName& InDataType, FDataTypeRegistryInfo& OutInfo) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					OutInfo = Entry->GetDataTypeInfo();
					return true;
				}
				return false;
			}

			void FDataTypeRegistry::IterateDataTypeInfo(TFunctionRef<void(const FDataTypeRegistryInfo&)> InFunction) const
			{
				for (const TPair<FName, TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>>& Entry : RegisteredDataTypes)
				{
					InFunction(Entry.Value->GetDataTypeInfo());
				}
			}

			bool FDataTypeRegistry::IsRegistered(const FName& InDataType) const
			{
				return RegisteredDataTypes.Contains(InDataType);
			}

			// Return the enum interface for a data type. If the data type does not have 
			// an enum interface, returns a nullptr.
			TSharedPtr<const IEnumDataTypeInterface> FDataTypeRegistry::GetEnumInterfaceForDataType(const FName& InDataType) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->GetEnumInterface();
				}
				return nullptr;
			}

			ELiteralType FDataTypeRegistry::GetDesiredLiteralType(const FName& InDataType) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					const FDataTypeRegistryInfo& Info = Entry->GetDataTypeInfo();

					// If there's a designated preferred literal type for this datatype, use that.
					if (Info.PreferredLiteralType != Metasound::ELiteralType::Invalid)
					{
						return Info.PreferredLiteralType;
					}

					// Otherwise, we opt for the highest precision construction option available.
					return MetasoundFrontendDataTypeRegistryPrivate::GetMostDescriptiveLiteralForDataType(Info);
				}
				return Metasound::ELiteralType::Invalid;
			}

			bool FDataTypeRegistry::IsLiteralTypeSupported(const FName& InDataType, ELiteralType InLiteralType) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					const FDataTypeRegistryInfo& Info = Entry->GetDataTypeInfo();

					switch (InLiteralType)
					{
						case Metasound::ELiteralType::Boolean:
						{
							return Info.bIsBoolParsable;
						}
						case Metasound::ELiteralType::BooleanArray:
						{
							return Info.bIsBoolArrayParsable;
						}

						case Metasound::ELiteralType::Integer:
						{
							return Info.bIsIntParsable;
						}
						case Metasound::ELiteralType::IntegerArray:
						{
							return Info.bIsIntArrayParsable;
						}

						case Metasound::ELiteralType::Float:
						{
							return Info.bIsFloatParsable;
						}
						case Metasound::ELiteralType::FloatArray:
						{
							return Info.bIsFloatArrayParsable;
						}

						case Metasound::ELiteralType::String:
						{
							return Info.bIsStringParsable;
						}
						case Metasound::ELiteralType::StringArray:
						{
							return Info.bIsStringArrayParsable;
						}

						case Metasound::ELiteralType::UObjectProxy:
						{
							return Info.bIsProxyParsable;
						}
						case Metasound::ELiteralType::UObjectProxyArray:
						{
							return Info.bIsProxyArrayParsable;
						}

						case Metasound::ELiteralType::None:
						{
							return Info.bIsDefaultParsable;
						}
						case Metasound::ELiteralType::NoneArray:
						{
							return Info.bIsDefaultArrayParsable;
						}

						case Metasound::ELiteralType::Invalid:
						default:
						{
							static_assert(static_cast<int32>(Metasound::ELiteralType::COUNT) == 13, "Possible missing case coverage for ELiteralType");
							return false;
						}
					}
				}

				return false;
			}

			bool FDataTypeRegistry::IsLiteralTypeSupported(const FName& InDataType, EMetasoundFrontendLiteralType InLiteralType) const
			{
				return IsLiteralTypeSupported(InDataType, GetMetasoundLiteralType(InLiteralType));
			}

			UClass* FDataTypeRegistry::GetUClassForDataType(const FName& InDataType) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->GetDataTypeInfo().ProxyGeneratorClass;
				}

				return nullptr;
			}

			FLiteral FDataTypeRegistry::CreateDefaultLiteral(const FName& InDataType) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					const FDataTypeRegistryInfo& Info = Entry->GetDataTypeInfo();
					if (Info.bIsEnum)
					{
						if (TSharedPtr<const IEnumDataTypeInterface> EnumInterface = Entry->GetEnumInterface())
						{
							return FLiteral(EnumInterface->GetDefaultValue());
						}
					}
					return FLiteral::GetDefaultForType(Info.PreferredLiteralType);
				}
				return FLiteral::CreateInvalid();
			}

			bool FDataTypeRegistry::IsUObjectProxyFactory(UObject* InObject) const
			{
				if (!InObject)
				{
					return false;
				}

				UClass* ObjectClass = InObject->GetClass();
				while (ObjectClass != UObject::StaticClass())
				{
					if (RegisteredObjectClasses.Contains(ObjectClass))
					{
						return true;
					}

					ObjectClass = ObjectClass->GetSuperClass();
				}

				return false;
			}

			Audio::IProxyDataPtr FDataTypeRegistry::CreateProxyFromUObject(const FName& InDataType, UObject* InObject) const
			{
				Audio::IProxyDataPtr ProxyPtr;

				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					ProxyPtr = Entry->CreateProxy(InObject);
					if (!ProxyPtr && InObject)
					{
						UE_LOG(LogMetaSound, Error, TEXT("Failed to create a valid proxy from UObject '%s'."), *InObject->GetName());
					}
				}

				return ProxyPtr;
			}

			FLiteral FDataTypeRegistry::CreateLiteralFromUObject(const FName& InDataType, UObject* InObject) const
			{
				Audio::IProxyDataPtr ProxyPtr = CreateProxyFromUObject(InDataType, InObject);
				return Metasound::FLiteral(MoveTemp(ProxyPtr));
			}

			FLiteral FDataTypeRegistry::CreateLiteralFromUObjectArray(const FName& InDataType, const TArray<UObject*>& InObjectArray) const
			{
				TArray<Audio::IProxyDataPtr> ProxyArray;
				const IDataTypeRegistryEntry* DataTypeEntry = FindDataTypeEntry(InDataType);
				if (!DataTypeEntry)
				{
					UE_LOG(LogMetaSound, Error, TEXT("Failed to create a valid proxy from Array DataType '%s': Type is not registered."), *InDataType.ToString());
					return Metasound::FLiteral(MoveTemp(ProxyArray));
				}

				const FDataTypeRegistryInfo& DataTypeInfo = DataTypeEntry->GetDataTypeInfo();

				const bool bIsArrayType = DataTypeInfo.bIsProxyArrayParsable;
				if (!bIsArrayType)
				{
					UE_LOG(LogMetaSound, Error, TEXT("Failed to create a valid proxy from DataType '%s': Type is not 'ArrayType'."), *InDataType.ToString());
					return Metasound::FLiteral(MoveTemp(ProxyArray));
				}

				const bool bIsProxyArrayParseable = DataTypeInfo.bIsProxyArrayParsable;
				if (!bIsProxyArrayParseable)
				{
					UE_LOG(LogMetaSound, Error, TEXT("Failed to create a valid proxy from DataType '%s': Type is not proxy parseable."), *InDataType.ToString());
					return Metasound::FLiteral(MoveTemp(ProxyArray));
				}

				const FName ElementDataType = CreateElementTypeNameFromArrayTypeName(InDataType);
				const IDataTypeRegistryEntry* ElementEntry = FindDataTypeEntry(ElementDataType);
				if (!ElementEntry)
				{
					UE_LOG(LogMetaSound, Error, TEXT("Failed to create a valid proxy from DataType '%s': ElementType '%s' is not registered."), *ElementDataType.ToString());
					return Metasound::FLiteral(MoveTemp(ProxyArray));
				}

				for (UObject* InObject : InObjectArray)
				{
					Audio::IProxyDataPtr ProxyPtr = CreateProxyFromUObject(ElementDataType, InObject);
					ProxyPtr = ElementEntry->CreateProxy(InObject);
					if (!ProxyPtr && InObject)
					{
						UE_LOG(LogMetaSound, Error, TEXT("Failed to create a valid proxy from UObject '%s'."), *InObject->GetName());
					}

					ProxyArray.Emplace(MoveTemp(ProxyPtr));
				}

				return Metasound::FLiteral(MoveTemp(ProxyArray));
			}

			TOptional<FAnyDataReference> FDataTypeRegistry::CreateDataReference(const FName& InDataType, EDataReferenceAccessType InAccessType, const FLiteral& InLiteral, const FOperatorSettings& InOperatorSettings) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateDataReference(InAccessType, InLiteral, InOperatorSettings);
				}
				return TOptional<FAnyDataReference>();
			}

			TSharedPtr<IDataChannel, ESPMode::ThreadSafe> FDataTypeRegistry::CreateDataChannel(const FName& InDataType, const FOperatorSettings& InOperatorSettings) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateDataChannel(InOperatorSettings);
				}
				return nullptr;
			}

			bool FDataTypeRegistry::GetFrontendInputClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					OutClass = Entry->GetFrontendInputClass();
					return true;
				}
				return false;
			}

			bool FDataTypeRegistry::GetFrontendConstructorInputClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					OutClass = Entry->GetFrontendConstructorInputClass();
					return true;
				}
				return false;
			}

			bool FDataTypeRegistry::GetFrontendLiteralClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					OutClass = Entry->GetFrontendLiteralClass();
					return true;
				}
				return false;
			}

			bool FDataTypeRegistry::GetFrontendOutputClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					OutClass = Entry->GetFrontendOutputClass();
					return true;
				}
				return false;
			}

			bool FDataTypeRegistry::GetFrontendConstructorOutputClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					OutClass = Entry->GetFrontendConstructorOutputClass();
					return true;
				}
				return false;
			}

			bool FDataTypeRegistry::GetFrontendVariableClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					OutClass = Entry->GetFrontendVariableClass();
					return true;
				}
				return false;
			}

			bool FDataTypeRegistry::GetFrontendVariableMutatorClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const

			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					OutClass = Entry->GetFrontendVariableMutatorClass();
					return true;
				}
				return false;
			}

			bool FDataTypeRegistry::GetFrontendVariableAccessorClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const

			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					OutClass = Entry->GetFrontendVariableAccessorClass();
					return true;
				}
				return false;
			}

			bool FDataTypeRegistry::GetFrontendVariableDeferredAccessorClass(const FName& InDataType, FMetasoundFrontendClass& OutClass) const

			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					OutClass = Entry->GetFrontendVariableDeferredAccessorClass();
					return true;
				}
				return false;
			}

			TUniquePtr<INode> FDataTypeRegistry::CreateInputNode(const FName& InDataType, FInputNodeConstructorParams&& InParams) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateInputNode(MoveTemp(InParams));
				}
				return nullptr;
			}

			TUniquePtr<INode> FDataTypeRegistry::CreateConstructorInputNode(const FName& InDataType, FInputNodeConstructorParams&& InParams) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateConstructorInputNode(MoveTemp(InParams));
				}
				return nullptr;
			}

			TUniquePtr<INode> FDataTypeRegistry::CreateLiteralNode(const FName& InDataType, FLiteralNodeConstructorParams&& InParams) const 
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateLiteralNode(MoveTemp(InParams));
				}
				return nullptr;
			}

			TUniquePtr<INode> FDataTypeRegistry::CreateOutputNode(const FName& InDataType, FOutputNodeConstructorParams&& InParams) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateOutputNode(MoveTemp(InParams));
				}
				return nullptr;
			}

			TUniquePtr<INode> FDataTypeRegistry::CreateConstructorOutputNode(const FName& InDataType, FOutputNodeConstructorParams&& InParams) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateConstructorOutputNode(MoveTemp(InParams));
				}
				return nullptr;
			}

			TUniquePtr<INode> FDataTypeRegistry::CreateReceiveNode(const FName& InDataType, const FNodeInitData& InParams) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateReceiveNode(InParams);
				}
				return nullptr;
			}

			TUniquePtr<INode> FDataTypeRegistry::CreateVariableNode(const FName& InDataType, FVariableNodeConstructorParams&& InParams) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateVariableNode(MoveTemp(InParams));
				}
				return nullptr;
			}

			TUniquePtr<INode> FDataTypeRegistry::CreateVariableMutatorNode(const FName& InDataType, const FNodeInitData& InParams) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateVariableMutatorNode(InParams);
				}
				return nullptr;
			}

			TUniquePtr<INode> FDataTypeRegistry::CreateVariableAccessorNode(const FName& InDataType, const FNodeInitData& InParams) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateVariableAccessorNode(InParams);
				}
				return nullptr;
			}

			TUniquePtr<INode> FDataTypeRegistry::CreateVariableDeferredAccessorNode(const FName& InDataType, const FNodeInitData& InParams) const
			{
				if (const IDataTypeRegistryEntry* Entry = FindDataTypeEntry(InDataType))
				{
					return Entry->CreateVariableDeferredAccessorNode(InParams);
				}
				return nullptr;
			}

			const IDataTypeRegistryEntry* FDataTypeRegistry::FindDataTypeEntry(const FName& InDataTypeName) const
			{
				const TSharedRef<IDataTypeRegistryEntry, ESPMode::ThreadSafe>* Entry = RegisteredDataTypes.Find(InDataTypeName);

				if (ensureMsgf(nullptr != Entry, TEXT("Data type not registered [Name:%s]"), *InDataTypeName.ToString()))
				{
					return &Entry->Get();
				}

				return nullptr;
			}
		}

		IDataTypeRegistry& IDataTypeRegistry::Get()
		{
			static MetasoundFrontendDataTypeRegistryPrivate::FDataTypeRegistry Registry;
			return Registry;
		}
	}
}

