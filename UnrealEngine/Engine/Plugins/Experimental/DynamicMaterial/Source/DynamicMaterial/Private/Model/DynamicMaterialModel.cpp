// Copyright Epic Games, Inc. All Rights Reserved.

#include "Model/DynamicMaterialModel.h"
#include "Components/DMMaterialParameter.h"
#include "Components/DMMaterialValue.h"
#include "Components/MaterialValues/DMMaterialValueFloat1.h"
#include "DMComponentPath.h"
#include "DMDefs.h"
#include "Material/DynamicMaterialInstance.h"
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"

#if WITH_EDITOR
#include "Components/DMTextureUV.h"
#include "DynamicMaterialModule.h"
#include "Materials/Material.h"
#endif

const FString UDynamicMaterialModel::ValuesPathToken          = FString(TEXT("Values"));
const FString UDynamicMaterialModel::ParametersPathToken      = FString(TEXT("Parameters"));
const FName UDynamicMaterialModel::GlobalOpacityParameterName = FName("GlobalOpacity");

UDynamicMaterialModel::UDynamicMaterialModel()
{
	DynamicMaterialInstance = nullptr;

#if WITH_EDITOR
	EditorOnlyDataSI.SetObject(nullptr);
#endif

	FDMUpdateGuard Guard;
	GlobalOpacityValue = CreateDefaultSubobject<UDMMaterialValueFloat1>(GlobalOpacityParameterName);

#if WITH_EDITOR
	GlobalOpacityValue->SetDefaultValue(1.f);
	GlobalOpacityValue->ApplyDefaultValue();
#endif

	GlobalOpacityParameter = CreateDefaultSubobject<UDMMaterialParameter>("GlobalOpacityParameter");
	GlobalOpacityParameter->ParameterName = GlobalOpacityParameterName;
	GlobalOpacityValue->Parameter = GlobalOpacityParameter;

	ParameterMap.Add(GlobalOpacityParameterName, GlobalOpacityParameter);
}

void UDynamicMaterialModel::SetDynamicMaterialInstance(UDynamicMaterialInstance* InDynamicMaterialInstance)
{
	DynamicMaterialInstance = InDynamicMaterialInstance;

#if WITH_EDITOR
	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->PostEditorDuplicate();
	}
#endif
}

bool UDynamicMaterialModel::IsModelValid() const
{
	return (!HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed) && IsValid(this));
}

UDMMaterialComponent* UDynamicMaterialModel::GetComponentByPath(const FString& InPath) const
{
	FDMComponentPath Path(InPath);
	return GetComponentByPath(Path);
}

UDMMaterialComponent* UDynamicMaterialModel::GetComponentByPath(FDMComponentPath& InPath) const
{
	if (InPath.IsLeaf())
	{
		return nullptr;
	}

	const FDMComponentPathSegment FirstComponent = InPath.GetFirstSegment();

	if (FirstComponent.GetToken() == ValuesPathToken)
	{
		int32 ValueIndex;

		if (FirstComponent.GetParameter(ValueIndex))
		{
			if (Values.IsValidIndex(ValueIndex))
			{
				return Values[ValueIndex]->GetComponentByPath(InPath);
			}
		}

		return nullptr;
	}

	if (FirstComponent.GetToken() == ParametersPathToken)
	{
		FString ParameterStr;

		if (FirstComponent.GetParameter(ParameterStr))
		{
			const FName ParameterName = FName(*ParameterStr);

			if (const TWeakObjectPtr<UDMMaterialParameter>* ParameterPtr = ParameterMap.Find(ParameterName))
			{
				return (*ParameterPtr)->GetComponentByPath(InPath);
			}
		}

		return nullptr;
	}

#if WITH_EDITOR
	if (IDynamicMaterialModelEditorOnlyDataInterface* EditorOnlyData = GetEditorOnlyData())
	{
		return EditorOnlyData->GetSubComponentByPath(InPath, FirstComponent);
	}
#endif

	return nullptr;
}

#if WITH_EDITOR
IDynamicMaterialModelEditorOnlyDataInterface* UDynamicMaterialModel::GetEditorOnlyData() const
{
	if (IsValid(EditorOnlyDataSI.GetObject()))
	{
		return EditorOnlyDataSI.GetInterface();
	}

	return nullptr;
}

UDMMaterialValue* UDynamicMaterialModel::GetValueByName(FName InName) const
{
	for (UDMMaterialValue* Value : Values)
	{
		if (Value->GetParameter() && Value->GetParameter()->GetParameterName() == InName)
		{
			return Value;
		}
	}

	return nullptr;
}

UDMMaterialValue* UDynamicMaterialModel::GetValueByIndex(int32 Index) const
{
	if (Values.IsValidIndex(Index))
	{
		return Values[Index];
	}

	return nullptr;
}

UDMMaterialValue* UDynamicMaterialModel::AddValue(EDMValueType InType)
{
	UDMMaterialValue* NewValue = UDMMaterialValue::CreateMaterialValue(this, TEXT(""), InType, false);
	Values.Add(NewValue);

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->OnValueListUpdate();
	}

	return NewValue;
}

