// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangePipelineBase.h"

#include "HAL/IConsoleManager.h"
#include "InterchangeLogPrivate.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangePipelineBase)

namespace UE
{
	namespace Interchange
	{
		namespace PipelinePrivate
		{
			FString CreateConfigSectionName(const FName PipelineStackName, UClass* PipelineClass)
			{
				FString Section = TEXT("Interchange_StackName__") + PipelineStackName.ToString() + TEXT("__PipelineClassName_") + PipelineClass->GetName();
				return Section;
			}
		}
	}
}

void UInterchangePipelineBase::LoadSettings(const FName PipelineStackName)
{
	LoadSettingsInternal(PipelineStackName, GEditorPerProjectIni, PropertiesStates);
}

void UInterchangePipelineBase::SaveSettings(const FName PipelineStackName)
{
	SaveSettingsInternal(PipelineStackName, GEditorPerProjectIni);
}

void UInterchangePipelineBase::AdjustSettingsForContext(EInterchangePipelineContext ReimportType, TObjectPtr<UObject> ReimportAsset)
{
	CachePipelineContext = ReimportType;
	CacheReimportObject = ReimportAsset;
	CachePropertiesStates = PropertiesStates;

	bAllowPropertyStatesEdition = (ReimportType == EInterchangePipelineContext::None);
	bIsReimportContext = false;
	switch (ReimportType)
	{
	case EInterchangePipelineContext::AssetReimport:
	case EInterchangePipelineContext::AssetAlternateSkinningReimport:
	case EInterchangePipelineContext::AssetCustomLODReimport:
	case EInterchangePipelineContext::SceneReimport:
	{
		bIsReimportContext = true;
	}
	break;
	}
}

void UInterchangePipelineBase::AdjustSettingsFromCache()
{
	PropertiesStates = CachePropertiesStates;
	AdjustSettingsForContext(CachePipelineContext, CacheReimportObject.Get());
}

void UInterchangePipelineBase::TransferAdjustSettings(UInterchangePipelineBase* SourcePipeline)
{
	CachePipelineContext = SourcePipeline->CachePipelineContext;
	CacheReimportObject = SourcePipeline->CacheReimportObject;
	CachePropertiesStates = SourcePipeline->CachePropertiesStates;
	bAllowPropertyStatesEdition = SourcePipeline->bAllowPropertyStatesEdition;
	bIsReimportContext = SourcePipeline->bIsReimportContext;
	bIsBasicLayout = SourcePipeline->bIsBasicLayout;
}

const FInterchangePipelinePropertyStates* UInterchangePipelineBase::GetPropertyStates(const FName PropertyPath) const
{
	return PropertiesStates.Find(PropertyPath);
}

FInterchangePipelinePropertyStates* UInterchangePipelineBase::GetMutablePropertyStates(const FName PropertyPath)
{
	return PropertiesStates.Find(PropertyPath);
}

bool UInterchangePipelineBase::DoesPropertyStatesExist(const FName PropertyPath) const
{
	return PropertiesStates.Contains(PropertyPath);
}

FInterchangePipelinePropertyStates& UInterchangePipelineBase::FindOrAddPropertyStates(const FName PropertyPath)
{
	return PropertiesStates.FindOrAdd(PropertyPath);
}

FName UInterchangePipelineBase::GetPropertiesStatesPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UInterchangePipelineBase, PropertiesStates);
}

FName UInterchangePipelineBase::GetResultsPropertyName()
{
	return GET_MEMBER_NAME_CHECKED(UInterchangePipelineBase, Results);
}

