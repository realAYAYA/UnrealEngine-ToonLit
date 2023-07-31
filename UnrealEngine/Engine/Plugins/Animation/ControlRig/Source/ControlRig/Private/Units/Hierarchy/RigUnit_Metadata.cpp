// Copyright Epic Games, Inc. All Rights Reserved.

#include "RigUnit_Metadata.h"
#include "RigVMTypeUtils.h"
#include "RigVMCore/RigVMStruct.h"
#include "Units/RigUnitContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(RigUnit_Metadata)

#if WITH_EDITOR
#include "RigVMModel/RigVMPin.h"
#endif

FName FRigDispatch_MetadataBase::ItemArgName = TEXT("Item");
FName FRigDispatch_MetadataBase::NameArgName = TEXT("Name");
FName FRigDispatch_MetadataBase::CacheArgName = TEXT("Cache");
FName FRigDispatch_MetadataBase::DefaultArgName = TEXT("Default");
FName FRigDispatch_MetadataBase::ValueArgName = TEXT("Value");
FName FRigDispatch_MetadataBase::FoundArgName = TEXT("Found");
FName FRigDispatch_MetadataBase::SuccessArgName = TEXT("Success");

#if WITH_EDITOR

FString FRigDispatch_MetadataBase::GetNodeTitle(const FRigVMTemplateTypeMap& InTypes) const
{
	if(const TRigVMTypeIndex* ValueTypeIndexPtr = InTypes.Find(ValueArgName))
	{
		const TRigVMTypeIndex& ValueTypeIndex = *ValueTypeIndexPtr;
		if(ValueTypeIndex != RigVMTypeUtils::TypeIndex::WildCard &&
			ValueTypeIndex != RigVMTypeUtils::TypeIndex::WildCardArray)
		{
			static constexpr TCHAR GetMetadataFormat[] = TEXT("Get %s Metadata");
			static constexpr TCHAR SetMetadataFormat[] = TEXT("Set %s Metadata");

			const FRigVMTemplateArgumentType& ValueType = FRigVMRegistry::Get().GetType(ValueTypeIndex);
			FString ValueName;
			if(ValueType.CPPTypeObject == FRigElementKey::StaticStruct())
			{
				ValueName = ItemArgName.ToString();
			}
			else if(ValueType.CPPTypeObject)
			{
				ValueName = ValueType.CPPTypeObject->GetName();
			}
			else if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::FName)
			{
				ValueName = NameArgName.ToString();
			}
			else
			{
				ValueName = ValueType.GetBaseCPPType();
				ValueName = ValueName.Left(1).ToUpper() + ValueName.Mid(1);
			}

			if(ValueType.IsArray())
			{
				ValueName += TEXT(" Array");
			}

			return FString::Printf(IsSetMetadata() ? SetMetadataFormat : GetMetadataFormat, *ValueName); 
		}
	}
	return FRigDispatchFactory::GetNodeTitle(InTypes);
}

#endif

TArray<FRigVMTemplateArgument> FRigDispatch_MetadataBase::GetArguments() const
{
	if(Arguments.IsEmpty())
	{
		if(IsSetMetadata())
		{
			ExecuteArgIndex = Arguments.Emplace(FRigVMStruct::ExecuteContextName, ERigVMPinDirection::IO, FRigVMRegistry::Get().GetTypeIndex<FControlRigExecuteContext>());
		}
		ItemArgIndex = Arguments.Emplace(ItemArgName, ERigVMPinDirection::Input, FRigVMRegistry::Get().GetTypeIndex<FRigElementKey>());
		NameArgIndex = Arguments.Emplace(NameArgName, ERigVMPinDirection::Input, RigVMTypeUtils::TypeIndex::FName);
		CacheArgIndex = Arguments.Emplace(CacheArgName, ERigVMPinDirection::Hidden, FRigVMRegistry::Get().GetTypeIndex<FCachedRigElement>());
	};
	return Arguments;
}

#if WITH_EDITOR