void UDynamicMaterialModel::RemoveValueByName(FName InName)
{
	int32 FoundIndex = Values.IndexOfByPredicate([InName](UDMMaterialValue* Value)
		{
			return Value->GetParameter() && Value->GetParameter()->GetParameterName() == InName;
		});

	if (FoundIndex == INDEX_NONE)
	{
		return;
	}

	Values.RemoveAt(FoundIndex);

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->RequestMaterialBuild();
		ModelEditorOnlyData->OnValueListUpdate();
	}
}

void UDynamicMaterialModel::RemoveValueByIndex(int32 Index)
{
	if (Values.IsValidIndex(Index))
	{
		Values.RemoveAt(Index);

		if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
		{
			ModelEditorOnlyData->RequestMaterialBuild();
			ModelEditorOnlyData->OnValueListUpdate();
		}
	}
}

bool UDynamicMaterialModel::HasParameterName(FName InParameterName) const
{
	if (Values.ContainsByPredicate([InParameterName](const UDMMaterialValue* InValue)
		{
			return IsValid(InValue) && InValue->GetParameter() && InValue->GetParameter()->GetParameterName() == InParameterName;
		}))
	{
		return true;
	}

	const TWeakObjectPtr<UDMMaterialParameter>* ParameterWeak = ParameterMap.Find(InParameterName);

	return (ParameterWeak && ParameterWeak->IsValid());
}

UDMMaterialParameter* UDynamicMaterialModel::CreateUniqueParameter(FName InBaseName)
{
	check(!InBaseName.IsNone());

	UDMMaterialParameter* NewParameter = nullptr;

	{
		const FDMInitializationGuard InitGuard;

		NewParameter = NewObject<UDMMaterialParameter>(this, NAME_None, RF_Transactional);
		check(NewParameter);

		RenameParameter(NewParameter, InBaseName);
	}

	ParameterMap.Emplace(NewParameter->GetParameterName(), NewParameter);
	NewParameter->SetComponentState(EDMComponentLifetimeState::Added);

	return NewParameter;
}

void UDynamicMaterialModel::RenameParameter(UDMMaterialParameter* InParameter, FName InBaseName)
{
	check(InParameter);
	check(!InBaseName.IsNone());

	if (!InParameter->ParameterName.IsNone())
	{
		FreeParameter(InParameter);
	}

	if (GUndo)
	{
		InParameter->Modify();
	}

	InParameter->ParameterName = CreateUniqueParameterName(InBaseName);
	ParameterMap.Emplace(InParameter->ParameterName, InParameter);
}

void UDynamicMaterialModel::FreeParameter(UDMMaterialParameter* InParameter)
{
	check(InParameter);

	if (InParameter->ParameterName.IsNone())
	{
		return;
	}

	for (TMap<FName, TWeakObjectPtr<UDMMaterialParameter>>::TIterator It(ParameterMap); It; ++It)
	{
		const TWeakObjectPtr<UDMMaterialParameter>& ParameterWeak = It->Value;

		if (ParameterWeak.IsValid() == false || ParameterWeak->GetParameterName() == InParameter->GetParameterName())
		{
			It.RemoveCurrent();
		}
	}

	if (GUndo)
	{
		InParameter->Modify();
	}

	InParameter->ParameterName = NAME_None;
	InParameter->SetComponentState(EDMComponentLifetimeState::Removed);
}

bool UDynamicMaterialModel::ConditionalFreeParameter(UDMMaterialParameter* InParameter)
{
	check(InParameter);

	// Parameteres without names are not in the map
	if (InParameter->ParameterName.IsNone())
	{
		return true;
	}

	TWeakObjectPtr<UDMMaterialParameter>* ParameterPtr = ParameterMap.Find(InParameter->ParameterName);

	// Parameter name isn't in the map or it's mapped to a different object.
	if (!ParameterPtr || InParameter != ParameterPtr->Get())
	{
		return true;
	}

	// We're in the map at the given name.
	return false;
}

void UDynamicMaterialModel::ResetData()
{
	Values.Empty();

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->ResetData();
	}
}

void UDynamicMaterialModel::PostLoad()
{
	Super::PostLoad();

	SetFlags(RF_Transactional);

	FixGlobalOpacityVars();

	IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData();

	if (!ModelEditorOnlyData)
	{
		EditorOnlyDataSI = FDynamicMaterialModule::CreateEditorOnlyData(this);
		EditorOnlyDataSI->LoadDeprecatedModelData(this);
	}

	ReinitComponents();
}

void UDynamicMaterialModel::PostEditUndo()
{
	Super::PostEditUndo();

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->RequestMaterialBuild();
	}
}

void UDynamicMaterialModel::PostEditImport()
{
	Super::PostEditImport();

	FixGlobalOpacityVars();
	PostEditorDuplicate();
	ReinitComponents();

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->RequestMaterialBuild();
	}
}

