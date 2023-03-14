// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "MetasoundFrontendRegistries.h"

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "MetasoundFrontendRegistryTransaction.h"

namespace Metasound
{
	namespace Frontend
	{
		using FNodeRegistryTransactionBuffer = TTransactionBuffer<FNodeRegistryTransaction>;
		using FNodeRegistryTransactionStream = TTransactionStream<FNodeRegistryTransaction>; 

		/** INodeRegistryTemplateEntry declares the interface for a node registry entry.
		 * Each node class in the registry must satisfy this interface.
		 */
		class INodeRegistryTemplateEntry
		{
		public:
			virtual ~INodeRegistryTemplateEntry() = default;

			/** Return FNodeClassInfo for the node class.
			 *
			 * Implementations of method should avoid any expensive operations
			 * (e.g. loading from disk, allocating memory) as this method is called
			 * frequently when querying nodes.
			 */
			virtual const FNodeClassInfo& GetClassInfo() const = 0;

			/** Return a FMetasoundFrontendClass which describes the node. */
			virtual const FMetasoundFrontendClass& GetFrontendClass() const = 0;
		};

		// Registry container private implementation.
		class FRegistryContainerImpl : public FMetasoundFrontendRegistryContainer
		{

		public:
			using FConverterNodeRegistryKey = ::Metasound::Frontend::FConverterNodeRegistryKey;
			using FConverterNodeRegistryValue = ::Metasound::Frontend::FConverterNodeRegistryValue;
			using FConverterNodeInfo = ::Metasound::Frontend::FConverterNodeInfo;

			using FNodeRegistryKey = Metasound::Frontend::FNodeRegistryKey;

			using FDataTypeRegistryInfo = Metasound::Frontend::FDataTypeRegistryInfo;
			using FNodeClassMetadata = Metasound::FNodeClassMetadata;
			using IEnumDataTypeInterface = Metasound::Frontend::IEnumDataTypeInterface;

			FRegistryContainerImpl();

			FRegistryContainerImpl(const FRegistryContainerImpl&) = delete;
			FRegistryContainerImpl& operator=(const FRegistryContainerImpl&) = delete;

			static FRegistryContainerImpl& Get();
			static void Shutdown();

			virtual ~FRegistryContainerImpl() = default;

			// Add a function to the init command array.
			virtual bool EnqueueInitCommand(TUniqueFunction<void()>&& InFunc) override;

			// This is called on module startup. This invokes any registration commands enqueued by our registration macros.
			virtual void RegisterPendingNodes() override;

			/** Register external node with the frontend.
			 *
			 * @param InCreateNode - Function for creating node from FNodeInitData.
			 * @param InCreateDescription - Function for creating a FMetasoundFrontendClass.
			 *
			 * @return True on success.
			 */
			virtual FNodeRegistryKey RegisterNode(TUniquePtr<Metasound::Frontend::INodeRegistryEntry>&& InEntry) override;

			virtual void ForEachNodeRegistryTransactionSince(Metasound::Frontend::FRegistryTransactionID InSince, Metasound::Frontend::FRegistryTransactionID* OutCurrentRegistryTransactionID, TFunctionRef<void(const Metasound::Frontend::FNodeRegistryTransaction&)> InFunc) const override;
			virtual bool UnregisterNode(const FNodeRegistryKey& InKey) override;
			virtual bool IsNodeRegistered(const FNodeRegistryKey& InKey) const override;
			virtual bool IsNodeNative(const FNodeRegistryKey& InKey) const override;

			virtual bool RegisterConversionNode(const FConverterNodeRegistryKey& InNodeKey, const FConverterNodeInfo& InNodeInfo) override;

			virtual void IterateRegistry(Metasound::FIterateMetasoundFrontendClassFunction InIterFunc, EMetasoundFrontendClassType InClassType = EMetasoundFrontendClassType::Invalid) const override;

			// Find Frontend Document data.
			virtual bool FindFrontendClassFromRegistered(const FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass) override;
			virtual bool FindNodeClassInfoFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo) override;
			UE_DEPRECATED(5.1, "Use FindInputNodeRegistryKeyForDataType with EMetasoundFrontendVertexAccessType instead.")
			virtual bool FindInputNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey) override;
			virtual bool FindInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey) override;
			virtual bool FindVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey) override;
			UE_DEPRECATED(5.1, "Use FindOutputNodeRegistryKeyForDataType with EMetasoundFrontendVertexAccessType instead.")
			virtual bool FindOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey) override;
			virtual bool FindOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey) override;

			// Create a new instance of a C++ implemented node from the registry.
			virtual TUniquePtr<Metasound::INode> CreateNode(const FNodeRegistryKey& InKey, const Metasound::FNodeInitData& InInitData) const override;
			virtual TUniquePtr<INode> CreateNode(const FNodeRegistryKey& InKey, FDefaultLiteralNodeConstructorParams&&) const override;
			virtual TUniquePtr<INode> CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexNodeConstructorParams&&) const override;
			virtual TUniquePtr<INode> CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexWithLiteralNodeConstructorParams&&) const override;

			// Returns a list of possible nodes to use to convert from FromDataType to ToDataType.
			// Returns an empty array if none are available.
			virtual TArray<FConverterNodeInfo> GetPossibleConverterNodes(const FName& FromDataType, const FName& ToDataType) override;

			// Private implementation until hardened and used for template nodes other than reroutes.
			FNodeRegistryKey RegisterNodeTemplate(TUniquePtr<Metasound::Frontend::INodeRegistryTemplateEntry>&& InEntry);
			bool UnregisterNodeTemplate(const FNodeRegistryKey& InKey);

			// Create a transaction stream for any newly transactions
			TUniquePtr<FNodeRegistryTransactionStream> CreateTransactionStream();

		private:
			static FRegistryContainerImpl* LazySingleton;

			const INodeRegistryEntry* FindNodeEntry(const FNodeRegistryKey& InKey) const;

			const INodeRegistryTemplateEntry* FindNodeTemplateEntry(const FNodeRegistryKey& InKey) const;

			// This buffer is used to enqueue nodes and datatypes to register when the module has been initialized,
			// in order to avoid bad behavior with ensures, logs, etc. on static initialization.
			// The bad news is that TInlineAllocator is the safest allocator to use on static init.
			// The good news is that none of these lambdas typically have captures, so this should have low memory overhead.
			static constexpr int32 MaxNumNodesAndDatatypesToInitialize = 2048;
			TArray<TUniqueFunction<void()>, TInlineAllocator<MaxNumNodesAndDatatypesToInitialize>> LazyInitCommands;
			
			FCriticalSection LazyInitCommandCritSection;

			// Registry in which we keep all information about nodes implemented in C++.
			TMap<FNodeRegistryKey, TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe>> RegisteredNodes;

			// Registry in which we keep all information about dynamically-generated templated nodes via in C++.
			TMap<FNodeRegistryKey, TSharedRef<INodeRegistryTemplateEntry, ESPMode::ThreadSafe>> RegisteredNodeTemplates;

			// Registry in which we keep lists of possible nodes to use to convert between two datatypes
			TMap<FConverterNodeRegistryKey, FConverterNodeRegistryValue> ConverterNodeRegistry;

			TSharedRef<FNodeRegistryTransactionBuffer> TransactionBuffer;
		};
	}
}

