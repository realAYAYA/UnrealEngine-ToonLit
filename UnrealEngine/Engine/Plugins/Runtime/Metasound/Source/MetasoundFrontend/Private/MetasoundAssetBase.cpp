// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundAssetBase.h"

#include "Algo/AnyOf.h"
#include "Algo/Copy.h"
#include "Algo/ForEach.h"
#include "Algo/Transform.h"
#include "Containers/Set.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "IAudioParameterTransmitter.h"
#include "Interfaces/MetasoundFrontendInterface.h"
#include "Interfaces/MetasoundFrontendInterfaceRegistry.h"
#include "Internationalization/Text.h"
#include "IStructSerializerBackend.h"
#include "Logging/LogMacros.h"
#include "MetasoundAssetManager.h"
#include "MetasoundDocumentInterface.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentBuilder.h"
#include "MetasoundFrontendDocumentIdGenerator.h"
#include "MetasoundFrontendDocumentVersioning.h"
#include "MetasoundFrontendGraph.h"
#include "MetasoundFrontendNodeTemplateRegistry.h"
#include "MetasoundFrontendProxyDataCache.h"
#include "MetasoundFrontendRegistries.h"
#include "MetasoundFrontendRegistryContainerImpl.h"
#include "MetasoundFrontendSearchEngine.h"
#include "MetasoundFrontendTransform.h"
#include "MetasoundGraph.h"
#include "MetasoundJsonBackend.h"
#include "MetasoundLog.h"
#include "MetasoundParameterPack.h"
#include "MetasoundParameterTransmitter.h"
#include "MetasoundTrace.h"
#include "MetasoundVertex.h"
#include "StructSerializer.h"
#include "Templates/SharedPointer.h"
#include "UObject/MetaData.h"

#define LOCTEXT_NAMESPACE "MetaSound"

namespace Metasound
{
	namespace Frontend
	{
		namespace AssetBasePrivate
		{
			// Zero values means, that these don't do anything.
			static float BlockRateOverride = 0;
			static int32 SampleRateOverride = 0;

			void DepthFirstTraversal(const FMetasoundAssetBase& InInitAsset, TFunctionRef<TSet<const FMetasoundAssetBase*>(const FMetasoundAssetBase&)> InVisitFunction)
			{
				// Non recursive depth first traversal.
				TArray<const FMetasoundAssetBase*> Stack({ &InInitAsset });
				TSet<const FMetasoundAssetBase*> Visited;

				while (!Stack.IsEmpty())
				{
					const FMetasoundAssetBase* CurrentNode = Stack.Pop();
					if (!Visited.Contains(CurrentNode))
					{
						TArray<const FMetasoundAssetBase*> Children = InVisitFunction(*CurrentNode).Array();
						Stack.Append(Children);

						Visited.Add(CurrentNode);
					}
				}
			}

			// Registers node by copying document. Updates to document require re-registration.
			// This registry entry does not support node creation as it is only intended to be
			// used when cooking MetaSounds. 
			class FDocumentNodeRegistryEntryForCook : public INodeRegistryEntry
			{
			public:
				FDocumentNodeRegistryEntryForCook(const FMetasoundFrontendDocument& InDocument, const FTopLevelAssetPath& InAssetPath)
					: Interfaces(InDocument.Interfaces)
					, FrontendClass(InDocument.RootGraph)
					, ClassInfo(InDocument.RootGraph, InAssetPath)
				{
					// Copy FrontendClass to preserve original document.
					FrontendClass.Metadata.SetType(EMetasoundFrontendClassType::External);
				}

				FDocumentNodeRegistryEntryForCook(const FDocumentNodeRegistryEntryForCook& InOther) = default;

				virtual ~FDocumentNodeRegistryEntryForCook() = default;

				virtual const FNodeClassInfo& GetClassInfo() const override
				{
					return ClassInfo;
				}

				virtual TUniquePtr<INode> CreateNode(const FNodeInitData&) const override { return nullptr; }
				virtual TUniquePtr<INode> CreateNode(FDefaultLiteralNodeConstructorParams&&) const override { return nullptr; }
				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexNodeConstructorParams&&) const override { return nullptr; }
				virtual TUniquePtr<INode> CreateNode(FDefaultNamedVertexWithLiteralNodeConstructorParams&&) const override { return nullptr; }

				virtual const FMetasoundFrontendClass& GetFrontendClass() const override
				{
					return FrontendClass;
				}

				virtual TUniquePtr<INodeRegistryEntry> Clone() const override
				{
					return MakeUnique<FDocumentNodeRegistryEntryForCook>(*this);
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
				TSet<FMetasoundFrontendVersion> Interfaces;
				FMetasoundFrontendClass FrontendClass;
				FNodeClassInfo ClassInfo;
			};
		} // namespace AssetBasePrivate