void UInterchangePipelineBase::LoadSettingsInternal(const FName PipelineStackName, const FString& ConfigFilename, TMap<FName, FInterchangePipelinePropertyStates>& ParentPropertiesStates)
{
	int32 PortFlags = 0;
	UClass* Class = this->GetClass();
	for (FProperty* Property = Class->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		//Do not load a transient property
		if (Property->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		FString SectionName = UE::Interchange::PipelinePrivate::CreateConfigSectionName(PipelineStackName, Class);
		FString Key = Property->GetName();
		const FName PropertyName = Property->GetFName();
		const FName PropertyPath = FName(Property->GetPathName());
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UInterchangePipelineBase, PropertiesStates))
		{
			continue;
		}
#if WITH_EDITOR
		if (Property->GetBoolMetaData(FName("AlwaysResetToDefault")))
		{
			//Stand alone pipeline property
			continue;
		}
#endif //WITH_EDITOR

		if (const FInterchangePipelinePropertyStates* PropertyStates = ParentPropertiesStates.Find(PropertyPath))
		{
			if (PropertyStates->IsPropertyLocked())
			{
				//Skip this locked property
				continue;
			}
		}

		const bool bIsPropertyInherited = Property->GetOwnerClass() != Class;
		UObject* SuperClassDefaultObject = Class->GetSuperClass()->GetDefaultObject();

		FObjectProperty* SubObject = CastField<FObjectProperty>(Property);
		FArrayProperty* Array = CastField<FArrayProperty>(Property);
		if (Array)
		{
			const bool bForce = false;
			const FConfigSection* Section = GConfig->GetSection(*SectionName, bForce, ConfigFilename);
			if (Section != nullptr)
			{
				TArray<FConfigValue> List;
				const FName KeyName(*Key, FNAME_Find);
				Section->MultiFind(KeyName, List);

				FScriptArrayHelper_InContainer ArrayHelper(Array, this);
				// Only override default properties if there is something to override them with.
				if (List.Num() > 0)
				{
					ArrayHelper.EmptyAndAddValues(List.Num());
					for (int32 i = List.Num() - 1, c = 0; i >= 0; i--, c++)
					{
						Array->Inner->ImportText_Direct(*List[i].GetValue(), ArrayHelper.GetRawPtr(c), this, PortFlags);
					}
				}
				else
				{
					int32 Index = 0;
					const FConfigValue* ElementValue = nullptr;
					do
					{
						// Add array index number to end of key
						FString IndexedKey = FString::Printf(TEXT("%s[%i]"), *Key, Index);

						// Try to find value of key
						const FName IndexedName(*IndexedKey, FNAME_Find);
						if (IndexedName == NAME_None)
						{
							break;
						}
						ElementValue = Section->Find(IndexedName);

						// If found, import the element
						if (ElementValue != nullptr)
						{
							// expand the array if necessary so that Index is a valid element
							ArrayHelper.ExpandForIndex(Index);
							Array->Inner->ImportText_Direct(*ElementValue->GetValue(), ArrayHelper.GetRawPtr(Index), this, PortFlags);
						}

						Index++;
					} while (ElementValue || Index < ArrayHelper.Num());
				}
			}
		}
		else if (UInterchangePipelineBase* SubPipeline = SubObject ? Cast<UInterchangePipelineBase>(SubObject->GetObjectPropertyValue_InContainer(this)) : nullptr)
		{
			// Load the settings if the referenced pipeline is a subobject of ours
			if (SubPipeline->IsInOuter(this))
			{
				SubPipeline->LoadSettingsInternal(PipelineStackName, ConfigFilename, ParentPropertiesStates);
			}
		}
		else
		{
			for (int32 i = 0; i < Property->ArrayDim; i++)
			{
				if (Property->ArrayDim != 1)
				{
					Key = FString::Printf(TEXT("%s[%i]"), *Property->GetName(), i);
				}

				FString Value;
				bool bFoundValue = GConfig->GetString(*SectionName, *Key, Value, ConfigFilename);

				if (bFoundValue)
				{
					if (Property->ImportText_Direct(*Value, Property->ContainerPtrToValuePtr<uint8>(this, i), this, PortFlags) == NULL)
					{
						// this should be an error as the properties from the .ini / .int file are not correctly being read in and probably are affecting things in subtle ways
						UE_LOG(LogInterchangeCore, Error, TEXT("UInterchangePipeline (class:%s) failed to load settings. Property: %s Value: %s"), *this->GetClass()->GetName(), *Property->GetName(), *Value);
					}
				}
			}
		}
	}
}

