// Copyright Epic Games, Inc. All Rights Reserved.

#include "Engine/BlueprintGeneratedClass.h"
#include "Misc/CoreMisc.h"
#include "Stats/StatsMisc.h"
#include "UObject/UObjectHash.h"
#include "UObject/CoreNet.h"
#include "UObject/CoreRedirects.h"
#include "UObject/Package.h"
#include "UObject/LinkerLoad.h"
#include "UObject/ObjectSaveContext.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "CookedMetaData.h"
#include "Engine/Blueprint.h"
#include "Components/ActorComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/DynamicBlueprintBinding.h"
#include "Components/TimelineComponent.h"
#include "Engine/TimelineTemplate.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/SCS_Node.h"
#include "Engine/InheritableComponentHandler.h"
#include "Misc/ScopeLock.h"
#include "UObject/CoreObjectVersion.h"
#include "Net/Core/PushModel/PushModel.h"
#include "UObject/CoreObjectVersion.h"
#include "UObject/FortniteMainBranchObjectVersion.h"
#include "UObject/UE5MainStreamObjectVersion.h"
#include "GenericPlatform/GenericPlatformCrashContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(BlueprintGeneratedClass)

#if WITH_EDITOR
#include "CookerSettings.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "BlueprintCompilationManager.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Editor/EditorEngine.h"
extern UNREALED_API UEditorEngine* GEditor;
#endif //WITH_EDITOR

DEFINE_STAT(STAT_PersistentUberGraphFrameMemory);
DEFINE_STAT(STAT_BPCompInstancingFastPathMemory);

int32 GBlueprintClusteringEnabled = 0;
static FAutoConsoleVariableRef CVarBlueprintClusteringEnabled(
	TEXT("gc.BlueprintClusteringEnabled"),
	GBlueprintClusteringEnabled,
	TEXT("Whether to allow Blueprint classes to create GC clusters."),
	ECVF_Default
);

int32 GBlueprintComponentInstancingFastPathDisabled = 0;
static FAutoConsoleVariableRef CVarBlueprintComponentInstancingFastPathDisabled(
	TEXT("bp.ComponentInstancingFastPathDisabled"),
	GBlueprintComponentInstancingFastPathDisabled,
	TEXT("Disable the Blueprint component instancing fast path."),
	ECVF_Default
);

#if WITH_ADDITIONAL_CRASH_CONTEXTS
struct BPGCBreadcrumbsParams
{
	UObject& Object;
	UBlueprintGeneratedClass& BPGC;
	FArchive& Ar;
	uint32 ThreadId;
};

// Called during a crash: dynamic memory allocations may not be reliable here.
void WriteBPGCBreadcrumbs(FCrashContextExtendedWriter& Writer, const BPGCBreadcrumbsParams& Params)
{
	constexpr int32 MAX_DATA_SIZE = 1024;
	constexpr int32 MAX_FILENAME_SIZE = 64;
	constexpr int32 MAX_THREADS_TO_LOG = 16;

	static int32 ThreadCount = 0;

	// In the unlikely case that there are too many threads that reported a BPGC-related crash, we'll just ignore the excess.
	// Theoretically, there should be enough information written by the remaining threads.
	if (ThreadCount < MAX_THREADS_TO_LOG)
	{
		// Note: TStringBuilder *can* potentially allocate dynamic memory if its inline storage is exceeded.
		// In practice, we stay under the current threshold by only recording the minimum amount data that we need.
		TStringBuilder<MAX_DATA_SIZE> Builder;

		Params.Object.GetPathName(nullptr, Builder);
		Builder.AppendChar(TEXT('\n'));
		Params.BPGC.GetPathName(nullptr, Builder);
		Builder.AppendChar(TEXT('\n'));

		if (Params.Ar.GetSerializedProperty() != nullptr)
		{
			Params.Ar.GetSerializedProperty()->GetPathName(nullptr, Builder);
		}
		else
		{
			Builder.Append(TEXT("Serialized property was null!"));
		}

		TStringBuilder<MAX_FILENAME_SIZE> Filename;

		Filename.Appendf(TEXT("BPGCBreadcrumb_%u"), Params.ThreadId);

		Writer.AddString(Filename.ToString(), Builder.ToString());
	}

	++ThreadCount;
}
#endif // WITH_ADDITIONAL_CRASH_CONTEXTS

PRAGMA_DISABLE_DEPRECATION_WARNINGS
UBlueprintGeneratedClass::UBlueprintGeneratedClass(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if VALIDATE_UBER_GRAPH_PERSISTENT_FRAME
	, UberGraphFunctionKey(0)
#endif//VALIDATE_UBER_GRAPH_PERSISTENT_FRAME
{
	NumReplicatedProperties = 0;
	// @todo: BP2CPP_remove
	bHasNativizedParent_DEPRECATED = false;
	bHasCookedComponentInstancingData = false;
	bCustomPropertyListForPostConstructionInitialized = false;
#if WITH_EDITORONLY_DATA
	bIsSparseClassDataSerializable = false;
#endif
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void UBlueprintGeneratedClass::PostInitProperties()
{
	Super::PostInitProperties();
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		// Default__BlueprintGeneratedClass uses its own AddReferencedObjects function.
		CppClassStaticFunctions = UOBJECT_CPPCLASS_STATICFUNCTIONS_FORCLASS(UBlueprintGeneratedClass);
	}
}

void UBlueprintGeneratedClass::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITORONLY_DATA
	if (bCooked)
	{
		if (const UClassCookedMetaData* CookedMetaData = FindCookedMetaData())
		{
			CookedMetaData->ApplyMetaData(this);
			PurgeCookedMetaData();
		}
	}

#if WITH_EDITOR
	// Make BPGC from a cooked package standalone so it doesn't get GCed
	if (GEditor && bCooked)
	{
		SetFlags(RF_Standalone);
	}
#endif //if WITH_EDITOR

	if (!bCooked)
	{
		if (GetAuthoritativeClass() != this)
		{
			return;
		}

		UObject* ClassCDO = ClassDefaultObject;

		// Go through the CDO of the class, and make sure we don't have any legacy components that aren't instanced hanging on.
		struct FCheckIfComponentChildHelper
		{
			static bool IsComponentChild(UObject* CurrObj, const UObject* CDO)
			{
				UObject*  OuterObject = CurrObj ? CurrObj->GetOuter() : nullptr;
				const bool bValidOuter = OuterObject && (OuterObject != CDO);
				return bValidOuter ? (OuterObject->IsDefaultSubobject() || IsComponentChild(OuterObject, CDO)) : false;
			};
		};

		if (ClassCDO)
		{
			ForEachObjectWithOuter(ClassCDO, [ClassCDO](UObject* CurrObj)
			{
				const bool bComponentChild = FCheckIfComponentChildHelper::IsComponentChild(CurrObj, ClassCDO);
				if (!CurrObj->IsDefaultSubobject() && !CurrObj->IsRooted() && !bComponentChild)
				{
					CurrObj->MarkAsGarbage();
				}
			});
		}

		if (GetLinkerUEVersion() < VER_UE4_CLASS_NOTPLACEABLE_ADDED)
		{
			// Make sure the placeable flag is correct for all blueprints
			UBlueprint* Blueprint = Cast<UBlueprint>(ClassGeneratedBy);
			if (ensure(Blueprint) && Blueprint->BlueprintType != BPTYPE_MacroLibrary)
			{
				ClassFlags &= ~CLASS_NotPlaceable;
			}
		}

#if UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
		// Patch the fast calls (needed as we can't bump engine version to serialize it directly in UFunction right now)
		for (const FEventGraphFastCallPair& Pair : FastCallPairs_DEPRECATED)
		{
			Pair.FunctionToPatch->EventGraphFunction = UberGraphFunction;
			Pair.FunctionToPatch->EventGraphCallOffset = Pair.EventGraphCallOffset;
		}
#endif
	}
#endif // WITH_EDITORONLY_DATA

	// Update any component names that have been redirected
	if (!FPlatformProperties::RequiresCookedData() && GetAllowNativeComponentClassOverrides())
	{
		for (FBPComponentClassOverride& Override : ComponentClassOverrides)
		{
			const FString ComponentName = Override.ComponentName.ToString();
			UClass* ClassToCheck = this;
			while (ClassToCheck)
			{
				if (const TMap<FString, FString>* ValueChanges = FCoreRedirects::GetValueRedirects(ECoreRedirectFlags::Type_Class, ClassToCheck))
				{
					if (const FString* NewComponentName = ValueChanges->Find(ComponentName))
					{
						Override.ComponentName = **NewComponentName;
						break;
					}
				}
				ClassToCheck = ClassToCheck->GetSuperClass();
			}
		}
	}

	AssembleReferenceTokenStream(true);
}