FText FRigDispatch_MetadataBase::GetArgumentTooltip(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const
{
	if(InArgumentName == ItemArgName)
	{
		return NSLOCTEXT("FRigDispatch_MetadataBase", "ItemArgTooltip", "The item storing the metadata");
	}
	if(InArgumentName == NameArgName)
	{
		return NSLOCTEXT("FRigDispatch_MetadataBase", "NameArgTooltip", "The name of the metadata");
	}
	if(InArgumentName == DefaultArgName)
	{
		return NSLOCTEXT("FRigDispatch_MetadataBase", "DefaultArgTooltip", "The default value used as a fallback if the metadata does not exist");
	}
	if(InArgumentName == ValueArgName)
	{
		return NSLOCTEXT("FRigDispatch_MetadataBase", "ValueArgTooltip", "The value to get / set");
	}
	if(InArgumentName == FoundArgName)
	{
		return NSLOCTEXT("FRigDispatch_MetadataBase", "FoundArgTooltip", "Returns true if the metadata exists with the specific type");
	}
	if(InArgumentName == SuccessArgName)
	{
		return NSLOCTEXT("FRigDispatch_MetadataBase", "SuccessArgTooltip", "Returns true if the metadata was successfully stored");
	}
	return FRigDispatchFactory::GetArgumentTooltip(InArgumentName, InTypeIndex);
}

FString FRigDispatch_MetadataBase::GetArgumentMetaData(const FName& InArgumentName, const FName& InMetaDataKey) const
{
	if(InArgumentName == TEXT("Name"))
	{
		if(InMetaDataKey == FRigVMStruct::CustomWidgetMetaName)
		{
			return TEXT("MetadataName");
		}
	}
	return Super::GetArgumentMetaData(InArgumentName, InMetaDataKey);
}

#endif

const TArray<TRigVMTypeIndex>& FRigDispatch_MetadataBase::GetValueTypes() const
{
	static TArray<TRigVMTypeIndex> Types;
	if(Types.IsEmpty())
	{
		const FRigVMRegistry& Registry = FRigVMRegistry::Get();
		Types = {
			RigVMTypeUtils::TypeIndex::Bool,
			RigVMTypeUtils::TypeIndex::Float,
			RigVMTypeUtils::TypeIndex::Int32,
			RigVMTypeUtils::TypeIndex::FName,
			Registry.GetTypeIndex<FVector>(false),
			Registry.GetTypeIndex<FRotator>(false),
			Registry.GetTypeIndex<FQuat>(false),
			Registry.GetTypeIndex<FTransform>(false),
			Registry.GetTypeIndex<FLinearColor>(false),
			Registry.GetTypeIndex<FRigElementKey>(false),
			RigVMTypeUtils::TypeIndex::BoolArray,
			RigVMTypeUtils::TypeIndex::FloatArray,
			RigVMTypeUtils::TypeIndex::Int32Array,
			RigVMTypeUtils::TypeIndex::FNameArray,
			Registry.GetTypeIndex<FVector>(true),
			Registry.GetTypeIndex<FRotator>(true),
			Registry.GetTypeIndex<FQuat>(true),
			Registry.GetTypeIndex<FTransform>(true),
			Registry.GetTypeIndex<FLinearColor>(true),
			Registry.GetTypeIndex<FRigElementKey>(true)
		};
	}
	return Types;
}

////////////////////////////////////////////////////////////////////////////////////////////////

TArray<FRigVMTemplateArgument> FRigDispatch_GetMetadata::GetArguments() const
{
	if(ValueArgIndex == INDEX_NONE)
	{
		Arguments = Super::GetArguments(); 
		DefaultArgIndex = Arguments.Emplace(DefaultArgName, ERigVMPinDirection::Input, GetValueTypes());
		ValueArgIndex = Arguments.Emplace(ValueArgName, ERigVMPinDirection::Output, GetValueTypes());
		FoundArgIndex = Arguments.Emplace(FoundArgName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	};
	return Arguments;
}

FRigBaseMetadata* FRigDispatch_GetMetadata::FindMetadata(const FRigVMExtendedExecuteContext& InContext,
                                                         const FRigElementKey& InKey, const FName& InName, ERigMetadataType InType, FCachedRigElement& Cache)
{
	const FRigUnitContext& Context = GetRigUnitContext(InContext);
	if(Cache.UpdateCache(InKey, Context.Hierarchy))
	{
		if(FRigBaseElement* Element = Context.Hierarchy->Get(Cache.GetIndex()))
		{
			return Element->GetMetadata(InName, InType);
		}
	}
	return nullptr;
}

FRigVMFunctionPtr FRigDispatch_GetMetadata::GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	const TRigVMTypeIndex& ValueTypeIndex = InTypes.FindChecked(TEXT("Value"));
	
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::Bool)
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<bool, FRigBoolMetadata, ERigMetadataType::Bool>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::Float)
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<float, FRigFloatMetadata, ERigMetadataType::Float>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::Int32)
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<int32, FRigInt32Metadata, ERigMetadataType::Int32>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::FName)
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<FName, FRigNameMetadata, ERigMetadataType::Name>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FVector>(false))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<FVector, FRigVectorMetadata, ERigMetadataType::Vector>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FRotator>(false))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<FRotator, FRigRotatorMetadata, ERigMetadataType::Rotator>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FQuat>(false))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<FQuat, FRigQuatMetadata, ERigMetadataType::Quat>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FTransform>(false))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<FTransform, FRigTransformMetadata, ERigMetadataType::Transform>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FLinearColor>(false))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<FLinearColor, FRigLinearColorMetadata, ERigMetadataType::LinearColor>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FRigElementKey>(false))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<FRigElementKey, FRigElementKeyMetadata, ERigMetadataType::RigElementKey>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::BoolArray)
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<TArray<bool>, FRigBoolArrayMetadata, ERigMetadataType::BoolArray>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::FloatArray)
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<TArray<float>, FRigFloatArrayMetadata, ERigMetadataType::FloatArray>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::Int32Array)
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<TArray<int32>, FRigInt32ArrayMetadata, ERigMetadataType::Int32Array>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::FNameArray)
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<TArray<FName>, FRigNameArrayMetadata, ERigMetadataType::NameArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FVector>(true))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<TArray<FVector>, FRigVectorArrayMetadata, ERigMetadataType::VectorArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FRotator>(true))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<TArray<FRotator>, FRigRotatorArrayMetadata, ERigMetadataType::RotatorArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FQuat>(true))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<TArray<FQuat>, FRigQuatArrayMetadata, ERigMetadataType::QuatArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FTransform>(true))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<TArray<FTransform>, FRigTransformArrayMetadata, ERigMetadataType::TransformArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FLinearColor>(true))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<TArray<FLinearColor>, FRigLinearColorArrayMetadata, ERigMetadataType::LinearColorArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FRigElementKey>(true))
	{
		return &FRigDispatch_GetMetadata::GetMetadataDispatch<TArray<FRigElementKey>, FRigElementKeyArrayMetadata, ERigMetadataType::RigElementKeyArray>;
	}

	return nullptr;
}