		namespace ConsoleVariables
		{
			static bool bDisableAsyncGraphRegistration = false;
		}

		FAutoConsoleVariableRef CVarMetaSoundDisableAsyncGraphRegistration(
			TEXT("au.MetaSound.DisableAsyncGraphRegistration"),
			Metasound::Frontend::ConsoleVariables::bDisableAsyncGraphRegistration,
			TEXT("Disables async registration of MetaSound graphs\n")
			TEXT("Default: false"),
			ECVF_Default);
		FConsoleVariableMulticastDelegate CVarMetaSoundBlockRateChanged;

		FAutoConsoleVariableRef CVarMetaSoundBlockRate(
			TEXT("au.MetaSound.BlockRate"),
			AssetBasePrivate::BlockRateOverride,
			TEXT("Sets block rate (blocks per second) of MetaSounds.\n")
			TEXT("Default: 100.0f, Min: 1.0f, Max: 1000.0f"),
			FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Var) { CVarMetaSoundBlockRateChanged.Broadcast(Var); }),
			ECVF_Default);

		FConsoleVariableMulticastDelegate CVarMetaSoundSampleRateChanged;
		FAutoConsoleVariableRef CVarMetaSoundSampleRate(
			TEXT("au.MetaSound.SampleRate"),
			AssetBasePrivate::SampleRateOverride,
			TEXT("Overrides the sample rate of metasounds. Negative values default to audio mixer sample rate.\n")
			TEXT("Default: 0, Min: 8000, Max: 48000"),
			FConsoleVariableDelegate::CreateLambda([](IConsoleVariable* Var) { CVarMetaSoundSampleRateChanged.Broadcast(Var); }),
			ECVF_Default);

		float GetBlockRateOverride()
		{
			if(AssetBasePrivate::BlockRateOverride > 0)
			{
				return FMath::Clamp(AssetBasePrivate::BlockRateOverride, 
					GetBlockRateClampRange().GetLowerBoundValue(), 
					GetBlockRateClampRange().GetUpperBoundValue()
				);
			}
			return AssetBasePrivate::BlockRateOverride;
		}

		FConsoleVariableMulticastDelegate& GetBlockRateOverrideChangedDelegate()
		{
			return CVarMetaSoundBlockRateChanged;
		}

		int32 GetSampleRateOverride()
		{
			if (AssetBasePrivate::SampleRateOverride > 0)
			{
				return FMath::Clamp(AssetBasePrivate::SampleRateOverride, 
					GetSampleRateClampRange().GetLowerBoundValue(),
					GetSampleRateClampRange().GetUpperBoundValue()
				);
			}
			return AssetBasePrivate::SampleRateOverride;
		}

		FConsoleVariableMulticastDelegate& GetSampleRateOverrideChangedDelegate()
		{
			return CVarMetaSoundSampleRateChanged;
		}
		
		TRange<float> GetBlockRateClampRange()
		{
			return TRange<float>(1.f,1000.f);
		}

		TRange<int32> GetSampleRateClampRange()
		{
			return TRange<int32>(8000, 96000);
		}
	} // namespace Frontend
} // namespace Metasound

const FString FMetasoundAssetBase::FileExtension(TEXT(".metasound"));

void FMetasoundAssetBase::RegisterGraphWithFrontend(Metasound::Frontend::FMetaSoundAssetRegistrationOptions InRegistrationOptions)
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	// Graph registration must only happen on one thread to avoid race conditions on graph registration.
	checkf(IsInGameThread(), TEXT("MetaSound %s graph can only be registered on the GameThread"), *GetOwningAssetName());
	checkf(!IsRunningCookCommandlet(), TEXT("Cook of asset must call RegisterNode directly providing FDocumentNodeRegistryEntryForCook to avoid proxy/runtime graph generation."));

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::RegisterGraphWithFrontend);
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE_TEXT(*FString::Printf(TEXT("MetaSoundAssetBase::RegisterGraphWithFrontend asset %s"), *this->GetOwningAssetName()));
	if (!InRegistrationOptions.bForceReregister)
	{
		if (IsRegistered())
		{
			return;
		}
	}


#if WITH_EDITOR
	if (InRegistrationOptions.bRebuildReferencedAssetClasses)
	{
		RebuildReferencedAssetClasses();
	}
#endif

	if (InRegistrationOptions.bRegisterDependencies)
	{
		RegisterAssetDependencies(InRegistrationOptions);
	}
	IMetaSoundAssetManager::GetChecked().AddOrUpdateAsset(*GetOwningAsset());

	// Auto update must be done after all referenced asset classes are registered
	if (InRegistrationOptions.bAutoUpdate)
	{
		const bool bDidUpdate = AutoUpdate(InRegistrationOptions.bAutoUpdateLogWarningOnDroppedConnection);
#if WITH_EDITOR
		if (bDidUpdate || InRegistrationOptions.bForceViewSynchronization)
		{
			GetModifyContext().SetForceRefreshViews();
		}
#endif // WITH_EDITOR
	}
	else
	{
#if WITH_EDITOR
		if (InRegistrationOptions.bForceViewSynchronization)
		{
			GetModifyContext().SetForceRefreshViews();
		}
#endif // WITH_EDITOR
	}