void UBlueprintGeneratedClass::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	FString NativeParentClassName;
	FString ParentClassName;

	if (UClass* ParentClass = GetSuperClass())
	{
		ParentClassName = FObjectPropertyBase::GetExportPath(ParentClass);

		// Walk up until we find a native class (ie 'while they are BP classes')
		UClass* NativeParentClass = ParentClass;
		while (Cast<UBlueprintGeneratedClass>(NativeParentClass))
		{
			NativeParentClass = NativeParentClass->GetSuperClass();
		}
		NativeParentClassName = FObjectPropertyBase::GetExportPath(NativeParentClass);
	}
	else
	{
		NativeParentClassName = ParentClassName = TEXT("None");
	}

	OutTags.Add(FAssetRegistryTag(FBlueprintTags::ParentClassPath, ParentClassName, FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag(FBlueprintTags::NativeParentClassPath, NativeParentClassName, FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag(FBlueprintTags::ClassFlags, FString::FromInt((uint32)GetClassFlags()), FAssetRegistryTag::TT_Hidden));

#if WITH_EDITORONLY_DATA
	// Get editor-only tags; on a cooked BPGC, those tags are deserialized into CookedEditorTags, otherwise generate them for the BP
	const FEditorTags* EditorTagsToAdd = &CookedEditorTags;
	FEditorTags EditorTags;
	if (CookedEditorTags.Num() == 0)
	{
		GetEditorTags(EditorTags);
		EditorTagsToAdd = &EditorTags;
	}

	for (const auto& EditorTag : *EditorTagsToAdd)
	{
		OutTags.Add(FAssetRegistryTag(EditorTag.Key, EditorTag.Value, FAssetRegistryTag::TT_Hidden));
	}
#endif //#if WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void UBlueprintGeneratedClass::PostLoadAssetRegistryTags(const FAssetData& InAssetData, TArray<FAssetRegistryTag>& OutTagsAndValuesToUpdate) const
{
	Super::PostLoadAssetRegistryTags(InAssetData, OutTagsAndValuesToUpdate);

	auto FixTagValueShortClassName = [&InAssetData, &OutTagsAndValuesToUpdate](FName TagName, FAssetRegistryTag::ETagType TagType)
	{
		FString TagValue = InAssetData.GetTagValueRef<FString>(TagName);
		if (!TagValue.IsEmpty() && TagValue != TEXT("None"))
		{
			if (UClass::TryFixShortClassNameExportPath(TagValue, ELogVerbosity::Warning, TEXT("UBlueprintGeneratedClass::PostLoadAssetRegistryTags")))
			{
				OutTagsAndValuesToUpdate.Add(FAssetRegistryTag(TagName, TagValue, TagType));
			}
		}
	};

	FixTagValueShortClassName(FBlueprintTags::ParentClassPath, FAssetRegistryTag::TT_Alphabetical);
	FixTagValueShortClassName(FBlueprintTags::NativeParentClassPath, FAssetRegistryTag::TT_Alphabetical);
}
#endif // WITH_EDITOR

FPrimaryAssetId UBlueprintGeneratedClass::GetPrimaryAssetId() const
{
	FPrimaryAssetId AssetId;
	if (!ClassDefaultObject)
	{
		// All UBlueprintGeneratedClass objects should have a pointer to their generated ClassDefaultObject, except for the
		// CDO itself of the UBlueprintGeneratedClass class.
		verify(HasAnyFlags(RF_ClassDefaultObject));
		return AssetId;
	}

	AssetId = ClassDefaultObject->GetPrimaryAssetId();

	/*
	if (!AssetId.IsValid())
	{ 
		FName AssetType = NAME_None; // TODO: Support blueprint-only primary assets with a class flag. No way to guess at type currently
		FName AssetName = FPackageName::GetShortFName(GetOutermost()->GetFName());
		return FPrimaryAssetId(AssetType, AssetName);
	}
	*/

	return AssetId;
}

#if WITH_EDITOR

UClass* UBlueprintGeneratedClass::GetAuthoritativeClass()
{
 	if (nullptr == ClassGeneratedBy) // to track UE-11597 and UE-11595
 	{
		// If this is a cooked blueprint, the generatedby class will have been discarded so we'll just have to assume we're authoritative!
		if (bCooked || RootPackageHasAnyFlags(PKG_FilterEditorOnly))
		{ 
			return this;
		}
		else
		{
			UE_LOG(LogBlueprint, Fatal, TEXT("UBlueprintGeneratedClass::GetAuthoritativeClass: ClassGeneratedBy is null. class '%s'"), *GetPathName());
		}
 	}

	UBlueprint* GeneratingBP = CastChecked<UBlueprint>(ClassGeneratedBy);

	check(GeneratingBP);

	return (GeneratingBP->GeneratedClass != NULL) ? GeneratingBP->GeneratedClass : this;
}

struct FConditionalRecompileClassHepler
{
	enum class ENeededAction : uint8
	{
		None,
		StaticLink,
		Recompile,
	};

	static bool HasTheSameLayoutAsParent(const UStruct* Struct)
	{
		const UStruct* Parent = Struct ? Struct->GetSuperStruct() : NULL;
		return FStructUtils::TheSameLayout(Struct, Parent);
	}

	static ENeededAction IsConditionalRecompilationNecessary(const UBlueprint* GeneratingBP)
	{
		if (FBlueprintEditorUtils::IsInterfaceBlueprint(GeneratingBP))
		{
			return ENeededAction::None;
		}

		if (FBlueprintEditorUtils::IsDataOnlyBlueprint(GeneratingBP))
		{
			// If my parent is native, my layout wasn't changed.
			const UClass* ParentClass = *GeneratingBP->ParentClass;
			if (!GeneratingBP->GeneratedClass || (GeneratingBP->GeneratedClass->GetSuperClass() != ParentClass))
			{
				return ENeededAction::Recompile;
			}

			if (ParentClass && ParentClass->HasAllClassFlags(CLASS_Native))
			{
				return ENeededAction::None;
			}

			if (HasTheSameLayoutAsParent(*GeneratingBP->GeneratedClass))
			{
				return ENeededAction::StaticLink;
			}
			else
			{
				UE_LOG(LogBlueprint, Log, TEXT("During ConditionalRecompilation the layout of DataOnly BP should not be changed. It will be handled, but it's bad for performence. Blueprint %s"), *GeneratingBP->GetName());
			}
		}

		return ENeededAction::Recompile;
	}
};

extern UNREALED_API FSecondsCounterData BlueprintCompileAndLoadTimerData;

void UBlueprintGeneratedClass::ConditionalRecompileClass(FUObjectSerializeContext* InLoadContext)
{
	FBlueprintCompilationManager::FlushCompilationQueue(InLoadContext);
}

void UBlueprintGeneratedClass::FlushCompilationQueueForLevel()
{
	if(Cast<ULevelScriptBlueprint>(ClassGeneratedBy))
	{
		FBlueprintCompilationManager::FlushCompilationQueue(nullptr);
	}
}

UObject* UBlueprintGeneratedClass::GetArchetypeForCDO() const
{
	if (OverridenArchetypeForCDO)
	{
		ensure(OverridenArchetypeForCDO->IsA(GetSuperClass()));
		return OverridenArchetypeForCDO;
	}

	return Super::GetArchetypeForCDO();
}
#endif //WITH_EDITOR

void UBlueprintGeneratedClass::SerializeDefaultObject(UObject* Object, FStructuredArchive::FSlot Slot)
{
	FScopeLock SerializeAndPostLoadLock(&SerializeAndPostLoadCritical);
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();

	Super::SerializeDefaultObject(Object, Slot);

	if (UnderlyingArchive.IsLoading() && !UnderlyingArchive.IsObjectReferenceCollector() && Object == ClassDefaultObject)
	{
		// On load, build the custom property list used in post-construct initialization logic. Note that in the editor, this will be refreshed during compile-on-load.
		// @TODO - Potentially make this serializable (or cooked data) to eliminate the slight load time cost we'll incur below to generate this list in a cooked build. For now, it's not serialized since the raw FProperty references cannot be saved out.
		UpdateCustomPropertyListForPostConstruction();

		const FString BPGCName = GetName();
		auto BuildCachedPropertyDataLambda = [BPGCName](FBlueprintCookedComponentInstancingData& CookedData, UActorComponent* SourceTemplate, FString CompVarName)
		{
			if (CookedData.bHasValidCookedData)
			{
				// This feature requires EDL at cook time, so ensure that the source template is also fully loaded at this point.
				if (SourceTemplate != nullptr
					&& ensure(!SourceTemplate->HasAnyFlags(RF_NeedLoad)))
				{
					CookedData.BuildCachedPropertyDataFromTemplate(SourceTemplate);
				}
				else
				{
					// This situation is unexpected; templates that are filtered out by context should not be generating fast path data at cook time. Emit a warning about this.
					UE_LOG(LogBlueprint, Warning, TEXT("BPComp fast path (%s.%s) : Invalid source template. Will use slow path for dynamic instancing."), *BPGCName, *CompVarName);

					// Invalidate the cooked data so that we fall back to using the slow path when dynamically instancing this node.
					CookedData.bHasValidCookedData = false;
				}
			}
		};

#if WITH_EDITOR
		const bool bShouldUseCookedComponentInstancingData = bHasCookedComponentInstancingData && !GIsEditor;
#else
		const bool bShouldUseCookedComponentInstancingData = bHasCookedComponentInstancingData;
#endif
		// Generate "fast path" instancing data for inherited SCS node templates.
		if (InheritableComponentHandler && bShouldUseCookedComponentInstancingData)
		{
			for (auto RecordIt = InheritableComponentHandler->CreateRecordIterator(); RecordIt; ++RecordIt)
			{
				BuildCachedPropertyDataLambda(RecordIt->CookedComponentInstancingData, RecordIt->ComponentTemplate, RecordIt->ComponentKey.GetSCSVariableName().ToString());
			}
		}

		if (bShouldUseCookedComponentInstancingData)
		{
			// Generate "fast path" instancing data for SCS node templates owned by this Blueprint class.
			if (SimpleConstructionScript)
			{
				const TArray<USCS_Node*>& AllSCSNodes = SimpleConstructionScript->GetAllNodes();
				for (USCS_Node* SCSNode : AllSCSNodes)
				{
					BuildCachedPropertyDataLambda(SCSNode->CookedComponentInstancingData, SCSNode->ComponentTemplate, SCSNode->GetVariableName().ToString());
				}
			}

			// Generate "fast path" instancing data for UCS/AddComponent node templates.
			if (CookedComponentInstancingData.Num() > 0)
			{
				for (UActorComponent* ComponentTemplate : ComponentTemplates)
				{
					if (ComponentTemplate)
					{
						FBlueprintCookedComponentInstancingData* ComponentInstancingData = CookedComponentInstancingData.Find(ComponentTemplate->GetFName());
						if (ComponentInstancingData != nullptr)
						{
							BuildCachedPropertyDataLambda(*ComponentInstancingData, ComponentTemplate, ComponentTemplate->GetName());
						}
					}
				}
			}
		}
	}

	UnderlyingArchive.UsingCustomVersion(FUE5MainStreamObjectVersion::GUID);

	bool bSkipSparseClassDataSerialization = false;
	if(UnderlyingArchive.CustomVer(FUE5MainStreamObjectVersion::GUID) >= FUE5MainStreamObjectVersion::SparseClassDataStructSerialization)
	{
		if (UnderlyingArchive.IsSaving())
		{
			UScriptStruct* SerializedSparseClassDataStruct = SparseClassDataStruct;
			if (!OverridesSparseClassDataArchetype())
			{
				// If this class doesn't override the sparse class data of its archetype, then we can skip saving it 
				// since it can be lazily regenerated from the archetype data on load
				SerializedSparseClassDataStruct = nullptr;
				bSkipSparseClassDataSerialization = true;
			}
			UnderlyingArchive << SerializedSparseClassDataStruct;
		}
		else if (UnderlyingArchive.IsLoading())
		{
			UScriptStruct* SerializedSparseClassDataStruct = nullptr;
			UnderlyingArchive << SerializedSparseClassDataStruct;
			if (SparseClassDataStruct != SerializedSparseClassDataStruct)
			{
				CleanupSparseClassData();
				SparseClassDataStruct = SerializedSparseClassDataStruct;
			}
			if (!SparseClassDataStruct)
			{
				// Missing or failed to load sparse class data struct - possible that the parent class was deleted, or regenerated on load
				// so seek past where this CDO was serialized as we cannot read the serialized sparse class data any more.
				// Note this happens in majority of cases with classes that have no sparse class data attached too. In that case 
				// this 'skip' over the remaining part of the archive should have no ill effects as the seek should effectively be zero
				bSkipSparseClassDataSerialization = true;
			}
		}
	}

#if WITH_EDITORONLY_DATA
	if (bIsSparseClassDataSerializable || GetOutermost()->bIsCookedForEditor)
#endif
	{
		if(bSkipSparseClassDataSerialization)
		{
			// Seek to after sparse class data rather than load it.
			if (UnderlyingArchive.IsLoading())
			{
				if (const FLinkerLoad* Linker = Object->GetLinker())
				{
					const int32 LinkerIndex = Object->GetLinkerIndex();
					const FObjectExport& Export = Linker->ExportMap[LinkerIndex];
					UnderlyingArchive.Seek(Export.SerialOffset + Export.SerialSize);
				}
			}
		}
		else if (SparseClassDataStruct)
		{
			SerializeSparseClassData(FStructuredArchiveFromArchive(UnderlyingArchive).GetSlot());
		}
	}

	if (UnderlyingArchive.IsLoading())
	{
		if (SparseClassDataStruct)
		{
			// TODO: We should make sure that this instance conforms to its archetype (keeping data where possible, potentially via 
			// an explicit upgrade path for sparse data), as it may have changed since this BPGC was saved (UE-127121)
		}
		else
		{
			SparseClassDataStruct = GetSparseClassDataArchetypeStruct();
		}
	}
}

void UBlueprintGeneratedClass::PostLoadDefaultObject(UObject* Object)
{
	FScopeLock SerializeAndPostLoadLock(&SerializeAndPostLoadCritical);

	Super::PostLoadDefaultObject(Object);

	if (Object == ClassDefaultObject)
	{
		// Rebuild the custom property list used in post-construct initialization logic. Note that PostLoad() may have altered some serialized properties.
		UpdateCustomPropertyListForPostConstruction();

		// Restore any property values from config file
		if (HasAnyClassFlags(CLASS_Config))
		{
			ClassDefaultObject->LoadConfig();
		}
	}

#if WITH_EDITOR
#if WITH_EDITORONLY_DATA
	Object->MoveDataToSparseClassDataStruct();

	if (Object->GetSparseClassDataStruct())
	{
		// now that any data has been moved into the sparse data structure we can safely serialize it
		bIsSparseClassDataSerializable = true;
	}
#endif
#endif
}

bool UBlueprintGeneratedClass::BuildCustomPropertyListForPostConstruction(FCustomPropertyListNode*& InPropertyList, UStruct* InStruct, const uint8* DataPtr, const uint8* DefaultDataPtr)
{
	const UClass* OwnerClass = Cast<UClass>(InStruct);
	FCustomPropertyListNode** CurrentNodePtr = &InPropertyList;

	for (FProperty* Property = InStruct->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		const bool bIsConfigProperty = Property->HasAnyPropertyFlags(CPF_Config) && !(OwnerClass && OwnerClass->HasAnyClassFlags(CLASS_PerObjectConfig));
		const bool bIsTransientProperty = Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient);

		// Skip config properties as they're already in the PostConstructLink chain. Also skip transient properties if they contain a reference to an instanced subobjects (as those should not be initialized from defaults).
		if (!bIsConfigProperty && (!bIsTransientProperty || !Property->ContainsInstancedObjectProperty()))
		{
			for (int32 Idx = 0; Idx < Property->ArrayDim; Idx++)
			{
				const uint8* PropertyValue = Property->ContainerPtrToValuePtr<uint8>(DataPtr, Idx);
				const uint8* DefaultPropertyValue = Property->ContainerPtrToValuePtrForDefaults<uint8>(InStruct, DefaultDataPtr, Idx);

				// If this is a struct property, recurse to pull out any fields that differ from the native CDO.
				if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					// Create a new node for the struct property.
					*CurrentNodePtr = new FCustomPropertyListNode(Property, Idx);
					CustomPropertyListForPostConstruction.Add(*CurrentNodePtr);

					UScriptStruct::ICppStructOps* CppStructOps = nullptr;
					if (StructProperty->Struct)
					{
						CppStructOps = StructProperty->Struct->GetCppStructOps();
					}

					// Check if we should initialize using the full value (e.g. a USTRUCT with one or more non-reflected fields).
					bool bIsIdentical = false;
					const uint32 PortFlags = 0;
					if(!CppStructOps || !CppStructOps->HasIdentical() || !CppStructOps->Identical(PropertyValue, DefaultPropertyValue, PortFlags, bIsIdentical))
					{
						// Recursively gather up all struct fields that differ and assign to the current node's sub property list.
						bIsIdentical = !BuildCustomPropertyListForPostConstruction((*CurrentNodePtr)->SubPropertyList, StructProperty->Struct, PropertyValue, DefaultPropertyValue);
					}

					if (!bIsIdentical)
					{
						// Advance to the next node in the list.
						CurrentNodePtr = &(*CurrentNodePtr)->PropertyListNext;
					}
					else
					{
						// Remove the node for the struct property since it does not differ from the native CDO.
						CustomPropertyListForPostConstruction.RemoveAt(CustomPropertyListForPostConstruction.Num() - 1);

						// Clear the current node ptr since the array will have freed up the memory it referenced.
						*CurrentNodePtr = nullptr;
					}
				}
				else if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
				{
					// Create a new node for the array property.
					*CurrentNodePtr = new FCustomPropertyListNode(Property, Idx);
					CustomPropertyListForPostConstruction.Add(*CurrentNodePtr);

					// Recursively gather up all array item indices that differ and assign to the current node's sub property list.
					if (BuildCustomArrayPropertyListForPostConstruction(ArrayProperty, (*CurrentNodePtr)->SubPropertyList, PropertyValue, DefaultPropertyValue))
					{
						// Advance to the next node in the list.
						CurrentNodePtr = &(*CurrentNodePtr)->PropertyListNext;
					}
					else
					{
						// Remove the node for the array property since it does not differ from the native CDO.
						CustomPropertyListForPostConstruction.RemoveAt(CustomPropertyListForPostConstruction.Num() - 1);

						// Clear the current node ptr since the array will have freed up the memory it referenced.
						*CurrentNodePtr = nullptr;
					}
				}
				else if (!Property->Identical(PropertyValue, DefaultPropertyValue))
				{
					// Create a new node, link it into the chain and add it into the array.
					*CurrentNodePtr = new FCustomPropertyListNode(Property, Idx);
					CustomPropertyListForPostConstruction.Add(*CurrentNodePtr);

					// Advance to the next node ptr.
					CurrentNodePtr = &(*CurrentNodePtr)->PropertyListNext;
				}
			}
		}
	}

	// This will be non-NULL if the above found at least one property value that differs from the native CDO.
	return (InPropertyList != nullptr);
}