void UInterchangePipelineBase::SaveSettingsInternal(const FName PipelineStackName, const FString& ConfigFilename)
{
	int32 PortFlags = 0;
	UClass* Class = this->GetClass();
	for (FProperty* Property = Class->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		//Do not save a transient property
		if (Property->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		FString SectionName = UE::Interchange::PipelinePrivate::CreateConfigSectionName(PipelineStackName, Class);
		FString Key = Property->GetName();
		const FName PropertyName = Property->GetFName();
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UInterchangePipelineBase, PropertiesStates))
		{
			continue;
		}

		const bool bIsPropertyInherited = Property->GetOwnerClass() != Class;
		UObject* SuperClassDefaultObject = Class->GetSuperClass()->GetDefaultObject();
		FObjectProperty* SubObject = CastField<FObjectProperty>(Property);
		FArrayProperty* Array = CastField<FArrayProperty>(Property);
		if (Array)
		{
			GConfig->RemoveKeyFromSection(*SectionName, *Key, ConfigFilename);

			FScriptArrayHelper_InContainer ArrayHelper(Array, this);
			for (int32 i = 0; i < ArrayHelper.Num(); i++)
			{
				FString	Buffer;
				Array->Inner->ExportTextItem_Direct(Buffer, ArrayHelper.GetRawPtr(i), ArrayHelper.GetRawPtr(i), this, PortFlags);
				GConfig->AddToSection(*SectionName, *Key, Buffer, ConfigFilename);
			}
		}
		else if (UInterchangePipelineBase* SubPipeline = SubObject ? Cast<UInterchangePipelineBase>(SubObject->GetObjectPropertyValue_InContainer(this)) : nullptr)
		{
			// Save the settings if the referenced pipeline is a subobject of ours
			if (SubPipeline->IsInOuter(this))
			{
				SubPipeline->SaveSettingsInternal(PipelineStackName, ConfigFilename);
			}
		}
		else
		{
			TCHAR TempKey[MAX_SPRINTF] = TEXT("");
			for (int32 Index = 0; Index < Property->ArrayDim; Index++)
			{
				if (Property->ArrayDim != 1)
				{
					FCString::Sprintf(TempKey, TEXT("%s[%i]"), *Property->GetName(), Index);
					Key = TempKey;
				}

				FString	Value;
				Property->ExportText_InContainer(Index, Value, this, this, this, PortFlags);
				GConfig->SetString(*SectionName, *Key, *Value, ConfigFilename);
			}
		}
	}
	GConfig->Flush(0);
}


UInterchangePipelineBase* UInterchangePipelineBase::GetMostPipelineOuter() const
{
	UInterchangePipelineBase* Top = (UInterchangePipelineBase*)this;
	for (;;)
	{
		if (UInterchangePipelineBase* CurrentOuter = Cast<UInterchangePipelineBase>(Top->GetOuter()))
		{
			Top = CurrentOuter;
			continue;
		}
		break;
	}
	return Top;
}

FString UInterchangePipelineBase::GetPipelineDisplayName() const
{
	int32 PortFlags = 0;
	UClass* Class = this->GetClass();
	for (FProperty* Property = Class->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		FStrProperty* StringProperty = CastField<FStrProperty>(Property);
		if (!StringProperty)
		{
			continue;
		}
		const FName PropertyName = Property->GetFName();
		if (PropertyName != FName("PipelineDisplayName"))
		{
			continue;
		}
		//We found the property
		FString Value = StringProperty->GetPropertyValue_InContainer(this, 0);
		if (!Value.IsEmpty())
		{
			return Value;
		}
		//Stop field iteration
		break;
	}
	//Did not found a valid DisplayName property, return the name of the object
	return GetName();
}

#if WITH_EDITOR

