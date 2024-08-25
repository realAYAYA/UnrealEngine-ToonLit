// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "RigVMCore/RigVMStruct.h"
#include <RigVMCore/RigVMDecorator.h>
#include "RigVMTypeUtils.h"
#include "RigVMModule.h"
#include "Animation/AttributeTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "UObject/UObjectIterator.h"
#include "Misc/CoreDelegates.h"
#include "Misc/DelayedAutoRegister.h"
#include "RigVMFunctions/RigVMDispatch_Core.h"
#include "Interfaces/IPluginManager.h"

const FName FRigVMRegistry::TemplateNameMetaName = TEXT("TemplateName");

FCriticalSection FRigVMRegistry::FindOrAddTypeMutex;

FCriticalSection FRigVMRegistry::FunctionRegistryMutex;
FCriticalSection FRigVMRegistry::FactoryRegistryMutex;
FCriticalSection FRigVMRegistry::TemplateRegistryMutex;

FCriticalSection FRigVMRegistry::DispatchFunctionMutex;
FCriticalSection FRigVMRegistry::DispatchPredicatesMutex;


// When the object system has been completely loaded, load in all the engine types that we haven't registered already in InitializeIfNeeded 
static FDelayedAutoRegisterHelper GRigVMRegistrySingletonHelper(EDelayedRegisterRunPhase::EndOfEngineInit, []() -> void
{
	FRigVMRegistry::Get().RefreshEngineTypes();
});


FRigVMRegistry::FRigVMRegistry() :
	bIsRefreshingEngineTypes(false),
	bEverRefreshedEngineTypes(false)
{
	Initialize();
}

FRigVMRegistry::~FRigVMRegistry()
{
	Reset();
}

FRigVMRegistry& FRigVMRegistry::Get()
{
	// static in a function scope ensures that the GC system is initiated before 
	// the registry constructor is called
	static FRigVMRegistry s_RigVMRegistry;
	return s_RigVMRegistry;
}

void FRigVMRegistry::AddReferencedObjects(FReferenceCollector& Collector)
{
	// registry should hold strong references to these type objects
	// otherwise GC may remove them without the registry known it
	// which can happen during cook time.
	for (FTypeInfo& Type : Types)
	{
		// the Object needs to be checked for validity since it may be a user defined type (struct or enum)
		// which is about to get removed. 
		if (IsValid(Type.Type.CPPTypeObject))
		{
			Collector.AddReferencedObject(Type.Type.CPPTypeObject);
		}
	}
}

FString FRigVMRegistry::GetReferencerName() const
{
	return TEXT("FRigVMRegistry");
}

const TArray<UScriptStruct*>& FRigVMRegistry::GetMathTypes()
{
	// The list of base math types to automatically register 
	static const TArray<UScriptStruct*> MathTypes = { 
		TBaseStructure<FRotator>::Get(),
		TBaseStructure<FQuat>::Get(),
		TBaseStructure<FTransform>::Get(),
		TBaseStructure<FLinearColor>::Get(),
		TBaseStructure<FColor>::Get(),
		TBaseStructure<FPlane>::Get(),
		TBaseStructure<FVector>::Get(),
		TBaseStructure<FVector2D>::Get(),
		TBaseStructure<FVector4>::Get(),
		TBaseStructure<FBox2D>::Get()
	};

	return MathTypes;
}

uint32 FRigVMRegistry::GetHashForType(TRigVMTypeIndex InTypeIndex) const
{
	if(!Types.IsValidIndex(InTypeIndex))
	{
		return UINT32_MAX;
	}

	FRigVMRegistry* MutableThis = (FRigVMRegistry*)this; 
	FTypeInfo& TypeInfo = MutableThis->Types[InTypeIndex];
	
	if(TypeInfo.Hash != UINT32_MAX)
	{
		return TypeInfo.Hash;
	}

	uint32 Hash = INDEX_NONE;
	if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(TypeInfo.Type.CPPTypeObject))
	{
		Hash = GetHashForScriptStruct(ScriptStruct, false);
	}
	else if(const UStruct* Struct = Cast<UStruct>(TypeInfo.Type.CPPTypeObject))
	{
		Hash = GetHashForStruct(Struct);
	}
	else if(const UEnum* Enum = Cast<UEnum>(TypeInfo.Type.CPPTypeObject))
    {
    	Hash = GetHashForEnum(Enum, false);
    }
    else
    {
    	Hash = GetTypeHash(TypeInfo.Type.CPPType.ToString());
    }

	// for used defined structs - always recompute it
	if(Cast<UUserDefinedStruct>(TypeInfo.Type.CPPTypeObject))
	{
		return Hash;
	}

	TypeInfo.Hash = Hash;
	return Hash;
}

uint32 FRigVMRegistry::GetHashForScriptStruct(const UScriptStruct* InScriptStruct, bool bCheckTypeIndex) const
{
	if(bCheckTypeIndex)
	{
		const TRigVMTypeIndex TypeIndex = GetTypeIndex(*InScriptStruct->GetStructCPPName(), (UObject*)InScriptStruct);
		if(TypeIndex != INDEX_NONE)
		{
			return GetHashForType(TypeIndex);
		}
	}
	
	const uint32 NameHash = GetTypeHash(InScriptStruct->GetStructCPPName());
	return HashCombine(NameHash, GetHashForStruct(InScriptStruct));
}

uint32 FRigVMRegistry::GetHashForStruct(const UStruct* InStruct) const
{
	uint32 Hash = GetTypeHash(InStruct->GetPathName());
	for (TFieldIterator<FProperty> It(InStruct); It; ++It)
	{
		const FProperty* Property = *It;
		if(IsAllowedType(Property))
		{
			Hash = HashCombine(Hash, GetHashForProperty(Property));
		}
	}
	return Hash;
}

uint32 FRigVMRegistry::GetHashForEnum(const UEnum* InEnum, bool bCheckTypeIndex) const
{
	if(bCheckTypeIndex)
	{
		const TRigVMTypeIndex TypeIndex = GetTypeIndex(*InEnum->CppType, (UObject*)InEnum);
		if(TypeIndex != INDEX_NONE)
		{
			return GetHashForType(TypeIndex);
		}
	}
	
	uint32 Hash = GetTypeHash(InEnum->GetName());
	for(int32 Index = 0; Index < InEnum->NumEnums(); Index++)
	{
		Hash = HashCombine(Hash, GetTypeHash(InEnum->GetValueByIndex(Index)));
		Hash = HashCombine(Hash, GetTypeHash(InEnum->GetDisplayNameTextByIndex(Index).ToString()));
	}
	return Hash;
}