////////////////////////////////////////////////////////////////////////////////////////////////

TArray<FRigVMTemplateArgument> FRigDispatch_SetMetadata::GetArguments() const
{
	if(ValueArgIndex == INDEX_NONE)
	{
		Arguments = Super::GetArguments(); 
		ValueArgIndex = Arguments.Emplace(ValueArgName, ERigVMPinDirection::Input, GetValueTypes());
		SuccessArgIndex = Arguments.Emplace(SuccessArgName, ERigVMPinDirection::Output, RigVMTypeUtils::TypeIndex::Bool);
	};
	return Arguments;
}

FRigBaseMetadata* FRigDispatch_SetMetadata::FindOrAddMetadata(FControlRigExecuteContext& InContext,
                                                              const FRigElementKey& InKey, const FName& InName, ERigMetadataType InType, FCachedRigElement& Cache)
{
	if(Cache.UpdateCache(InKey, InContext.Hierarchy))
	{
		if(FRigBaseElement* Element = InContext.Hierarchy->Get(Cache.GetIndex()))
		{
			return Element->SetupValidMetadata(InName, InType);
		}
	}
	return nullptr;
}

FRigVMFunctionPtr FRigDispatch_SetMetadata::GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const
{
	const FRigVMRegistry& Registry = FRigVMRegistry::Get();
	const TRigVMTypeIndex& ValueTypeIndex = InTypes.FindChecked(TEXT("Value"));
	
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::Bool)
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<bool, FRigBoolMetadata, ERigMetadataType::Bool>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::Float)
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<float, FRigFloatMetadata, ERigMetadataType::Float>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::Int32)
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<int32, FRigInt32Metadata, ERigMetadataType::Int32>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::FName)
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<FName, FRigNameMetadata, ERigMetadataType::Name>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FVector>(false))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<FVector, FRigVectorMetadata, ERigMetadataType::Vector>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FRotator>(false))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<FRotator, FRigRotatorMetadata, ERigMetadataType::Rotator>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FQuat>(false))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<FQuat, FRigQuatMetadata, ERigMetadataType::Quat>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FTransform>(false))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<FTransform, FRigTransformMetadata, ERigMetadataType::Transform>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FLinearColor>(false))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<FLinearColor, FRigLinearColorMetadata, ERigMetadataType::LinearColor>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FRigElementKey>(false))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<FRigElementKey, FRigElementKeyMetadata, ERigMetadataType::RigElementKey>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::BoolArray)
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<TArray<bool>, FRigBoolArrayMetadata, ERigMetadataType::BoolArray>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::FloatArray)
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<TArray<float>, FRigFloatArrayMetadata, ERigMetadataType::FloatArray>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::Int32Array)
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<TArray<int32>, FRigInt32ArrayMetadata, ERigMetadataType::Int32Array>;
	}
	if(ValueTypeIndex == RigVMTypeUtils::TypeIndex::FNameArray)
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<TArray<FName>, FRigNameArrayMetadata, ERigMetadataType::NameArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FVector>(true))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<TArray<FVector>, FRigVectorArrayMetadata, ERigMetadataType::VectorArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FRotator>(true))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<TArray<FRotator>, FRigRotatorArrayMetadata, ERigMetadataType::RotatorArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FQuat>(true))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<TArray<FQuat>, FRigQuatArrayMetadata, ERigMetadataType::QuatArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FTransform>(true))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<TArray<FTransform>, FRigTransformArrayMetadata, ERigMetadataType::TransformArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FLinearColor>(true))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<TArray<FLinearColor>, FRigLinearColorArrayMetadata, ERigMetadataType::LinearColorArray>;
	}
	if(ValueTypeIndex == Registry.GetTypeIndex<FRigElementKey>(true))
	{
		return &FRigDispatch_SetMetadata::SetMetadataDispatch<TArray<FRigElementKey>, FRigElementKeyArrayMetadata, ERigMetadataType::RigElementKeyArray>;
	}

	return nullptr;
}