bool UBlueprintGeneratedClass::BuildCustomArrayPropertyListForPostConstruction(FArrayProperty* ArrayProperty, FCustomPropertyListNode*& InPropertyList, const uint8* DataPtr, const uint8* DefaultDataPtr, int32 StartIndex)
{
	FCustomPropertyListNode** CurrentArrayNodePtr = &InPropertyList;

	FScriptArrayHelper ArrayValueHelper(ArrayProperty, DataPtr);
	FScriptArrayHelper DefaultArrayValueHelper(ArrayProperty, DefaultDataPtr);

	for (int32 ArrayValueIndex = StartIndex; ArrayValueIndex < ArrayValueHelper.Num(); ++ArrayValueIndex)
	{
		const int32 DefaultArrayValueIndex = ArrayValueIndex - StartIndex;
		if (DefaultArrayValueIndex < DefaultArrayValueHelper.Num())
		{
			const uint8* ArrayPropertyValue = ArrayValueHelper.GetRawPtr(ArrayValueIndex);
			const uint8* DefaultArrayPropertyValue = DefaultArrayValueHelper.GetRawPtr(DefaultArrayValueIndex);

			if (FStructProperty* InnerStructProperty = CastField<FStructProperty>(ArrayProperty->Inner))
			{
				// Create a new node for the item value at this index.
				*CurrentArrayNodePtr = new FCustomPropertyListNode(ArrayProperty, ArrayValueIndex);
				CustomPropertyListForPostConstruction.Add(*CurrentArrayNodePtr);

				// Recursively gather up all struct fields that differ and assign to the array item value node's sub property list.
				if (BuildCustomPropertyListForPostConstruction((*CurrentArrayNodePtr)->SubPropertyList, InnerStructProperty->Struct, ArrayPropertyValue, DefaultArrayPropertyValue))
				{
					// Advance to the next node in the list.
					CurrentArrayNodePtr = &(*CurrentArrayNodePtr)->PropertyListNext;
				}
				else
				{
					// Remove the node for the struct property since it does not differ from the native CDO.
					CustomPropertyListForPostConstruction.RemoveAt(CustomPropertyListForPostConstruction.Num() - 1);

					// Clear the current array item node ptr
					*CurrentArrayNodePtr = nullptr;
				}
			}
			else if (FArrayProperty* InnerArrayProperty = CastField<FArrayProperty>(ArrayProperty->Inner))
			{
				// Create a new node for the item value at this index.
				*CurrentArrayNodePtr = new FCustomPropertyListNode(ArrayProperty, ArrayValueIndex);
				CustomPropertyListForPostConstruction.Add(*CurrentArrayNodePtr);

				// Recursively gather up all array item indices that differ and assign to the array item value node's sub property list.
				if (BuildCustomArrayPropertyListForPostConstruction(InnerArrayProperty, (*CurrentArrayNodePtr)->SubPropertyList, ArrayPropertyValue, DefaultArrayPropertyValue))
				{
					// Advance to the next node in the list.
					CurrentArrayNodePtr = &(*CurrentArrayNodePtr)->PropertyListNext;
				}
				else
				{
					// Remove the node for the array property since it does not differ from the native CDO.
					CustomPropertyListForPostConstruction.RemoveAt(CustomPropertyListForPostConstruction.Num() - 1);

					// Clear the current array item node ptr
					*CurrentArrayNodePtr = nullptr;
				}
			}
			else if (!ArrayProperty->Inner->Identical(ArrayPropertyValue, DefaultArrayPropertyValue))
			{
				// Create a new node, link it into the chain and add it into the array.
				*CurrentArrayNodePtr = new FCustomPropertyListNode(ArrayProperty, ArrayValueIndex);
				CustomPropertyListForPostConstruction.Add(*CurrentArrayNodePtr);

				// Advance to the next array item node ptr.
				CurrentArrayNodePtr = &(*CurrentArrayNodePtr)->PropertyListNext;
			}
		}
		else
		{
			// Create a temp default array as a placeholder to compare against the remaining elements in the value.
			FScriptArray TempDefaultArray;
			const int32 Count = ArrayValueHelper.Num() - DefaultArrayValueHelper.Num();
			TempDefaultArray.Add(Count, ArrayProperty->Inner->ElementSize, ArrayProperty->Inner->GetMinAlignment());
			uint8 *Dest = (uint8*)TempDefaultArray.GetData();
			if (ArrayProperty->Inner->PropertyFlags & CPF_ZeroConstructor)
			{
				FMemory::Memzero(Dest, Count * ArrayProperty->Inner->ElementSize);
			}
			else
			{
				for (int32 i = 0; i < Count; i++, Dest += ArrayProperty->Inner->ElementSize)
				{
					ArrayProperty->Inner->InitializeValue(Dest);
				}
			}

			// Recursively fill out the property list for the remainder of the elements in the value that extend beyond the size of the default value.
			BuildCustomArrayPropertyListForPostConstruction(ArrayProperty, *CurrentArrayNodePtr, DataPtr, (uint8*)&TempDefaultArray, ArrayValueIndex);

			// Don't need to record anything else.
			break;
		}
	}

	// Return true if the above found at least one array element that differs from the native CDO, or otherwise if the array sizes are different.
	return (InPropertyList != nullptr || ArrayValueHelper.Num() != DefaultArrayValueHelper.Num());
}

void UBlueprintGeneratedClass::UpdateCustomPropertyListForPostConstruction()
{
	// Empty the current list.
	CustomPropertyListForPostConstruction.Reset();
	bCustomPropertyListForPostConstructionInitialized = false;

	// Find the first native antecedent. All non-native decendant properties are attached to the PostConstructLink chain (see UStruct::Link), so we only need to worry about properties owned by native super classes here.
	UClass* SuperClass = GetSuperClass();
	while (SuperClass && !SuperClass->HasAnyClassFlags(CLASS_Native | CLASS_Intrinsic))
	{
		SuperClass = SuperClass->GetSuperClass();
	}

	if (SuperClass)
	{
		check(ClassDefaultObject != nullptr);

		// Recursively gather native class-owned property values that differ from defaults.
		FCustomPropertyListNode* PropertyList = nullptr;
		BuildCustomPropertyListForPostConstruction(PropertyList, SuperClass, (uint8*)ClassDefaultObject, (uint8*)SuperClass->GetDefaultObject(false));
	}

	bCustomPropertyListForPostConstructionInitialized = true;
}

void UBlueprintGeneratedClass::SetupObjectInitializer(FObjectInitializer& ObjectInitializer) const
{
	for (const FBPComponentClassOverride& Override : ComponentClassOverrides)
	{
		ObjectInitializer.SetDefaultSubobjectClass(Override.ComponentName, Override.ComponentClass);
	}

	GetSuperClass()->SetupObjectInitializer(ObjectInitializer);
}

void UBlueprintGeneratedClass::InitPropertiesFromCustomList(uint8* DataPtr, const uint8* DefaultDataPtr)
{
	FScopeLock SerializeAndPostLoadLock(&SerializeAndPostLoadCritical);
	checkf(bCustomPropertyListForPostConstructionInitialized, TEXT("Custom Property List Not Initialized for %s"), *GetPathNameSafe(this)); // Something went wrong, probably a race condition

	if (const FCustomPropertyListNode* CustomPropertyList = GetCustomPropertyListForPostConstruction())
	{
		InitPropertiesFromCustomList(CustomPropertyList, this, DataPtr, DefaultDataPtr);
	}
}

void UBlueprintGeneratedClass::InitPropertiesFromCustomList(const FCustomPropertyListNode* InPropertyList, UStruct* InStruct, uint8* DataPtr, const uint8* DefaultDataPtr)
{
	for (const FCustomPropertyListNode* CustomPropertyListNode = InPropertyList; CustomPropertyListNode; CustomPropertyListNode = CustomPropertyListNode->PropertyListNext)
	{
		uint8* PropertyValue = CustomPropertyListNode->Property->ContainerPtrToValuePtr<uint8>(DataPtr, CustomPropertyListNode->ArrayIndex);
		const uint8* DefaultPropertyValue = CustomPropertyListNode->Property->ContainerPtrToValuePtr<uint8>(DefaultDataPtr, CustomPropertyListNode->ArrayIndex);

		if (!InitPropertyFromSubPropertyList(CustomPropertyListNode->Property, CustomPropertyListNode->SubPropertyList, PropertyValue, DefaultPropertyValue))
		{
			// Unable to init properties from sub custom property list, fall back to the default copy value behavior
			CustomPropertyListNode->Property->CopySingleValue(PropertyValue, DefaultPropertyValue);
		}
	}
}