uint32 FRigVMRegistry::GetHashForProperty(const FProperty* InProperty) const
{
	uint32 Hash = GetTypeHash(InProperty->GetName());

	FString ExtendedCPPType;
	const FString CPPType = InProperty->GetCPPType(&ExtendedCPPType);
	Hash = HashCombine(Hash, GetTypeHash(CPPType + ExtendedCPPType));
	
	if(const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(InProperty))
	{
		InProperty = ArrayProperty->Inner;
	}
	
	if(const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
	{
		Hash = HashCombine(Hash, GetHashForStruct(StructProperty->Struct));
	}
	else if(const FByteProperty* ByteProperty = CastField<FByteProperty>(InProperty))
	{
		if(ByteProperty->Enum)
		{
			Hash = HashCombine(Hash, GetHashForEnum(ByteProperty->Enum));
		}
	}
	else if(const FEnumProperty* EnumProperty = CastField<FEnumProperty>(InProperty))
	{
		Hash = HashCombine(Hash, GetHashForEnum(EnumProperty->GetEnum()));
	}
	
	return Hash;
}


void FRigVMRegistry::Initialize()
{
	// this should not be necessary since the initialize is used only
	// on a constructor on a static variable (thread safe)
	// but in case code paths change in the future we'll also lock here.
	const FScopeLock FindOrAddTypeLock(&FindOrAddTypeMutex);
	
	Types.Reserve(512);
	TypeToIndex.Reserve(512);
	TypesPerCategory.Reserve(19);
	TemplatesPerCategory.Reserve(19);
	
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_Execute, TArray<TRigVMTypeIndex>()).Reserve(8);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, TArray<TRigVMTypeIndex>()).Reserve(256);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, TArray<TRigVMTypeIndex>()).Reserve(256);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, TArray<TRigVMTypeIndex>()).Reserve(256);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleSimpleValue, TArray<TRigVMTypeIndex>()).Reserve(8);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArraySimpleValue, TArray<TRigVMTypeIndex>()).Reserve(8);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArraySimpleValue, TArray<TRigVMTypeIndex>()).Reserve(8);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleMathStructValue, TArray<TRigVMTypeIndex>()).Reserve(GetMathTypes().Num());
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayMathStructValue, TArray<TRigVMTypeIndex>()).Reserve(GetMathTypes().Num());
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayMathStructValue, TArray<TRigVMTypeIndex>()).Reserve(GetMathTypes().Num());
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue, TArray<TRigVMTypeIndex>()).Reserve(128);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayScriptStructValue, TArray<TRigVMTypeIndex>()).Reserve(128);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayScriptStructValue, TArray<TRigVMTypeIndex>()).Reserve(128);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleEnumValue, TArray<TRigVMTypeIndex>()).Reserve(128);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayEnumValue, TArray<TRigVMTypeIndex>()).Reserve(128);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayEnumValue, TArray<TRigVMTypeIndex>()).Reserve(128);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleObjectValue, TArray<TRigVMTypeIndex>()).Reserve(128);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayObjectValue, TArray<TRigVMTypeIndex>()).Reserve(128);
	TypesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayObjectValue, TArray<TRigVMTypeIndex>()).Reserve(128);

	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_Execute, TArray<int32>()).Reserve(8);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleSimpleValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArraySimpleValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArraySimpleValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleMathStructValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayMathStructValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayMathStructValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayScriptStructValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayScriptStructValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleEnumValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayEnumValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayEnumValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleObjectValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayObjectValue, TArray<int32>()).Reserve(64);
	TemplatesPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayObjectValue, TArray<int32>()).Reserve(64);

	RigVMTypeUtils::TypeIndex::Execute = FindOrAddType_NoLock(FRigVMTemplateArgumentType(FRigVMExecuteContext::StaticStruct()), false);
	RigVMTypeUtils::TypeIndex::ExecuteArray = FindOrAddType_NoLock(FRigVMTemplateArgumentType(FRigVMExecuteContext::StaticStruct()).ConvertToArray(), false);
	RigVMTypeUtils::TypeIndex::Bool = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::BoolTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::Float = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::FloatTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::Double = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::DoubleTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::Int32 = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::Int32TypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::UInt32 = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::UInt32TypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::UInt8 = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::UInt8TypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::FName = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::FNameTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::FString = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::FStringTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::WildCard = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::GetWildCardCPPTypeName(), RigVMTypeUtils::GetWildCardCPPTypeObject()), false);
	RigVMTypeUtils::TypeIndex::BoolArray = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::BoolArrayTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::FloatArray = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::FloatArrayTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::DoubleArray = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::DoubleArrayTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::Int32Array = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::Int32ArrayTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::UInt32Array = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::UInt32ArrayTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::UInt8Array = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::UInt8ArrayTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::FNameArray = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::FNameArrayTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::FStringArray = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::FStringArrayTypeName, nullptr), false);
	RigVMTypeUtils::TypeIndex::WildCardArray = FindOrAddType_NoLock(FRigVMTemplateArgumentType(RigVMTypeUtils::GetWildCardArrayCPPTypeName(), RigVMTypeUtils::GetWildCardCPPTypeObject()), false);

	// register the default math types
	for(UScriptStruct* MathType : GetMathTypes())
	{
		FindOrAddType_NoLock(FRigVMTemplateArgumentType(MathType), false);
	}

	// hook the registry to prepare for engine shutdown
	FCoreDelegates::OnExit.AddLambda([&]()
	{
		Reset();

		if (FAssetRegistryModule* AssetRegistryModule = FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry")))
		{
			if (AssetRegistryModule->TryGet())
			{
				AssetRegistryModule->Get().OnAssetRemoved().RemoveAll(this);
				AssetRegistryModule->Get().OnAssetRenamed().RemoveAll(this);
			}
		}

		IPluginManager::Get().OnPluginUnmounted().RemoveAll(this);

		UE::Anim::AttributeTypes::GetOnAttributeTypesChanged().RemoveAll(this);
	});
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FRigVMRegistry::OnAssetRemoved);
	AssetRegistryModule.Get().OnAssetRenamed().AddRaw(this, &FRigVMRegistry::OnAssetRenamed);

	IPluginManager::Get().OnPluginUnmounted().AddRaw(this, &FRigVMRegistry::OnPluginUnloaded);
	
	UE::Anim::AttributeTypes::GetOnAttributeTypesChanged().AddRaw(this, &FRigVMRegistry::OnAnimationAttributeTypesChanged);
}

void FRigVMRegistry::RefreshEngineTypes()
{
	FScopeLock FindOrAddTypeLock(&FindOrAddTypeMutex);
	RefreshEngineTypes_NoLock();
}

void FRigVMRegistry::RefreshEngineTypesIfRequired()
{
	FScopeLock FindOrAddTypeLock(&FindOrAddTypeMutex);
	if(bEverRefreshedEngineTypes)
	{
		return;
	}
	RefreshEngineTypes_NoLock();
}

void FRigVMRegistry::RefreshEngineTypes_NoLock()
{
	TGuardValue<bool> EnableGuardRefresh(bIsRefreshingEngineTypes, true);

	const int32 NumTypesBefore = Types.Num(); 
	
	// Register all user-defined types that the engine knows about. Enumerating over the entire object hierarchy is
	// slow, so we do it for structs, enums and dispatch factories in one shot.
	TArray<UScriptStruct*> DispatchFactoriesToRegister;
	DispatchFactoriesToRegister.Reserve(32);

	for (TObjectIterator<UScriptStruct> ScriptStructIt; ScriptStructIt; ++ScriptStructIt)
	{
		UScriptStruct* ScriptStruct = *ScriptStructIt;
		
		// if this is a C++ type - skip it
		if(ScriptStruct->IsA<UUserDefinedStruct>() || ScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
		{
			FindOrAddType_NoLock(FRigVMTemplateArgumentType(ScriptStruct), false);
		}
		else if (ScriptStruct != FRigVMDispatchFactory::StaticStruct() &&
				 ScriptStruct->IsChildOf(FRigVMDispatchFactory::StaticStruct()))
		{
			DispatchFactoriesToRegister.Add(ScriptStruct);
		}
	}

	for (TObjectIterator<UEnum> EnumIt; EnumIt; ++EnumIt)
	{
		UEnum* Enum = *EnumIt;
		if(IsAllowedType(Enum))
		{
			const FString CPPType = Enum->CppType.IsEmpty() ? Enum->GetName() : Enum->CppType;
			FindOrAddType_NoLock(FRigVMTemplateArgumentType(*CPPType, Enum), false);
		}
	}
	
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (IsAllowedType(Class))
		{
			// Register both the class and the object type for use
			FindOrAddType_NoLock(FRigVMTemplateArgumentType(Class, RigVMTypeUtils::EClassArgType::AsClass), false);
			FindOrAddType_NoLock(FRigVMTemplateArgumentType(Class, RigVMTypeUtils::EClassArgType::AsObject), false);
		}
	}

	// Register all dispatch factories only after all other types have been registered.
	for (UScriptStruct* DispatchFactoryStruct: DispatchFactoriesToRegister)
	{
		RegisterFactory(DispatchFactoryStruct);
	}

	const int32 NumTypesNow = Types.Num();
	if(NumTypesBefore != NumTypesNow)
	{
		// update all of the templates once
		TArray<bool> TemplateProcessed;
		TemplateProcessed.AddZeroed(Templates.Num());
		for(const TPair<FRigVMTemplateArgument::ETypeCategory, TArray<int32>>& Pair : TemplatesPerCategory)
		{
			for(const int32 TemplateIndex : Pair.Value)
			{
				if(!TemplateProcessed[TemplateIndex])
				{
					FRigVMTemplate& Template = Templates[TemplateIndex];
					(void)Template.UpdateArgumentTypes();
					TemplateProcessed[TemplateIndex] = true;
				}
			}
		}
	}
	
	bEverRefreshedEngineTypes = true;
}

