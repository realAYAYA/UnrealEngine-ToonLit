// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Containers/Array.h"
#include "Containers/Map.h"
#include "HAL/CriticalSection.h"
#include "Tasks/Pipe.h"

#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendRegistryContainer.h"
#include "MetasoundFrontendRegistryTransaction.h"

struct FMetasoundFrontendDocument; 

namespace Metasound
{
	class FProxyDataCache;
	class FGraph;

	namespace Frontend
	{
		struct FNodeClassInfo;

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
		class FRegistryContainerImpl
			: public FMetasoundFrontendRegistryContainer
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

			virtual void SetObjectReferencer(TUniquePtr<IObjectReferencer> InReferencer) override;

			// This is called on module startup. This invokes any registration commands enqueued by our registration macros.
			virtual void RegisterPendingNodes() override;

			// Wait for async graph registration to complete for a specific graph
			virtual void WaitForAsyncGraphRegistration(const FGraphRegistryKey& InRegistryKey) const override;

			// Retrieve a registered graph. 
			//
			// If the graph is registered asynchronously, this will wait until the registration task has completed.
			virtual TSharedPtr<const FGraph> GetGraph(const FGraphRegistryKey& InRegistryKey) const override;

			/** Register external node with the frontend.
			 *
			 * @param InEntry - Entry to register.
			 *
			 * @return True on success.
			 */
			virtual FNodeRegistryKey RegisterNode(TUniquePtr<Metasound::Frontend::INodeRegistryEntry>&& InEntry) override;

			virtual bool UnregisterNode(const FNodeRegistryKey& InKey) override;
			virtual bool IsNodeRegistered(const FNodeRegistryKey& InKey) const override;
			virtual bool IsGraphRegistered(const FGraphRegistryKey& InKey) const override;
			virtual bool IsNodeNative(const FNodeRegistryKey& InKey) const override;

			virtual bool RegisterConversionNode(const FConverterNodeRegistryKey& InNodeKey, const FConverterNodeInfo& InNodeInfo) override;

			virtual void IterateRegistry(Metasound::FIterateMetasoundFrontendClassFunction InIterFunc, EMetasoundFrontendClassType InClassType = EMetasoundFrontendClassType::Invalid) const override;

			// Find Frontend class data.
			virtual bool FindFrontendClassFromRegistered(const FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass) override;

			UE_DEPRECATED(5.4, "This implementation of FindImplementedInterfacesFromRegistered(...). Please use bool FindImplementedInterfacesFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, TSet<FMetasoundFrontendVersion>& OutInterfacVersion)")
			virtual const TSet<FMetasoundFrontendVersion>* FindImplementedInterfacesFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey) const override;

