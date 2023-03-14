// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendRegistries.h"

#include "Algo/ForEach.h"
#include "CoreMinimal.h"
#include "HAL/PlatformTime.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendNodeRegistryPrivate.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundLog.h"
#include "MetasoundRouter.h"
#include "MetasoundTrace.h"
#include "Misc/ScopeLock.h"


namespace Metasound
{
	namespace Frontend
	{
		namespace MetasoundFrontendRegistryPrivate
		{
			const FString& GetClassTypeString(EMetasoundFrontendClassType InType)
			{
				static const FString InputType(TEXT("Input"));
				static const FString OutputType(TEXT("Output"));
				static const FString ExternalType(TEXT("External"));
				static const FString VariableAccessorType(TEXT("VariableAccessor"));
				static const FString VariableDeferredAccessorType(TEXT("VariableDeferredAccessor"));
				static const FString VariableMutatorType(TEXT("VariableMutator"));
				static const FString VariableType(TEXT("Variable"));
				static const FString LiteralType(TEXT("Literal"));
				static const FString GraphType(TEXT("Graph"));
				static const FString TemplateType(TEXT("Template"));
				static const FString InvalidType(TEXT("Invalid"));

				switch (InType)
				{
					case EMetasoundFrontendClassType::Input:
						return InputType;

					case EMetasoundFrontendClassType::Output:
						return OutputType;

					case EMetasoundFrontendClassType::External:
						return ExternalType;

					case EMetasoundFrontendClassType::Literal:
						return LiteralType;

					case EMetasoundFrontendClassType::Variable:
						return VariableType;
						
					case EMetasoundFrontendClassType::VariableDeferredAccessor:
						return VariableDeferredAccessorType;

					case EMetasoundFrontendClassType::VariableAccessor:
						return VariableAccessorType;

					case EMetasoundFrontendClassType::VariableMutator:
						return VariableMutatorType;

					case EMetasoundFrontendClassType::Template:
						return TemplateType;

					case EMetasoundFrontendClassType::Graph:
						return GraphType;

					default:
						static_assert(static_cast<uint8>(EMetasoundFrontendClassType::Invalid) == 10, "Missing EMetasoundFrontendClassType case coverage");
						return InvalidType;
				}
			}
		}

		FRegistryContainerImpl* FRegistryContainerImpl::LazySingleton = nullptr;

		FRegistryContainerImpl& FRegistryContainerImpl::Get()
		{
			if (!LazySingleton)
			{
				LazySingleton = new Metasound::Frontend::FRegistryContainerImpl();
			}

			return *LazySingleton;
		}

		void FRegistryContainerImpl::Shutdown()
		{
			if (nullptr != LazySingleton)
			{
				delete LazySingleton;
				LazySingleton = nullptr;
			}
		}

		FRegistryContainerImpl::FRegistryContainerImpl()
		: TransactionBuffer(MakeShared<FNodeRegistryTransactionBuffer>())
		{
		}

		void FRegistryContainerImpl::RegisterPendingNodes()
		{
			METASOUND_LLM_SCOPE;
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(metasound::FRegistryContainerImpl::RegisterPendingNodes);
			{
				FScopeLock ScopeLock(&LazyInitCommandCritSection);

				for (TUniqueFunction<void()>& Command : LazyInitCommands)
				{
					Command();
				}

				LazyInitCommands.Empty();
			}

			// Prime search engine after bulk registration.
			ISearchEngine::Get().Prime();
		}

		bool FRegistryContainerImpl::EnqueueInitCommand(TUniqueFunction<void()>&& InFunc)
		{

			FScopeLock ScopeLock(&LazyInitCommandCritSection);
			if (LazyInitCommands.Num() >= MaxNumNodesAndDatatypesToInitialize)
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Registering more that %d nodes and datatypes for metasounds! Consider increasing MetasoundFrontendRegistryContainer::MaxNumNodesAndDatatypesToInitialize."));
			}

			LazyInitCommands.Add(MoveTemp(InFunc));
			return true;
		}

