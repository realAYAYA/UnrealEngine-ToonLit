// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigVMCore/RigVMRegistry.h"
#include "AssetRegistry/AssetData.h"
#include "RigVMCore/RigVMStruct.h"
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

const FName FRigVMRegistry::TemplateNameMetaName = TEXT("TemplateName");
FCriticalSection FRigVMRegistry::RefreshTypesMutex;
FCriticalSection FRigVMRegistry::RegisterFunctionMutex;
FCriticalSection FRigVMRegistry::RegisterTemplateMutex;
FCriticalSection FRigVMRegistry::RegisterFactoryMutex;
FCriticalSection FRigVMRegistry::FindFunctionMutex;
FCriticalSection FRigVMRegistry::FindTemplateMutex;
FCriticalSection FRigVMRegistry::FindFactoryMutex;
FCriticalSection FRigVMRegistry::GetDispatchFunctionMutex;
FCriticalSection FRigVMRegistry::GetPermutationMutex;


// When the object system has been completely loaded, load in all the engine types that we haven't registered already in InitializeIfNeeded 
static FDelayedAutoRegisterHelper GRigVMRegistrySingletonHelper(EDelayedRegisterRunPhase::EndOfEngineInit, []() -> void
{
	FRigVMRegistry::Get().RefreshEngineTypes();
});


FRigVMRegistry::~FRigVMRegistry()
{
	Reset();
}

FRigVMRegistry& FRigVMRegistry::Get()
{
	// static in a function scope ensures that the GC system is initiated before 
	// the registry constructor is called
	static FRigVMRegistry s_RigVMRegistry;
	s_RigVMRegistry.InitializeIfNeeded();
	return s_RigVMRegistry;
}

void FRigVMRegistry::AddReferencedObjects(FReferenceCollector& Collector)
{
	// registry should hold strong references to these type objects
	// otherwise GC may remove them without the registry known it
	// which can happen during cook time.
	for (FTypeInfo& Type : Types)
	{
		if (Type.Type.CPPTypeObject)
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

void FRigVMRegistry::InitializeIfNeeded()
{
	// we don't need to use a mutex here since this is called on
	// the main thread during engine startup for the first time
	
	if(!Types.IsEmpty())
	{
		return;
	}

	Types.Reserve(512);
	TypeToIndex.Reserve(512);
	TypesPerCategory.Reserve(19);
	ArgumentsPerCategory.Reserve(19);
	
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

	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_Execute, TArray<TPair<int32,int32>>()).Reserve(8);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleSimpleValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArraySimpleValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArraySimpleValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleMathStructValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayMathStructValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayMathStructValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayScriptStructValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayScriptStructValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleEnumValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayEnumValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayEnumValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_SingleObjectValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayObjectValue, TArray<TPair<int32,int32>>()).Reserve(64);
	ArgumentsPerCategory.Add(FRigVMTemplateArgument::ETypeCategory_ArrayArrayObjectValue, TArray<TPair<int32,int32>>()).Reserve(64);

	RigVMTypeUtils::TypeIndex::Execute = FindOrAddType(FRigVMTemplateArgumentType(FRigVMExecuteContext::StaticStruct()));
	RigVMTypeUtils::TypeIndex::ExecuteArray = FindOrAddType(FRigVMTemplateArgumentType(FRigVMExecuteContext::StaticStruct()).ConvertToArray());
	RigVMTypeUtils::TypeIndex::Bool = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::BoolTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::Float = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::FloatTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::Double = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::DoubleTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::Int32 = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::Int32TypeName, nullptr));
	RigVMTypeUtils::TypeIndex::UInt8 = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::UInt8TypeName, nullptr));
	RigVMTypeUtils::TypeIndex::FName = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::FNameTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::FString = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::FStringTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::WildCard = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::GetWildCardCPPTypeName(), RigVMTypeUtils::GetWildCardCPPTypeObject()));
	RigVMTypeUtils::TypeIndex::BoolArray = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::BoolArrayTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::FloatArray = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::FloatArrayTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::DoubleArray = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::DoubleArrayTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::Int32Array = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::Int32ArrayTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::UInt8Array = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::UInt8ArrayTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::FNameArray = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::FNameArrayTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::FStringArray = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::FStringArrayTypeName, nullptr));
	RigVMTypeUtils::TypeIndex::WildCardArray = FindOrAddType(FRigVMTemplateArgumentType(RigVMTypeUtils::GetWildCardArrayCPPTypeName(), RigVMTypeUtils::GetWildCardCPPTypeObject()));

	// register the default math types
	for(UScriptStruct* MathType : GetMathTypes())
	{
		FindOrAddType(FRigVMTemplateArgumentType(MathType));
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

		UE::Anim::AttributeTypes::GetOnAttributeTypesChanged().RemoveAll(this);
	});
	
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().OnAssetRemoved().AddRaw(this, &FRigVMRegistry::OnAssetRemoved);
	AssetRegistryModule.Get().OnAssetRenamed().AddRaw(this, &FRigVMRegistry::OnAssetRenamed);

	UE::Anim::AttributeTypes::GetOnAttributeTypesChanged().AddRaw(this, &FRigVMRegistry::OnAnimationAttributeTypesChanged);
}