#if WITH_EDITOR
	// Must be completed after auto-update to ensure all non-transient referenced dependency data is up-to-date (ex.
	// class version), which is required for most accurately caching current registry metadata.
	CacheRegistryMetadata();
#endif // WITH_EDITOR

	UObject* Owner = GetOwningAsset();
	check(Owner);
	const FString AssetName = Owner->GetName();

	// Register graphs async by default;
	const bool bAsync = !ConsoleVariables::bDisableAsyncGraphRegistration;
	// Force a copy if async registration is enabled and we need to protect against
	// race conditions from external modifications.
	bool bForceCopy = (IsBuilderActive() && bAsync);
#if WITH_EDITOR
	bForceCopy |= InRegistrationOptions.bRegisterCopyIfAsync;
#endif // WITH_EDITOR

	GraphRegistryKey = FRegistryContainerImpl::Get().RegisterGraph(Owner, bAsync, bForceCopy);

	if (GraphRegistryKey.IsValid())
	{
#if WITH_EDITORONLY_DATA
		UpdateAssetRegistry();
#endif // WITH_EDITORONLY_DATA
	}
	else
	{
		UClass* Class = Owner->GetClass();
		check(Class);
		const FString ClassName = Class->GetName();
		UE_LOG(LogMetaSound, Error, TEXT("Registration failed for MetaSound node class '%s' of UObject class '%s'"), *AssetName, *ClassName);
	}
}

void FMetasoundAssetBase::CookMetaSound()
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::CookMetaSound);
	if (IsRegistered())
	{
		return;
	}

	CookReferencedMetaSounds();
	IMetaSoundAssetManager::GetChecked().AddOrUpdateAsset(*GetOwningAsset());

	// Auto update must be done after all referenced asset classes are registered
	const bool bDidUpdate = AutoUpdate(/*bAutoUpdateLogWarningOnDroppedConnection=*/true);
#if WITH_EDITOR
	if (bDidUpdate)
	{
		GetModifyContext().SetForceRefreshViews();
	}
#endif // WITH_EDITOR

#if WITH_EDITOR
	// Must be completed after auto-update to ensure all non-transient referenced dependency data is up-to-date (ex.
	// class version), which is required for most accurately caching current registry metadata.
	CacheRegistryMetadata();
#endif // WITH_EDITOR

	UObject* Owner = GetOwningAsset();
	check(Owner);

	{
		// Performs document transforms on local copy, which reduces document footprint & renders transforming unnecessary unless altered at runtime when registering
		FMetaSoundFrontendDocumentBuilder DocBuilder(Owner);
		const bool bContainsTemplateDependency = DocBuilder.ContainsDependencyOfType(EMetasoundFrontendClassType::Template);
		if (bContainsTemplateDependency)
		{
			DocBuilder.TransformTemplateNodes();
		}

		if (GraphRegistryKey.IsValid())
		{
			FRegistryContainerImpl::Get().UnregisterNode(GraphRegistryKey.NodeKey);
			GraphRegistryKey = { };
		}

		// During cook, we need to register the node so that it is available for other graphs, but we need to avoid
		// creating proxies. To do so, we use a special node registration object which reflects the necessary information
		// for the node registry, but does not create INodes.
		TScriptInterface<IMetaSoundDocumentInterface> DocInterface(Owner);
		const FMetasoundFrontendDocument& Document = DocInterface->GetConstDocument();
		const FTopLevelAssetPath AssetPath = DocInterface->GetAssetPathChecked();
		TUniquePtr<INodeRegistryEntry> RegistryEntry = MakeUnique<AssetBasePrivate::FDocumentNodeRegistryEntryForCook>(Document, AssetPath);

		const FNodeRegistryKey NodeKey = FRegistryContainerImpl::Get().RegisterNode(MoveTemp(RegistryEntry));
		GraphRegistryKey = FGraphRegistryKey { NodeKey, AssetPath };
	}

	if (GraphRegistryKey.IsValid())
	{
#if WITH_EDITORONLY_DATA
		UpdateAssetRegistry();
#endif // WITH_EDITORONLY_DATA
	}
	else
	{
		const UClass* Class = Owner->GetClass();
		check(Class);
		const FString ClassName = Class->GetName();
		UE_LOG(LogMetaSound, Error, TEXT("Registration failed during cook for MetaSound node class '%s' of UObject class '%s'"), *GetOwningAssetName(), *ClassName);
	}
}