		void FRegistryContainerImpl::ForEachNodeRegistryTransactionSince(FRegistryTransactionID InSince, FRegistryTransactionID* OutCurrentRegistryTransactionID, TFunctionRef<void(const FNodeRegistryTransaction&)> InFunc) const 
		{
			UE_LOG(LogMetaSound, Error, TEXT("FMetasoundFrontendRegistryContainer::ForEachNodeRegistryTransactionSince is no longer supported and will not function as expected."));
		}

		TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, const Metasound::FNodeInitData& InInitData) const
		{
			if (const INodeRegistryEntry* Entry = FindNodeEntry(InKey))
			{
				return Entry->CreateNode(InInitData);
			}

			// Because creation of external nodes can rely on assets being unavailable due to errors in loading order, asset(s)
			// missing, etc. only log error and don't throw ensure to avoid blocking start-up if assets are missing. All other
			// CreateNode calls are natively managed and thus better suited to throw ensures.
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey);
			return nullptr;
		}

		TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultLiteralNodeConstructorParams&& InParams) const
		{
			const INodeRegistryEntry* Entry = FindNodeEntry(InKey);

			if (ensureAlwaysMsgf(nullptr != Entry, TEXT("Could not find node [RegistryKey:%s]"), *InKey))
			{
				return Entry->CreateNode(MoveTemp(InParams));
			}

			return nullptr;
		}

		TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexNodeConstructorParams&& InParams) const
		{
			const INodeRegistryEntry* Entry = FindNodeEntry(InKey);

			if (ensureAlwaysMsgf(nullptr != Entry, TEXT("Could not find node [RegistryKey:%s]"), *InKey))
			{
				return Entry->CreateNode(MoveTemp(InParams));
			}

			return nullptr;
		}

		TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexWithLiteralNodeConstructorParams&& InParams) const
		{
			const INodeRegistryEntry* Entry = FindNodeEntry(InKey);

			if (ensureAlwaysMsgf(nullptr != Entry, TEXT("Could not find node [RegistryKey:%s]"), *InKey))
			{
				return Entry->CreateNode(MoveTemp(InParams));
			}

			return nullptr;
		}

		TArray<::Metasound::Frontend::FConverterNodeInfo> FRegistryContainerImpl::GetPossibleConverterNodes(const FName& FromDataType, const FName& ToDataType)
		{
			FConverterNodeRegistryKey InKey = { FromDataType, ToDataType };
			if (!ConverterNodeRegistry.Contains(InKey))
			{
				return TArray<FConverterNodeInfo>();
			}
			else
			{
				return ConverterNodeRegistry[InKey].PotentialConverterNodes;
			}
		}

		TUniquePtr<FNodeRegistryTransactionStream> FRegistryContainerImpl::CreateTransactionStream()
		{
			return MakeUnique<FNodeRegistryTransactionStream>(TransactionBuffer);
		}

		FNodeRegistryKey FRegistryContainerImpl::RegisterNode(TUniquePtr<INodeRegistryEntry>&& InEntry)
		{
			METASOUND_LLM_SCOPE;

			FNodeRegistryKey Key;

			if (InEntry.IsValid())
			{
				TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe> Entry(InEntry.Release());

				FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();

				Key = NodeRegistryKey::CreateKey(Entry->GetClassInfo());

				// check to see if an identical node was already registered, and log
				ensureAlwaysMsgf(
					!RegisteredNodes.Contains(Key),
					TEXT("Node with registry key '%s' already registered. "
						"The previously registered node will be overwritten. "
						"This can happen if two classes share the same name or if METASOUND_REGISTER_NODE is defined in a public header."),
					*Key);

				// Store update to newly registered node in history so nodes
				// can be queried by transaction ID
				
				TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeRegistration, Entry->GetClassInfo(), Timestamp));

				// Store registry elements in map so nodes can be queried using registry key.
				RegisteredNodes.Add(Key, Entry);
			}

			return Key;
		}

		FNodeRegistryKey FRegistryContainerImpl::RegisterNodeTemplate(TUniquePtr<INodeRegistryTemplateEntry>&& InEntry)
		{
			METASOUND_LLM_SCOPE;

			FNodeRegistryKey Key;

			if (InEntry.IsValid())
			{
				TSharedRef<INodeRegistryTemplateEntry, ESPMode::ThreadSafe> Entry(InEntry.Release());

				FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();

				Key = NodeRegistryKey::CreateKey(Entry->GetClassInfo());

				// check to see if an identical node was already registered, and log
				ensureAlwaysMsgf(
					!RegisteredNodes.Contains(Key),
					TEXT("Node template with registry key '%s' already registered. The previously registered node will be overwritten."),
					*Key);

				// Store update to newly registered node in history so nodes
				// can be queried by transaction ID

				TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeRegistration, Entry->GetClassInfo(), Timestamp));

				// Store registry elements in map so nodes can be queried using registry key.
				RegisteredNodeTemplates.Add(Key, Entry);
			}

			return Key;
		}

		bool FRegistryContainerImpl::UnregisterNode(const FNodeRegistryKey& InKey)
		{
			METASOUND_LLM_SCOPE;

			if (NodeRegistryKey::IsValid(InKey))
			{
				if (const INodeRegistryEntry* Entry = FindNodeEntry(InKey))
				{
					FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();

					TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeUnregistration, Entry->GetClassInfo(), Timestamp));

					RegisteredNodes.Remove(InKey);
					return true;
				}
			}

			return false;
		}

		bool FRegistryContainerImpl::UnregisterNodeTemplate(const FNodeRegistryKey& InKey)
		{
			METASOUND_LLM_SCOPE;

			if (NodeRegistryKey::IsValid(InKey))
			{
				if (const INodeRegistryTemplateEntry* Entry = FindNodeTemplateEntry(InKey))
				{
					FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();

					TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeUnregistration, Entry->GetClassInfo(), Timestamp));

					RegisteredNodeTemplates.Remove(InKey);
					return true;
				}
			}

			return false;
		}

		bool FRegistryContainerImpl::RegisterConversionNode(const FConverterNodeRegistryKey& InNodeKey, const FConverterNodeInfo& InNodeInfo)
		{
			if (!ConverterNodeRegistry.Contains(InNodeKey))
			{
				ConverterNodeRegistry.Add(InNodeKey);
			}

			FConverterNodeRegistryValue& ConverterNodeList = ConverterNodeRegistry[InNodeKey];

			if (ensureAlways(!ConverterNodeList.PotentialConverterNodes.Contains(InNodeInfo)))
			{
				ConverterNodeList.PotentialConverterNodes.Add(InNodeInfo);
				return true;
			}
			else
			{
				// If we hit this, someone attempted to add the same converter node to our list multiple times.
				return false;
			}
		}

		bool FRegistryContainerImpl::IsNodeRegistered(const FNodeRegistryKey& InKey) const
		{
			return RegisteredNodes.Contains(InKey) || RegisteredNodeTemplates.Contains(InKey);
		}

		bool FRegistryContainerImpl::IsNodeNative(const FNodeRegistryKey& InKey) const
		{
			if (const INodeRegistryEntry* Entry = FindNodeEntry(InKey))
			{
				return Entry->IsNative();
			}

			if (const INodeRegistryTemplateEntry* TemplateEntry = FindNodeTemplateEntry(InKey))
			{
				return true;
			}

			return false;
		}

		bool FRegistryContainerImpl::FindFrontendClassFromRegistered(const FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass)
		{
			if (const INodeRegistryEntry* Entry = FindNodeEntry(InKey))
			{
				OutClass = Entry->GetFrontendClass();
				return true;
			}

			if (const INodeRegistryTemplateEntry* Entry = FindNodeTemplateEntry(InKey))
			{
				OutClass = Entry->GetFrontendClass();
				return true;
			}

			return false;
		}

		bool FRegistryContainerImpl::FindNodeClassInfoFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo)
		{
			if (const INodeRegistryEntry* Entry = FindNodeEntry(InKey))
			{
				OutInfo = Entry->GetClassInfo();
				return true;
			}

			if (const INodeRegistryTemplateEntry* Entry = FindNodeTemplateEntry(InKey))
			{
				OutInfo = Entry->GetClassInfo();
				return true;
			}

			return false;
		}

		bool FRegistryContainerImpl::FindInputNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey)
		{
			UE_LOG(LogMetaSound, Error, TEXT("FRegistryContainerImpl::FindInputNodeRegistryKeyForDataType with these parameters is no longer supported and should not be called. Use the one with EMetasoundFrontendVertexAccessType instead."));
			return false;
		}

		bool FRegistryContainerImpl::FindInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey)
		{
			FMetasoundFrontendClass Class;
			switch (InAccessType)
			{
				case EMetasoundFrontendVertexAccessType::Reference:
				{
					if (IDataTypeRegistry::Get().GetFrontendInputClass(InDataTypeName, Class))
					{
						OutKey = NodeRegistryKey::CreateKey(Class.Metadata);
						return true;
					}
				}
				break;

				case EMetasoundFrontendVertexAccessType::Value:
				{
					if (IDataTypeRegistry::Get().GetFrontendConstructorInputClass(InDataTypeName, Class))
					{
						OutKey = NodeRegistryKey::CreateKey(Class.Metadata);
						return true;
					}
				}
				break;

				default:
				case EMetasoundFrontendVertexAccessType::Unset:
				{
					return false;
				}
				break;
			}

			return false;
		}

		bool FRegistryContainerImpl::FindVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey)
		{
			FMetasoundFrontendClass Class;
			if (IDataTypeRegistry::Get().GetFrontendLiteralClass(InDataTypeName, Class))
			{
				OutKey = NodeRegistryKey::CreateKey(Class.Metadata);
				return true;
			}
			return false;
		}

		bool FRegistryContainerImpl::FindOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey)
		{
			UE_LOG(LogMetaSound, Error, TEXT("FRegistryContainerImpl::FindOutputNodeRegistryKeyForDataType with these parameters is no longer supported and should not be called. Use the one with EMetasoundFrontendVertexAccessType instead."));
			return false;
		}

		bool FRegistryContainerImpl::FindOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey)
		{
			FMetasoundFrontendClass Class;
			switch (InAccessType)
			{
				case EMetasoundFrontendVertexAccessType::Reference:
				{
					if (IDataTypeRegistry::Get().GetFrontendOutputClass(InDataTypeName, Class))
					{
						OutKey = NodeRegistryKey::CreateKey(Class.Metadata);
						return true;
					}
				}
				break;

				case EMetasoundFrontendVertexAccessType::Value:
				{
					if (IDataTypeRegistry::Get().GetFrontendConstructorOutputClass(InDataTypeName, Class))
					{
						OutKey = NodeRegistryKey::CreateKey(Class.Metadata);
						return true;
					}
				}
				break;
			}

			return false;
		}

		void FRegistryContainerImpl::IterateRegistry(Metasound::FIterateMetasoundFrontendClassFunction InIterFunc, EMetasoundFrontendClassType InClassType) const
		{
			auto WrappedFunc = [&](const TPair<FNodeRegistryKey, TSharedPtr<INodeRegistryEntry, ESPMode::ThreadSafe>>& Pair)
			{
				InIterFunc(Pair.Value->GetFrontendClass());
			};

			if (EMetasoundFrontendClassType::Invalid == InClassType)
			{
				// Iterate through all classes. 
				Algo::ForEach(RegisteredNodes, WrappedFunc);
			}
			else
			{
				// Only call function on classes of certain type.
				auto IsMatchingClassType = [&](const TPair<FNodeRegistryKey, TSharedPtr<INodeRegistryEntry, ESPMode::ThreadSafe>>& Pair)
				{
					return Pair.Value->GetClassInfo().Type == InClassType;
				};
				Algo::ForEachIf(RegisteredNodes, IsMatchingClassType, WrappedFunc);
			}
		}

		const INodeRegistryEntry* FRegistryContainerImpl::FindNodeEntry(const FNodeRegistryKey& InKey) const
		{
			if (const TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe>* Entry = RegisteredNodes.Find(InKey))
			{
				return &Entry->Get();
			}

			return nullptr;
		}

		const INodeRegistryTemplateEntry* FRegistryContainerImpl::FindNodeTemplateEntry(const FNodeRegistryKey& InKey) const
		{
			if (const TSharedRef<INodeRegistryTemplateEntry, ESPMode::ThreadSafe>* Entry = RegisteredNodeTemplates.Find(InKey))
			{
				return &Entry->Get();
			}

			return nullptr;
		}

		FNodeRegistryTransaction::FNodeRegistryTransaction(ETransactionType InType, const FNodeClassInfo& InNodeClassInfo, FNodeRegistryTransaction::FTimeType InTimestamp)
		: Type(InType)
		, NodeClassInfo(InNodeClassInfo)
		, Timestamp(InTimestamp)
		{
		}

		FNodeRegistryTransaction::FNodeRegistryTransaction(ETransactionType InType, const FNodeRegistryKey& InKey, const FNodeClassInfo& InNodeClassInfo, FNodeRegistryTransaction::FTimeType InTimestamp)
		: FNodeRegistryTransaction(InType, InNodeClassInfo, InTimestamp)
		{
		}

		FNodeRegistryTransaction::ETransactionType FNodeRegistryTransaction::GetTransactionType() const
		{
			return Type;
		}

		const FNodeClassInfo& FNodeRegistryTransaction::GetNodeClassInfo() const
		{
			return NodeClassInfo;
		}

		FNodeRegistryKey FNodeRegistryTransaction::GetNodeRegistryKey() const
		{
			return NodeRegistryKey::CreateKey(NodeClassInfo);
		}

		FNodeRegistryTransaction::FTimeType FNodeRegistryTransaction::GetTimestamp() const
		{
			return Timestamp;
		}

		namespace NodeRegistryKey
		{
			// All registry keys should be created through this function to ensure consistency.
			FNodeRegistryKey CreateKey(EMetasoundFrontendClassType InType, const FString& InFullClassName, int32 InMajorVersion, int32 InMinorVersion)
			{
				using namespace MetasoundFrontendRegistryPrivate;
				const FString RegistryKey = FString::Format(TEXT("{0}_{1}_{2}.{3}"), {*GetClassTypeString(InType), *InFullClassName, InMajorVersion, InMinorVersion});
				return RegistryKey;
			}

			const FNodeRegistryKey& GetInvalid()
			{
				static const FNodeRegistryKey InvalidKey;
				return InvalidKey;
			}

			bool IsValid(const FNodeRegistryKey& InKey)
			{
				return InKey != GetInvalid();
			}

			bool IsEqual(const FNodeRegistryKey& InLHS, const FNodeRegistryKey& InRHS)
			{
				return InLHS == InRHS;
			}

			bool IsEqual(const FMetasoundFrontendClassMetadata& InLHS, const FMetasoundFrontendClassMetadata& InRHS)
			{
				if (InLHS.GetClassName() == InRHS.GetClassName())
				{
					if (InLHS.GetType() == InRHS.GetType())
					{
						if (InLHS.GetVersion() == InRHS.GetVersion())
						{
							return true;
						}
					}
				}
				return false;
			}

			bool IsEqual(const FNodeClassInfo& InLHS, const FMetasoundFrontendClassMetadata& InRHS)
			{
				if (InLHS.ClassName == InRHS.GetClassName())
				{
					if (InLHS.Type == InRHS.GetType())
					{
						if (InLHS.Version == InRHS.GetVersion())
						{
							return true;
						}
					}
				}
				return false;
			}

			FNodeRegistryKey CreateKey(const FNodeClassMetadata& InNodeMetadata)
			{
				return CreateKey(EMetasoundFrontendClassType::External, InNodeMetadata.ClassName.GetFullName().ToString(), InNodeMetadata.MajorVersion, InNodeMetadata.MinorVersion);
			}

			FNodeRegistryKey CreateKey(const FMetasoundFrontendClassMetadata& InNodeMetadata)
			{
				return CreateKey(InNodeMetadata.GetType(), InNodeMetadata.GetClassName().GetFullName().ToString(), InNodeMetadata.GetVersion().Major, InNodeMetadata.GetVersion().Minor);
			}

			FNodeRegistryKey CreateKey(const FNodeClassInfo& InClassInfo)
			{
				return CreateKey(InClassInfo.Type, InClassInfo.ClassName.GetFullName().ToString(), InClassInfo.Version.Major, InClassInfo.Version.Minor);
			}
		}

		FNodeClassInfo::FNodeClassInfo(const FMetasoundFrontendClassMetadata& InMetadata)
			: ClassName(InMetadata.GetClassName())
			, Type(InMetadata.GetType())
			, Version(InMetadata.GetVersion())
		{
		}

		FNodeClassInfo::FNodeClassInfo(const FMetasoundFrontendGraphClass& InClass, const FSoftObjectPath& InAssetPath)
			: ClassName(InClass.Metadata.GetClassName())
			, Type(EMetasoundFrontendClassType::External) // Overridden as it is considered the same as an external class in the registry
			, AssetClassID(FGuid(ClassName.Name.ToString()))
			, AssetPath(InAssetPath)
			, Version(InClass.Metadata.GetVersion())
		{
			ensure(!AssetPath.IsNull());
#if WITH_EDITORONLY_DATA
			for (const FMetasoundFrontendClassInput& Input : InClass.Interface.Inputs)
			{
				InputTypes.Add(Input.TypeName);
			}

			for (const FMetasoundFrontendClassOutput& Output : InClass.Interface.Outputs)
			{
				OutputTypes.Add(Output.TypeName);
			}

			bIsPreset = InClass.PresetOptions.bIsPreset;
#endif // WITH_EDITORONLY_DATA
		}
	} // namespace Frontend
} // namespace Metasound