void UBlueprintGeneratedClass::InitArrayPropertyFromCustomList(const FArrayProperty* ArrayProperty, const FCustomPropertyListNode* InPropertyList, uint8* DataPtr, const uint8* DefaultDataPtr)
{
	FScriptArrayHelper DstArrayValueHelper(ArrayProperty, DataPtr);
	FScriptArrayHelper SrcArrayValueHelper(ArrayProperty, DefaultDataPtr);

	const int32 SrcNum = SrcArrayValueHelper.Num();
	const int32 DstNum = DstArrayValueHelper.Num();

	if (SrcNum > DstNum)
	{
		DstArrayValueHelper.AddValues(SrcNum - DstNum);
	}
	else if (SrcNum < DstNum)
	{
		DstArrayValueHelper.RemoveValues(SrcNum, DstNum - SrcNum);
	}

	for (const FCustomPropertyListNode* CustomArrayPropertyListNode = InPropertyList; CustomArrayPropertyListNode; CustomArrayPropertyListNode = CustomArrayPropertyListNode->PropertyListNext)
	{
		int32 ArrayIndex = CustomArrayPropertyListNode->ArrayIndex;

		uint8* DstArrayItemValue = DstArrayValueHelper.GetRawPtr(ArrayIndex);
		const uint8* SrcArrayItemValue = SrcArrayValueHelper.GetRawPtr(ArrayIndex);

		if (DstArrayItemValue == nullptr && SrcArrayItemValue == nullptr)
		{
			continue;
		}

		if (!InitPropertyFromSubPropertyList(ArrayProperty->Inner, CustomArrayPropertyListNode->SubPropertyList, DstArrayItemValue, SrcArrayItemValue))
		{
			// Unable to init properties from sub custom property list, fall back to the default copy value behavior
			ArrayProperty->Inner->CopyCompleteValue(DstArrayItemValue, SrcArrayItemValue);
		}
	}
}

bool UBlueprintGeneratedClass::InitPropertyFromSubPropertyList(const FProperty* Property, const FCustomPropertyListNode* SubPropertyList, uint8* PropertyValue, const uint8* DefaultPropertyValue)
{
	if (SubPropertyList != nullptr)
	{
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			InitPropertiesFromCustomList(SubPropertyList, StructProperty->Struct, PropertyValue, DefaultPropertyValue);
			return true;
		}
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			InitArrayPropertyFromCustomList(ArrayProperty, SubPropertyList, PropertyValue, DefaultPropertyValue);
			return true;
		}
		// No need to handle Sets and Maps as they are not optimized through the custom property list
	}
	return false;
}

bool UBlueprintGeneratedClass::IsFunctionImplementedInScript(FName InFunctionName) const
{
	UFunction* Function = FindFunctionByName(InFunctionName);
	return Function && Function->GetOuter() && Function->GetOuter()->IsA(UBlueprintGeneratedClass::StaticClass());
}

UInheritableComponentHandler* UBlueprintGeneratedClass::GetInheritableComponentHandler(const bool bCreateIfNecessary)
{
	static const FBoolConfigValueHelper EnableInheritableComponents(TEXT("Kismet"), TEXT("bEnableInheritableComponents"), GEngineIni);
	if (!EnableInheritableComponents)
	{
		return nullptr;
	}
	
	if (InheritableComponentHandler)
	{
		if (!GEventDrivenLoaderEnabled || !EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
		{
			// This preload will not succeed in EDL
			InheritableComponentHandler->PreloadAll();
		}	
	}

	if (!InheritableComponentHandler && bCreateIfNecessary)
	{
		InheritableComponentHandler = NewObject<UInheritableComponentHandler>(this, FName(TEXT("InheritableComponentHandler")));
	}

	return InheritableComponentHandler;
}


UObject* UBlueprintGeneratedClass::FindArchetype(const UClass* ArchetypeClass, const FName ArchetypeName) const
{
	UObject* Archetype = nullptr;

	// There are some rogue LevelScriptActors that still have a SimpleConstructionScript
	// and since preloading the SCS of a script in a world package is bad news, we need to filter them out
	if (SimpleConstructionScript && !IsChildOf<ALevelScriptActor>())
	{
#if WITH_EDITORONLY_DATA
		// On load, we may fix up AddComponent node templates to conform to the newer archetype naming convention. In that case, we use a map to find
		// the new template name in order to redirect to the appropriate archetype.
		const UBlueprint* Blueprint = Cast<const UBlueprint>(ClassGeneratedBy);
		const FName NewArchetypeName = Blueprint ? Blueprint->OldToNewComponentTemplateNames.FindRef(ArchetypeName) : NAME_None;
#endif
		// Component templates (archetypes) differ from the component class default object, and they are considered to be "default subobjects" owned
		// by the Blueprint Class instance. Also, unlike "default subobjects" on the native C++ side, component templates are not currently owned by the
		// Blueprint Class default object. Instead, they are owned by the Blueprint Class itself. And, just as native C++ default subobjects serve as the
		// "archetype" object for components instanced and outered to a native Actor class instance at construction time, Blueprint Component templates
		// also serve as the "archetype" object for components instanced and outered to a Blueprint Class instance at construction time. However, since
		// Blueprint Component templates are not owned by the Blueprint Class default object, we must search for them by name within the Blueprint Class.
		//
		// Native component subobjects are instanced using the same name as the default subobject (archetype). Thus, it's easy to find the archetype -
		// we just look for an object with the same name that's owned by (i.e. outered to) the Actor class default object. This is the default logic
		// that we're overriding here.
		//
		// Blueprint (non-native) component templates are split between SCS (SimpleConstructionScript) and AddComponent nodes in Blueprint function
		// graphs (e.g. ConstructionScript). Both templates use a unique naming convention within the scope of the Blueprint Class, but at construction
		// time, we choose a unique name that differs from the archetype name for each component instance. We do this partially to support nativization,
		// in which we need to explicitly guard against recycling objects at allocation time. For SCS component instances, the name we choose matches the
		// "variable" name that's also user-facing. Thus, when we search for archetypes, we do so using the SCS variable name, and not the archetype name.
		// Conversely, for AddComponent node-spawned instances, we do not have a user-facing variable name, so instead we choose a unique name that
		// incorporates the archetype name, but we append an index as well. The index is needed to support multiple invocations of the same AddComponent
		// node in a function graph, which can occur when the AddComponent node is wired to a flow-control node such as a ForEach loop, for example. Thus,
		// we still look for the archetype by name, but we must first ensure that the instance name is converted to its "base" name by removing the index.
#if WITH_EDITORONLY_DATA
		const FName ArchetypeBaseName = (NewArchetypeName != NAME_None) ? NewArchetypeName : FName(ArchetypeName, 0);
#else
		const FName ArchetypeBaseName = FName(ArchetypeName, 0);
#endif
		UBlueprintGeneratedClass* Class = const_cast<UBlueprintGeneratedClass*>(this);
		while (Class)
		{
			USimpleConstructionScript* ClassSCS = Class->SimpleConstructionScript;
			USCS_Node* SCSNode = nullptr;
			if (ClassSCS)
			{
				if (ClassSCS->HasAnyFlags(RF_NeedLoad))
				{
					ClassSCS->PreloadChain();
				}

				// We keep the index name here rather than the base name, in order to avoid potential
				// collisions between an SCS variable name and an existing AddComponent node template.
				// This is because old AddComponent node templates were based on the class display name.
				SCSNode = ClassSCS->FindSCSNode(ArchetypeName);
			}

			if (SCSNode)
			{
				// Ensure that the stored template is of the same type as the serialized object. Since
				// we match these by name, this handles the case where the Blueprint class was updated
				// after having previously serialized an instanced into another package (e.g. map). In
				// that case, the Blueprint class might contain an SCS node with the same name as the
				// previously-serialized object, but it might also have been switched to a different type.
				if (SCSNode->ComponentTemplate)
				{
					if (SCSNode->ComponentTemplate->IsA(ArchetypeClass))
					{
						Archetype = SCSNode->ComponentTemplate;
					}

					// If the component class is in the process of being reinstanced, archetype lookup
					// will fail the class check because the SCS template will already have been replaced
					// when instances try to look up their archetype, so return the already reinstanced one
					else if (ArchetypeClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
					{
						const FString ArchetypeClassName = ArchetypeClass->GetName();
						if (ArchetypeClassName.StartsWith("REINST_") && FStringView(ArchetypeClassName).RightChop(7).StartsWith(SCSNode->ComponentTemplate->GetClass()->GetName()))
						{
							Archetype = SCSNode->ComponentTemplate;
						}
					}
				}
			}
			else if (UInheritableComponentHandler* ICH = Class->GetInheritableComponentHandler())
			{
				if (GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
				{
					if (ICH->HasAnyFlags(RF_NeedLoad))
					{
						UE_LOG(LogClass, Fatal, TEXT("%s had RF_NeedLoad when searching for an archetype of %s named %s"), *GetFullNameSafe(ICH), *GetFullNameSafe(ArchetypeClass), *ArchetypeName.ToString());
					}
				}
				// This would find either an SCS component template override (for which the archetype
				// name will match the SCS variable name), or an old AddComponent node template override
				// (for which the archetype name will match the override record's component template name).
				FComponentKey ComponentKey = ICH->FindKey(ArchetypeName);
				if (!ComponentKey.IsValid() && ArchetypeName != ArchetypeBaseName)
				{
					// We didn't find either an SCS override or an old AddComponent template override,
					// so now we look for a match with the base name; this would apply to new AddComponent
					// node template overrides, which use the base name (non-index form).
					ComponentKey = ICH->FindKey(ArchetypeBaseName);

					// If we found a match with an SCS key instead, treat this as a collision and throw it
					// out, because it should have already been found in the first search. This could happen
					// if an old AddComponent node template's base name collides with an SCS variable name.
					if (ComponentKey.IsValid() && ComponentKey.IsSCSKey())
					{
						ComponentKey = FComponentKey();
					}
				}

				// Avoid searching for an invalid key.
				if (ComponentKey.IsValid())
				{
					Archetype = ICH->GetOverridenComponentTemplate(ComponentKey);

					if (GEventDrivenLoaderEnabled && EVENT_DRIVEN_ASYNC_LOAD_ACTIVE_AT_RUNTIME)
					{
						if (Archetype && Archetype->HasAnyFlags(RF_NeedLoad))
						{
							UE_LOG(LogClass, Fatal, TEXT("%s had RF_NeedLoad when searching for an archetype of %s named %s"), *GetFullNameSafe(Archetype), *GetFullNameSafe(ArchetypeClass), *ArchetypeName.ToString());
						}
					}
				}
			}

			if (Archetype == nullptr)
			{
				// We'll get here if we failed to find the archetype in either the SCS or the ICH. In that case,
				// we first check the base name case. If that fails, then we may be looking for something other
				// than an AddComponent template. In that case, we check for an object that shares the instance name.
				Archetype = static_cast<UObject*>(FindObjectWithOuter(Class, ArchetypeClass, ArchetypeBaseName));
				if (Archetype == nullptr && ArchetypeName != ArchetypeBaseName)
				{
					Archetype = static_cast<UObject*>(FindObjectWithOuter(Class, ArchetypeClass, ArchetypeName));
				}

				// Walk up the class hierarchy until we either find a match or hit a native class.
				Class = (Archetype ? nullptr : Cast<UBlueprintGeneratedClass>(Class->GetSuperClass()));
			}
			else
			{
				Class = nullptr;
			}
		}
	}

	return Archetype;
}

UDynamicBlueprintBinding* UBlueprintGeneratedClass::GetDynamicBindingObject(const UClass* ThisClass, UClass* BindingClass)
{
	check(ThisClass);
	UDynamicBlueprintBinding* DynamicBlueprintBinding = nullptr;
	if (const UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ThisClass))
	{
		for (UDynamicBlueprintBinding* DynamicBindingObject : BPGC->DynamicBindingObjects)
		{
			if (DynamicBindingObject && (DynamicBindingObject->GetClass() == BindingClass))
			{
				DynamicBlueprintBinding = DynamicBindingObject;
				break;
			}
		}
	}

	return DynamicBlueprintBinding;
}

void UBlueprintGeneratedClass::BindDynamicDelegates(const UClass* ThisClass, UObject* InInstance)
{
	check(ThisClass && InInstance);
	if (!InInstance->IsA(ThisClass))
	{
		UE_LOG(LogBlueprint, Warning, TEXT("BindComponentDelegates: '%s' is not an instance of '%s'."), *InInstance->GetName(), *ThisClass->GetName());
		return;
	}

	if (const UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ThisClass))
	{
		for (UDynamicBlueprintBinding* DynamicBindingObject : BPGC->DynamicBindingObjects)
		{
			if (ensure(DynamicBindingObject))
			{
				DynamicBindingObject->BindDynamicDelegates(InInstance);
			}
		}
	}

	if (UClass* TheSuperClass = ThisClass->GetSuperClass())
	{
		BindDynamicDelegates(TheSuperClass, InInstance);
	}
}

#if WITH_EDITOR
void UBlueprintGeneratedClass::UnbindDynamicDelegates(const UClass* ThisClass, UObject* InInstance)
{
	check(ThisClass && InInstance);
	if (!InInstance->IsA(ThisClass))
	{
		UE_LOG(LogBlueprint, Warning, TEXT("UnbindDynamicDelegates: '%s' is not an instance of '%s'."), *InInstance->GetName(), *ThisClass->GetName());
		return;
	}

	if (const UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(ThisClass))
	{
		for (UDynamicBlueprintBinding* DynamicBindingObject : BPGC->DynamicBindingObjects)
		{
			if (ensure(DynamicBindingObject))
			{
				DynamicBindingObject->UnbindDynamicDelegates(InInstance);
			}
		}
	}

	if (UClass* TheSuperClass = ThisClass->GetSuperClass())
	{
		UnbindDynamicDelegates(TheSuperClass, InInstance);
	}
}

void UBlueprintGeneratedClass::UnbindDynamicDelegatesForProperty(UObject* InInstance, const FObjectProperty* InObjectProperty)
{
	for (int32 Index = 0; Index < DynamicBindingObjects.Num(); ++Index)
	{
		if ( ensure(DynamicBindingObjects[Index] != NULL) )
		{
			DynamicBindingObjects[Index]->UnbindDynamicDelegatesForProperty(InInstance, InObjectProperty);
		}
	}
}
#endif

bool UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(const UClass* InClass, TArray<const UBlueprintGeneratedClass*>& OutBPGClasses)
{
	OutBPGClasses.Empty();
	bool bNoErrors = true;
	while(const UBlueprintGeneratedClass* BPGClass = Cast<const UBlueprintGeneratedClass>(InClass))
	{
#if WITH_EDITORONLY_DATA
		// A cooked class has already been validated and will not have a source Blueprint asset.
		if(!BPGClass->bCooked)
		{
			const UBlueprint* BP = Cast<const UBlueprint>(BPGClass->ClassGeneratedBy);
			bNoErrors &= (NULL != BP) && (BP->Status != BS_Error);
		}
#endif
		OutBPGClasses.Add(BPGClass);
		InClass = BPGClass->GetSuperClass();
	}
	return bNoErrors;
}

UActorComponent* UBlueprintGeneratedClass::FindComponentTemplateByName(const FName& TemplateName) const
{
	for(int32 i = 0; i < ComponentTemplates.Num(); i++)
	{
		UActorComponent* Template = ComponentTemplates[i];
		if(Template != NULL && Template->GetFName() == TemplateName)
		{
			return Template;
		}
	}

	return NULL;
}

void UBlueprintGeneratedClass::CreateTimelineComponent(AActor* Actor, const UTimelineTemplate* TimelineTemplate)
{
	if (!IsValid(Actor)
		|| !TimelineTemplate
		|| Actor->IsTemplate())
	{
		return;
	}

	FName NewName = TimelineTemplate->GetVariableName();
	UTimelineComponent* NewTimeline = NewObject<UTimelineComponent>(Actor, NewName);
	NewTimeline->CreationMethod = EComponentCreationMethod::UserConstructionScript; // Indicate it comes from a blueprint so it gets cleared when we rerun construction scripts
	Actor->BlueprintCreatedComponents.Add(NewTimeline); // Add to array so it gets saved
	NewTimeline->SetNetAddressable();	// This component has a stable name that can be referenced for replication

	NewTimeline->SetPropertySetObject(Actor); // Set which object the timeline should drive properties on
	NewTimeline->SetDirectionPropertyName(TimelineTemplate->GetDirectionPropertyName());

	NewTimeline->SetTimelineLength(TimelineTemplate->TimelineLength); // copy length
	NewTimeline->SetTimelineLengthMode(TimelineTemplate->LengthMode);

	NewTimeline->PrimaryComponentTick.TickGroup = TimelineTemplate->TimelineTickGroup;

	// Find property with the same name as the template and assign the new Timeline to it
	UClass* ActorClass = Actor->GetClass();
	FObjectPropertyBase* Prop = FindFProperty<FObjectPropertyBase>(ActorClass, TimelineTemplate->GetVariableName());
	if (Prop)
	{
		Prop->SetObjectPropertyValue_InContainer(Actor, NewTimeline);
	}

	// Event tracks
	// In the template there is a track for each function, but in the runtime Timeline each key has its own delegate, so we fold them together
	for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->EventTracks.Num(); TrackIdx++)
	{
		const FTTEventTrack* EventTrackTemplate = &TimelineTemplate->EventTracks[TrackIdx];
		if (EventTrackTemplate->CurveKeys != nullptr)
		{
			// Create delegate for all keys in this track
			FScriptDelegate EventDelegate;
			EventDelegate.BindUFunction(Actor, EventTrackTemplate->GetFunctionName());

			// Create an entry in Events for each key of this track
			for (auto It(EventTrackTemplate->CurveKeys->FloatCurve.GetKeyIterator()); It; ++It)
			{
				NewTimeline->AddEvent(It->Time, FOnTimelineEvent(EventDelegate));
			}
		}
	}

	// Float tracks
	for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->FloatTracks.Num(); TrackIdx++)
	{
		const FTTFloatTrack* FloatTrackTemplate = &TimelineTemplate->FloatTracks[TrackIdx];
		if (FloatTrackTemplate->CurveFloat != NULL)
		{
			NewTimeline->AddInterpFloat(FloatTrackTemplate->CurveFloat, FOnTimelineFloat(), FloatTrackTemplate->GetPropertyName(), FloatTrackTemplate->GetTrackName());
		}
	}

	// Vector tracks
	for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->VectorTracks.Num(); TrackIdx++)
	{
		const FTTVectorTrack* VectorTrackTemplate = &TimelineTemplate->VectorTracks[TrackIdx];
		if (VectorTrackTemplate->CurveVector != NULL)
		{
			NewTimeline->AddInterpVector(VectorTrackTemplate->CurveVector, FOnTimelineVector(), VectorTrackTemplate->GetPropertyName(), VectorTrackTemplate->GetTrackName());
		}
	}

	// Linear color tracks
	for (int32 TrackIdx = 0; TrackIdx < TimelineTemplate->LinearColorTracks.Num(); TrackIdx++)
	{
		const FTTLinearColorTrack* LinearColorTrackTemplate = &TimelineTemplate->LinearColorTracks[TrackIdx];
		if (LinearColorTrackTemplate->CurveLinearColor != NULL)
		{
			NewTimeline->AddInterpLinearColor(LinearColorTrackTemplate->CurveLinearColor, FOnTimelineLinearColor(), LinearColorTrackTemplate->GetPropertyName(), LinearColorTrackTemplate->GetTrackName());
		}
	}

	// Set up delegate that gets called after all properties are updated
	FScriptDelegate UpdateDelegate;
	UpdateDelegate.BindUFunction(Actor, TimelineTemplate->GetUpdateFunctionName());
	NewTimeline->SetTimelinePostUpdateFunc(FOnTimelineEvent(UpdateDelegate));

	// Set up finished delegate that gets called after all properties are updated
	FScriptDelegate FinishedDelegate;
	FinishedDelegate.BindUFunction(Actor, TimelineTemplate->GetFinishedFunctionName());
	NewTimeline->SetTimelineFinishedFunc(FOnTimelineEvent(FinishedDelegate));

	NewTimeline->RegisterComponent();

	// Start playing now, if desired
	if (TimelineTemplate->bAutoPlay)
	{
		// Needed for autoplay timelines in cooked builds, since they won't have Activate() called via the Play call below
		NewTimeline->bAutoActivate = true;
		NewTimeline->Play();
	}

	// Set to loop, if desired
	if (TimelineTemplate->bLoop)
	{
		NewTimeline->SetLooping(true);
	}

	// Set replication, if desired
	if (TimelineTemplate->bReplicated)
	{
		NewTimeline->SetIsReplicated(true);
	}

	// Set replication, if desired
	if (TimelineTemplate->bIgnoreTimeDilation)
	{
		NewTimeline->SetIgnoreTimeDilation(true);
	}
}