void FRigVMRegistry::OnAssetRenamed(const FAssetData& InAssetData, const FString& InOldObjectPath)
{
	const FSoftObjectPath OldPath(InOldObjectPath);
	
	if (const TRigVMTypeIndex* TypeIndexPtr = UserDefinedTypeToIndex.Find(OldPath))
	{
		const TRigVMTypeIndex TypeIndex = *TypeIndexPtr;
		UserDefinedTypeToIndex.Remove(OldPath);
		UserDefinedTypeToIndex.Add(InAssetData.ToSoftObjectPath()) = TypeIndex;
	}
}

void FRigVMRegistry::OnAssetRemoved(const FAssetData& InAssetData)
{
	if (RemoveType(InAssetData.ToSoftObjectPath(), InAssetData.GetClass()))
	{
		OnRigVMRegistryChangedDelegate.Broadcast();
	}
}

void FRigVMRegistry::OnPluginUnloaded(IPlugin& InPlugin)
{
	const FString PluginContentPath = InPlugin.GetMountedAssetPath();

	TSet<FSoftObjectPath> PathsToRemove;
	for (const TPair<FSoftObjectPath, TRigVMTypeIndex>& Item: UserDefinedTypeToIndex)
	{
		const FSoftObjectPath ObjectPath = Item.Key;
		const FString PackageName = ObjectPath.GetLongPackageName();
		
		if (PackageName.StartsWith(PluginContentPath))
		{
			PathsToRemove.Add(ObjectPath);
		}
	}

	bool bRegistryChanged = false;
	for (FSoftObjectPath ObjectPath: PathsToRemove)
	{
		const UClass* ObjectClass = nullptr;
		if (const UObject* TypeObject = ObjectPath.ResolveObject())
		{
			ObjectClass = TypeObject->GetClass();
		}
		
		if (RemoveType(ObjectPath, ObjectClass))
		{
			bRegistryChanged = true;
		}
	}

	if (bRegistryChanged)
	{
		OnRigVMRegistryChangedDelegate.Broadcast();
	}
}

void FRigVMRegistry::OnAnimationAttributeTypesChanged(const UScriptStruct* InStruct, bool bIsAdded)
{
	if (!ensure(InStruct))
	{
		return;
	}

	if (bIsAdded)
	{
		FindOrAddType(FRigVMTemplateArgumentType(const_cast<UScriptStruct*>(InStruct)));
		OnRigVMRegistryChangedDelegate.Broadcast();		
	}
}


void FRigVMRegistry::Reset()
{
	for(FRigVMDispatchFactory* Factory : Factories)
	{
		if(const UScriptStruct* ScriptStruct = Factory->GetScriptStruct())
		{
			ScriptStruct->DestroyStruct(Factory, 1);
		}
		FMemory::Free(Factory);
	}
	Factories.Reset();
}

TRigVMTypeIndex FRigVMRegistry::FindOrAddType(const FRigVMTemplateArgumentType& InType, bool bForce)
{
	const FScopeLock FindOrAddTypesLock(&FindOrAddTypeMutex);
	return FindOrAddType_NoLock(InType, bForce);
}

TRigVMTypeIndex FRigVMRegistry::FindOrAddType_NoLock(const FRigVMTemplateArgumentType& InType, bool bForce)
{
	// we don't use a mutex here since by the time the engine relies on worker
	// thread for execution or async loading all types will have been registered.
	
	TRigVMTypeIndex Index = GetTypeIndex(InType);
	if(Index == INDEX_NONE)
	{
		FRigVMTemplateArgumentType ElementType = InType;
		while(ElementType.IsArray())
		{
			ElementType.ConvertToBaseElement();
		}
		
		const UObject* CPPTypeObject = ElementType.CPPTypeObject;
		if(!bForce && (CPPTypeObject != nullptr))
		{
			if(const UClass* Class = Cast<UClass>(CPPTypeObject))
			{
				if(!IsAllowedType(Class))
				{
					return Index;
				}	
			}
			else if(const UEnum* Enum = Cast<UEnum>(CPPTypeObject))
			{
				if(!IsAllowedType(Enum))
				{
					return Index;
				}
			}
			else if(const UStruct* Struct = Cast<UStruct>(CPPTypeObject))
			{
				if(!IsAllowedType(Struct))
				{					
					return Index;
				}
			}
		}

		bool bIsExecute = false;
		if(const UScriptStruct* ScriptStruct = Cast<UScriptStruct>(CPPTypeObject))
		{
			bIsExecute = ScriptStruct->IsChildOf(FRigVMExecuteContext::StaticStruct());
		}

		TArray<TRigVMTypeIndex> Indices;
		Indices.Reserve(3);
		for (int32 ArrayDimension=0; ArrayDimension<3; ++ArrayDimension)
		{
			if (bIsExecute && ArrayDimension > 1)
			{
				break;
			}
			
			FRigVMTemplateArgumentType CurType = ElementType;
			for (int32 j=0; j<ArrayDimension; ++j)
			{
				CurType.ConvertToArray();
			}

			FTypeInfo Info;
			Info.Type = CurType;
			Info.bIsArray = ArrayDimension > 0;
			Info.bIsExecute = bIsExecute;
			
			Index = Types.Add(Info);
#if UE_RIGVM_DEBUG_TYPEINDEX
			Index.Name = Info.Type.CPPType;
#endif
			TypeToIndex.Add(CurType, Index);

			Indices.Add(Index);
		}

		Types[Indices[1]].BaseTypeIndex = Indices[0];
		Types[Indices[0]].ArrayTypeIndex = Indices[1];

		if (!bIsExecute)
		{
			Types[Indices[2]].BaseTypeIndex = Indices[1];
			Types[Indices[1]].ArrayTypeIndex = Indices[2];
		}

		// update the categories first then propagate to TemplatesPerCategory once all categories up to date
		TArray<TPair<FRigVMTemplateArgument::ETypeCategory, int32>> ToPropagate;
		auto RegisterNewType = [&](FRigVMTemplateArgument::ETypeCategory InCategory, int32 NewIndex)
		{
			RegisterTypeInCategory(InCategory, NewIndex);
			ToPropagate.Emplace(InCategory, NewIndex);
		}; 

		for (int32 ArrayDimension=0; ArrayDimension<3; ++ArrayDimension)
		{
			if(bIsExecute && ArrayDimension > 1)
			{
				break;
			}
			Index = Indices[ArrayDimension];

			// Add to category
			// simple types
			if(CPPTypeObject == nullptr)
			{
				switch(ArrayDimension)
				{
					default:
					case 0:
					{
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_SingleSimpleValue, Index);
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, Index);
						break;
					}
					case 1:
					{
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArraySimpleValue, Index);
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, Index);
						break;
					}
					case 2:
					{
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayArraySimpleValue, Index);
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, Index);
						break;
					}
				}
			}
			else if(const UClass* Class = Cast<UClass>(CPPTypeObject))
			{
				switch(ArrayDimension)
				{
					default:
					case 0:
					{
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_SingleObjectValue, Index);
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, Index);
						break;
					}
					case 1:
					{
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayObjectValue, Index);
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, Index);
						break;
					}
					case 2:
					{
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayArrayObjectValue, Index);
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, Index);
						break;
					}
				}
			}
			else if(const UEnum* Enum = Cast<UEnum>(CPPTypeObject))
			{
				switch(ArrayDimension)
				{
					default:
					case 0:
					{
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_SingleEnumValue, Index);
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, Index);
						break;
					}
					case 1:
					{
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayEnumValue, Index);
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, Index);
						break;
					}
					case 2:
					{
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayArrayEnumValue, Index);
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, Index);
						break;
					}
				}
			}
			else if(const UStruct* Struct = Cast<UStruct>(CPPTypeObject))
			{
				if(Struct->IsChildOf(FRigVMExecuteContext::StaticStruct()))
				{
					if(ArrayDimension == 0)
					{
						RegisterNewType(FRigVMTemplateArgument::ETypeCategory_Execute, Index);
					}
				}
				else
				{
					if(GetMathTypes().Contains(CPPTypeObject))
					{
						switch(ArrayDimension)
						{
							default:
							case 0:
							{
								RegisterNewType(FRigVMTemplateArgument::ETypeCategory_SingleMathStructValue, Index);
								break;
							}
							case 1:
							{
								RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayMathStructValue, Index);
								break;
							}
							case 2:
							{
								RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayArrayMathStructValue, Index);
								break;
							}
						}
					}
					
					switch(ArrayDimension)
					{
						default:
						case 0:
						{
							RegisterNewType(FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue, Index);
							RegisterNewType(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, Index);
							break;
						}
						case 1:
						{
							RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayScriptStructValue, Index);
							RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, Index);
							break;
						}
						case 2:
						{
							RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayArrayScriptStructValue, Index);
							RegisterNewType(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, Index);
							break;
						}
					}
				}
			}
		}

		// propagate new type to templates once they have all been added to the categories
		for (const auto& [Category, NewIndex]: ToPropagate)
		{
			PropagateTypeAddedToCategory(Category, NewIndex);	
		}

		// if the type is a structure
		// then add all of its sub property types
		if(const UStruct* Struct = Cast<UStruct>(CPPTypeObject))
		{
			for (TFieldIterator<FProperty> It(Struct); It; ++It)
			{
				FProperty* Property = *It;
				if(IsAllowedType(Property))
				{
					// by creating a template argument for the child property
					// the type will be added by calling ::FindOrAddType_Internal recursively.
					FRigVMTemplateArgument DummyArgument(Property, *this);
				}
#if WITH_EDITOR
				else
				{
					// If the subproperty is not allowed, let's make sure it's hidden. Otherwise we end up with
					// subpins with invalid types 
					check(FRigVMStruct::GetPinDirectionFromProperty(Property) == ERigVMPinDirection::Hidden);
				}
#endif
			}			
		}
		
		Index = GetTypeIndex(InType);
		if (IsValid(CPPTypeObject))
		{
			if (CPPTypeObject->IsA<UUserDefinedStruct>() || CPPTypeObject->IsA<UUserDefinedEnum>())
			{
				TRigVMTypeIndex ElementTypeIndex = GetTypeIndex(ElementType);
				// used to track name changes to user defined types, stores the element type index, see RemoveType()
				UserDefinedTypeToIndex.FindOrAdd(CPPTypeObject) = ElementTypeIndex;
			}
		}
		
		return Index;
	}
	
	return Index;
}

