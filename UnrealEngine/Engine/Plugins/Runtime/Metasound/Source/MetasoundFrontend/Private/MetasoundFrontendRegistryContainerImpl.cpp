// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundFrontendRegistryContainerImpl.h"

#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Containers/UnrealString.h"
#include "Containers/Array.h"
#include "Containers/Set.h"
#include "HAL/PlatformTime.h"
#include "Misc/Guid.h"
#include "Misc/ScopeLock.h"
#include "Tasks/Pipe.h"
#include "Tasks/Task.h"
#include "Templates/SharedPointer.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"
#include "UObject/ScriptInterface.h"

#include "MetasoundAssetManager.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendDataTypeRegistry.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocumentIdGenerator.h"
#include "MetasoundFrontendGraph.h"
#include "MetasoundFrontendProxyDataCache.h"
#include "MetasoundFrontendRegistryContainerImpl.h"
#include "MetasoundFrontendRegistryTransaction.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundLog.h"
#include "MetasoundRouter.h"
#include "MetasoundTrace.h"

bool bBusyWaitOnAsyncRegistrationTasks = true;
static FAutoConsoleVariableRef CVarAsyncRegistrationTasksBusyWait(
	TEXT("au.MetaSound.BusyWaitOnAsyncRegistrationTasks"),
	bBusyWaitOnAsyncRegistrationTasks,
	TEXT("Use TaskGraph BusyWait instead of simple Wait. Required to avoid hangs on platforms with low number of cores."),
	ECVF_Default);

namespace Metasound::Frontend
{
	namespace RegistryPrivate
	{
		TScriptInterface<IMetaSoundDocumentInterface> BuildRegistryDocument(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface, bool bForceCopy)
		{
			using namespace Metasound::Frontend;

			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::Frontend::BuildRegistryDocument);

#if WITH_EDITOR
			// Node template transform is performed on copy of local document to avoid overwriting editable data
			constexpr bool bTransformDocumentBeforeRegistering = true;
#else // !WITH_EDITOR
			// Node template transform is performed on local document only if cook determinism ID generation
			// is enabled to avoid transforms potentially creating new edges with non-deterministic IDs.
			const bool bTransformDocumentBeforeRegistering = MetaSoundEnableCookDeterministicIDGeneration == 0;
#endif // !WITH_EDITOR

			if (bTransformDocumentBeforeRegistering)
			{
				// 1. Find template dependencies to build prior to making new document/builder as an optimization
				// (no sense in creating new document/builder if no templates need processing)
				FMetaSoundFrontendDocumentBuilder OriginalDocBuilder(DocumentInterface);
				const bool bContainsTemplateDependency = OriginalDocBuilder.ContainsDependencyOfType(EMetasoundFrontendClassType::Template);
				if (bContainsTemplateDependency)
				{
					UMetaSoundBuilderDocument& RegistryDocObject = UMetaSoundBuilderDocument::Create(*DocumentInterface.GetInterface());
					FMetaSoundFrontendDocumentBuilder RegistryDocBuilder(&RegistryDocObject);

					RegistryDocBuilder.TransformTemplateNodes();

					return &RegistryDocObject;
				}
			}
#if !NO_LOGGING
			else
			{
				FMetaSoundFrontendDocumentBuilder OriginalDocBuilder(DocumentInterface);
				const bool bContainsTemplateDependency = OriginalDocBuilder.ContainsDependencyOfType(EMetasoundFrontendClassType::Template);
				if (bContainsTemplateDependency)
				{
					UE_LOG(LogMetaSound, Error,
						TEXT("Template node processing disabled but provided asset class at '%s' to register contains template nodes. Runtime graph will fail to build."),
						*OriginalDocBuilder.GetDebugName());
				}
			}
#endif // !NO_LOGGING

			if (bForceCopy)
			{
				return &UMetaSoundBuilderDocument::Create(*DocumentInterface.GetInterface());
			}
			else
			{
				return DocumentInterface;
			}
		}

		// FGraphNode is used to create unique INodes based off of a IGraph.
		//
		// Individual nodes need to reflect their InstanceName and InstanceID, but otherwise
		// they simply encapsulate a shared set of behavior. To minimize memory usage, a single
		// shared IGraph is used for all nodes referring to the same IGraph.
		class FGraphNode : public INode
		{
			// This adapter class forwards the correct FBuilderOperatorParams
			// to the graph's operator creation method. Many operator creation
			// methods downcast the supplied INode in `FBuilderOperatorParams`
			// and so it is required that it point to the correct runtime instance
			// when calling CreateOperator(...)
			class FGraphOperatorFactoryAdapter : public IOperatorFactory 
			{
			public:
				FGraphOperatorFactoryAdapter(const IGraph& InGraph)
				: Graph(&InGraph)
				, GraphFactory(InGraph.GetDefaultOperatorFactory())
				{
				}

				virtual ~FGraphOperatorFactoryAdapter() = default;

				virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) override
				{
					FBuildOperatorParams ForwardParams
					{
						*Graph,  // Point to correct INode instance
						InParams.OperatorSettings,
						InParams.InputData,
						InParams.Environment,
						InParams.Builder
					};
					return GraphFactory->CreateOperator(ForwardParams, OutResults);
				}
			private:
				const IGraph* Graph; // Only store pointer because owning node keeps wrapped IGraph alive.  
				FOperatorFactorySharedRef GraphFactory;
			};

		public:
			FGraphNode(const FNodeInitData& InNodeInitData, TSharedRef<const IGraph> InGraphToWrap)
			: InstanceID(InNodeInitData.InstanceID)
			, Factory(MakeShared<FGraphOperatorFactoryAdapter>(*InGraphToWrap))
			, Graph(MoveTemp(InGraphToWrap))
			{
			}

			virtual const FName& GetInstanceName() const override
			{
				// Use the instance name of underlying graph because it refers
				// to the actual asset name.
				return Graph->GetInstanceName();
			}

			virtual const FGuid& GetInstanceID() const override
			{
				return InstanceID;
			}

			virtual const FNodeClassMetadata& GetMetadata() const override
			{
				return Graph->GetMetadata();
			}

			virtual const FVertexInterface& GetVertexInterface() const override
			{
				return Graph->GetVertexInterface();
			}

			virtual bool SetVertexInterface(const FVertexInterface& InInterface) override
			{
				// Cannot set vertex interface because of const reference to IGraph
				// Return true if the supplied interface is the same as the existing interface.
				return Graph->GetVertexInterface() == InInterface;
			}

			virtual bool IsVertexInterfaceSupported(const FVertexInterface& InInterface) const override
			{
				return Graph->IsVertexInterfaceSupported(InInterface);
			}

			virtual FOperatorFactorySharedRef GetDefaultOperatorFactory() const override
			{
				return Factory;
			}

		private:

			FGuid InstanceID;
			TSharedRef<FGraphOperatorFactoryAdapter> Factory;
			TSharedRef<const IGraph> Graph;
		};


		// FDocumentNodeRegistryEntry encapsulates a node registry entry for a FGraph
		class FDocumentNodeRegistryEntry : public INodeRegistryEntry
		{
		public:
			FDocumentNodeRegistryEntry(const FMetasoundFrontendGraphClass& InGraphClass, const TSet<FMetasoundFrontendVersion>& InInterfaces, FNodeClassInfo&& InNodeClassInfo, TSharedPtr<const IGraph> InGraph)
			: FrontendClass(InGraphClass)
			, Interfaces(InInterfaces)
			, ClassInfo(MoveTemp(InNodeClassInfo))
			, Graph(InGraph)
			{
				FrontendClass.Metadata.SetType(EMetasoundFrontendClassType::External);
			}

			FDocumentNodeRegistryEntry(const FDocumentNodeRegistryEntry&) = default;

			virtual ~FDocumentNodeRegistryEntry() = default;

			virtual const FNodeClassInfo& GetClassInfo() const override
			{
				return ClassInfo;
			}

			virtual TUniquePtr<INode> CreateNode(const FNodeInitData& InNodeInitData) const override
			{
				if (Graph.IsValid())
				{
					return MakeUnique<FGraphNode>(InNodeInitData, Graph.ToSharedRef());
				}
				else
				{
					UE_LOG(LogMetaSound, Error, TEXT("Cannot create MetaSound node from asset %s due to prior failure to build graph"), *ClassInfo.AssetPath.ToString());
					return TUniquePtr<INode>();
				}
			}

			virtual TUniquePtr<INode> CreateNode(FDefaultLiteralNodeConstructorParams&&) const override { return nullptr; }
			virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexNodeConstructorParams&&) const override { return nullptr; }
			virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexWithLiteralNodeConstructorParams&&) const override { return nullptr; }

			virtual const FMetasoundFrontendClass& GetFrontendClass() const override
			{
				return FrontendClass;
			}

			virtual TUniquePtr<INodeRegistryEntry> Clone() const override
			{
				return MakeUnique<FDocumentNodeRegistryEntry>(*this);
			}

			virtual const TSet<FMetasoundFrontendVersion>* GetImplementedInterfaces() const override
			{
				return &Interfaces;
			}

			virtual bool IsNative() const override
			{
				return false;
			}

		private:

			FMetasoundFrontendClass FrontendClass;
			TSet<FMetasoundFrontendVersion> Interfaces;
			FNodeClassInfo ClassInfo;
			TSharedPtr<const IGraph> Graph;
		};
	} // namespace RegistryPrivate

	void FRegistryContainerImpl::BuildAndRegisterGraphFromDocument(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface, const FProxyDataCache& InProxyDataCache, FNodeClassInfo&& InNodeClassInfo)
	{
		using namespace RegistryPrivate;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FRegistryContainerImpl::BuildAndRegisterGraphFromDocument);
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("Metasound::FRegistryContainerImpl::BuildAndRegisterGraphFromDocument asset %s"), *InNodeClassInfo.AssetPath.ToString()));

		const FMetasoundFrontendDocument& Document = DocumentInterface->GetConstDocument();
		TUniquePtr<FFrontendGraph> FrontendGraph = FFrontendGraphBuilder::CreateGraph(Document, InProxyDataCache, InNodeClassInfo.AssetPath.ToString());
		if (!FrontendGraph.IsValid())
		{
			UE_LOG(LogMetaSound, Error, TEXT("Failed to build MetaSound graph in asset '%s'"), *InNodeClassInfo.AssetPath.ToString());
		}

		TSharedPtr<const FGraph> GraphToRegister = MakeShareable<const FGraph>(FrontendGraph.Release());

		TUniquePtr<INodeRegistryEntry> RegistryEntry = MakeUnique<FDocumentNodeRegistryEntry>(Document.RootGraph, Document.Interfaces, MoveTemp(InNodeClassInfo), GraphToRegister);

		const FNodeRegistryKey RegistryKey = RegisterNodeInternal(MoveTemp(RegistryEntry));

		// Key must use the asset path provided to class info and *NOT* that of the
		// provided DocumentInterface object, as that may be a built transient object
		// with a different transient asset path.
		const FGraphRegistryKey GraphKey { RegistryKey, InNodeClassInfo.AssetPath };
		RegisterGraphInternal(GraphKey, GraphToRegister);
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
	, AsyncRegistrationPipe( UE_SOURCE_LOCATION )
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
			UE_LOG(LogMetaSound, Warning, TEXT("Registering more that %d nodes and datatypes for metasounds! Consider increasing MetasoundFrontendRegistryContainer::MaxNumNodesAndDatatypesToInitialize."), MaxNumNodesAndDatatypesToInitialize);
		}

		LazyInitCommands.Add(MoveTemp(InFunc));
		return true;
	}

	void FRegistryContainerImpl::SetObjectReferencer(TUniquePtr<IObjectReferencer> InReferencer)
	{
		FScopeLock LockActiveReg(&ActiveRegistrationTasksCriticalSection);
		checkf(ActiveRegistrationTasks.IsEmpty(), TEXT("Object Referencer cannot be set while registry is actively being manipulated"));
		ObjectReferencer = MoveTemp(InReferencer);
	}

	TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, const Metasound::FNodeInitData& InInitData) const
	{
		TUniquePtr<INode> Node;
		auto CreateNodeLambda = [&Node, &InInitData](const INodeRegistryEntry& Entry)
		{ 
			Node = Entry.CreateNode(InInitData); 
		};

		if (!AccessNodeEntryThreadSafe(InKey, CreateNodeLambda))
		{
			// Creation of external nodes can rely on assets being unavailable due to errors in loading order, asset(s)
			// missing, etc. 
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey.ToString());
		}

		return MoveTemp(Node);
	}

	TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultLiteralNodeConstructorParams&& InParams) const
	{
		TUniquePtr<INode> Node;
		auto CreateNodeLambda = [&Node, &InParams](const INodeRegistryEntry& Entry)
		{ 
			Node = Entry.CreateNode(MoveTemp(InParams)); 
		};

		if (!AccessNodeEntryThreadSafe(InKey, CreateNodeLambda))
		{
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey.ToString());
		}

		return MoveTemp(Node);
	}

	TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexNodeConstructorParams&& InParams) const
	{
		TUniquePtr<INode> Node;
		auto CreateNodeLambda = [&Node, &InParams](const INodeRegistryEntry& Entry) 
		{ 
			Node = Entry.CreateNode(MoveTemp(InParams)); 
		};

		if (!AccessNodeEntryThreadSafe(InKey, CreateNodeLambda))
		{
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey.ToString());
		}

		return MoveTemp(Node);
	}

	TUniquePtr<Metasound::INode> FRegistryContainerImpl::CreateNode(const FNodeRegistryKey& InKey, FDefaultNamedVertexWithLiteralNodeConstructorParams&& InParams) const
	{
		TUniquePtr<INode> Node;
		auto CreateNodeLambda = [&Node, &InParams](const INodeRegistryEntry& Entry)
		{ 
			Node = Entry.CreateNode(MoveTemp(InParams));
		};

		if (!AccessNodeEntryThreadSafe(InKey, CreateNodeLambda))
		{
			UE_LOG(LogMetaSound, Error, TEXT("Could not find node [RegistryKey:%s]"), *InKey.ToString());
		}

		return MoveTemp(Node);
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

	FGraphRegistryKey FRegistryContainerImpl::RegisterGraph(const TScriptInterface<IMetaSoundDocumentInterface>& InDocumentInterface, bool bAsync, bool bForceCopy)
	{
		using namespace UE;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FRegistryContainerImpl::RegisterGraph);

		check(InDocumentInterface);
		check(IsInGameThread());

		// Use the asset path of the provided document interface object for identification, *NOT* the
		// built version as the build process may in fact create a new object with a transient path.
		const FTopLevelAssetPath AssetPath = InDocumentInterface->GetAssetPathChecked();
		const TScriptInterface<IMetaSoundDocumentInterface> RegistryDocInterface = RegistryPrivate::BuildRegistryDocument(InDocumentInterface, bForceCopy);

		UObject* OwningObject = RegistryDocInterface.GetObject();
		check(OwningObject);

		const FMetasoundFrontendDocument& Document = RegistryDocInterface->GetConstDocument();
		const FGraphRegistryKey RegistryKey { FNodeRegistryKey(Document.RootGraph), AssetPath };

		if (!RegistryKey.IsValid())
		{
			// Do not attempt to build and register a MetaSound with an invalid registry key
			UE_LOG(LogMetaSound, Warning, TEXT("Registry key is invalid when attemping to register graph for asset %s"), *AssetPath.ToString());
			return RegistryKey;
		}

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FRegistryContainerImpl::RegisterGraph key:%s, asset %s"), *RegistryKey.ToString(), *AssetPath.ToString()));

		FNodeClassInfo NodeClassInfo(Document.RootGraph, AssetPath);

		// Proxies are created synchronously to avoid creating proxies in async tasks. Proxies
		// are created from UObjects which need to be protected from GC and non-GT access.
		FProxyDataCache ProxyDataCache;
		ProxyDataCache.CreateAndCacheProxies(Document);

		// Store update to newly registered node in history so nodes
		// can be queried by transaction ID
		{
			FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeRegistration, NodeClassInfo, Timestamp));
		}

		if (bAsync)
		{
			// Wait for any async tasks that are in flight which correspond to the same graph
			WaitForAsyncGraphRegistration(RegistryKey);

			Tasks::FTask BuildAndRegisterTask = AsyncRegistrationPipe.Launch(
				UE_SOURCE_LOCATION,
				[RegistryKey, ClassInfo = MoveTemp(NodeClassInfo), RegistryDocInterface, ProxyDataCache = MoveTemp(ProxyDataCache)]() mutable
				{
					FRegistryContainerImpl& Registry = FRegistryContainerImpl::Get();
					// Unregister the graph before re-registering
					if (Registry.IsGraphRegistered(RegistryKey))
					{
						Registry.UnregisterGraphInternal(RegistryKey);
					}

					Registry.BuildAndRegisterGraphFromDocument(RegistryDocInterface, ProxyDataCache, MoveTemp(ClassInfo));
					Registry.RemoveRegistrationTask(RegistryKey, FNodeRegistryTransaction::ETransactionType::NodeRegistration);
					Registry.RemoveDocumentReference(RegistryDocInterface);
				}
			);

			AddDocumentReference(RegistryDocInterface);
			AddRegistrationTask(RegistryKey, FActiveRegistrationTaskInfo
			{
				FNodeRegistryTransaction::ETransactionType::NodeRegistration,
				BuildAndRegisterTask,
				AssetPath
			});
		}
		else
		{
			if (IsGraphRegistered(RegistryKey))
			{
				UnregisterGraphInternal(RegistryKey);
			}

			// Build and register graph synchronously
			BuildAndRegisterGraphFromDocument(RegistryDocInterface, ProxyDataCache, MoveTemp(NodeClassInfo));
		}

		return RegistryKey;
	}

	void FRegistryContainerImpl::AddRegistrationTask(const FGraphRegistryKey& InKey, FActiveRegistrationTaskInfo&& TaskInfo)
	{
		FScopeLock LockActiveReg(&ActiveRegistrationTasksCriticalSection);
		ActiveRegistrationTasks.FindOrAdd(InKey.NodeKey).Add(MoveTemp(TaskInfo));
	}

	void FRegistryContainerImpl::RemoveRegistrationTask(const FGraphRegistryKey& InKey, FNodeRegistryTransaction::ETransactionType TransactionType)
	{
		FScopeLock LockActiveReg(&ActiveRegistrationTasksCriticalSection);

		int32 NumRemoved = 0;
		if (InKey.AssetPath.IsNull())  // Null provided path instructs to remove all tasks related to the underlying Node Registry Key
		{
			NumRemoved = ActiveRegistrationTasks.Remove(InKey.NodeKey);
		}
		else if (TArray<FActiveRegistrationTaskInfo>* TaskInfos = ActiveRegistrationTasks.Find(InKey.NodeKey))
		{
			auto MatchesEntryInTask = [&InKey, &TransactionType](const FActiveRegistrationTaskInfo& Info)
			{
				const bool bIsAssetPath = Info.AssetPath == InKey.AssetPath;
				const bool bIsTransactionType = Info.TransactionType == TransactionType;
				return bIsAssetPath && bIsTransactionType;
			};

			NumRemoved = TaskInfos->RemoveAllSwap(MatchesEntryInTask, EAllowShrinking::No);
			if (TaskInfos->IsEmpty())
			{
				ActiveRegistrationTasks.Remove(InKey.NodeKey);
			}
		}


		if (NumRemoved == 0)
		{
			const bool bIsCooking = IsRunningCookCommandlet();
			if (ensureMsgf(!bIsCooking,
				TEXT("Failed to find active %s tasks for the graph '%s': Async registration is not supported while cooking"),
				*FNodeRegistryTransaction::LexToString(TransactionType),
				*InKey.ToString()))
			{
				UE_LOG(LogMetaSound, Warning,
					TEXT("Failed to find active %s tasks for the graph '%s'."),
					*FNodeRegistryTransaction::LexToString(TransactionType),
					*InKey.ToString());
			}
		}
	}

	void FRegistryContainerImpl::AddDocumentReference(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface)
	{
		FScopeLock LockActiveReg(&ObjectReferencerCriticalSection);
		if (UObject* Object = DocumentInterface.GetObject())
		{
			if (ObjectReferencer)
			{
				ObjectReferencer->AddObject(Object);
			}
		}
	}

	void FRegistryContainerImpl::RemoveDocumentReference(TScriptInterface<IMetaSoundDocumentInterface> DocumentInterface)
	{
		FScopeLock LockActiveReg(&ObjectReferencerCriticalSection);
		if (UObject* Object = DocumentInterface.GetObject())
		{
			if (ObjectReferencer)
			{
				ObjectReferencer->RemoveObject(Object);
			}
		}
	}

	void FRegistryContainerImpl::RegisterGraphInternal(const FGraphRegistryKey& InKey, TSharedPtr<const FGraph> InGraph)
	{
		using namespace RegistryPrivate;

		FScopeLock Lock(&RegistryMapsCriticalSection);

#if !NO_LOGGING
		if (RegisteredGraphs.Contains(InKey))
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Graph is already registered with the same registry key '%s'. The existing registered graph will be replaced with the new graph."), *InKey.ToString());
		}