void UBlueprintGeneratedClass::CreateComponentsForActor(const UClass* ThisClass, AActor* Actor)
{
	check(ThisClass && Actor);
	if (Actor->IsTemplate() || !IsValid(Actor))
	{
		return;
	}

	if (const UBlueprintGeneratedClass* BPGC = Cast<const UBlueprintGeneratedClass>(ThisClass))
	{
		for (UTimelineTemplate* TimelineTemplate : BPGC->Timelines)
		{
			// Not fatal if NULL, but shouldn't happen and ignored if not wired up in graph
			if (TimelineTemplate)
			{
				CreateTimelineComponent(Actor, TimelineTemplate);
			}
		}
	}
}

bool UBlueprintGeneratedClass::UseFastPathComponentInstancing()
{
	return bHasCookedComponentInstancingData && FPlatformProperties::RequiresCookedData() && !GBlueprintComponentInstancingFastPathDisabled;
}

uint8* UBlueprintGeneratedClass::GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const
{
	if (Obj && UsePersistentUberGraphFrame() && UberGraphFramePointerProperty && UberGraphFunction)
	{
		if (UberGraphFunction == FuncToCheck)
		{
			FPointerToUberGraphFrame* PointerToUberGraphFrame = UberGraphFramePointerProperty->ContainerPtrToValuePtr<FPointerToUberGraphFrame>(Obj);
			checkSlow(PointerToUberGraphFrame);
			ensure(PointerToUberGraphFrame->RawPointer);
			return PointerToUberGraphFrame->RawPointer;
		}
	}
	UClass* ParentClass = GetSuperClass();
	checkSlow(ParentClass);
	return ParentClass->GetPersistentUberGraphFrame(Obj, FuncToCheck);
}

void UBlueprintGeneratedClass::CreatePersistentUberGraphFrame(UObject* Obj, bool bCreateOnlyIfEmpty, bool bSkipSuperClass, UClass* OldClass) const
{
#if USE_UBER_GRAPH_PERSISTENT_FRAME
#if WITH_EDITORONLY_DATA
	/** Macros should not create uber graph frames as they have no uber graph. If UBlueprints are cooked out the macro class probably does not exist as well */
	UBlueprint* Blueprint = Cast<UBlueprint>(ClassGeneratedBy);
	if (Blueprint && Blueprint->BlueprintType == BPTYPE_MacroLibrary)
	{
		return;
	}
#endif

	ensure(!UberGraphFramePointerProperty == !UberGraphFunction);
	if (Obj && UsePersistentUberGraphFrame() && UberGraphFramePointerProperty && UberGraphFunction)
	{
		FPointerToUberGraphFrame* PointerToUberGraphFrame = UberGraphFramePointerProperty->ContainerPtrToValuePtr<FPointerToUberGraphFrame>(Obj);
		check(PointerToUberGraphFrame);

		if ( !ensureMsgf(bCreateOnlyIfEmpty || !PointerToUberGraphFrame->RawPointer
			, TEXT("Attempting to recreate an object's UberGraphFrame when the previous one was not properly destroyed (transitioning '%s' from '%s' to '%s'). We'll attempt to free the frame memory, but cannot clean up its properties (this may result in leaks and undesired side effects).")
			, *Obj->GetPathName()
			, (OldClass == nullptr) ? TEXT("<NULL>") : *OldClass->GetName()
			, *GetName()) )
		{
			FMemory::Free(PointerToUberGraphFrame->RawPointer);
			PointerToUberGraphFrame->RawPointer = nullptr;
		}
		
		if (!PointerToUberGraphFrame->RawPointer)
		{
			uint8* FrameMemory = NULL;
			const bool bUberGraphFunctionIsReady = UberGraphFunction->HasAllFlags(RF_LoadCompleted); // is fully loaded
			if (bUberGraphFunctionIsReady)
			{
				INC_MEMORY_STAT_BY(STAT_PersistentUberGraphFrameMemory, UberGraphFunction->GetStructureSize());
				FrameMemory = (uint8*)FMemory::Malloc(UberGraphFunction->GetStructureSize(), UberGraphFunction->GetMinAlignment());

				FMemory::Memzero(FrameMemory, UberGraphFunction->GetStructureSize());
				for (FProperty* Property = UberGraphFunction->PropertyLink; Property; Property = Property->PropertyLinkNext)
				{
					Property->InitializeValue_InContainer(FrameMemory);
				}
			}
			else
			{
				UE_LOG(LogBlueprint, Verbose, TEXT("Function '%s' is not ready to create frame for '%s'"),
					*GetPathNameSafe(UberGraphFunction), *GetPathNameSafe(Obj));
			}
			PointerToUberGraphFrame->RawPointer = FrameMemory;
#if VALIDATE_UBER_GRAPH_PERSISTENT_FRAME
			PointerToUberGraphFrame->UberGraphFunctionKey = UberGraphFunctionKey;
#endif//VALIDATE_UBER_GRAPH_PERSISTENT_FRAME
		}
	}

	if (!bSkipSuperClass)
	{
		UClass* ParentClass = GetSuperClass();
		checkSlow(ParentClass);
		ParentClass->CreatePersistentUberGraphFrame(Obj, bCreateOnlyIfEmpty);
	}
#endif // USE_UBER_GRAPH_PERSISTENT_FRAME
}