void FRigVMRegistry::RefreshEngineTypes()
{
	FScopeLock RefreshTypesScopeLock(&RefreshTypesMutex);
	
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
			FindOrAddType(FRigVMTemplateArgumentType(ScriptStruct));
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
			FindOrAddType(FRigVMTemplateArgumentType(*CPPType, Enum));
		}
	}
	
	// Register all dispatch factories only after all other types have been registered.
	for (UScriptStruct* DispatchFactoryStruct: DispatchFactoriesToRegister)
	{
		RegisterFactory(DispatchFactoryStruct);
	}
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
	if (RemoveType(InAssetData))
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

TRigVMTypeIndex FRigVMRegistry::FindOrAddType_Internal(const FRigVMTemplateArgumentType& InType, bool bForce)
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
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleSimpleValue, Index);
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, Index);
						break;
					}
					case 1:
					{
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArraySimpleValue, Index);
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, Index);
						break;
					}
					case 2:
					{
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArraySimpleValue, Index);
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, Index);
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
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleObjectValue, Index);
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, Index);
						break;
					}
					case 1:
					{
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayObjectValue, Index);
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, Index);
						break;
					}
					case 2:
					{
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayObjectValue, Index);
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, Index);
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
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleEnumValue, Index);
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, Index);
						break;
					}
					case 1:
					{
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayEnumValue, Index);
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, Index);
						break;
					}
					case 2:
					{
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayEnumValue, Index);
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, Index);
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
						RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_Execute, Index);
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
								RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleMathStructValue, Index);
								break;
							}
							case 1:
							{
								RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayMathStructValue, Index);
								break;
							}
							case 2:
							{
								RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayMathStructValue, Index);
								break;
							}
						}
					}
					
					switch(ArrayDimension)
					{
						default:
						case 0:
						{
							RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleScriptStructValue, Index);
							RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue, Index);
							break;
						}
						case 1:
						{
							RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayScriptStructValue, Index);
							RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue, Index);
							break;
						}
						case 2:
						{
							RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayScriptStructValue, Index);
							RegisterTypeInCategory(FRigVMTemplateArgument::ETypeCategory_ArrayArrayAnyValue, Index);
							break;
						}
					}
				}
			}
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
					FRigVMTemplateArgument DummyArgument(Property);
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
				// used to track name changes to user defined types
				UserDefinedTypeToIndex.FindOrAdd(CPPTypeObject) = Index;
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

	// when adding a new type - we need to update template arguments which expect to have access to that type 
	const TArray<TPair<int32,int32>>& ArgumentsToUseType = ArgumentsPerCategory.FindChecked(InCategory);
	for(const TPair<int32,int32>& Pair : ArgumentsToUseType)
	{
		FRigVMTemplate& Template = Templates[Pair.Key];
		const FRigVMTemplateArgument* Argument = Template.GetArgument(Pair.Value);
		Template.AddTypeForArgument(Argument->GetName(), InTypeIndex);
	}
}