void UInterchangePipelineBase::InternalToggleVisibilityPropertiesOfMetaDataValue(UInterchangePipelineBase* OuterMostPipeline, UInterchangePipelineBase* Pipeline, bool bDoTransientSubPipeline, const FString& MetaDataKey, const FString& MetaDataValue, const bool VisibilityState)
{
	UClass* PipelineClass = Pipeline->GetClass();
	for (FProperty* Property = PipelineClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		FObjectProperty* SubObject = CastField<FObjectProperty>(Property);
		UInterchangePipelineBase* SubPipeline = SubObject ? Cast<UInterchangePipelineBase>(SubObject->GetObjectPropertyValue_InContainer(Pipeline)) : nullptr;
		const bool bSkipTransient = !bDoTransientSubPipeline || !SubPipeline;
		//Do not save a transient property
		if (bSkipTransient && Property->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		const FName PropertyName = Property->GetFName();
		const FName PropertyPath = FName(Property->GetPathName());

		if (SubPipeline)
		{
			InternalToggleVisibilityPropertiesOfMetaDataValue(OuterMostPipeline, SubPipeline, bDoTransientSubPipeline, MetaDataKey, MetaDataValue, VisibilityState);
		}
		else
		{
			FString CategoryName = Property->GetMetaData(*MetaDataKey);
			if (CategoryName.Equals(MetaDataValue))
			{
				OuterMostPipeline->FindOrAddPropertyStates(PropertyPath).ReimportStates.bVisible = VisibilityState;
				OuterMostPipeline->FindOrAddPropertyStates(PropertyPath).ImportStates.bVisible = VisibilityState;
			}
		}
	}
}

void UInterchangePipelineBase::HidePropertiesOfCategory(UInterchangePipelineBase* OuterMostPipeline, UInterchangePipelineBase* Pipeline, const FString& HideCategoryName, bool bDoTransientSubPipeline /*= false*/)
{
	constexpr bool bVisibilityState = false;
	InternalToggleVisibilityPropertiesOfMetaDataValue(OuterMostPipeline, Pipeline, bDoTransientSubPipeline, TEXT("Category"), HideCategoryName, bVisibilityState);
}

void UInterchangePipelineBase::HidePropertiesOfSubCategory(UInterchangePipelineBase* OuterMostPipeline, UInterchangePipelineBase* Pipeline, const FString& HideSubCategoryName, bool bDoTransientSubPipeline /*= false*/)
{
	constexpr bool bVisibilityState = false;
	InternalToggleVisibilityPropertiesOfMetaDataValue(OuterMostPipeline, Pipeline, bDoTransientSubPipeline, TEXT("SubCategory"), HideSubCategoryName, bVisibilityState);
}

void UInterchangePipelineBase::HideProperty(UInterchangePipelineBase* OuterMostPipeline, UInterchangePipelineBase* Pipeline, const FName& HidePropertyName)
{
	constexpr bool bVisibilityState = false;
	UClass* PipelineClass = Pipeline->GetClass();
	for (FProperty* Property = PipelineClass->PropertyLink; Property; Property = Property->PropertyLinkNext)
	{
		FObjectProperty* SubObject = CastField<FObjectProperty>(Property);
		//Do not save a transient property
		if (SubObject || Property->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		const FName PropertyName = Property->GetFName();
		const FName PropertyPath = FName(Property->GetPathName());
		if (HidePropertyName != PropertyName)
		{
			continue;
		}
		OuterMostPipeline->FindOrAddPropertyStates(PropertyPath).ImportStates.bVisible = bVisibilityState;
		OuterMostPipeline->FindOrAddPropertyStates(PropertyPath).ReimportStates.bVisible = bVisibilityState;
	}
}

#endif //WITH_EDITOR

struct FWeakObjectPtrData
{
	FString PropertyName;
	FWeakObjectProperty* WeakObjectProperty;
	void* ValuePtr;
};

void GatherObjectAndWeakObjectPtrs(UClass* Class, UObject* Object/*Value*/, TMap<FString, UObject*>& ObjectPtrs, TArray<FWeakObjectPtrData>& WeakObjectPtrs)
{
	for (TFieldIterator<FProperty> It(Class); It; ++It)
	{
		FProperty* Property = *It;

		FString VariableName = Property->GetName();

		if (FWeakObjectProperty* WeakObjectProperty = CastField<FWeakObjectProperty>(Property))
		{
			FWeakObjectPtrData& Data = WeakObjectPtrs.AddDefaulted_GetRef();
			Data.PropertyName = Property->GetName();
			Data.WeakObjectProperty = WeakObjectProperty;
			Data.ValuePtr = Property->ContainerPtrToValuePtr<uint8>(Object);
		}
		else if (FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
		{
			const void* SubValue = Property->ContainerPtrToValuePtr<uint8>(Object);
			UObject* SubObject = ObjectProperty->GetObjectPropertyValue(SubValue);
			if (SubObject)
			{
				FString PropertyName = Property->GetName();
				ObjectPtrs.Add(PropertyName, SubObject);

				UClass* SubObjectPropertyClass = SubObject->GetClass();

				GatherObjectAndWeakObjectPtrs(SubObjectPropertyClass, SubObject, ObjectPtrs, WeakObjectPtrs);
			}
		}
	}
}

void UInterchangePipelineBase::UpdateWeakObjectPtrs()
{
	//Fix WeakObjectPtr connections:
	TMap<FString, UObject*> ObjectPtrs;
	TArray<FWeakObjectPtrData> WeakObjectPtrs;
	GatherObjectAndWeakObjectPtrs(GetClass(), this, ObjectPtrs, WeakObjectPtrs);

	for (FWeakObjectPtrData& WeakObjectPtrData : WeakObjectPtrs)
	{
		if (ObjectPtrs.Contains(WeakObjectPtrData.PropertyName))
		{
			WeakObjectPtrData.WeakObjectProperty->SetPropertyValue(WeakObjectPtrData.ValuePtr, ObjectPtrs[WeakObjectPtrData.PropertyName]);
		}
	}
}

void UInterchangePipelineBase::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	UpdateWeakObjectPtrs();
}