void UBlueprintGeneratedClass::DestroyPersistentUberGraphFrame(UObject* Obj, bool bSkipSuperClass) const
{
#if USE_UBER_GRAPH_PERSISTENT_FRAME
	ensure(!UberGraphFramePointerProperty == !UberGraphFunction);
	if (Obj && UsePersistentUberGraphFrame() && UberGraphFramePointerProperty && UberGraphFunction)
	{
		FPointerToUberGraphFrame* PointerToUberGraphFrame = UberGraphFramePointerProperty->ContainerPtrToValuePtr<FPointerToUberGraphFrame>(Obj);
		checkSlow(PointerToUberGraphFrame);
		uint8* FrameMemory = PointerToUberGraphFrame->RawPointer;
		PointerToUberGraphFrame->RawPointer = NULL;
		if (FrameMemory)
		{
			for (FProperty* Property = UberGraphFunction->PropertyLink; Property; Property = Property->PropertyLinkNext)
			{
				Property->DestroyValue_InContainer(FrameMemory);
			}
			FMemory::Free(FrameMemory);
			DEC_MEMORY_STAT_BY(STAT_PersistentUberGraphFrameMemory, UberGraphFunction->GetStructureSize());
		}
		else
		{
			UE_LOG(LogBlueprint, Log, TEXT("Object '%s' had no Uber Graph Persistent Frame"), *GetPathNameSafe(Obj));
		}
	}

	if (!bSkipSuperClass)
	{
		UClass* ParentClass = GetSuperClass();
		checkSlow(ParentClass);
		ParentClass->DestroyPersistentUberGraphFrame(Obj);
	}
#endif // USE_UBER_GRAPH_PERSISTENT_FRAME
}

void UBlueprintGeneratedClass::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	
	// Super handles parent class and fields
	OutDeps.Add(GetSuperClass()->GetDefaultObject());

	if (UberGraphFunction)
	{
		OutDeps.Add(UberGraphFunction);
	}
	
	UObject *CDO = GetDefaultObject();
	if (CDO)
	{
		ForEachObjectWithOuter(CDO, [&OutDeps](UObject* SubObj)
		{
			if (SubObj->HasAllFlags(RF_DefaultSubObject))
			{
				OutDeps.Add(SubObj->GetClass());
				OutDeps.Add(SubObj->GetArchetype());
			}
		});
	}

	if (InheritableComponentHandler)
	{
		OutDeps.Add(InheritableComponentHandler);
	}

	if (SimpleConstructionScript)
	{
		OutDeps.Add(SimpleConstructionScript);
	}
}

void UBlueprintGeneratedClass::GetDefaultObjectPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetDefaultObjectPreloadDependencies(OutDeps);

	// Ensure that BPGC-owned component templates (archetypes) are loaded prior to CDO serialization in order to support the following use cases:
	//
	//	1) When the "fast path" component instancing optimization is enabled, we generate a cached delta binary at BPGC load time that we then deserialize into
	//	   new component instances after we spawn them at runtime. Generating the cached delta requires component templates to be loaded so that we can use them
	//	   as the basis for delta serialization. However, we cannot add them a preload dependency of the class without introducing a cycle, so we add them as a
	//	   preload dependency on the CDO here instead.
	//	2) When Blueprint nativization is enabled, any Blueprint class assets that are not converted to C++ may still inherit from a Blueprint class asset that is
	//	   converted to C++. In that case, the non-nativized child Blueprint class may still inherit one or more SCS nodes from the parent class. However, when
	//	   we nativize a Blueprint class, we convert the class-owned SCS component templates into CDO-owned default subobjects. In the non-nativized child Blueprint
	//	   class, these remain stored in the ICH as override templates. In order to ensure that the inherited default subobject in the CDO reflects the defaults that
	//	   are recorded into the override template, we bake out the list of changed properties at cook time and then use it to also generate a cached delta binary
	//	   when the non-nativized BPGC child asset is loaded in the cooked build. We then use binary serialization to update the default subobject instance (see
	//	   CheckAndApplyComponentTemplateOverrides). That must occur prior to serializing instances of the non-nativized BPGC so that delta serialization works
	//	   correctly, so adding them as preload dependencies here ensures that the override templates will all be loaded prior to serialization of the CDO.

	// Walk up the SCS inheritance hierarchy and add component templates (archetypes). This may include override templates contained in the ICH for inherited SCS nodes.
	UBlueprintGeneratedClass* CurrentBPClass = this;
	while (CurrentBPClass)
	{
		if (CurrentBPClass->SimpleConstructionScript)
		{
			const TArray<USCS_Node*>& AllSCSNodes = CurrentBPClass->SimpleConstructionScript->GetAllNodes();
			for (USCS_Node* SCSNode : AllSCSNodes)
			{
				// An SCS node that's owned by this class must also be considered a preload dependency since we will access its serialized template reference property. Any SCS
				// nodes that are inherited from a parent class will reference templates through the ICH instead, and that's already a preload dependency on the BP class itself.
				if (CurrentBPClass == this)
				{
					OutDeps.Add(SCSNode);
				}

				OutDeps.Add(SCSNode->GetActualComponentTemplate(this));
			}
		}

		CurrentBPClass = Cast<UBlueprintGeneratedClass>(CurrentBPClass->GetSuperClass());
	}

	// Also add UCS/AddComponent node templates (archetypes).
	for (UActorComponent* ComponentTemplate : ComponentTemplates)
	{
		if (ComponentTemplate)
		{
			OutDeps.Add(ComponentTemplate);
		}
	}

	if (GetAllowNativeComponentClassOverrides())
	{
		// Add the classes that will be used for overriding components defined in base classes
		for (const FBPComponentClassOverride& Override : ComponentClassOverrides)
		{
			if (Override.ComponentClass)
			{
				OutDeps.Add(const_cast<UClass*>(Override.ComponentClass.Get()));
			}
		}
	}

	// Add objects related to the sparse class data struct
	if (SparseClassDataStruct)
	{
		// Add the super classes CDO, as it needs to be in place to properly serialize the sparse class data's archetype
		OutDeps.Add(GetSuperClass()->GetDefaultObject());
	
		// Add the sparse class data struct, as it is serialized as part of the CDO
		OutDeps.Add(SparseClassDataStruct);

		// Also add the sparse class data archetype as it will be copied into any newly created sparse class data 
		// pre-serialization
		if(UScriptStruct* SparseClassDataArchetype = GetSparseClassDataArchetypeStruct())
		{
			OutDeps.Add(SparseClassDataArchetype);
		}

		// Add anything that the sparse class data also depends on
		const void* SparseClassDataToUse = GetSparseClassData(EGetSparseClassDataMethod::ArchetypeIfNull);
		if (UScriptStruct::ICppStructOps* CppStructOps = SparseClassDataStruct->GetCppStructOps())
		{
			CppStructOps->GetPreloadDependencies(const_cast<void*>(SparseClassDataToUse), OutDeps);
		}
		// The iterator will recursively loop through all structs in structs/containers too.
		for (TPropertyValueIterator<FStructProperty> It(SparseClassDataStruct, SparseClassDataToUse); It; ++It)
		{
			const UScriptStruct* StructType = It.Key()->Struct;
			if (UScriptStruct::ICppStructOps* CppStructOps = StructType->GetCppStructOps())
			{
				void* StructDataPtr = const_cast<void*>(It.Value());
				CppStructOps->GetPreloadDependencies(StructDataPtr, OutDeps);
			}
		}
	}
}

bool UBlueprintGeneratedClass::NeedsLoadForServer() const
{
	// This logic can't be used for targets that use editor content because UBlueprint::NeedsLoadForEditorGame
	// returns true and forces all UBlueprints to be loaded for -game or -server runs. The ideal fix would be
	// to remove UBlueprint::NeedsLoadForEditorGame, after that it would be nice if we could just implement
	// UBlueprint::NeedsLoadForEditorGame here, but we can't because then our CDO doesn't get loaded. We *could*
	// fix that behavior, but instead I'm just abusing IsRunningCommandlet() so that this logic only runs during cook:
	if (IsRunningCommandlet() && !HasAnyFlags(RF_ClassDefaultObject))
	{
		if (ensure(GetSuperClass()) && !GetSuperClass()->NeedsLoadForServer())
		{
			return false;
		}
		if (ensure(ClassDefaultObject) && !ClassDefaultObject->NeedsLoadForServer())
		{
			return false;
		}
	}
	return Super::NeedsLoadForServer();
}

bool UBlueprintGeneratedClass::NeedsLoadForClient() const
{
	// This logic can't be used for targets that use editor content because UBlueprint::NeedsLoadForEditorGame
	// returns true and forces all UBlueprints to be loaded for -game or -server runs. The ideal fix would be
	// to remove UBlueprint::NeedsLoadForEditorGame, after that it would be nice if we could just implement
	// UBlueprint::NeedsLoadForEditorGame here, but we can't because then our CDO doesn't get loaded. We *could*
	// fix that behavior, but instead I'm just abusing IsRunningCommandlet() so that this logic only runs during cook:
	if (IsRunningCommandlet() && !HasAnyFlags(RF_ClassDefaultObject))
	{
		if (ensure(GetSuperClass()) && !GetSuperClass()->NeedsLoadForClient())
		{
			return false;
		}
		if (ensure(ClassDefaultObject) && !ClassDefaultObject->NeedsLoadForClient())
		{
			return false;
		}
	}
	return Super::NeedsLoadForClient();
}

bool UBlueprintGeneratedClass::NeedsLoadForEditorGame() const
{
	return true;
}

bool UBlueprintGeneratedClass::CanBeClusterRoot() const
{
	// Clustering level BPs doesn't work yet
	return GBlueprintClusteringEnabled && !GetOutermost()->ContainsMap();
}

#if WITH_EDITOR
UClass* UBlueprintGeneratedClass::RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO)
{
	if (HasAnyFlags(RF_BeingRegenerated))
	{
		if (ensure(ClassDefaultObject))
		{
			UBlueprint::ForceLoadMembers(this);
			UBlueprint::ForceLoadMembers(ClassDefaultObject);
		}

		if (SimpleConstructionScript)
		{
			FBlueprintEditorUtils::PreloadConstructionScript(SimpleConstructionScript);
		}

		if (InheritableComponentHandler)
		{
			InheritableComponentHandler->PreloadAll();
		}

		// If this is a cooked class, warn about any uncooked classes appearing above it in the inheritance hierarchy.
		if(bCooked)
		{
			const UBlueprintGeneratedClass* CurrentBPGC = this;
			while (const UBlueprintGeneratedClass* ParentBPGC = Cast<const UBlueprintGeneratedClass>(CurrentBPGC->GetSuperClass()))
			{
				if (!ParentBPGC->bCooked)
				{
					UE_LOG(LogBlueprint, Warning, TEXT("%s: found an uncooked class (%s) as an ancestor of a cooked class (%s) in the hierarchy."),
						*this->GetPathName(),
						*ParentBPGC->GetPathName(),
						*CurrentBPGC->GetPathName());
				}

				CurrentBPGC = ParentBPGC;
			}
		}
	}

	return this;
}