FMetasoundFrontendRegistryContainer* FMetasoundFrontendRegistryContainer::Get()
{
	return &Metasound::Frontend::FRegistryContainerImpl::Get();
}

void FMetasoundFrontendRegistryContainer::ShutdownMetasoundFrontend()
{
	Metasound::Frontend::FRegistryContainerImpl::Shutdown();
}

Metasound::Frontend::FNodeRegistryKey FMetasoundFrontendRegistryContainer::GetRegistryKey(const FNodeClassMetadata& InNodeMetadata)
{
	return Metasound::Frontend::NodeRegistryKey::CreateKey(InNodeMetadata);
}

Metasound::Frontend::FNodeRegistryKey FMetasoundFrontendRegistryContainer::GetRegistryKey(const FMetasoundFrontendClassMetadata& InNodeMetadata)
{
	return Metasound::Frontend::NodeRegistryKey::CreateKey(InNodeMetadata);
}

Metasound::Frontend::FNodeRegistryKey FMetasoundFrontendRegistryContainer::GetRegistryKey(const FNodeClassInfo& InClassInfo)
{
	return Metasound::Frontend::NodeRegistryKey::CreateKey(InClassInfo);
}

bool FMetasoundFrontendRegistryContainer::GetFrontendClassFromRegistered(const FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass)
{
	FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get();

	if (ensure(nullptr != Registry))
	{
		return Registry->FindFrontendClassFromRegistered(InKey, OutClass);
	}

	return false;
}