bool FRigVMRegistry::RemoveType(const FAssetData& InAssetData)
{
	const FSoftObjectPath AssetPath = InAssetData.ToSoftObjectPath();
	const UClass* TypeClass = InAssetData.GetClass();
	
	if (const TRigVMTypeIndex* TypeIndexPtr = UserDefinedTypeToIndex.Find(AssetPath))
	{
		const TRigVMTypeIndex TypeIndex = *TypeIndexPtr;
		
		UserDefinedTypeToIndex.Remove(AssetPath);
		
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
			
			if(TypeClass == UUserDefinedEnum::StaticClass())
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
			else if(TypeClass == UUserDefinedStruct::StaticClass())
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

	const TArray<TPair<int32,int32>>& ArgumentsToUseType = ArgumentsPerCategory.FindChecked(InCategory);

	TSet<int32> TemplatesToUseType;
	
	for(const TPair<int32,int32>& Pair : ArgumentsToUseType)
	{
		TemplatesToUseType.Add(Pair.Key);
	}
	
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
	if(ensure(Types.IsValidIndex(InTypeIndex)))
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
	if(ensure(!InCPPType.IsEmpty()))
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
	if(ensure(Types.IsValidIndex(InTypeIndex)))
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

bool FRigVMRegistry::IsAllowedType(const FProperty* InProperty)
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

bool FRigVMRegistry::IsAllowedType(const UEnum* InEnum)
{
	return !InEnum->HasAnyFlags(DisallowedFlags()) && InEnum->HasAllFlags(NeededFlags());
}

bool FRigVMRegistry::IsAllowedType(const UStruct* InStruct)
{
	if(InStruct->HasAnyFlags(DisallowedFlags()) || !InStruct->HasAllFlags(NeededFlags()))
	{
		return false;
	}
	if(InStruct->IsChildOf(FRigVMStruct::StaticStruct()))
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

bool FRigVMRegistry::IsAllowedType(const UClass* InClass)
{
	if(InClass->HasAnyClassFlags(CLASS_Hidden | CLASS_Abstract))
	{
		return false;
	}

	// note: currently we don't allow UObjects
	return false;
	//return IsAllowedType(Cast<UStruct>(InClass));
}


void FRigVMRegistry::Register(const TCHAR* InName, FRigVMFunctionPtr InFunctionPtr, UScriptStruct* InStruct, const TArray<FRigVMFunctionArgument>& InArguments)
{
	FScopeLock RegisterFunctionScopeLock(&RegisterFunctionMutex);
	
	if (FindFunction(InName) != nullptr)
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
	FScopeLock RegisterFactoryScopeLock(&RegisterFactoryMutex);

	check(InFactoryStruct);
	check(InFactoryStruct != FRigVMDispatchFactory::StaticStruct());
	check(InFactoryStruct->IsChildOf(FRigVMDispatchFactory::StaticStruct()));

	// ensure to register factories only once
	const FRigVMDispatchFactory* ExistingFactory = nullptr;
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

const FRigVMFunction* FRigVMRegistry::FindFunction(const TCHAR* InName) const
{
	FScopeLock FindFunctionScopeLock(&FindFunctionMutex);
	return FindFunction_NoLock(InName);
}

const FRigVMFunction* FRigVMRegistry::FindFunction_NoLock(const TCHAR* InName) const
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
				const FRigVMTemplateTypeMap ArgumentTypes = Template->GetArgumentTypesFromString(SuffixString);
				if(ArgumentTypes.Num() == Template->NumArguments())
				{
					const int32 PermutationIndex = Template->FindPermutation(ArgumentTypes);
					if(PermutationIndex != INDEX_NONE)
					{
						return ((FRigVMTemplate*)Template)->GetOrCreatePermutation(PermutationIndex);
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
				const FRigVMFunction* RedirectedFunction = FindFunction_NoLock(*(NewStructOrFactoryName + TEXT("::") + SuffixString));
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

const FRigVMFunction* FRigVMRegistry::FindFunction(UScriptStruct* InStruct, const TCHAR* InName) const
{
	check(InStruct);
	check(InName);
	
	const FString FunctionName = FString::Printf(TEXT("%s::%s"), *InStruct->GetStructCPPName(), InName);
	return FindFunction(*FunctionName);
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

	FScopeLock FindTemplateScopeLock(&FindTemplateMutex);
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

const FRigVMTemplate* FRigVMRegistry::GetOrAddTemplateFromArguments(const FName& InName, const TArray<FRigVMTemplateArgument>& InArguments, const FRigVMTemplateDelegates& InDelegates)
{
	FScopeLock RegisterTemplateScopeLock(&RegisterTemplateMutex);
	
	FRigVMTemplate Template(InName, InArguments, INDEX_NONE);

	// avoid reentry in FindTemplate. try to find an existing
	// template only if we are not yet in ::FindTemplate.
	if(const FRigVMTemplate* ExistingTemplate = FindTemplate(Template.GetNotation()))
	{
		return ExistingTemplate;
	}

	return AddTemplateFromArguments_NoLock(InName, InArguments, InDelegates);
}

const FRigVMTemplate* FRigVMRegistry::AddTemplateFromArguments(const FName& InName, const TArray<FRigVMTemplateArgument>& InArguments, const FRigVMTemplateDelegates& InDelegates)
{
	FScopeLock RegisterTemplateScopeLock(&RegisterTemplateMutex);
	return AddTemplateFromArguments_NoLock(InName, InArguments, InDelegates);
}

const FRigVMTemplate* FRigVMRegistry::AddTemplateFromArguments_NoLock(const FName& InName, const TArray<FRigVMTemplateArgument>& InArguments, const FRigVMTemplateDelegates& InDelegates)
{
	FRigVMTemplate Template(InName, InArguments, INDEX_NONE);
	
	// we only support to ask for templates here which provide singleton types
	int32 NumPermutations = 1;
	for(const FRigVMTemplateArgument& Argument : InArguments)
	{
		if(!Argument.IsSingleton() && NumPermutations > 1)
		{
			if(Argument.TypeIndices.Num() != NumPermutations)
			{
				UE_LOG(LogRigVM, Error, TEXT("Failed to add template '%s' since the arguments' types counts don't match."), *InName.ToString());
				return nullptr;
			}
		}
		NumPermutations = FMath::Max(NumPermutations, Argument.TypeIndices.Num()); 
	}

	// if any of the arguments are wildcards we'll need to update the types
	for(FRigVMTemplateArgument& Argument : Template.Arguments)
	{
		if(Argument.TypeIndices.Num() == 1 && IsWildCardType(Argument.TypeIndices[0]))
		{
			if(IsArrayType(Argument.TypeIndices[0]))
			{
				Argument.TypeIndices = GetTypesForCategory(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue);
				Argument.TypeCategories.Add(FRigVMTemplateArgument::ETypeCategory_ArrayAnyValue);
			}
			else
			{
				Argument.TypeIndices = GetTypesForCategory(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue);
				Argument.TypeCategories.Add(FRigVMTemplateArgument::ETypeCategory_SingleAnyValue);
			}

			NumPermutations = FMath::Max(NumPermutations, Argument.TypeIndices.Num()); 
		}
	}

	// if we have more than one permutation we may need to upgrade the types for singleton args
	if(NumPermutations > 1)
	{
		for(FRigVMTemplateArgument& Argument : Template.Arguments)
		{
			if(Argument.TypeIndices.Num() == 1)
			{
				const int32 TypeIndex = Argument.TypeIndices[0];
				Argument.TypeIndices.SetNum(NumPermutations);
				for(int32 Index=0;Index<NumPermutations;Index++)
				{
					Argument.TypeIndices[Index] = TypeIndex;
				}
			}
		}
	}

	// Remove duplicate permutations
	{
		TArray<int32> ToRemove;
		TArray<TArray<TRigVMTypeIndex>> PermutationTypes;
		PermutationTypes.Reserve(NumPermutations);
		for(int32 Index=0;Index<NumPermutations;Index++)
		{
			TArray<TRigVMTypeIndex> ArgTypes;
			ArgTypes.Reserve(Template.Arguments.Num());
			for(FRigVMTemplateArgument& Argument : Template.Arguments)
			{
				ArgTypes.Add(Argument.TypeIndices[Index]);
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
		for (int32 i=ToRemove.Num()-1; i>=0; --i)
		{
			for(FRigVMTemplateArgument& Argument : Template.Arguments)
			{
				Argument.TypeIndices.RemoveAt(ToRemove[i]);
			}
		}
		NumPermutations -= ToRemove.Num();
	}
	
	for(FRigVMTemplateArgument& Argument : Template.Arguments)
	{
		Argument.UpdateTypeToPermutations();
	}

	Template.Permutations.SetNum(NumPermutations);
	for(int32 Index=0;Index<NumPermutations;Index++)
	{
		Template.Permutations[Index] = INDEX_NONE;
	}
	Template.RecomputeTypesHashToPermutations();

	const int32 Index = Templates.AddElement(Template);
	Templates[Index].Index = Index;
	Templates[Index].Delegates = InDelegates;
	TemplateNotationToIndex.Add(Template.GetNotation(), Index);

	for(int32 ArgumentIndex=0; ArgumentIndex < Templates[Index].Arguments.Num(); ArgumentIndex++)
	{
		for(const FRigVMTemplateArgument::ETypeCategory& ArgumentTypeCategory : Templates[Index].Arguments[ArgumentIndex].TypeCategories)
		{
			ArgumentsPerCategory.FindChecked(ArgumentTypeCategory).AddUnique(TPair<int32, int32>(Index, ArgumentIndex));
		}
	}
	
	return &Templates[Index];
}

FRigVMDispatchFactory* FRigVMRegistry::FindDispatchFactory(const FName& InFactoryName) const
{
	FScopeLock FindFactoryScopeLock(&FindFactoryMutex);
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
{
	return Factories;
}