#endif // !NO_LOGGING

		RegisteredGraphs.Add(InKey, InGraph);
	}

	bool FRegistryContainerImpl::UnregisterGraphInternal(const FGraphRegistryKey& InKey)
	{
		using namespace RegistryPrivate;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*InKey.ToString(TEXT("FRegistryContainerImpl::UnregisterGraphInternal")));

		FScopeLock Lock(&RegistryMapsCriticalSection);
		{
			const int32 GraphUnregistered = RegisteredGraphs.Remove(InKey) > 0;
			const bool bNodeUnregistered = UnregisterNodeInternal(InKey.NodeKey);

#if !NO_LOGGING
			if (GraphUnregistered)
			{
				UE_LOG(LogMetaSound, VeryVerbose, TEXT("Unregistered graph with key '%s'"), *InKey.ToString());
			}
			else
			{
				// Avoid warning if in cook as we always expect a graph to not get registered/
				// unregistered while cooking (as its unnecessary for serialization).
				if (bNodeUnregistered && !IsRunningCookCommandlet())
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Graph '%s' was not found, but analogous registered node class was when unregistering."), *InKey.ToString());
				}
			}
#endif // !NO_LOGGING

			return bNodeUnregistered;
		}

		return false;
	}

	bool FRegistryContainerImpl::UnregisterGraph(const FGraphRegistryKey& InRegistryKey, const TScriptInterface<IMetaSoundDocumentInterface>& InDocumentInterface, bool bAsync)
	{
		using namespace UE;
		using namespace RegistryPrivate;

		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FRegistryContainerImpl::UnregisterGraph);
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*InRegistryKey.ToString(TEXT("FRegistryContainerImpl::UnregisterGraph")));

		check(IsInGameThread());
		check(InDocumentInterface);

		// Do not attempt to unregister a MetaSound with an invalid registry key
		if (!InRegistryKey.IsValid())
		{
			UE_LOG(LogMetaSound, Warning, TEXT("Registry key is invalid when attempting to unregister graph (%s)"), *InRegistryKey.ToString());
			return false;
		}

		const FMetasoundFrontendDocument& Document = InDocumentInterface->GetConstDocument();
		FNodeClassInfo NodeClassInfo(Document.RootGraph.Metadata);

		// This is a hack to avoid requiring the asset path to be passed while unregistering.
		// The asset path may be invalid by this point if the object being unregistered is being GC'ed.
		// FNodeClassInfo needs to be deprecated in favor of more precise types as a key, editor data, etc.
		// Its currently kind of a dumping ground as it stands.
		NodeClassInfo.Type = EMetasoundFrontendClassType::External;

		// Store update to unregistered node in history so nodes can be queried by transaction ID
		{
			FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeUnregistration, NodeClassInfo, Timestamp));
		}

		if (bAsync)
		{
			// Wait for any async tasks that are in flight which correspond to the same graph
			WaitForAsyncGraphRegistration(InRegistryKey);

			Tasks::FTask UnregisterTask = AsyncRegistrationPipe.Launch(UE_SOURCE_LOCATION, [RegistryKey = InRegistryKey]()
			{
				FRegistryContainerImpl& Registry = FRegistryContainerImpl::Get();
				Registry.UnregisterGraphInternal(RegistryKey);
				Registry.RemoveRegistrationTask(RegistryKey, FNodeRegistryTransaction::ETransactionType::NodeUnregistration);
			});

			AddRegistrationTask(InRegistryKey, FActiveRegistrationTaskInfo
			{
				FNodeRegistryTransaction::ETransactionType::NodeUnregistration,
				UnregisterTask,
				InRegistryKey.AssetPath
			});
		}
		else
		{
			UnregisterGraphInternal(InRegistryKey);
		}

		return true;
	}

	TSharedPtr<const Metasound::FGraph> FRegistryContainerImpl::GetGraph(const FGraphRegistryKey& InKey) const
	{
		WaitForAsyncGraphRegistration(InKey);

		TSharedPtr<const FGraph> Graph;
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			if (const TSharedPtr<const FGraph>* RegisteredGraph = RegisteredGraphs.Find(InKey))
			{
				Graph = *RegisteredGraph;
			}
		}

		if (!Graph)
		{
			UE_LOG(LogMetaSound, Error, TEXT("Could not find graph with registry graph key '%s'."), *InKey.ToString());
		}

		return Graph;
	}

	FNodeRegistryKey FRegistryContainerImpl::RegisterNodeInternal(TUniquePtr<INodeRegistryEntry>&& InEntry)
	{
		using namespace RegistryPrivate;

		METASOUND_LLM_SCOPE;

		if (!InEntry.IsValid())
		{
			return { };
		}

		const FNodeRegistryKey Key = FNodeRegistryKey(InEntry->GetClassInfo());
		METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*Key.ToString(TEXT("FRegistryContainerImpl::RegisterNodeInternal")))

		TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe> Entry(InEntry.Release());
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);

			// check to see if an identical node was already registered, and log
			if (const TSharedRef<INodeRegistryEntry>* ExistingEntry = RegisteredNodes.Find(Key))
			{
				const FNodeClassInfo& ClassInfo = (*ExistingEntry)->GetClassInfo();
				UE_LOG(LogMetaSound, Error,
					TEXT("Node with registry key '%s' already registered by asset '%s' encountered while registering node with asset %s. MetaSounds which depend on these assets may utilize incorrect asset."),
					*Key.ToString(),
					*ClassInfo.AssetPath.ToString(),
					*Entry->GetClassInfo().AssetPath.ToString());
			}

			// Store registry elements in map so nodes can be queried using registry key.
			RegisteredNodes.Add(Key, Entry);
		}

		return Key;
	}

	FNodeRegistryKey FRegistryContainerImpl::RegisterNode(TUniquePtr<INodeRegistryEntry>&& InEntry)
	{
		const FNodeClassInfo ClassInfo = InEntry->GetClassInfo();
		const FNodeRegistryKey Key = RegisterNodeInternal(MoveTemp(InEntry));

		if (Key.IsValid())
		{
			// Store update to newly registered node in history so nodes
			// can be queried by transaction ID
			const FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeRegistration, ClassInfo, Timestamp));
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

			Key = FNodeRegistryKey(Entry->GetClassInfo());

			{
				FScopeLock Lock(&RegistryMapsCriticalSection);
				// check to see if an identical node was already registered, and log
				ensureAlwaysMsgf(
					!RegisteredNodeTemplates.Contains(Key),
					TEXT("Node template with registry key '%s' already registered. The previously registered node will be overwritten."),
					*Key.ToString());

				// Store registry elements in map so nodes can be queried using registry key.
				RegisteredNodeTemplates.Add(Key, Entry);
			}

			// Store update to newly registered node in history so nodes
			// can be queried by transaction ID

			TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeRegistration, Entry->GetClassInfo(), Timestamp));
		}

		return Key;
	}

	bool FRegistryContainerImpl::UnregisterNodeInternal(const FNodeRegistryKey& InKey, FNodeClassInfo* OutClassInfo)
	{
		METASOUND_LLM_SCOPE;

		if (InKey.IsValid())
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("FRegistryContainerImpl::UnregisterNodeInternal key %s"), *InKey.ToString()))

			FScopeLock Lock(&RegistryMapsCriticalSection);
			if (const TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe>* EntryPtr = RegisteredNodes.Find(InKey))
			{
				const TSharedRef<INodeRegistryEntry>& Entry = *EntryPtr;
				if (OutClassInfo)
				{
					*OutClassInfo = Entry->GetClassInfo();
				}
				const uint32 NumRemoved = RegisteredNodes.RemoveSingle(InKey, Entry);
				if (ensure(NumRemoved == 1))
				{
					return true;
				}
			}
		}

		if (OutClassInfo)
		{
			*OutClassInfo = { };
		}
		return false;
	}

	bool FRegistryContainerImpl::UnregisterNode(const FNodeRegistryKey& InKey)
	{
		FNodeClassInfo ClassInfo;
		if (UnregisterNodeInternal(InKey, &ClassInfo))
		{
			const FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();
			TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeUnregistration, ClassInfo, Timestamp));

			return true;
		}

		return false;
	}

	bool FRegistryContainerImpl::UnregisterNodeTemplate(const FNodeRegistryKey& InKey)
	{
		METASOUND_LLM_SCOPE;

		if (InKey.IsValid())
		{
			if (const INodeRegistryTemplateEntry* Entry = FindNodeTemplateEntry(InKey))
			{
				FNodeRegistryTransaction::FTimeType Timestamp = FPlatformTime::Cycles64();

				TransactionBuffer->AddTransaction(FNodeRegistryTransaction(FNodeRegistryTransaction::ETransactionType::NodeUnregistration, Entry->GetClassInfo(), Timestamp));

				{
					FScopeLock Lock(&RegistryMapsCriticalSection);
					RegisteredNodeTemplates.Remove(InKey);
				}
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
		auto IsNodeRegisteredInternal = [this, &InKey]() -> bool
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			return RegisteredNodes.Contains(InKey) || RegisteredNodeTemplates.Contains(InKey);
		};

		if (IsNodeRegisteredInternal())
		{
			return true;
		}
		else
		{
			WaitForAsyncRegistrationInternal(InKey, nullptr /* InAssetPath */);
			return IsNodeRegisteredInternal();
		}
	}

	bool FRegistryContainerImpl::IsGraphRegistered(const FGraphRegistryKey& InKey) const
	{
		WaitForAsyncGraphRegistration(InKey);

		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			return RegisteredGraphs.Contains(InKey);
		}
	}

	bool FRegistryContainerImpl::IsNodeNative(const FNodeRegistryKey& InKey) const
	{
		bool bIsNative = false;
		auto SetIsNative = [&bIsNative](const INodeRegistryEntry& Entry) 
		{ 
			bIsNative = Entry.IsNative();
		};

		if (AccessNodeEntryThreadSafe(InKey, SetIsNative))
		{
			return bIsNative;
		}

		if (const INodeRegistryTemplateEntry* TemplateEntry = FindNodeTemplateEntry(InKey))
		{
			return true;
		}
		return false;
	}

	bool FRegistryContainerImpl::FindFrontendClassFromRegistered(const FNodeRegistryKey& InKey, FMetasoundFrontendClass& OutClass)
	{
		auto SetFrontendClass = [&OutClass](const INodeRegistryEntry& Entry)
		{
			OutClass = Entry.GetFrontendClass();
		};

		if (AccessNodeEntryThreadSafe(InKey, SetFrontendClass))
		{
			return true;
		}

		if (const INodeRegistryTemplateEntry* Entry = FindNodeTemplateEntry(InKey))
		{
			OutClass = Entry->GetFrontendClass();
			return true;
		}

		return false;
	}

	const TSet<FMetasoundFrontendVersion>* FRegistryContainerImpl::FindImplementedInterfacesFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey) const
	{
		static bool bHasWarningBeenIssued = false;
		if (!bHasWarningBeenIssued)
		{
			// This function is known to be thread-unsafe and should not longer be used.
			// This implementation exists to support deprecated usage. 
			UE_LOG(LogMetaSound, Warning, TEXT("Accessing non-thread-safe implementation of FindImplementedInterfacesFromRegistered(...) is known to cause crashes. Please update your code to use the non-deprecated version of this function with the same name"));
			bHasWarningBeenIssued = true;
		}

		const TSet<FMetasoundFrontendVersion>* Interfaces = nullptr;
		AccessNodeEntryThreadSafe(InKey, 
			[&Interfaces](const INodeRegistryEntry& Entry)
			{
				Interfaces = Entry.GetImplementedInterfaces();
			}
		);

		return Interfaces;
	}

	bool FRegistryContainerImpl::FindImplementedInterfacesFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, TSet<FMetasoundFrontendVersion>& OutInterfaceVersions) const 
	{
		bool bDidCopy = false;

		auto CopyImplementedInterfaces = [&OutInterfaceVersions, &bDidCopy](const INodeRegistryEntry& Entry)
		{
			if (const TSet<FMetasoundFrontendVersion>* Interfaces = Entry.GetImplementedInterfaces())
			{
				OutInterfaceVersions = *Interfaces;
				bDidCopy = true;
			}
		};

		AccessNodeEntryThreadSafe(InKey, CopyImplementedInterfaces);

		return bDidCopy;
	}

	bool FRegistryContainerImpl::FindNodeClassInfoFromRegistered(const Metasound::Frontend::FNodeRegistryKey& InKey, FNodeClassInfo& OutInfo)
	{
		auto CopyClassInfo = [&OutInfo](const INodeRegistryEntry& Entry)
		{
			OutInfo = Entry.GetClassInfo();
		};
		if (AccessNodeEntryThreadSafe(InKey, CopyClassInfo))
		{
			return true;
		}

		if (const INodeRegistryTemplateEntry* Entry = FindNodeTemplateEntry(InKey))
		{
			OutInfo = Entry->GetClassInfo();
			return true;
		}

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
					OutKey = FNodeRegistryKey(Class.Metadata);
					return true;
				}
			}
			break;

			case EMetasoundFrontendVertexAccessType::Value:
			{
				if (IDataTypeRegistry::Get().GetFrontendConstructorInputClass(InDataTypeName, Class))
				{
					OutKey = FNodeRegistryKey(Class.Metadata);
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
			OutKey = FNodeRegistryKey(Class.Metadata);
			return true;
		}
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
					OutKey = FNodeRegistryKey(Class.Metadata);
					return true;
				}
			}
			break;

			case EMetasoundFrontendVertexAccessType::Value:
			{
				if (IDataTypeRegistry::Get().GetFrontendConstructorOutputClass(InDataTypeName, Class))
				{
					OutKey = FNodeRegistryKey(Class.Metadata);
					return true;
				}
			}
			break;
		}

		return false;
	}

	void FRegistryContainerImpl::IterateRegistry(Metasound::FIterateMetasoundFrontendClassFunction InIterFunc, EMetasoundFrontendClassType InClassType) const
	{
		UE_LOG(LogMetaSound, Warning, TEXT("Calling FMetasoundRegistryContainer::IterateRegistry(...) is not threadsafe. Please use Metasound::Frontend::ISearchEngine instead"));
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

	bool FRegistryContainerImpl::AccessNodeEntryThreadSafe(const FNodeRegistryKey& InKey, TFunctionRef<void(const INodeRegistryEntry&)> InFunc) const
	{
		auto TryAccessNodeEntry = [this, &InKey, &InFunc]() -> bool
		{
			FScopeLock Lock(&RegistryMapsCriticalSection);
			if (const TSharedRef<INodeRegistryEntry, ESPMode::ThreadSafe>* Entry = RegisteredNodes.Find(InKey))
			{
				InFunc(*(*Entry));
				return true;
			}
			return false;
		};

		if (TryAccessNodeEntry())
		{
			return true;
		}
		else
		{
			// Wait for any async registration tasks related to the registry key. 
			WaitForAsyncRegistrationInternal(InKey, nullptr /* InAssetPath */);
			return TryAccessNodeEntry();
		}
	}

	const INodeRegistryTemplateEntry* FRegistryContainerImpl::FindNodeTemplateEntry(const FNodeRegistryKey& InKey) const
	{
		FScopeLock Lock(&RegistryMapsCriticalSection);
		if (const TSharedRef<INodeRegistryTemplateEntry, ESPMode::ThreadSafe>* Entry = RegisteredNodeTemplates.Find(InKey))
		{
			return &Entry->Get();
		}

		return nullptr;
	}

	void FRegistryContainerImpl::WaitForAsyncGraphRegistration(const FGraphRegistryKey& InKey) const
	{
		WaitForAsyncRegistrationInternal(InKey.NodeKey, &InKey.AssetPath);
	}

	void FRegistryContainerImpl::WaitForAsyncRegistrationInternal(const FNodeRegistryKey& InRegistryKey, const FTopLevelAssetPath* InAssetPath) const
	{
		using namespace UE::Tasks;

		if (AsyncRegistrationPipe.IsInContext())
		{
			// It is not safe to wait for an async registration task from within the async registration pipe because it will result in a deadlock. 
			UE_LOG(LogMetaSound, Verbose, TEXT("Async registration pipe is already in context for registering key %s. Task will not be waited for."), *InRegistryKey.ToString());
			return;
		}

		TArray<FTask> TasksToWaitFor;
		{
			FScopeLock Lock(&ActiveRegistrationTasksCriticalSection);
			if (const TArray<FActiveRegistrationTaskInfo>* FoundTasks = ActiveRegistrationTasks.Find(InRegistryKey))
			{
				// Filter by asset path or ignore if not provided
				Algo::TransformIf(*FoundTasks, TasksToWaitFor,
					[&InAssetPath](const FActiveRegistrationTaskInfo& TaskInfo) { return !InAssetPath || InAssetPath->IsNull() || TaskInfo.AssetPath == *InAssetPath; },
					[](const FActiveRegistrationTaskInfo& TaskInfo) { return TaskInfo.Task; });
			}
		}

		for (const FTask& Task : TasksToWaitFor)
		{
			METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(Metasound::FRegistryContainerImpl::WaitForRegistrationTaskToComplete);
			if (Task.IsValid())
			{
				if (bBusyWaitOnAsyncRegistrationTasks)
				{
					Task.BusyWait();
				}
				else
				{
					Task.Wait();
				}
			}	
		}
	}
} // namespace Metasound::Frontend