void FMetasoundAssetBase::OnNotifyBeginDestroy()
{
	using namespace Metasound::Frontend;

	// Unregistration of graph is not necessary when cooking as deserialized objects are not mutable and, should they be reloaded,
	// omitting unregistration avoids potentially kicking off an invalid asynchronous task to unregister a non-existent runtime graph.
	if (IsRunningCookCommandlet())
	{
		if (GraphRegistryKey.IsValid())
		{
			FRegistryContainerImpl::Get().UnregisterNode(GraphRegistryKey.NodeKey);
			GraphRegistryKey = { };
		}
	}
	else
	{
		UnregisterGraphWithFrontend();
	}
}

void FMetasoundAssetBase::UnregisterGraphWithFrontend()
{
	using namespace Metasound::Frontend;
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::UnregisterGraphWithFrontend);

	check(IsInGameThread());
	checkf(!IsRunningCookCommandlet(), TEXT("Cook of asset must call UnregisterNode directly providing FDocumentNodeRegistryEntryForCook to avoid proxy/runtime graph generation."));

	if (GraphRegistryKey.IsValid())
	{
		UObject* OwningAsset = GetOwningAsset();
		if (ensureAlways(OwningAsset))
		{
			// Async registration is only available if:
			// 1. The IMetaSoundDocumentInterface is not actively modified by a builder
			//    (built graph must be released synchronously to avoid a race condition on
			//    reading/writing the IMetaSoundDocumentInterface on the Game Thread)
			// 2. Async registration is globally disabled via console variable.
			const bool bAsync = !(IsBuilderActive() || ConsoleVariables::bDisableAsyncGraphRegistration);
			const bool bSuccess = FRegistryContainerImpl::Get().UnregisterGraph(GraphRegistryKey, OwningAsset, bAsync);
			if (!bSuccess)
			{
				UE_LOG(LogMetaSound, Verbose, TEXT("Failed to unregister node with key %s for asset %s. No registry entry exists with that key."), *GraphRegistryKey.ToString(), *GetOwningAssetName());
			}
		}

		GraphRegistryKey = { };
	}
}

void FMetasoundAssetBase::SetMetadata(FMetasoundFrontendClassMetadata& InMetadata)
{
	FMetasoundFrontendDocument& Doc = GetDocumentChecked();
	Doc.RootGraph.Metadata = InMetadata;

	if (Doc.RootGraph.Metadata.GetType() != EMetasoundFrontendClassType::Graph)
	{
		UE_LOG(LogMetaSound, Display, TEXT("Forcing class type to EMetasoundFrontendClassType::Graph on root graph metadata"));
		Doc.RootGraph.Metadata.SetType(EMetasoundFrontendClassType::Graph);
	}
}

bool FMetasoundAssetBase::GetDeclaredInterfaces(TArray<const Metasound::Frontend::IInterfaceRegistryEntry*>& OutInterfaces) const
{
	return FMetaSoundFrontendDocumentBuilder::FindDeclaredInterfaces(GetDocumentChecked(), OutInterfaces);
}

bool FMetasoundAssetBase::IsInterfaceDeclared(const FMetasoundFrontendVersion& InVersion) const
{
	return GetDocumentChecked().Interfaces.Contains(InVersion);
}

Metasound::Frontend::FNodeClassInfo FMetasoundAssetBase::GetAssetClassInfo() const
{
	using namespace Metasound::Frontend;

	const UObject* Owner = GetOwningAsset();
	check(Owner);
	TScriptInterface<const IMetaSoundDocumentInterface> DocInterface((UObject*)Owner);
	return FNodeClassInfo { GetDocumentChecked().RootGraph, DocInterface->GetAssetPathChecked() };
}

void FMetasoundAssetBase::SetDocument(const FMetasoundFrontendDocument& InDocument, bool bMarkDirty)
{
	FMetasoundFrontendDocument& Document = GetDocumentChecked();
	Document = InDocument;
	if (bMarkDirty)
	{
		MarkMetasoundDocumentDirty();
	}
}

void FMetasoundAssetBase::AddDefaultInterfaces()
{
	using namespace Metasound::Frontend;

	UObject* OwningAsset = GetOwningAsset();
	check(OwningAsset);

	UClass* AssetClass = OwningAsset->GetClass();
	check(AssetClass);
	const FTopLevelAssetPath& ClassPath = AssetClass->GetClassPathName();

	TArray<FMetasoundFrontendVersion> InitVersions = ISearchEngine::Get().FindUClassDefaultInterfaceVersions(ClassPath);
	FModifyRootGraphInterfaces({ }, InitVersions).Transform(GetDocumentChecked());
}

void FMetasoundAssetBase::SetDocument(FMetasoundFrontendDocument&& InDocument, bool bMarkDirty)
{
	FMetasoundFrontendDocument& Document = GetDocumentChecked();
	Document = MoveTemp(InDocument);
	if (bMarkDirty)
	{
		MarkMetasoundDocumentDirty();
	}
}