void UDynamicMaterialModel::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);

	FixGlobalOpacityVars();
	PostEditorDuplicate();
	ReinitComponents();

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->RequestMaterialBuild();
	}
}

void UDynamicMaterialModel::PostEditorDuplicate()
{
	for (const TObjectPtr<UDMMaterialValue>& Value : Values)
	{
		if (GUndo)
		{
			Value->Modify();
		}

		Value->PostEditorDuplicate(this, nullptr);
	}

	if (GUndo)
	{
		GlobalOpacityValue->Modify();
	}

	GlobalOpacityValue->PostEditorDuplicate(this, nullptr);

	for (const TPair<FName, TWeakObjectPtr<UDMMaterialParameter>>& Pair : ParameterMap)
	{
		if (UDMMaterialParameter* Parameter = Pair.Value.Get())
		{
			if (GUndo)
			{
				GlobalOpacityValue->Modify();
			}

			Parameter->PostEditorDuplicate(this, nullptr);
		}
	}

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->PostEditorDuplicate();
	}
}

void UDynamicMaterialModel::ReinitComponents()
{
	if (IsValid(DynamicMaterial))
	{
		if (GUndo)
		{
			DynamicMaterial->Modify();
		}

		DynamicMaterial->AtomicallySetFlags(RF_DuplicateTransient);
	}

	if (IsValid(GlobalOpacityValue) && GlobalOpacityValue->GetMaterialParameterName() != GlobalOpacityParameterName)
	{
		if (GUndo)
		{
			GlobalOpacityValue->Modify();
		}

		GlobalOpacityValue->SetParameterName(GlobalOpacityParameterName);
	}

	// Clean up old parameters
	ParameterMap.Empty();

	TArray<UObject*> Subobjects;
	GetObjectsWithOuter(this, Subobjects, false);

	for (UObject* Subobject : Subobjects)
	{
		if (UDMMaterialValue* Value = Cast<UDMMaterialValue>(Subobject))
		{
			if (UDMMaterialParameter* Parameter = Value->GetParameter())
			{
				ParameterMap.Emplace(Parameter->GetParameterName(), Parameter);
			}
		}
		else if (UDMTextureUV* TextureUV = Cast<UDMTextureUV>(Subobject))
		{
			TArray<UDMMaterialParameter*> Parameters = TextureUV->GetParameters();

			for (UDMMaterialParameter* Parameter : Parameters)
			{
				ParameterMap.Emplace(Parameter->GetParameterName(), Parameter);
			}
		}
	}

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = GetEditorOnlyData())
	{
		ModelEditorOnlyData->ReinitComponents();
	}
}

void UDynamicMaterialModel::FixGlobalOpacityVars()
{
	if (const TWeakObjectPtr<UDMMaterialParameter>* ParameterPtr = ParameterMap.Find(GlobalOpacityParameterName))
	{
		UDMMaterialParameter* Parameter = ParameterPtr->Get();

		if (!Parameter || Parameter->HasAnyFlags(RF_ArchetypeObject)
			|| (GlobalOpacityParameter && Parameter != GlobalOpacityParameter))
		{
			if (GlobalOpacityParameter)
			{
				ParameterMap[GlobalOpacityParameterName] = GlobalOpacityParameter;
			}
			else
			{
				ParameterMap.Remove(GlobalOpacityParameterName);
			}
		}
	}

	if (!GlobalOpacityValue || GlobalOpacityParameter == GlobalOpacityValue->Parameter)
	{
		return;
	}

	if (GUndo)
	{
		GlobalOpacityParameter->Modify();
		GlobalOpacityValue->Modify();
	}

	GlobalOpacityParameter->ParameterName = GlobalOpacityParameterName;
	GlobalOpacityValue->Parameter = GlobalOpacityParameter;
}

FName UDynamicMaterialModel::CreateUniqueParameterName(FName InBaseName)
{
	int32 CurrentTest = 0;
	FName UniqueName = InBaseName;

	auto UpdateName = [&InBaseName, &CurrentTest, &UniqueName]()
		{
			++CurrentTest;
			UniqueName = FName(InBaseName.ToString() + TEXT("_") + FString::FromInt(CurrentTest));
		};

	bool bIsNameUnique = false;

	while (!bIsNameUnique)
	{
		bIsNameUnique = true;

		if (Values.ContainsByPredicate([UniqueName](const UDMMaterialValue* Value)
			{
				return Value->GetMaterialParameterName() == UniqueName;
			}))
		{
			bIsNameUnique = false;
			UpdateName();
		}
	}

	bIsNameUnique = false;

	while (!bIsNameUnique)
	{
		bIsNameUnique = true;

		if (const TWeakObjectPtr<UDMMaterialParameter>* CurrentParameter = ParameterMap.Find(UniqueName))
		{
			if (CurrentParameter->IsValid())
			{
				bIsNameUnique = false;
				UpdateName();
			}
			else
			{
				ParameterMap.Remove(UniqueName);
				break; // Not needed, but informative.
			}
		}
	}

	return UniqueName;
}
#endif