void FRigVMRegistry::RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory InCategory, TRigVMTypeIndex InTypeIndex)
{
	check(InCategory != FRigVMTemplateArgument::ETypeCategory_Invalid);

	TypesPerCategory.FindChecked(InCategory).Add(InTypeIndex);
}

void FRigVMRegistry::PropagateTypeAddedToCategory(const FRigVMTemplateArgument::ETypeCategory InCategory, const TRigVMTypeIndex InTypeIndex)
{
	if(bIsRefreshingEngineTypes)
	{
		return;
	}
	
	check(InCategory != FRigVMTemplateArgument::ETypeCategory_Invalid);
	if ( ensure(TypesPerCategory.FindChecked(InCategory).Contains(InTypeIndex)) )
	{
		// when adding a new type - we need to update template arguments which expect to have access to that type 
		const TArray<int32>& TemplatesToUseType = TemplatesPerCategory.FindChecked(InCategory);
		for(const int32 TemplateIndex : TemplatesToUseType)
		{
			FRigVMTemplate& Template = Templates[TemplateIndex];
			(void)Template.UpdateArgumentTypes();
		}
	}
}

bool FRigVMRegistry::RemoveType(const FSoftObjectPath& InObjectPath, const UClass* InObjectClass)
{
	if (const TRigVMTypeIndex* TypeIndexPtr = UserDefinedTypeToIndex.Find(InObjectPath))
	{
		const TRigVMTypeIndex TypeIndex = *TypeIndexPtr;
		
		UserDefinedTypeToIndex.Remove(InObjectPath);
		
		if(TypeIndex == INDEX_NONE)
		{
			return false;
		}

		check(!IsArrayType(TypeIndex));

		TArray<TRigVMTypeIndex> Indices;
		Indices.Init(INDEX_NONE, 3);
		Indices[0] = TypeIndex;
		Indices[1] = GetArrayTypeFromBaseTypeIndex(Indices[0]);

		// any type that can be removed should have 3 entries in the registry
		if (ensure(Indices[1] != INDEX_NONE))
		{
			Indices[2] = GetArrayTypeFromBaseTypeIndex(Indices[1]);
		}
		
		for (int32 ArrayDimension=0; ArrayDimension<3; ++ArrayDimension)
		{
			const TRigVMTypeIndex Index = Indices[ArrayDimension];
			
			if (Index == INDEX_NONE)
			{
				break;
			}
			
			if(InObjectClass == UUserDefinedEnum::StaticClass())
			{
				switch(ArrayDimension)
				{
				default:
				case 0:
					{
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleEnumValue, Index);
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, Index);
						break;
					}
				case 1:
					{
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayEnumValue, Index);
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, Index);
						break;
					}
				case 2:
					{
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayEnumValue, Index);
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, Index);
						break;
					}
				}
			}
			else if(InObjectClass == UUserDefinedStruct::StaticClass())
			{
				switch(ArrayDimension)
				{
				default:
				case 0:
					{
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue, Index);
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, Index);
						break;
					}
				case 1:
					{
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayScriptStructValue, Index);
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, Index);
						break;
					}
				case 2:
					{
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayScriptStructValue, Index);
						RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, Index);
						break;
					}
				}
			}

			// remove the type from the registry entirely
			TypeToIndex.Remove(GetType(Index));
			Types[Index] = FTypeInfo();
		}

		return true;	
	}
	
	return false;
}

void FRigVMRegistry::RemoveTypeInCategory(FRigVMTemplateArgument::ETypeCategory InCategory, TRigVMTypeIndex InTypeIndex)
{
	check(InCategory != FRigVMTemplateArgument::ETypeCategory_Invalid);

	TypesPerCategory.FindChecked(InCategory).Remove(InTypeIndex);

	const TArray<int32>& TemplatesToUseType = TemplatesPerCategory.FindChecked(InCategory);
	for (const int32 TemplateIndex : TemplatesToUseType)
	{
		FRigVMTemplate& Template = Templates[TemplateIndex];
		Template.HandleTypeRemoval(InTypeIndex);
	}
}

TRigVMTypeIndex FRigVMRegistry::GetTypeIndex(const FRigVMTemplateArgumentType& InType) const
{
	if(const TRigVMTypeIndex* Index = TypeToIndex.Find(InType))
	{
		return *Index;
	}
	return INDEX_NONE;
}

const FRigVMTemplateArgumentType& FRigVMRegistry::GetType(TRigVMTypeIndex InTypeIndex) const
{
	if((Types.IsValidIndex(InTypeIndex)))
	{
		return Types[InTypeIndex].Type;
	}
	static FRigVMTemplateArgumentType EmptyType;
	return EmptyType;
}

const FRigVMTemplateArgumentType& FRigVMRegistry::FindTypeFromCPPType(const FString& InCPPType) const
{
	const int32 TypeIndex = GetTypeIndexFromCPPType(InCPPType);
	if(ensure(Types.IsValidIndex(TypeIndex)))
	{
		return Types[TypeIndex].Type;
	}

	static FRigVMTemplateArgumentType EmptyType;
	return EmptyType;
}

TRigVMTypeIndex FRigVMRegistry::GetTypeIndexFromCPPType(const FString& InCPPType) const
{
	TRigVMTypeIndex Result = INDEX_NONE;
	if(!InCPPType.IsEmpty())
	{
		const FName CPPTypeName = *InCPPType;

		auto Predicate = [CPPTypeName](const FTypeInfo& Info) -> bool
		{
			return Info.Type.CPPType == CPPTypeName;
		};

		Result = Types.IndexOfByPredicate(Predicate);

#if !WITH_EDITOR
		// in game / non-editor it's possible that a user defined struct or enum 
		// has not been registered. thus we'll call RefreshEngineTypes to bring
		// things up to date here. 
		if(Result == INDEX_NONE)
		{
			// we may need ot 
			FRigVMRegistry::Get().RefreshEngineTypes();
			Result = Types.IndexOfByPredicate(Predicate);
		}
#endif

		// If not found, try to find a redirect
		if (Result == INDEX_NONE)
		{
			const FString NewCPPType = RigVMTypeUtils::PostProcessCPPType(InCPPType);
			Result = Types.IndexOfByPredicate([NewCPPType](const FTypeInfo& Info) -> bool
			{
				return Info.Type.CPPType == *NewCPPType;
			});
		}
	}
	return Result;
}