bool FMetasoundAssetBase::VersionAsset()
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::VersionAsset);
	constexpr bool bIsDeterministic = true;
	FDocumentIDGenerator::FScopeDeterminism DeterminismScope(bIsDeterministic);

	FName AssetName;
	FString AssetPath;
	if (const UObject* OwningAsset = GetOwningAsset())
	{
		AssetName = FName(OwningAsset->GetName());
		AssetPath = OwningAsset->GetPathName();
	}

	FMetasoundFrontendDocument* Doc = GetDocumentAccessPtr().Get();
	if (!ensure(Doc))
	{
		return false;
	}

	bool bDidEdit = Doc->VersionInterfaces();

	// Version Document Model
	FDocumentHandle DocHandle = GetDocumentHandle();
	{
		bDidEdit |= FVersionDocument(AssetName, AssetPath).Transform(DocHandle);
	}

	// Version Interfaces. Has to be re-run until no pass reports an update in case
	// versions fork (ex. an interface splits into two newly named interfaces).
	{
		bool bInterfaceUpdated = false;
		bool bPassUpdated = true;
		while (bPassUpdated)
		{
			bPassUpdated = false;

			const TArray<FMetasoundFrontendVersion> Versions = Doc->Interfaces.Array();
			for (const FMetasoundFrontendVersion& Version : Versions)
			{
				bPassUpdated |= FUpdateRootGraphInterface(Version, GetOwningAssetName()).Transform(DocHandle);
			}

			bInterfaceUpdated |= bPassUpdated;
		}

		if (bInterfaceUpdated)
		{
			ConformObjectDataToInterfaces();
		}
		bDidEdit |= bInterfaceUpdated;
	}

	return bDidEdit;
}

#if WITH_EDITOR
void FMetasoundAssetBase::CacheRegistryMetadata()
{
	using namespace Metasound::Frontend;

	FMetasoundFrontendDocument* Document = GetDocumentAccessPtr().Get();
	if (!ensure(Document))
	{
		return;
	}

	using FNameDataTypePair = TPair<FName, FName>;
	const TSet<FMetasoundFrontendVersion>& InterfaceVersions = Document->Interfaces;
	FMetasoundFrontendClassInterface& RootGraphClassInterface = Document->RootGraph.Interface;

	// 1. Gather inputs/outputs managed by interfaces
	TMap<FNameDataTypePair, FMetasoundFrontendClassInput*> Inputs;
	for (FMetasoundFrontendClassInput& Input : RootGraphClassInterface.Inputs)
	{
		FNameDataTypePair NameDataTypePair = FNameDataTypePair(Input.Name, Input.TypeName);
		Inputs.Add(MoveTemp(NameDataTypePair), &Input);
	}

	TMap<FNameDataTypePair, FMetasoundFrontendClassOutput*> Outputs;
	for (FMetasoundFrontendClassOutput& Output : RootGraphClassInterface.Outputs)
	{
		FNameDataTypePair NameDataTypePair = FNameDataTypePair(Output.Name, Output.TypeName);
		Outputs.Add(MoveTemp(NameDataTypePair), &Output);
	}

	// 2. Copy metadata for inputs/outputs managed by interfaces, removing them from maps generated
	auto CacheInterfaceMetadata = [](const FMetasoundFrontendVertexMetadata & InRegistryMetadata, FMetasoundFrontendVertexMetadata& OutMetadata)
	{
		const int32 CachedSortOrderIndex = OutMetadata.SortOrderIndex;
		OutMetadata = InRegistryMetadata;
		OutMetadata.SortOrderIndex = CachedSortOrderIndex;
	};

	for (const FMetasoundFrontendVersion& Version : InterfaceVersions)
	{
		const FInterfaceRegistryKey InterfaceKey = GetInterfaceRegistryKey(Version);
		const IInterfaceRegistryEntry* Entry = IInterfaceRegistry::Get().FindInterfaceRegistryEntry(InterfaceKey);
		if (ensure(Entry))
		{
			for (const FMetasoundFrontendClassInput& InterfaceInput : Entry->GetInterface().Inputs)
			{
				const FNameDataTypePair NameDataTypePair = FNameDataTypePair(InterfaceInput.Name, InterfaceInput.TypeName);
				if (FMetasoundFrontendClassInput* Input = Inputs.FindRef(NameDataTypePair))
				{
					CacheInterfaceMetadata(InterfaceInput.Metadata, Input->Metadata);
					Inputs.Remove(NameDataTypePair);
				}
			}

			for (const FMetasoundFrontendClassOutput& InterfaceOutput : Entry->GetInterface().Outputs)
			{
				const FNameDataTypePair NameDataTypePair = FNameDataTypePair(InterfaceOutput.Name, InterfaceOutput.TypeName);
				if (FMetasoundFrontendClassOutput* Output = Outputs.FindRef(NameDataTypePair))
				{
					CacheInterfaceMetadata(InterfaceOutput.Metadata, Output->Metadata);
					Outputs.Remove(NameDataTypePair);
				}
			}
		}
	}

	// 3. Iterate remaining inputs/outputs not managed by interfaces and set to serialize text
	// (in case they were orphaned by an interface no longer being implemented).
	for (TPair<FNameDataTypePair, FMetasoundFrontendClassInput*>& Pair : Inputs)
	{
		Pair.Value->Metadata.SetSerializeText(true);
	}

	for (TPair<FNameDataTypePair, FMetasoundFrontendClassOutput*>& Pair : Outputs)
	{
		Pair.Value->Metadata.SetSerializeText(true);
	}

	// 4. Refresh style as order of members could've changed
	{
		FMetasoundFrontendInterfaceStyle InputStyle;
		Algo::ForEach(RootGraphClassInterface.Inputs, [&InputStyle](const FMetasoundFrontendClassInput& Input)
		{
			InputStyle.DefaultSortOrder.Add(Input.Metadata.SortOrderIndex);
		});
		RootGraphClassInterface.SetInputStyle(InputStyle);
	}

	{
		FMetasoundFrontendInterfaceStyle OutputStyle;
		Algo::ForEach(RootGraphClassInterface.Outputs, [&OutputStyle](const FMetasoundFrontendClassOutput& Output)
		{
			OutputStyle.DefaultSortOrder.Add(Output.Metadata.SortOrderIndex);
		});
		RootGraphClassInterface.SetOutputStyle(OutputStyle);
	}

	// 5. Cache registry data on document dependencies
	for (FMetasoundFrontendClass& Dependency : Document->Dependencies)
	{
		if (!FMetasoundFrontendClass::CacheGraphDependencyMetadataFromRegistry(Dependency))
		{
			UE_LOG(LogMetaSound, Warning,
				TEXT("'%s' failed to cache dependency registry data: Registry missing class with key '%s'"),
				*GetOwningAssetName(),
				*Dependency.Metadata.GetClassName().ToString());
			UE_LOG(LogMetaSound, Warning,
				TEXT("Asset '%s' may fail to build runtime graph unless re-registered after dependency with given key is loaded."),
				*GetOwningAssetName());
		}
	}
}