bool FMetasoundFrontendRegistryContainer::GetNodeClassInfoFromRegistered(const FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		return Registry->FindNodeClassInfoFromRegistered(InKey, OutInfo);
	}
	return false;
}

bool FMetasoundFrontendRegistryContainer::GetInputNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey)
{
	UE_LOG(LogMetaSound, Error, TEXT("FMetasoundFrontendRegistryContainer::GetInputNodeRegistryKeyForDataType with these parameters is no longer supported and should not be called. Use the one with EMetasoundFrontendVertexAccessType instead."));
	return false;
}

bool FMetasoundFrontendRegistryContainer::GetInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		return Registry->FindInputNodeRegistryKeyForDataType(InDataTypeName, InAccessType, OutKey);
	}
	return false;
}

bool FMetasoundFrontendRegistryContainer::GetVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		return Registry->FindVariableNodeRegistryKeyForDataType(InDataTypeName, OutKey);
	}
	return false;
}

bool FMetasoundFrontendRegistryContainer::GetOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey)
{
	UE_LOG(LogMetaSound, Error, TEXT("FMetasoundFrontendRegistryContainer::GetOutputNodeRegistryKeyForDataType with these parameters is no longer supported and should not be called. Use the one with EMetasoundFrontendVertexAccessType instead."));
	return false;
}

bool FMetasoundFrontendRegistryContainer::GetOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InVertexAccessType, FNodeRegistryKey& OutKey)
{
	if (FMetasoundFrontendRegistryContainer* Registry = FMetasoundFrontendRegistryContainer::Get())
	{
		return Registry->FindOutputNodeRegistryKeyForDataType(InDataTypeName, InVertexAccessType, OutKey);
	}
	return false;
}