bool FRigVMRegistry::IsArrayType(TRigVMTypeIndex InTypeIndex) const
{
	if((Types.IsValidIndex(InTypeIndex)))
	{
		return Types[InTypeIndex].bIsArray;
	}
	return false;
}

bool FRigVMRegistry::IsExecuteType(TRigVMTypeIndex InTypeIndex) const
{
	if(InTypeIndex == INDEX_NONE)
	{
		return false;
	}
	
	if(ensure(Types.IsValidIndex(InTypeIndex)))
	{
		return Types[InTypeIndex].bIsExecute;
	}
	return false;
}

bool FRigVMRegistry::ConvertExecuteContextToBaseType(TRigVMTypeIndex& InOutTypeIndex) const
{
	if(InOutTypeIndex == INDEX_NONE)
	{
		return false;
	}
		
	if(InOutTypeIndex == RigVMTypeUtils::TypeIndex::Execute) 
	{
		return true;
	}

	if(!IsExecuteType(InOutTypeIndex))
	{
		return false;
	}

	// execute arguments can have various execute context types. but we always
	// convert them to the base execute type to make matching types easier later.
	// this means that the execute argument in every permutations shares 
	// the same type index of RigVMTypeUtils::TypeIndex::Execute
	if(IsArrayType(InOutTypeIndex))
	{
		InOutTypeIndex = GetArrayTypeFromBaseTypeIndex(RigVMTypeUtils::TypeIndex::Execute);
	}
	else
	{
		InOutTypeIndex = RigVMTypeUtils::TypeIndex::Execute;
	}

	return true;
}

int32 FRigVMRegistry::GetArrayDimensionsForType(TRigVMTypeIndex InTypeIndex) const
{
	if(ensure(Types.IsValidIndex(InTypeIndex)))
	{
		const FTypeInfo& Info = Types[InTypeIndex];
		if(Info.bIsArray)
		{
			return 1 + GetArrayDimensionsForType(Info.BaseTypeIndex);
		}
	}
	return 0;
}

bool FRigVMRegistry::IsWildCardType(TRigVMTypeIndex InTypeIndex) const
{
	return RigVMTypeUtils::TypeIndex::WildCard == InTypeIndex ||
		RigVMTypeUtils::TypeIndex::WildCardArray == InTypeIndex;
}

bool FRigVMRegistry::CanMatchTypes(TRigVMTypeIndex InTypeIndexA, TRigVMTypeIndex InTypeIndexB, bool bAllowFloatingPointCasts) const
{
	if(!Types.IsValidIndex(InTypeIndexA) || !Types.IsValidIndex(InTypeIndexB))
	{
		return false;
	}

	if(InTypeIndexA == InTypeIndexB)
	{
		return true;
	}

	// execute types can always be connected
	if(IsExecuteType(InTypeIndexA) && IsExecuteType(InTypeIndexB))
	{
		return GetArrayDimensionsForType(InTypeIndexA) == GetArrayDimensionsForType(InTypeIndexB);
	}

	if(bAllowFloatingPointCasts)
	{
		// swap order since float is known to registered before double
		if(InTypeIndexA > InTypeIndexB)
		{
			Swap(InTypeIndexA, InTypeIndexB);
		}
		if(InTypeIndexA == RigVMTypeUtils::TypeIndex::Float && InTypeIndexB == RigVMTypeUtils::TypeIndex::Double)
		{
			return true;
		}
		if(InTypeIndexA == RigVMTypeUtils::TypeIndex::FloatArray && InTypeIndexB == RigVMTypeUtils::TypeIndex::DoubleArray)
		{
			return true;
		}
	}
	return false;
}

const TArray<TRigVMTypeIndex>& FRigVMRegistry::GetCompatibleTypes(TRigVMTypeIndex InTypeIndex) const
{
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::Float)
	{
		static const TArray<TRigVMTypeIndex> CompatibleTypes = {RigVMTypeUtils::TypeIndex::Double};
		return CompatibleTypes;
	}
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::Double)
	{
		static const TArray<TRigVMTypeIndex> CompatibleTypes = {RigVMTypeUtils::TypeIndex::Float};
		return CompatibleTypes;
	}
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::FloatArray)
	{
		static const TArray<TRigVMTypeIndex> CompatibleTypes = {RigVMTypeUtils::TypeIndex::DoubleArray};
		return CompatibleTypes;
	}
	if(InTypeIndex == RigVMTypeUtils::TypeIndex::DoubleArray)
	{
		static const TArray<TRigVMTypeIndex> CompatibleTypes = {RigVMTypeUtils::TypeIndex::FloatArray};
		return CompatibleTypes;
	}

	static const TArray<TRigVMTypeIndex> EmptyTypes;
	return EmptyTypes;
}

const TArray<TRigVMTypeIndex>& FRigVMRegistry::GetTypesForCategory(FRigVMTemplateArgument::ETypeCategory InCategory) const
{
	check(InCategory != FRigVMTemplateArgument::ETypeCategory_Invalid);
	return TypesPerCategory.FindChecked(InCategory);
}

TRigVMTypeIndex FRigVMRegistry::GetArrayTypeFromBaseTypeIndex(TRigVMTypeIndex InTypeIndex) const
{
	if(ensure(Types.IsValidIndex(InTypeIndex)))
	{
		return Types[InTypeIndex].ArrayTypeIndex;
	}
	return INDEX_NONE;
}

TRigVMTypeIndex FRigVMRegistry::GetBaseTypeFromArrayTypeIndex(TRigVMTypeIndex InTypeIndex) const
{
	if(ensure(Types.IsValidIndex(InTypeIndex)))
	{
		return Types[InTypeIndex].BaseTypeIndex;
	}
	return INDEX_NONE;
}