FMetasoundFrontendDocumentModifyContext& FMetasoundAssetBase::GetModifyContext()
{
	return GetDocumentChecked().Metadata.ModifyContext;
}

const FMetasoundFrontendDocumentModifyContext& FMetasoundAssetBase::GetModifyContext() const
{
	return GetDocumentChecked().Metadata.ModifyContext;
}
#endif // WITH_EDITOR



bool FMetasoundAssetBase::IsRegistered() const
{
	using namespace Metasound::Frontend;

	return GraphRegistryKey.IsValid();
}

bool FMetasoundAssetBase::IsReferencedAsset(const FMetasoundAssetBase& InAsset) const
{
	using namespace Metasound::Frontend;

	bool bIsReferenced = false;
	AssetBasePrivate::DepthFirstTraversal(*this, [&](const FMetasoundAssetBase& ChildAsset)
	{
		TSet<const FMetasoundAssetBase*> Children;
		if (&ChildAsset == &InAsset)
		{
			bIsReferenced = true;
			return Children;
		}

		TArray<FMetasoundAssetBase*> ChildRefs;
		ensureAlways(IMetaSoundAssetManager::GetChecked().TryLoadReferencedAssets(ChildAsset, ChildRefs));
		Algo::Transform(ChildRefs, Children, [](FMetasoundAssetBase* Child) { return Child; });
		return Children;

	});

	return bIsReferenced;
}

bool FMetasoundAssetBase::AddingReferenceCausesLoop(const FSoftObjectPath& InReferencePath) const
{
	using namespace Metasound::Frontend;

	const FMetasoundAssetBase* ReferenceAsset = IMetaSoundAssetManager::GetChecked().TryLoadAsset(InReferencePath);
	if (!ensureAlways(ReferenceAsset))
	{
		return false;
	}

	bool bCausesLoop = false;
	const FMetasoundAssetBase* Parent = this;
	AssetBasePrivate::DepthFirstTraversal(*ReferenceAsset, [&](const FMetasoundAssetBase& ChildAsset)
	{
		TSet<const FMetasoundAssetBase*> Children;
		if (Parent == &ChildAsset)
		{
			bCausesLoop = true;
			return Children;
		}

		TArray<FMetasoundAssetBase*> ChildRefs;
		ensureAlways(IMetaSoundAssetManager::GetChecked().TryLoadReferencedAssets(ChildAsset, ChildRefs));
		Algo::Transform(ChildRefs, Children, [] (FMetasoundAssetBase* Child) { return Child; });
		return Children;
	});

	return bCausesLoop;
}