void UBlueprintGeneratedClass::PreSaveRoot(FObjectPreSaveRootContext ObjectSaveContext)
{
	Super::PreSaveRoot(ObjectSaveContext);

	auto ShouldCookBlueprintPropertyGuids = [this]() -> bool
	{
		switch (GetDefault<UCookerSettings>()->BlueprintPropertyGuidsCookingMethod)
		{
		case EBlueprintPropertyGuidsCookingMethod::EnabledBlueprintsOnly:
			if (const UBlueprint* BP = Cast<UBlueprint>(ClassGeneratedBy))
			{
				return BP->ShouldCookPropertyGuids();
			}
			break;

		case EBlueprintPropertyGuidsCookingMethod::AllBlueprints:
			return true;

		case EBlueprintPropertyGuidsCookingMethod::Disabled:
		default:
			break;
		}

		return false;
	};

	if (ObjectSaveContext.IsCooking() && ShouldCookBlueprintPropertyGuids())
	{
		CookedPropertyGuids = PropertyGuids;

		if (GetDefault<UCookerSettings>()->BlueprintPropertyGuidsCookingMethod == EBlueprintPropertyGuidsCookingMethod::EnabledBlueprintsOnly)
		{
			// Ensure that we also have the GUIDs from our parent classes available (if they're not cooking their own GUIDs)
			for (UBlueprintGeneratedClass* ParentBPGC = Cast<UBlueprintGeneratedClass>(GetSuperClass()); ParentBPGC; ParentBPGC = Cast<UBlueprintGeneratedClass>(ParentBPGC->GetSuperClass()))
			{
				const UBlueprint* ParentBP = Cast<UBlueprint>(ParentBPGC->ClassGeneratedBy);
				if (!ParentBP || ParentBP->ShouldCookPropertyGuids())
				{
					break;
				}
				CookedPropertyGuids.Append(ParentBPGC->PropertyGuids);
			}
		}
	}
	else
	{
		CookedPropertyGuids.Reset();
	}

	if (ObjectSaveContext.IsCooking() && (ObjectSaveContext.GetSaveFlags() & SAVE_Optional))
	{
		UClassCookedMetaData* CookedMetaData = NewCookedMetaData();
		CookedMetaData->CacheMetaData(this);

		if (!CookedMetaData->HasMetaData())
		{
			PurgeCookedMetaData();
		}
	}
	else
	{
		PurgeCookedMetaData();
	}
}

void UBlueprintGeneratedClass::PostSaveRoot(FObjectPostSaveRootContext ObjectSaveContext)
{
	Super::PostSaveRoot(ObjectSaveContext);

	CookedPropertyGuids.Reset();

	PurgeCookedMetaData();
}
#endif

bool UBlueprintGeneratedClass::IsAsset() const
{
	// UClass::IsAsset returns false; override that to return true for BlueprintGeneratedClasses,
	// but only if the instance satisfies the regular definition of an asset (RF_Public, not transient, etc)
	// and only if it is the active BPGC matching the Blueprint
	return UObject::IsAsset() && !HasAnyClassFlags(CLASS_NewerVersionExists);
}

void UBlueprintGeneratedClass::Link(FArchive& Ar, bool bRelinkExistingProperties)
{
	Super::Link(Ar, bRelinkExistingProperties);

#if USE_UBER_GRAPH_PERSISTENT_FRAME
	if (UsePersistentUberGraphFrame())
	{
		if (UberGraphFunction)
		{
			Ar.Preload(UberGraphFunction);

			for (FStructProperty* Property : TFieldRange<FStructProperty>(this, EFieldIteratorFlags::ExcludeSuper))
			{
				if (Property->GetFName() == GetUberGraphFrameName())
				{
					UberGraphFramePointerProperty = Property;
					break;
				}
			}
			checkSlow(UberGraphFramePointerProperty);
		}
	}
#endif

	AssembleReferenceTokenStream(true);
}

void UBlueprintGeneratedClass::PurgeClass(bool bRecompilingOnLoad)
{
	Super::PurgeClass(bRecompilingOnLoad);

	UberGraphFramePointerProperty = NULL;
	UberGraphFunction = NULL;
#if VALIDATE_UBER_GRAPH_PERSISTENT_FRAME
	UberGraphFunctionKey = 0;
#endif//VALIDATE_UBER_GRAPH_PERSISTENT_FRAME
#if WITH_EDITORONLY_DATA
	OverridenArchetypeForCDO = NULL;

#if UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
	FastCallPairs_DEPRECATED.Empty();
#endif
	CalledFunctions.Empty();
#endif //WITH_EDITOR
}

void UBlueprintGeneratedClass::Bind()
{
	Super::Bind();

	if (UsePersistentUberGraphFrame() && UberGraphFunction)
	{
		CppClassStaticFunctions.SetAddReferencedObjects(&UBlueprintGeneratedClass::AddReferencedObjectsInUbergraphFrame);
	}
}

void UBlueprintGeneratedClass::AddReferencedObjectsInUbergraphFrame(UObject* InThis, FReferenceCollector& Collector)
{
	checkSlow(InThis);
	for (UClass* CurrentClass = InThis->GetClass(); CurrentClass; CurrentClass = CurrentClass->GetSuperClass())
	{
		if (UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(CurrentClass))
		{
#if USE_UBER_GRAPH_PERSISTENT_FRAME
			if (BPGC->UberGraphFramePointerProperty)
			{
				FPointerToUberGraphFrame* PointerToUberGraphFrame = BPGC->UberGraphFramePointerProperty->ContainerPtrToValuePtr<FPointerToUberGraphFrame>(InThis);
				checkSlow(PointerToUberGraphFrame)
				if (PointerToUberGraphFrame->RawPointer)
				{
#if VALIDATE_UBER_GRAPH_PERSISTENT_FRAME
					ensureMsgf(
						PointerToUberGraphFrame->UberGraphFunctionKey == BPGC->UberGraphFunctionKey,
						TEXT("Detected key mismatch in uber graph frame for instance %s of type %s, iteration will be unsafe"),
						*InThis->GetPathName(),
						*BPGC->GetPathName()
					);
#endif//VALIDATE_UBER_GRAPH_PERSISTENT_FRAME

					{
						checkSlow(BPGC->UberGraphFunction);
						FVerySlowReferenceCollectorArchiveScope CollectorScope(
							Collector.GetInternalPersistentFrameReferenceCollectorArchive(),
							BPGC->UberGraphFunction,
							BPGC->UberGraphFramePointerProperty,
							InThis,
							PointerToUberGraphFrame->RawPointer);

#if WITH_ADDITIONAL_CRASH_CONTEXTS
						BPGCBreadcrumbsParams Params =
						{
							*InThis,
							*BPGC,
							CollectorScope.GetArchive(),
							FPlatformTLS::GetCurrentThreadId()
						};

						UE_ADD_CRASH_CONTEXT_SCOPE([&](FCrashContextExtendedWriter& Writer) { WriteBPGCBreadcrumbs(Writer, Params); });
#endif // WITH_ADDITIONAL_CRASH_CONTEXTS
						// All encountered references should be treated as non-native by GC
						Collector.SetIsProcessingNativeReferences(false);
						BPGC->UberGraphFunction->SerializeBin(CollectorScope.GetArchive(), PointerToUberGraphFrame->RawPointer);
						Collector.SetIsProcessingNativeReferences(true);
					}

				}
			}
#endif // USE_UBER_GRAPH_PERSISTENT_FRAME
		}
		else if (CurrentClass->HasAllClassFlags(CLASS_Native))
		{
			CurrentClass->CallAddReferencedObjects(InThis, Collector);
			break;
		}
		else
		{
			checkSlow(false);
		}
	}
}

FName UBlueprintGeneratedClass::GetUberGraphFrameName()
{
	static const FName UberGraphFrameName(TEXT("UberGraphFrame"));
	return UberGraphFrameName;
}

bool UBlueprintGeneratedClass::UsePersistentUberGraphFrame()
{
#if USE_UBER_GRAPH_PERSISTENT_FRAME
	static const FBoolConfigValueHelper PersistentUberGraphFrame(TEXT("Kismet"), TEXT("bPersistentUberGraphFrame"), GEngineIni);
	return PersistentUberGraphFrame;
#else
	return false;
#endif
}

#if VALIDATE_UBER_GRAPH_PERSISTENT_FRAME
static TAtomic<int32> GUberGraphSerialNumber(0);

ENGINE_API int32 IncrementUberGraphSerialNumber()
{
	return ++GUberGraphSerialNumber;
}
#endif//VALIDATE_UBER_GRAPH_PERSISTENT_FRAME

void UBlueprintGeneratedClass::Serialize(FArchive& Ar)
{
#if VALIDATE_UBER_GRAPH_PERSISTENT_FRAME
	if (Ar.IsLoading() && 0 == (Ar.GetPortFlags() & PPF_Duplicate))
	{
		UberGraphFunctionKey = IncrementUberGraphSerialNumber();
	}
#endif//VALIDATE_UBER_GRAPH_PERSISTENT_FRAME

	Super::Serialize(Ar);

	Ar.UsingCustomVersion(FFortniteMainBranchObjectVersion::GUID);
	if ((Ar.CustomVer(FFortniteMainBranchObjectVersion::GUID) >= FFortniteMainBranchObjectVersion::BPGCCookedEditorTags) &&
		Ar.IsFilterEditorOnly())
	{
#if !WITH_EDITORONLY_DATA
		FEditorTags CookedEditorTags; // Unused at runtime
#else
		CookedEditorTags.Reset();
		if (Ar.IsSaving())
		{
			GetEditorTags(CookedEditorTags);
		}
#endif
		Ar << CookedEditorTags;
	}

	if (Ar.IsLoading() && 0 == (Ar.GetPortFlags() & PPF_Duplicate))
	{
		CreatePersistentUberGraphFrame(ClassDefaultObject, true);

		UPackage* Package = GetOutermost();
		if (Package && Package->HasAnyPackageFlags(PKG_ForDiffing))
		{
			// If this is a diff package, set class to deprecated. This happens here to make sure it gets hit in all load cases
			ClassFlags |= CLASS_Deprecated;
		}
	}

#if WITH_EDITORONLY_DATA
	if (Ar.IsLoading())
	{
		UberGraphFramePointerProperty_DEPRECATED = nullptr;
	}
#endif
}

void UBlueprintGeneratedClass::GetLifetimeBlueprintReplicationList(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	uint32 PropertiesLeft = NumReplicatedProperties;

	for (TFieldIterator<FProperty> It(this, EFieldIteratorFlags::ExcludeSuper); It && PropertiesLeft > 0; ++It)
	{
		FProperty * Prop = *It;
		if (Prop != NULL && Prop->GetPropertyFlags() & CPF_Net)
		{
			PropertiesLeft--;
			
			OutLifetimeProps.AddUnique(FLifetimeProperty(Prop->RepIndex, Prop->GetBlueprintReplicationCondition(), REPNOTIFY_OnChanged, PUSH_MAKE_BP_PROPERTIES_PUSH_MODEL()));
		}
	}

	UBlueprintGeneratedClass* SuperBPClass = Cast<UBlueprintGeneratedClass>(GetSuperStruct());
	if (SuperBPClass != NULL)
	{
		SuperBPClass->GetLifetimeBlueprintReplicationList(OutLifetimeProps);
	}
}

FBlueprintCookedComponentInstancingData::~FBlueprintCookedComponentInstancingData()
{
	DEC_MEMORY_STAT_BY(STAT_BPCompInstancingFastPathMemory, CachedPropertyData.GetAllocatedSize());
	DEC_MEMORY_STAT_BY(STAT_BPCompInstancingFastPathMemory, CachedPropertyListForSerialization.GetAllocatedSize());
}

void FBlueprintCookedComponentInstancingData::BuildCachedPropertyList(FCustomPropertyListNode** CurrentNode, const UStruct* CurrentScope, int32* CurrentSourceIdx) const
{
	int32 LocalSourceIdx = 0;

	if (CurrentSourceIdx == nullptr)
	{
		CurrentSourceIdx = &LocalSourceIdx;
	}

	// The serialized list is stored linearly, so stop iterating once we no longer match the scope (this indicates that we've finished parsing out "sub" properties for a UStruct).
	while (*CurrentSourceIdx < ChangedPropertyList.Num() && ChangedPropertyList[*CurrentSourceIdx].PropertyScope == CurrentScope)
	{
		// Find changed property by name/scope.
		const FBlueprintComponentChangedPropertyInfo& ChangedPropertyInfo = ChangedPropertyList[(*CurrentSourceIdx)++];
		FProperty* Property = nullptr;
		const UStruct* PropertyScope = CurrentScope;
		while (!Property && PropertyScope)
		{
			Property = FindFProperty<FProperty>(PropertyScope, ChangedPropertyInfo.PropertyName);
			PropertyScope = PropertyScope->GetSuperStruct();
		}

		// Create a new node to hold property info.
		FCustomPropertyListNode* NewNode = new FCustomPropertyListNode(Property, ChangedPropertyInfo.ArrayIndex);
		CachedPropertyListForSerialization.Add(NewNode);

		// Link the new node into the current property list.
		if (CurrentNode)
		{
			*CurrentNode = NewNode;
		}

		// If this is a UStruct property, recursively build a sub-property list.
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			BuildCachedPropertyList(&NewNode->SubPropertyList, StructProperty->Struct, CurrentSourceIdx);
		}
		else if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			// If this is an array property, recursively build a sub-property list.
			BuildCachedArrayPropertyList(ArrayProperty, &NewNode->SubPropertyList, CurrentSourceIdx);
		}

		// Advance current location to the next linked node.
		CurrentNode = &NewNode->PropertyListNext;
	}
}