FRigUnit_RemoveMetadata_Execute()
{
	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;

	Removed = false;

	if(CachedIndex.UpdateCache(Item, Hierarchy))
	{
		if(FRigBaseElement* Element = Hierarchy->Get(CachedIndex))
		{
			Removed = Element->RemoveMetadata(Name);
		}
	}
}

FRigUnit_RemoveAllMetadata_Execute()
{
	URigHierarchy* Hierarchy = ExecuteContext.Hierarchy;

	Removed = false;

	if(CachedIndex.UpdateCache(Item, Hierarchy))
	{
		if(FRigBaseElement* Element = Hierarchy->Get(CachedIndex))
		{
			Removed = Element->RemoveAllMetadata();
		}
	}
}

FRigUnit_HasMetadata_Execute()
{
	const URigHierarchy* Hierarchy = Context.Hierarchy;

	Found = false;

	if(CachedIndex.UpdateCache(Item, Hierarchy))
	{
		if(const FRigBaseElement* Element = Hierarchy->Get(CachedIndex))
		{
			Found = Element->GetMetadata(Name, Type) != nullptr;
		}
	}
}

FRigUnit_FindItemsWithMetadata_Execute()
{
	const URigHierarchy* Hierarchy = Context.Hierarchy;

	Items.Reset();

	Hierarchy->Traverse([&Items, Name, Type](const FRigBaseElement* Element, bool& bContinue)
	{
		if(Element->GetMetadata(Name, Type) != nullptr)
		{
			Items.AddUnique(Element->GetKey());
		}
		bContinue = true;
	});
}