			virtual bool FindImplementedInterfacesFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, TSet<FMetasoundFrontendVersion>& OutInterfaceVersions) const override;
			
			UE_DEPRECATED(5.4, "Use FindFrontendClassFromRegistered instead")
			virtual bool FindNodeClassInfoFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo) override;
			virtual bool FindInputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey) override;
			virtual bool FindVariableNodeRegistryKeyForDataType(const FName& InDataTypeName, FNodeRegistryKey& OutKey) override;
			virtual bool FindOutputNodeRegistryKeyForDataType(const FName& InDataTypeName, const EMetasoundFrontendVertexAccessType InAccessType, FNodeRegistryKey& OutKey) override;

			// Create a new instance of a C++ implemented node from the registry.
			virtual TUniquePtr<Metasound::INode> CreateNode(const FNodeRegistryKey& InKey, const Metasound::FNodeInitData& InInitData) const override;
			virtual TUniquePtr<INode> CreateNode(const FNodeRegistryKey& InKey, FDefaultLiteralNodeConstructorParams&&) const override;
			virtual TUniquePtr<INode> CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexNodeConstructorParams&&) const override;
			virtual TUniquePtr<INode> CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexWithLiteralNodeConstructorParams&&) const override;

			// Returns a list of possible nodes to use to convert from FromDataType to ToDataType.
			// Returns an empty array if none are available.
			virtual TArray<FConverterNodeInfo> GetPossibleConverterNodes(const FName& FromDataType, const FName& ToDataType) override;

			// Register a graph from an IMetaSoundDocumentInterface
			FGraphRegistryKey RegisterGraph(const TScriptInterface<IMetaSoundDocumentInterface>& InDocumentInterface, bool bAsync = true, bool bForceCopy = false);

			// Unregister a graph
			bool UnregisterGraph(const FGraphRegistryKey& InRegistryKey, const TScriptInterface<IMetaSoundDocumentInterface>& InDocumentInterface, bool bAsync = true);

			// Private implementation until hardened and used for template nodes other than reroutes.
			FNodeRegistryKey RegisterNodeTemplate(TUniquePtr<Metasound::Frontend::INodeRegistryTemplateEntry>&& InEntry);
			bool UnregisterNodeTemplate(const FNodeRegistryKey& InKey);

			// Create a transaction stream for any newly transactions
			TUniquePtr<FNodeRegistryTransactionStream> CreateTransactionStream();

		private:
			struct FActiveRegistrationTaskInfo
			{
				FNodeRegistryTransaction::ETransactionType TransactionType = FNodeRegistryTransaction::ETransactionType::NodeRegistration;
				UE::Tasks::FTask Task;
				FTopLevelAssetPath AssetPath;
			};

			void BuildAndRegisterGraphFromDocument(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface, const FProxyDataCache& InProxyDataCache, FNodeClassInfo&& InNodeClassInfo);

			void AddRegistrationTask(const FGraphRegistryKey& InKey, FActiveRegistrationTaskInfo&& TaskInfo);
			void RemoveRegistrationTask(const FGraphRegistryKey& InKey, FNodeRegistryTransaction::ETransactionType TransactionType);

			// Adds reference to document's owning UObject to internal ObjectReferencer,
			// indicating async registration/unregistration task(s) are depending on it.
			void AddDocumentReference(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface);

			// Removes reference to document's owning UObject from internal ObjectReferencer,
			// indicating async registration/unregistration task(s) are no longer depending on it.
			void RemoveDocumentReference(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface);

			bool UnregisterNodeInternal(const FNodeRegistryKey& InKey, FNodeClassInfo* OutClassInfo = nullptr);
			FNodeRegistryKey RegisterNodeInternal(TUniquePtr<INodeRegistryEntry>&& InEntry);

			static FRegistryContainerImpl* LazySingleton;

			void WaitForAsyncRegistrationInternal(const FNodeRegistryKey& InRegistryKey, const FTopLevelAssetPath* InAssetPath) const;

			void RegisterGraphInternal(const FGraphRegistryKey& InKey, TSharedPtr<const FGraph> InGraph);
			bool UnregisterGraphInternal(const FGraphRegistryKey& InKey);

			// Access a node entry safely. Node entries can be added/removed asynchronously. Functions passed to this method will be
			// executed in a manner where access to the node registry entry is safe from threading issues. 
			//
			// @returns true if a node registry entry was found and the function executed. False if the entry was not 
			//          found and the function not executed. 
			bool AccessNodeEntryThreadSafe(const FNodeRegistryKey& InKey, TFunctionRef<void(const INodeRegistryEntry&)> InFunc) const;

			const INodeRegistryTemplateEntry* FindNodeTemplateEntry(const FNodeRegistryKey& InKey) const;


			// This buffer is used to enqueue nodes and datatypes to register when the module has been initialized,
			// in order to avoid bad behavior with ensures, logs, etc. on static initialization.
			// The bad news is that TInlineAllocator is the safest allocator to use on static init.
			// The good news is that none of these lambdas typically have captures, so this should have low memory overhead.
			static constexpr int32 MaxNumNodesAndDatatypesToInitialize = 2048;
			TArray<TUniqueFunction<void()>, TInlineAllocator<MaxNumNodesAndDatatypesToInitialize>> LazyInitCommands;
			
			FCriticalSection LazyInitCommandCritSection;

			// Registry in which we keep all information about nodes implemented in C++.
			TMultiMap<FNodeRegistryKey, TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe>> RegisteredNodes;

			// Registry in which we keep all information about dynamically-generated templated nodes via in C++.
			TMap<FNodeRegistryKey, TSharedRef<INodeRegistryTemplateEntry, ESPMode::ThreadSafe>> RegisteredNodeTemplates;

			// Map of all registered graphs
			TMap<FGraphRegistryKey, TSharedPtr<const FGraph>> RegisteredGraphs;

			// Registry in which we keep lists of possible nodes to use to convert between two datatypes
			TMap<FConverterNodeRegistryKey, FConverterNodeRegistryValue> ConverterNodeRegistry;

			TSharedRef<FNodeRegistryTransactionBuffer> TransactionBuffer;

			mutable FCriticalSection RegistryMapsCriticalSection;
			mutable FCriticalSection ActiveRegistrationTasksCriticalSection;
			mutable FCriticalSection ObjectReferencerCriticalSection;

			UE::Tasks::FPipe AsyncRegistrationPipe;
			TMap<FNodeRegistryKey, TArray<FActiveRegistrationTaskInfo>> ActiveRegistrationTasks;
			TUniquePtr<FMetasoundFrontendRegistryContainer::IObjectReferencer> ObjectReferencer;
		};
	}
}