void FBlueprintCookedComponentInstancingData::BuildCachedArrayPropertyList(const FArrayProperty* ArrayProperty, FCustomPropertyListNode** ArraySubPropertyNode, int32* CurrentSourceIdx) const
{
	// Build the array property's sub-property list. An empty name field signals the end of the changed array property list.
	while (*CurrentSourceIdx < ChangedPropertyList.Num() &&
		(ChangedPropertyList[*CurrentSourceIdx].PropertyName == NAME_None
			|| ChangedPropertyList[*CurrentSourceIdx].PropertyName == ArrayProperty->GetFName()))
	{
		const FBlueprintComponentChangedPropertyInfo& ChangedArrayPropertyInfo = ChangedPropertyList[(*CurrentSourceIdx)++];
		FProperty* InnerProperty = ChangedArrayPropertyInfo.PropertyName != NAME_None ? ArrayProperty->Inner : nullptr;

		*ArraySubPropertyNode = new FCustomPropertyListNode(InnerProperty, ChangedArrayPropertyInfo.ArrayIndex);
		CachedPropertyListForSerialization.Add(*ArraySubPropertyNode);

		// If this is a UStruct property, recursively build a sub-property list.
		if (const FStructProperty* InnerStructProperty = CastField<FStructProperty>(InnerProperty))
		{
			BuildCachedPropertyList(&(*ArraySubPropertyNode)->SubPropertyList, InnerStructProperty->Struct, CurrentSourceIdx);
		}
		else if (const FArrayProperty* InnerArrayProperty = CastField<FArrayProperty>(InnerProperty))
		{
			// If this is an array property, recursively build a sub-property list.
			BuildCachedArrayPropertyList(InnerArrayProperty, &(*ArraySubPropertyNode)->SubPropertyList, CurrentSourceIdx);
		}

		ArraySubPropertyNode = &(*ArraySubPropertyNode)->PropertyListNext;
	}
}

const FCustomPropertyListNode* FBlueprintCookedComponentInstancingData::GetCachedPropertyList() const
{
	FCustomPropertyListNode* PropertyListRootNode = nullptr;

	// Construct the list if necessary.
	if (CachedPropertyListForSerialization.Num() == 0 && ChangedPropertyList.Num() > 0)
	{
		CachedPropertyListForSerialization.Reserve(ChangedPropertyList.Num());

		// Kick off construction of the cached property list.
		BuildCachedPropertyList(&PropertyListRootNode, ComponentTemplateClass);

		INC_MEMORY_STAT_BY(STAT_BPCompInstancingFastPathMemory, CachedPropertyListForSerialization.GetAllocatedSize());
	}
	else if (CachedPropertyListForSerialization.Num() > 0)
	{
		PropertyListRootNode = *CachedPropertyListForSerialization.GetData();
	}

	return PropertyListRootNode;
}

void FBlueprintCookedComponentInstancingData::BuildCachedPropertyDataFromTemplate(UActorComponent* SourceTemplate)
{
	// Blueprint component instance data writer implementation.
	class FBlueprintComponentInstanceDataWriter : public FObjectWriter
	{
	public:
		FBlueprintComponentInstanceDataWriter(TArray<uint8>& InDstBytes, const FCustomPropertyListNode* InPropertyList)
			:FObjectWriter(InDstBytes)
		{
			ArCustomPropertyList = InPropertyList;
			ArUseCustomPropertyList = true;
			this->SetWantBinaryPropertySerialization(true);

			// Set this flag to emulate things that would normally happen in the SDO case when this flag is set. This is needed to ensure consistency with serialization during instancing.
			ArPortFlags |= PPF_Duplicate;
		}
	};

	checkSlow(bHasValidCookedData);
	checkSlow(SourceTemplate != nullptr);
	checkSlow(!SourceTemplate->HasAnyFlags(RF_NeedLoad));

	// Cache source template attributes needed for instancing.
	ComponentTemplateName = SourceTemplate->GetFName();
	ComponentTemplateClass = SourceTemplate->GetClass();
	ComponentTemplateFlags = SourceTemplate->GetFlags();

	// This will also load the cached property list, if necessary.
	const FCustomPropertyListNode* PropertyList = GetCachedPropertyList();

	// Make sure we don't have any previously-built data.
	if (!ensure(CachedPropertyData.Num() == 0))
	{
		DEC_MEMORY_STAT_BY(STAT_BPCompInstancingFastPathMemory, CachedPropertyData.GetAllocatedSize());

		CachedPropertyData.Empty();
	}

	// Write template data out to the "fast path" buffer. All dependencies will be loaded at this point.
	FBlueprintComponentInstanceDataWriter InstanceDataWriter(CachedPropertyData, PropertyList);
	SourceTemplate->Serialize(InstanceDataWriter);

	INC_MEMORY_STAT_BY(STAT_BPCompInstancingFastPathMemory, CachedPropertyData.GetAllocatedSize());
}

FName UBlueprintGeneratedClass::FindBlueprintPropertyNameFromGuid(const FGuid& PropertyGuid) const
{
	auto FindPropertyNameFromGuidImpl = [&PropertyGuid](const TMap<FName, FGuid>& PropertyGuidsToUse) -> FName
	{
		if (const FName* Result = PropertyGuidsToUse.FindKey(PropertyGuid))
		{
			return *Result;
		}

		return NAME_None;
	};

	if (bCooked)
	{
		return FindPropertyNameFromGuidImpl(CookedPropertyGuids);
	}

#if WITH_EDITORONLY_DATA
	return FindPropertyNameFromGuidImpl(PropertyGuids);
#else	// WITH_EDITORONLY_DATA
	return NAME_None;
#endif	// WITH_EDITORONLY_DATA
}

FGuid UBlueprintGeneratedClass::FindBlueprintPropertyGuidFromName(const FName PropertyName) const
{
	auto FindPropertyGuidFromNameImpl = [&PropertyName](const TMap<FName, FGuid>& PropertyGuidsToUse) -> FGuid
	{
		if (const FGuid* Result = PropertyGuidsToUse.Find(PropertyName))
		{
			return *Result;
		}

		return FGuid();
	};

	if (bCooked)
	{
		return FindPropertyGuidFromNameImpl(CookedPropertyGuids);
	}

#if WITH_EDITORONLY_DATA
	return FindPropertyGuidFromNameImpl(PropertyGuids);
#else	// WITH_EDITORONLY_DATA
	return FGuid();
#endif	// WITH_EDITORONLY_DATA
}

bool UBlueprintGeneratedClass::ArePropertyGuidsAvailable() const
{
	auto AreBlueprintPropertyGuidsAvailable = [this]()
	{
		if (bCooked)
		{
			return CookedPropertyGuids.Num() > 0;
		}

#if WITH_EDITORONLY_DATA
		return PropertyGuids.Num() > 0;
#else	// WITH_EDITORONLY_DATA
		return false;
#endif	// WITH_EDITORONLY_DATA
	};

	if (AreBlueprintPropertyGuidsAvailable())
	{
		return true;
	}

	if (UBlueprintGeneratedClass* Super = Cast<UBlueprintGeneratedClass>(GetSuperStruct()))
	{
		// Our parent may have guids for inherited variables
		return Super->ArePropertyGuidsAvailable();
	}

	return false;
}

FName UBlueprintGeneratedClass::FindPropertyNameFromGuid(const FGuid& PropertyGuid) const
{
	// Check parent first as it may have renamed a property since this class was last saved
	if (UBlueprintGeneratedClass* Super = Cast<UBlueprintGeneratedClass>(GetSuperStruct()))
	{
		FName RedirectedName = Super->FindPropertyNameFromGuid(PropertyGuid);
		if (!RedirectedName.IsNone())
		{
			return RedirectedName;
		}
	}

	return FindBlueprintPropertyNameFromGuid(PropertyGuid);
}

FGuid UBlueprintGeneratedClass::FindPropertyGuidFromName(const FName InName) const
{
	const FGuid FoundPropertyGuid = FindBlueprintPropertyGuidFromName(InName);
	if (FoundPropertyGuid.IsValid())
	{
		return FoundPropertyGuid;
	}

	if (UBlueprintGeneratedClass* Super = Cast<UBlueprintGeneratedClass>(GetSuperStruct()))
	{
		// Fall back to parent if this is an inherited variable
		return Super->FindPropertyGuidFromName(InName);
	}

	return FGuid();
}

#if WITH_EDITORONLY_DATA
void UBlueprintGeneratedClass::GetEditorTags(FEditorTags& Tags) const
{
	if (UBlueprint* BP = Cast<UBlueprint>(ClassGeneratedBy))
	{
		auto AddEditorTag = [BP, &Tags](FName TagName, FName PropertyName, const uint8* PropertyValueOverride = nullptr)
		{
			const FProperty* Property = FindFieldChecked<FProperty>(UBlueprint::StaticClass(), PropertyName);
			const uint8* PropertyAddr = PropertyValueOverride ? PropertyValueOverride : Property->ContainerPtrToValuePtr<uint8>(BP);
			FString PropertyValueAsText;
			Property->ExportTextItem_Direct(PropertyValueAsText, PropertyAddr, PropertyAddr, nullptr, PPF_None);
			if (!PropertyValueAsText.IsEmpty())
			{
				Tags.Add(TagName, MoveTemp(PropertyValueAsText));
			}
		};

		AddEditorTag(FBlueprintTags::BlueprintType, GET_MEMBER_NAME_CHECKED(UBlueprint, BlueprintType));

		{
			// Clear the FBPInterfaceDescription Graphs because they are irrelevant to the BPGC
			TArray<FBPInterfaceDescription> GraphlessImplementedInterfaces;
			GraphlessImplementedInterfaces.Reserve(BP->ImplementedInterfaces.Num());
			for (const FBPInterfaceDescription& ImplementedInterface : BP->ImplementedInterfaces)
			{
				FBPInterfaceDescription& GraphlessImplementedInterface = GraphlessImplementedInterfaces.Emplace_GetRef(ImplementedInterface);
				GraphlessImplementedInterface.Graphs.Empty();
			}

			AddEditorTag(FBlueprintTags::ImplementedInterfaces, GET_MEMBER_NAME_CHECKED(UBlueprint, ImplementedInterfaces), (const uint8*)&GraphlessImplementedInterfaces);
		}
	}
}

TSubclassOf<UClassCookedMetaData> UBlueprintGeneratedClass::GetCookedMetaDataClass() const
{
	return UClassCookedMetaData::StaticClass();
}

UClassCookedMetaData* UBlueprintGeneratedClass::NewCookedMetaData()
{
	if (!CachedCookedMetaDataPtr)
	{
		CachedCookedMetaDataPtr = CookedMetaDataUtil::NewCookedMetaData<UClassCookedMetaData>(this, "CookedClassMetaData", GetCookedMetaDataClass());
	}
	return CachedCookedMetaDataPtr;
}

const UClassCookedMetaData* UBlueprintGeneratedClass::FindCookedMetaData()
{
	if (!CachedCookedMetaDataPtr)
	{
		CachedCookedMetaDataPtr = CookedMetaDataUtil::FindCookedMetaData<UClassCookedMetaData>(this, TEXT("CookedClassMetaData"));
	}
	return CachedCookedMetaDataPtr;
}

void UBlueprintGeneratedClass::PurgeCookedMetaData()
{
	if (CachedCookedMetaDataPtr)
	{
		CookedMetaDataUtil::PurgeCookedMetaData<UClassCookedMetaData>(CachedCookedMetaDataPtr);
	}
}
#endif //if WITH_EDITORONLY_DATA