FRigUnit_GetMetadataTags_Execute()
{
	Tags.Reset();
	
	const URigHierarchy* Hierarchy = Context.Hierarchy;
	if(CachedIndex.UpdateCache(Item, Hierarchy))
	{
		if(const FRigBaseElement* Element = Hierarchy->Get(CachedIndex))
		{
			if(const FRigNameArrayMetadata* Md = Cast<FRigNameArrayMetadata>(Element->GetMetadata(URigHierarchy::TagMetadataName, ERigMetadataType::NameArray)))
			{
				Tags = Md->GetValue();
			}
		}
	}
}

FRigUnit_SetMetadataTag_Execute()
{
	if(CachedIndex.UpdateCache(Item, ExecuteContext.Hierarchy))
	{
		if(FRigBaseElement* Element = ExecuteContext.Hierarchy->Get(CachedIndex))
		{
			if(FRigNameArrayMetadata* Md = Cast<FRigNameArrayMetadata>(Element->SetupValidMetadata(URigHierarchy::TagMetadataName, ERigMetadataType::NameArray)))
			{
				const int32 LastIndex = Md->GetValue().Num(); 
				if(Md->GetValue().AddUnique(Tag) == LastIndex)
				{
					Element->NotifyMetadataTagChanged(Tag, true);
				}
			}
		}
	}
}

FRigUnit_SetMetadataTagArray_Execute()
{
	if(CachedIndex.UpdateCache(Item, ExecuteContext.Hierarchy))
	{
		if(FRigBaseElement* Element = ExecuteContext.Hierarchy->Get(CachedIndex))
		{
			if(FRigNameArrayMetadata* Md = Cast<FRigNameArrayMetadata>(Element->SetupValidMetadata(URigHierarchy::TagMetadataName, ERigMetadataType::NameArray)))
			{
				for(const FName& Tag : Tags)
				{
					const int32 LastIndex = Md->GetValue().Num(); 
					if(Md->GetValue().AddUnique(Tag) == LastIndex)
					{
						Element->NotifyMetadataTagChanged(Tag, true);
					}
				}
			}
		}
	}
}

FRigUnit_RemoveMetadataTag_Execute()
{
	Removed = false;
	if(CachedIndex.UpdateCache(Item, ExecuteContext.Hierarchy))
	{
		if(FRigBaseElement* Element = ExecuteContext.Hierarchy->Get(CachedIndex))
		{
			if(FRigNameArrayMetadata* Md = Cast<FRigNameArrayMetadata>(Element->GetMetadata(URigHierarchy::TagMetadataName, ERigMetadataType::NameArray)))
			{
				Removed = Md->GetValue().Remove(Tag) > 0;
				if(Removed)
				{
					Element->NotifyMetadataTagChanged(Tag, false);
				}
			}
		}
	}
}

FRigUnit_HasMetadataTag_Execute()
{
	Found = false;
	
	const URigHierarchy* Hierarchy = Context.Hierarchy;
	if(CachedIndex.UpdateCache(Item, Hierarchy))
	{
		if(const FRigBaseElement* Element = Hierarchy->Get(CachedIndex))
		{
			if(const FRigNameArrayMetadata* Md = Cast<FRigNameArrayMetadata>(Element->GetMetadata(URigHierarchy::TagMetadataName, ERigMetadataType::NameArray)))
			{
				Found = Md->GetValue().Contains(Tag);
			}
		}
	}
}