void FMetasoundAssetBase::ConvertFromPreset()
{
	using namespace Metasound::Frontend;
	FGraphHandle GraphHandle = GetRootGraphHandle();

#if WITH_EDITOR
	FMetasoundFrontendGraphStyle Style = GraphHandle->GetGraphStyle();
	Style.bIsGraphEditable = true;
	GraphHandle->SetGraphStyle(Style);
#endif // WITH_EDITOR

	FMetasoundFrontendGraphClassPresetOptions PresetOptions = GraphHandle->GetGraphPresetOptions();
	PresetOptions.bIsPreset = false;
	GraphHandle->SetGraphPresetOptions(PresetOptions);
}

TArray<FMetasoundAssetBase::FSendInfoAndVertexName> FMetasoundAssetBase::GetSendInfos(uint64 InInstanceID) const
{
	return TArray<FSendInfoAndVertexName>();
}

#if WITH_EDITOR
FText FMetasoundAssetBase::GetDisplayName(FString&& InTypeName) const
{
	using namespace Metasound::Frontend;

	FConstGraphHandle GraphHandle = GetRootGraphHandle();
	const bool bIsPreset = !GraphHandle->GetGraphStyle().bIsGraphEditable;

	if (!bIsPreset)
	{
		return FText::FromString(MoveTemp(InTypeName));
	}

	return FText::Format(LOCTEXT("PresetDisplayNameFormat", "{0} (Preset)"), FText::FromString(MoveTemp(InTypeName)));
}
#endif // WITH_EDITOR

bool FMetasoundAssetBase::MarkMetasoundDocumentDirty() const
{
	if (const UObject* OwningAsset = GetOwningAsset())
	{
		return ensure(OwningAsset->MarkPackageDirty());
	}
	return false;
}

Metasound::Frontend::FDocumentHandle FMetasoundAssetBase::GetDocumentHandle()
{
	return Metasound::Frontend::IDocumentController::CreateDocumentHandle(GetDocumentAccessPtr());
}

Metasound::Frontend::FConstDocumentHandle FMetasoundAssetBase::GetDocumentHandle() const
{
	return Metasound::Frontend::IDocumentController::CreateDocumentHandle(GetDocumentConstAccessPtr());
}

Metasound::Frontend::FGraphHandle FMetasoundAssetBase::GetRootGraphHandle()
{
	return GetDocumentHandle()->GetRootGraph();
}

Metasound::Frontend::FConstGraphHandle FMetasoundAssetBase::GetRootGraphHandle() const
{
	return GetDocumentHandle()->GetRootGraph();
}

bool FMetasoundAssetBase::ImportFromJSON(const FString& InJSON)
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::ImportFromJSON);

	FMetasoundFrontendDocument* Document = GetDocumentAccessPtr().Get();
	if (ensure(nullptr != Document))
	{
		bool bSuccess = Metasound::Frontend::ImportJSONToMetasound(InJSON, *Document);

		if (bSuccess)
		{
			ensure(MarkMetasoundDocumentDirty());
		}

		return bSuccess;
	}
	return false;
}

bool FMetasoundAssetBase::ImportFromJSONAsset(const FString& InAbsolutePath)
{
	METASOUND_TRACE_CPUPROFILER_EVENT_SCOPE(MetaSoundAssetBase::ImportFromJSONAsset);

	Metasound::Frontend::FDocumentAccessPtr DocumentPtr = GetDocumentAccessPtr();
	if (FMetasoundFrontendDocument* Document = DocumentPtr.Get())
	{
		bool bSuccess = Metasound::Frontend::ImportJSONAssetToMetasound(InAbsolutePath, *Document);

		if (bSuccess)
		{
			ensure(MarkMetasoundDocumentDirty());
		}

		return bSuccess;
	}
	return false;
}

FMetasoundFrontendDocument& FMetasoundAssetBase::GetDocumentChecked()
{
	FMetasoundFrontendDocument* Document = GetDocumentAccessPtr().Get();
	check(nullptr != Document);
	return *Document;
}

const FMetasoundFrontendDocument& FMetasoundAssetBase::GetDocumentChecked() const
{
	const FMetasoundFrontendDocument* Document = GetDocumentConstAccessPtr().Get();

	check(nullptr != Document);
	return *Document;
}

const Metasound::Frontend::FGraphRegistryKey& FMetasoundAssetBase::GetGraphRegistryKey() const
{
	return GraphRegistryKey;
}

const Metasound::Frontend::FNodeRegistryKey& FMetasoundAssetBase::GetRegistryKey() const
{
	return GraphRegistryKey.NodeKey;
}