bool FRigVMRegistry::IsAllowedType(const FProperty* InProperty) const
{
	if(InProperty->IsA<FBoolProperty>() ||
		InProperty->IsA<FUInt32Property>() ||
		InProperty->IsA<FInt8Property>() ||
		InProperty->IsA<FInt16Property>() ||
		InProperty->IsA<FIntProperty>() ||
		InProperty->IsA<FInt64Property>() ||
		InProperty->IsA<FFloatProperty>() ||
		InProperty->IsA<FDoubleProperty>() ||
		InProperty->IsA<FNumericProperty>() ||
		InProperty->IsA<FNameProperty>() ||
		InProperty->IsA<FStrProperty>())
	{
		return true;
	}

	if(const FArrayProperty* ArrayProperty  = CastField<FArrayProperty>(InProperty))
	{
		return IsAllowedType(ArrayProperty->Inner);
	}
	if(const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
	{
		return IsAllowedType(StructProperty->Struct);
	}
	if(const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(InProperty))
	{
		return IsAllowedType(ObjectProperty->PropertyClass);
	}
	if(const FEnumProperty* EnumProperty = CastField<FEnumProperty>(InProperty))
	{
		return IsAllowedType(EnumProperty->GetEnum());
	}
	if(const FByteProperty* ByteProperty = CastField<FByteProperty>(InProperty))
	{
		if(const UEnum* Enum = ByteProperty->Enum)
		{
			return IsAllowedType(Enum);
		}
		return true;
	}
	return false;
}

bool FRigVMRegistry::IsAllowedType(const UEnum* InEnum) const
{
	return !InEnum->HasAnyFlags(DisallowedFlags()) && InEnum->HasAllFlags(NeededFlags());
}

bool FRigVMRegistry::IsAllowedType(const UStruct* InStruct) const
{
	if(InStruct->HasAnyFlags(DisallowedFlags()) || !InStruct->HasAllFlags(NeededFlags()))
	{
		return false;
	}
	if(InStruct->IsChildOf(FRigVMStruct::StaticStruct()) &&
		!InStruct->IsChildOf(FRigVMDecorator::StaticStruct()))
	{
		return false;
	}
	if(InStruct->IsChildOf(FRigVMDispatchFactory::StaticStruct()))
	{
		return false;
	}

	// allow all user defined structs since they can always be changed to be compliant with RigVM restrictions
	if (const UUserDefinedStruct* UserDefinedStruct = Cast<UUserDefinedStruct>(InStruct))
	{
		return true;
	}

	for (TFieldIterator<FProperty> It(InStruct); It; ++It)
	{
		if(!IsAllowedType(*It))
		{
			return false;
		}
	}
	return true;
}

bool FRigVMRegistry::IsAllowedType(const UClass* InClass) const
{
	if(!InClass || InClass->HasAnyClassFlags(CLASS_Hidden))
	{
		return false;
	}

	// Only allow native object types
	if (!InClass->HasAnyClassFlags(CLASS_Native))
	{
		return false;
	}

	return AllowedClasses.Contains(InClass);
}

void FRigVMRegistry::Register(const TCHAR* InName, FRigVMFunctionPtr InFunctionPtr, UScriptStruct* InStruct, const TArray<FRigVMFunctionArgument>& InArguments)
{
	FScopeLock FunctionRegistryScopeLock(&FunctionRegistryMutex);
	
	if (FindFunction_NoLock(InName) != nullptr)
	{
		return;
	}

#if WITH_EDITOR
	FString StructureError;
	if (!FRigVMStruct::ValidateStruct(InStruct, &StructureError))
	{
		UE_LOG(LogRigVM, Error, TEXT("Failed to validate struct '%s': %s"), *InStruct->GetName(), *StructureError);
		return;
	}
#endif

	const FRigVMFunction Function(InName, InFunctionPtr, InStruct, Functions.Num(), InArguments);
	Functions.AddElement(Function);
	FunctionNameToIndex.Add(InName, Function.Index);

	// register all of the types used by the function
	for (TFieldIterator<FProperty> It(InStruct); It; ++It)
	{
		// creating the argument causes the registration
		FRigVMTemplateArgument Argument(*It);
	}

#if WITH_EDITOR
	
	FString TemplateMetadata;
	if (InStruct->GetStringMetaDataHierarchical(TemplateNameMetaName, &TemplateMetadata))
	{
		bool bIsDeprecated = InStruct->HasMetaData(FRigVMStruct::DeprecatedMetaName);
		TChunkedArray<FRigVMTemplate>& TemplateArray = (bIsDeprecated) ? DeprecatedTemplates : Templates;
		TMap<FName, int32>& NotationToIndex = (bIsDeprecated) ? DeprecatedTemplateNotationToIndex : TemplateNotationToIndex;
		
		FString MethodName;
		if (FString(InName).Split(TEXT("::"), nullptr, &MethodName))
		{
			const FString TemplateName = FString::Printf(TEXT("%s::%s"), *TemplateMetadata, *MethodName);
			FRigVMTemplate Template(InStruct, TemplateName, Function.Index);
			if (Template.IsValid())
			{
				bool bWasMerged = false;

				const int32* ExistingTemplateIndexPtr = NotationToIndex.Find(Template.GetNotation());
				if(ExistingTemplateIndexPtr)
				{
					FRigVMTemplate& ExistingTemplate = TemplateArray[*ExistingTemplateIndexPtr];
					if (ExistingTemplate.Merge(Template))
					{
						if (!bIsDeprecated)
						{
							Functions[Function.Index].TemplateIndex = ExistingTemplate.Index;
						}
						bWasMerged = true;
					}
				}

				if (!bWasMerged)
				{
					Template.Index = TemplateArray.Num();
					if (!bIsDeprecated)
					{
						Functions[Function.Index].TemplateIndex = Template.Index;
					}
					TemplateArray.AddElement(Template);
					
					if(ExistingTemplateIndexPtr == nullptr)
					{
						NotationToIndex.Add(Template.GetNotation(), Template.Index);
					}
				}
			}
		}
	}

#endif
}

const FRigVMDispatchFactory* FRigVMRegistry::RegisterFactory(UScriptStruct* InFactoryStruct)
{

	check(InFactoryStruct);
	check(InFactoryStruct != FRigVMDispatchFactory::StaticStruct());
	check(InFactoryStruct->IsChildOf(FRigVMDispatchFactory::StaticStruct()));

	// ensure to register factories only once
	const FRigVMDispatchFactory* ExistingFactory = nullptr;

	FScopeLock FactoryRegistryScopeLock(&FactoryRegistryMutex);

	const bool bFactoryAlreadyRegistered = Factories.ContainsByPredicate([InFactoryStruct, &ExistingFactory](const FRigVMDispatchFactory* Factory)
	{
		if(Factory->GetScriptStruct() == InFactoryStruct)
		{
			ExistingFactory = Factory;
			return true;
		}
		return false;
	});
	if(bFactoryAlreadyRegistered)
	{
		return ExistingFactory;
	}

#if WITH_EDITOR
	if(InFactoryStruct->HasMetaData(TEXT("Abstract")))
	{
		return nullptr;
	}
#endif
	
	FRigVMDispatchFactory* Factory = (FRigVMDispatchFactory*)FMemory::Malloc(InFactoryStruct->GetStructureSize());
	InFactoryStruct->InitializeStruct(Factory, 1);
	Factory->FactoryScriptStruct = InFactoryStruct;
	Factories.Add(Factory);
	Factory->RegisterDependencyTypes();
	return Factory;
}

void FRigVMRegistry::RegisterPredicate(UScriptStruct* InStruct, const TCHAR* InName, const TArray<FRigVMFunctionArgument>& InArguments)
{
	// Make sure the predicate does not already exist
	TArray<FRigVMFunction>& Predicates = StructNameToPredicates.FindOrAdd(InStruct->GetFName());
	if (Predicates.ContainsByPredicate([InName](const FRigVMFunction& Predicate)
	{
		return Predicate.Name == InName;
	}))
	{
		
		return;
	}

	FRigVMFunction Function(InName, nullptr, InStruct, Predicates.Num(), InArguments);
	Predicates.Add(Function);
}

void FRigVMRegistry::RegisterObjectTypes(TConstArrayView<TPair<UClass*, ERegisterObjectOperation>> InClasses)
{
	for (TPair<UClass*, ERegisterObjectOperation> ClassOpPair : InClasses)
	{
		UClass* Class = ClassOpPair.Key;
		ERegisterObjectOperation Operation = ClassOpPair.Value;

		// Only allow native object types
		if (Class->HasAnyClassFlags(CLASS_Native))
		{
			switch (Operation)
			{
			case ERegisterObjectOperation::Class:
				AllowedClasses.Add(Class);
				break;
			case ERegisterObjectOperation::ClassAndParents:
				{
					// Add all parent classes
					do
					{
						AllowedClasses.Add(Class);
						Class = Class->GetSuperClass();
					} while (Class);
					break;
				}
			case ERegisterObjectOperation::ClassAndChildren:
				{
					// Add all child classes
					TArray<UClass*> DerivedClasses({ Class });
					GetDerivedClasses(Class, DerivedClasses, /*bRecursive=*/true);
					for (UClass* DerivedClass : DerivedClasses)
					{
						AllowedClasses.Add(DerivedClass);
					}
					break;
				}
			}

		}
	}
}

const FRigVMFunction* FRigVMRegistry::FindFunction(const TCHAR* InName, const FRigVMUserDefinedTypeResolver& InTypeResolver) const
{
	FScopeLock FunctionRegistryScopeLock(&FunctionRegistryMutex);
	return FindFunction_NoLock(InName, InTypeResolver);
}

const FRigVMFunction* FRigVMRegistry::FindFunction_NoLock(const TCHAR* InName, const FRigVMUserDefinedTypeResolver& InTypeResolver) const
{
	// Check first if the function is provided by internally registered rig units. 
	if(const int32* FunctionIndexPtr = FunctionNameToIndex.Find(InName))
	{
		return &Functions[*FunctionIndexPtr];
	}

	// Otherwise ask the associated dispatch factory for a function matching this signature.
	const FString NameString(InName);
	FString StructOrFactoryName, SuffixString;
	if(NameString.Split(TEXT("::"), &StructOrFactoryName, &SuffixString))
	{
		// if the factory has never been registered - FindDispatchFactory will try to look it up and register
		if(const FRigVMDispatchFactory* Factory = FindDispatchFactory(*StructOrFactoryName))
		{
			if(const FRigVMTemplate* Template = Factory->GetTemplate())
			{
				const FRigVMTemplateTypeMap ArgumentTypes = Template->GetArgumentTypesFromString(SuffixString, &InTypeResolver);
				if(ArgumentTypes.Num() == Template->NumArguments())
				{
					const int32 PermutationIndex = Template->FindPermutation(ArgumentTypes);
					if(PermutationIndex != INDEX_NONE)
					{
						return ((FRigVMTemplate*)Template)->GetOrCreatePermutation_NoLock(PermutationIndex);
					}
				}
			}
		}
	}

	// if we haven't been able to find the function - try to see if we can get the dispatch or rigvmstruct
	// from a core redirect
	if(!StructOrFactoryName.IsEmpty())
	{
		static const FString StructPrefix = TEXT("F");
		const bool bIsDispatchFactory = StructOrFactoryName.StartsWith(FRigVMDispatchFactory::DispatchPrefix, ESearchCase::CaseSensitive);
		if(bIsDispatchFactory)
		{
			StructOrFactoryName = StructOrFactoryName.Mid(FRigVMDispatchFactory::DispatchPrefix.Len());
		}
		else if(StructOrFactoryName.StartsWith(StructPrefix, ESearchCase::CaseSensitive))
		{
			StructOrFactoryName = StructOrFactoryName.Mid(StructPrefix.Len());
		}
		
		const FCoreRedirectObjectName OldObjectName(StructOrFactoryName);
		TArray<const FCoreRedirect*> Redirects;
		if(FCoreRedirects::GetMatchingRedirects(ECoreRedirectFlags::Type_Struct, OldObjectName, Redirects, ECoreRedirectMatchFlags::AllowPartialMatch))
		{
			for(const FCoreRedirect* Redirect : Redirects)
			{
				FString NewStructOrFactoryName = Redirect->NewName.ObjectName.ToString();
				if(bIsDispatchFactory)
				{
					NewStructOrFactoryName = FRigVMDispatchFactory::DispatchPrefix + NewStructOrFactoryName;
				}
				else
				{
					NewStructOrFactoryName = StructPrefix + NewStructOrFactoryName;
				}
				const FRigVMFunction* RedirectedFunction = FindFunction_NoLock(*(NewStructOrFactoryName + TEXT("::") + SuffixString), InTypeResolver);
				if(RedirectedFunction)
				{
					FRigVMRegistry& MutableRegistry = FRigVMRegistry::Get();
					MutableRegistry.FunctionNameToIndex.Add(InName, RedirectedFunction->Index);
					return RedirectedFunction;
				}
			}
		}
	}
	
	return nullptr;
}

const FRigVMFunction* FRigVMRegistry::FindFunction(UScriptStruct* InStruct, const TCHAR* InName, const FRigVMUserDefinedTypeResolver& InResolvalInfo) const
{
	check(InStruct);
	check(InName);
	
	const FString FunctionName = FString::Printf(TEXT("%s::%s"), *InStruct->GetStructCPPName(), InName);
	return FindFunction(*FunctionName, InResolvalInfo);
}

const TChunkedArray<FRigVMFunction>& FRigVMRegistry::GetFunctions() const
{
	return Functions;
}

const FRigVMTemplate* FRigVMRegistry::FindTemplate(const FName& InNotation, bool bIncludeDeprecated) const
{
	if (InNotation.IsNone())
	{
		return nullptr;
	}

	FScopeLock TemplateRegistryScopeLock(&TemplateRegistryMutex);
	return FindTemplate_NoLock(InNotation, bIncludeDeprecated);
}

const FRigVMTemplate* FRigVMRegistry::FindTemplate_NoLock(const FName& InNotation, bool bIncludeDeprecated) const
{
	if (InNotation.IsNone())
	{
		return nullptr;
	}

	if(const int32* TemplateIndexPtr = TemplateNotationToIndex.Find(InNotation))
	{
		return &Templates[*TemplateIndexPtr];
	}

	const FString NotationString(InNotation.ToString());
	FString FactoryName, ArgumentsString;
	if(NotationString.Split(TEXT("("), &FactoryName, &ArgumentsString))
	{
		FRigVMRegistry* MutableThis = (FRigVMRegistry*)this;
		
		// deal with a couple of custom cases
		static const TMap<FString, FString> CoreDispatchMap =
		{
			{
				TEXT("Equals::Execute"),
				MutableThis->FindOrAddDispatchFactory<FRigVMDispatch_CoreEquals>()->GetFactoryName().ToString()
			},
			{
				TEXT("NotEquals::Execute"),
				MutableThis->FindOrAddDispatchFactory<FRigVMDispatch_CoreNotEquals>()->GetFactoryName().ToString()
			},
		};

		if(const FString* RemappedDispatch = CoreDispatchMap.Find(FactoryName))
		{
			FactoryName = *RemappedDispatch;
		}
		
		if(const FRigVMDispatchFactory* Factory = FindDispatchFactory(*FactoryName))
		{
			return Factory->GetTemplate();
		}
	}

	if (bIncludeDeprecated)
	{
		if(const int32* TemplateIndexPtr = DeprecatedTemplateNotationToIndex.Find(InNotation))
		{
			return &DeprecatedTemplates[*TemplateIndexPtr];
		}
	}

	const FString OriginalNotation = InNotation.ToString();

	// we may have a dispatch factory which has to be redirected
#if WITH_EDITOR
	if(OriginalNotation.StartsWith(FRigVMDispatchFactory::DispatchPrefix))
	{
		const FString OriginalDispatchFactoryName = OriginalNotation
			.Left(OriginalNotation.Find(TEXT("(")))
			.RightChop(FRigVMDispatchFactory::DispatchPrefix.Len());

		const FCoreRedirectObjectName OldObjectName(OriginalDispatchFactoryName);
		TArray<const FCoreRedirect*> Redirects;
		if(FCoreRedirects::GetMatchingRedirects(ECoreRedirectFlags::Type_Struct, OldObjectName, Redirects, ECoreRedirectMatchFlags::AllowPartialMatch))
		{
			for(const FCoreRedirect* Redirect : Redirects)
			{
				const FString NewDispatchFactoryName = FRigVMDispatchFactory::DispatchPrefix + Redirect->NewName.ObjectName.ToString();
				if(const FRigVMDispatchFactory* NewDispatchFactory = FindDispatchFactory(*NewDispatchFactoryName))
				{
					return NewDispatchFactory->GetTemplate();
				}
			}
		}
	}
#endif

	// if we still arrive here we may have a template that used to contain an executecontext.
	{
		FString SanitizedNotation = OriginalNotation;

		static const TArray<TPair<FString, FString>> ExecuteContextArgs = {
			{ TEXT("FRigUnit_SequenceExecution::Execute(in ExecuteContext,out A,out B,out C,out D)"), TEXT("FRigUnit_SequenceExecution::Execute()") },
			{ TEXT("FRigUnit_SequenceAggregate::Execute(in ExecuteContext,out A,out B)"), TEXT("FRigUnit_SequenceAggregate::Execute()") },
			{ TEXT(",io ExecuteContext"), TEXT("") },
			{ TEXT("io ExecuteContext,"), TEXT("") },
			{ TEXT("(io ExecuteContext)"), TEXT("()") },
			{ TEXT(",out ExecuteContext"), TEXT("") },
			{ TEXT("out ExecuteContext,"), TEXT("") },
			{ TEXT("(out ExecuteContext)"), TEXT("()") },
			{ TEXT(",out Completed"), TEXT("") },
			{ TEXT("out Completed,"), TEXT("") },
			{ TEXT("(out Completed)"), TEXT("()") },
		};

		for(int32 Index = 0; Index < ExecuteContextArgs.Num(); Index++)
		{
			const TPair<FString, FString>& Pair = ExecuteContextArgs[Index];
			if(SanitizedNotation.Contains(Pair.Key))
			{
				SanitizedNotation = SanitizedNotation.Replace(*Pair.Key, *Pair.Value);
			}
		}

		if(SanitizedNotation != OriginalNotation)
		{
			return FindTemplate_NoLock(*SanitizedNotation, bIncludeDeprecated);
		}
	}

	return nullptr;
}

const TChunkedArray<FRigVMTemplate>& FRigVMRegistry::GetTemplates() const
{
	return Templates;
}

const FRigVMTemplate* FRigVMRegistry::GetOrAddTemplateFromArguments(const FName& InName, const TArray<FRigVMTemplateArgumentInfo>& InInfos, const FRigVMTemplateDelegates& InDelegates)
{
	FScopeLock TemplateRegistryScopeLock(&TemplateRegistryMutex);
	
	// avoid reentry in FindTemplate. try to find an existing
	// template only if we are not yet in ::FindTemplate.
	const FName Notation = FRigVMTemplateArgumentInfo::ComputeTemplateNotation(InName, InInfos);
	if(const FRigVMTemplate* ExistingTemplate = FindTemplate(Notation))
	{
		return ExistingTemplate;
	}

	return AddTemplateFromArguments_NoLock(InName, InInfos, InDelegates);
}

const FRigVMTemplate* FRigVMRegistry::AddTemplateFromArguments(const FName& InName, const TArray<FRigVMTemplateArgumentInfo>& InInfos, const FRigVMTemplateDelegates& InDelegates)
{
	FScopeLock TemplateRegistryScopeLock(&TemplateRegistryMutex);
	return AddTemplateFromArguments_NoLock(InName, InInfos, InDelegates);
}

const FRigVMTemplate* FRigVMRegistry::AddTemplateFromArguments_NoLock(const FName& InName, const TArray<FRigVMTemplateArgumentInfo>& InInfos, const FRigVMTemplateDelegates& InDelegates)
{
	// we only support to ask for templates here which provide singleton types
	int32 NumPermutations = 0;
	FRigVMTemplate Template(InName, InInfos);
	for(const FRigVMTemplateArgument& Argument : Template.Arguments)
	{
		const int32 NumIndices = Argument.GetNumTypes();
		if(!Argument.IsSingleton() && NumPermutations > 1)
		{
			if(NumIndices != NumPermutations)
			{
				UE_LOG(LogRigVM, Error, TEXT("Failed to add template '%s' since the arguments' types counts don't match."), *InName.ToString());
				return nullptr;
			}
		}
		NumPermutations = FMath::Max(NumPermutations, NumIndices); 
	}

	// if any of the arguments are wildcards we'll need to update the types
	for(FRigVMTemplateArgument& Argument : Template.Arguments)
	{
		if(Argument.GetNumTypes() == 1 && IsWildCardType(Argument.GetTypeIndex(0)))
		{
			Argument.InvalidatePermutations(Argument.GetTypeIndex(0));
			if(IsArrayType(Argument.GetTypeIndex(0)))
			{
				Argument.TypeCategories.Add(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue);
			}
			else
			{
				Argument.TypeCategories.Add(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue);
			}
			Argument.bUseCategories = true;
			Argument.TypeIndices.Reset();
	
			NumPermutations = FMath::Max(NumPermutations, Argument.GetNumTypes()); 
		}
	}

	// Remove duplicate permutations
	{
		TArray<int32> ToRemove;
		
		const int32 NumArguments = Template.Arguments.Num();
		if (NumArguments == 1)
		{
			TSet< TRigVMTypeIndex > PermutationTypes; PermutationTypes.Reserve(NumPermutations);
			for(int32 Index = 0; Index < NumPermutations; Index++)
			{
				const TRigVMTypeIndex ArgType = Template.Arguments[0].GetTypeIndex(Index);
				if (PermutationTypes.Contains(ArgType))
				{
					ToRemove.Add(Index);
				}
				else
				{
					PermutationTypes.Add(ArgType);
				}
			}
		}
		else
		{
			TSet< TArray<TRigVMTypeIndex> > PermutationTypes;
			PermutationTypes.Reserve(NumPermutations);
			TArray<TRigVMTypeIndex> ArgTypes; ArgTypes.SetNum(NumArguments);			
			for(int32 Index = 0; Index < NumPermutations; Index++)
			{
				for(int32 ArgIndex = 0; ArgIndex < NumArguments; ArgIndex++)
				{
					ArgTypes[ArgIndex] = Template.Arguments[ArgIndex].GetTypeIndex(Index);
				}
				
				if (PermutationTypes.Contains(ArgTypes))
				{
					ToRemove.Add(Index);
				}
				else
				{
					PermutationTypes.Add(ArgTypes);
				}
			}
		}
		
		for (int32 i=ToRemove.Num()-1; i>=0; --i)
		{
			for(FRigVMTemplateArgument& Argument : Template.Arguments)
			{
				if (Argument.TypeIndices.IsValidIndex(ToRemove[i]))
				{
					Argument.TypeIndices.RemoveAt(ToRemove[i]);
				}
			}
		}
		NumPermutations -= ToRemove.Num();
	}
	
	for(FRigVMTemplateArgument& Argument : Template.Arguments)
	{
		Argument.UpdateTypeToPermutations();
	}

	Template.Permutations.Init(INDEX_NONE, NumPermutations);
	Template.RecomputeTypesHashToPermutations();

	const int32 Index = Templates.AddElement(Template);
	Templates[Index].Index = Index;
	Templates[Index].Delegates = InDelegates;
	TemplateNotationToIndex.Add(Template.GetNotation(), Index);

	for(int32 ArgumentIndex=0; ArgumentIndex < Templates[Index].Arguments.Num(); ArgumentIndex++)
	{
		for(const FRigVMTemplateArgument::ETypeCategory& ArgumentTypeCategory : Templates[Index].Arguments[ArgumentIndex].TypeCategories)
		{
			TemplatesPerCategory.FindChecked(ArgumentTypeCategory).AddUnique(Index);
		}
	}
	
	return &Templates[Index];
}

FRigVMDispatchFactory* FRigVMRegistry::FindDispatchFactory(const FName& InFactoryName) const
{
	FScopeLock FactoryRegistryScopeLock(&FactoryRegistryMutex);
	return FindDispatchFactory_NoLock(InFactoryName);
}

FRigVMDispatchFactory* FRigVMRegistry::FindDispatchFactory_NoLock(const FName& InFactoryName) const
{
	FRigVMDispatchFactory* const* FactoryPtr = Factories.FindByPredicate([InFactoryName](const FRigVMDispatchFactory* Factory) -> bool
	{
		return Factory->GetFactoryName() == InFactoryName;
	});
	if(FactoryPtr)
	{
		return *FactoryPtr;
	}

	FString FactoryName = InFactoryName.ToString();
	
	// if the factory has never been registered - we should try to look it up	
	if(FactoryName.StartsWith(FRigVMDispatchFactory::DispatchPrefix))
	{
		const FString ScriptStructName = FactoryName.Mid(FRigVMDispatchFactory::DispatchPrefix.Len());
		if(UScriptStruct* FactoryStruct = FindFirstObject<UScriptStruct>(*ScriptStructName, EFindFirstObjectOptions::NativeFirst | EFindFirstObjectOptions::EnsureIfAmbiguous))
		{
			FRigVMRegistry* MutableThis = (FRigVMRegistry*)this;
			return (FRigVMDispatchFactory*)MutableThis->RegisterFactory(FactoryStruct);
		}
	}	
	
	return nullptr;
}

FRigVMDispatchFactory* FRigVMRegistry::FindOrAddDispatchFactory(UScriptStruct* InFactoryStruct)
{
	return (FRigVMDispatchFactory*)RegisterFactory(InFactoryStruct);
}

FString FRigVMRegistry::FindOrAddSingletonDispatchFunction(UScriptStruct* InFactoryStruct)
{
	if(const FRigVMDispatchFactory* Factory = FindOrAddDispatchFactory(InFactoryStruct))
	{
		if(Factory->IsSingleton())
		{
			if(const FRigVMTemplate* Template = Factory->GetTemplate())
			{
				// use the types for the first permutation - since we don't care
				// for a singleton dispatch
				const FRigVMTemplateTypeMap TypesForPrimaryPermutation = Template->GetTypesForPermutation(0);
				const FString Name = Factory->GetPermutationName(TypesForPrimaryPermutation);
				if(const FRigVMFunction* Function = FindFunction(*Name))
				{
					return Function->Name;
				}
			}
		}
	}
	return FString();
}

const TArray<FRigVMDispatchFactory*>& FRigVMRegistry::GetFactories() const
{	return Factories;
}

const TArray<FRigVMFunction>* FRigVMRegistry::GetPredicatesForStruct(const FName& InStructName) const
{
	return StructNameToPredicates.Find(InStructName);
}