FRigUnit_HasMetadataTagArray_Execute()
{
	Found = false;
	
	const URigHierarchy* Hierarchy = Context.Hierarchy;
	if(CachedIndex.UpdateCache(Item, Hierarchy))
	{
		if(const FRigBaseElement* Element = Hierarchy->Get(CachedIndex))
		{
			if(const FRigNameArrayMetadata* Md = Cast<FRigNameArrayMetadata>(Element->GetMetadata(URigHierarchy::TagMetadataName, ERigMetadataType::NameArray)))
			{
				Found = true;
				for(const FName& Tag : Tags)
				{
					if(!Md->GetValue().Contains(Tag))
					{
						Found = false;
						break;
					}
				}
			}
		}
	}
}

FRigUnit_FindItemsWithMetadataTag_Execute()
{
	const URigHierarchy* Hierarchy = Context.Hierarchy;

	Items.Reset();

	Hierarchy->Traverse([&Items, Tag](const FRigBaseElement* Element, bool& bContinue)
	{
		if(const FRigNameArrayMetadata* Md = Cast<FRigNameArrayMetadata>(Element->GetMetadata(URigHierarchy::TagMetadataName, ERigMetadataType::NameArray)))
		{
			if(Md->GetValue().Contains(Tag))
			{
				Items.AddUnique(Element->GetKey());
			}
		}
		bContinue = true;
	});
}

FRigUnit_FindItemsWithMetadataTagArray_Execute()
{
	const URigHierarchy* Hierarchy = Context.Hierarchy;

	Items.Reset();

	Hierarchy->Traverse([&Items, Tags](const FRigBaseElement* Element, bool& bContinue)
	{
		if(const FRigNameArrayMetadata* Md = Cast<FRigNameArrayMetadata>(Element->GetMetadata(URigHierarchy::TagMetadataName, ERigMetadataType::NameArray)))
		{
			bool bFoundAll = true;
			for(const FName& Tag : Tags)
			{
				if(!Md->GetValue().Contains(Tag))
				{
					bFoundAll = false;
					break;
				}
			}

			if(bFoundAll)
			{
				Items.AddUnique(Element->GetKey());
			}
		}
		bContinue = true;
	});
}

FRigUnit_FilterItemsByMetadataTags_Execute()
{
	const URigHierarchy* Hierarchy = Context.Hierarchy;

	Result.Reset();

	if(CachedIndices.Num() != Items.Num())
	{
		CachedIndices.Reset();
		CachedIndices.SetNumZeroed(Items.Num());
	}

	for(int32 Index = 0; Index < Items.Num(); Index++)
	{
		if(CachedIndices[Index].UpdateCache(Items[Index], Hierarchy))
		{
			if(const FRigBaseElement* Element = Hierarchy->Get(CachedIndices[Index]))
			{
				if(const FRigNameArrayMetadata* Md = Cast<FRigNameArrayMetadata>(Element->GetMetadata(URigHierarchy::TagMetadataName, ERigMetadataType::NameArray)))
				{
					if(Inclusive)
					{
						bool bFoundAll = true;
						for(const FName& Tag : Tags)
						{
							if(!Md->GetValue().Contains(Tag))
							{
								bFoundAll = false;
								break;
							}
						}
						if(bFoundAll)
						{
							Result.Add(Element->GetKey());
						}
					}
					else
					{
						bool bFoundAny = false;
						for(const FName& Tag : Tags)
						{
							if(Md->GetValue().Contains(Tag))
							{
								bFoundAny = true;
								break;
							}
						}
						if(!bFoundAny)
						{
							Result.Add(Element->GetKey());
						}
					}
				}
				else if(!Inclusive)
				{
					Result.Add(Element->GetKey());
				}
			}
		}
		else
		{
			UE_CONTROLRIG_RIGUNIT_REPORT_ERROR(TEXT("Item '%s' not found"), *Items[Index].ToString());
		}
	}
}