FString FMetasoundAssetBase::GetOwningAssetName() const
{
	if (const UObject* OwningAsset = GetOwningAsset())
	{
		return OwningAsset->GetPathName();
	}
	return FString();
}

#if WITH_EDITOR
void FMetasoundAssetBase::RebuildReferencedAssetClasses()
{
	using namespace Metasound::Frontend;

	IMetaSoundAssetManager& AssetManager = IMetaSoundAssetManager::GetChecked();

	AssetManager.AddAssetReferences(*this);
	TSet<IMetaSoundAssetManager::FAssetInfo> ReferencedAssetClasses = AssetManager.GetReferencedAssetClasses(*this);
	SetReferencedAssetClasses(MoveTemp(ReferencedAssetClasses));
}
#endif // WITH_EDITOR

void FMetasoundAssetBase::RegisterAssetDependencies(const Metasound::Frontend::FMetaSoundAssetRegistrationOptions& InRegistrationOptions)
{
	using namespace Metasound::Frontend;

	IMetaSoundAssetManager& AssetManager = IMetaSoundAssetManager::GetChecked();
	TArray<FMetasoundAssetBase*> References = GetReferencedAssets();
	for (FMetasoundAssetBase* Reference : References)
	{
		if (InRegistrationOptions.bForceReregister || !Reference->IsRegistered())
		{
			// TODO: Check for infinite recursion and error if so
			AssetManager.AddOrUpdateAsset(*(Reference->GetOwningAsset()));
			Reference->RegisterGraphWithFrontend(InRegistrationOptions);
		}
	}
}

void FMetasoundAssetBase::CookReferencedMetaSounds()
{
	using namespace Metasound::Frontend;

	IMetaSoundAssetManager& AssetManager = IMetaSoundAssetManager::GetChecked();
	TArray<FMetasoundAssetBase*> References = GetReferencedAssets();
	for (FMetasoundAssetBase* Reference : References)
	{
		if (!Reference->IsRegistered())
		{
			// TODO: Check for infinite recursion and error if so
			AssetManager.AddOrUpdateAsset(*(Reference->GetOwningAsset()));
			Reference->CookMetaSound();
		}
	}
}

bool FMetasoundAssetBase::AutoUpdate(bool bInLogWarningsOnDroppedConnection)
{
	using namespace Metasound::Frontend;

	FString OwningAssetName = GetOwningAssetName();
	const bool bAutoUpdated = FAutoUpdateRootGraph(MoveTemp(OwningAssetName), bInLogWarningsOnDroppedConnection).Transform(GetDocumentHandle());
	return bAutoUpdated;
}

#if WITH_EDITORONLY_DATA
void FMetasoundAssetBase::UpdateAssetRegistry() 
{
	using namespace Metasound;
	using namespace Metasound::Frontend;

	UObject* Owner = GetOwningAsset();
	check(Owner);
	TScriptInterface<IMetaSoundDocumentInterface> DocInterface(Owner);
	FNodeClassInfo AssetClassInfo(GetDocumentChecked().RootGraph, DocInterface->GetAssetPathChecked());

	// Refresh Asset Registry Info if successfully registered with Frontend
	const FMetasoundFrontendGraphClass& DocumentClassGraph = GetDocumentHandle()->GetRootGraphClass();
	const FMetasoundFrontendClassMetadata& DocumentClassMetadata = DocumentClassGraph.Metadata;
	AssetClassInfo.AssetClassID = FGuid(DocumentClassMetadata.GetClassName().Name.ToString());
	FNodeClassName ClassName = DocumentClassMetadata.GetClassName().ToNodeClassName();
	FMetasoundFrontendClass GraphClass;

	AssetClassInfo.bIsPreset = DocumentClassGraph.PresetOptions.bIsPreset;
	AssetClassInfo.Version = DocumentClassMetadata.GetVersion();
	AssetClassInfo.InputTypes.Reset();
	Algo::Transform(GraphClass.Interface.Inputs, AssetClassInfo.InputTypes, [] (const FMetasoundFrontendClassInput& Input) { return Input.TypeName; });

	AssetClassInfo.OutputTypes.Reset();
	Algo::Transform(GraphClass.Interface.Outputs, AssetClassInfo.OutputTypes, [](const FMetasoundFrontendClassOutput& Output) { return Output.TypeName; });

	SetRegistryAssetClassInfo(MoveTemp(AssetClassInfo));
}
#endif

TSharedPtr<FMetasoundFrontendDocument> FMetasoundAssetBase::PreprocessDocument()
{
	return nullptr;
}

PRAGMA_DISABLE_DEPRECATION_WARNINGS
const FMetasoundAssetBase::FRuntimeData& FMetasoundAssetBase::GetRuntimeData() const
{
	static const FRuntimeData PlaceholderForDeprecatedMethod;
	return PlaceholderForDeprecatedMethod;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

#undef LOCTEXT_NAMESPACE // "MetaSound"
