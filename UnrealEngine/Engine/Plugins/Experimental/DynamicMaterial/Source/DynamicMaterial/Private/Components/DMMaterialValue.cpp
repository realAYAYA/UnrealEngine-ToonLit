// Copyright Epic Games, Inc. All Rights Reserved.
 
#include "Components/DMMaterialValue.h"
#include "Components/DMMaterialParameter.h"
#include "DMComponentPath.h"
#include "Model/DynamicMaterialModel.h"
#include "Materials/Material.h"
#include "UObject/Package.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "DMValueDefinition.h"
#include "DynamicMaterialModule.h"
#include "Factories/MaterialFactoryNew.h"
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Model/IDMMaterialBuildStateInterface.h"
#include "Model/IDMMaterialBuildUtilsInterface.h"
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "DMMaterialValue"

TMap<EDMValueType, TStrongObjectPtr<UClass>> UDMMaterialValue::TypeClasses = {};

const FString UDMMaterialValue::ParameterPathToken = FString(TEXT("Parameter"));
const TCHAR* UDMMaterialValue::ParameterNamePrefix = TEXT("VALUE_");

#if WITH_EDITOR
const FName UDMMaterialValue::ValueName = "Value";

UDMMaterialValue* UDMMaterialValue::CreateMaterialValue(UDynamicMaterialModel* InMaterialModel, const FString& InName,
	EDMValueType InType, bool bInLocal)
{
	check(InMaterialModel);
 
	TStrongObjectPtr<UClass>* ValueClassPtr = TypeClasses.Find(InType);
	check(ValueClassPtr);
 
	UClass* ValueClass = (*ValueClassPtr).Get();
	check(ValueClass);
 
	UDMMaterialValue* NewValue = NewObject<UDMMaterialValue>(InMaterialModel, ValueClass, NAME_None, RF_Transactional);
	check(NewValue);
	NewValue->Type = InType;
	NewValue->bLocal = bInLocal;
 
	if (InName.IsEmpty() == false)
	{
		if (GUndo)
		{
			InMaterialModel->Modify();
		}

		NewValue->Parameter = InMaterialModel->CreateUniqueParameter(*InName);
		NewValue->Parameter->SetParentComponent(NewValue);
	}
 
	return NewValue;
}
#endif // WITH_EDITOR

UDMMaterialValue::UDMMaterialValue()
	: UDMMaterialValue(EDMValueType::VT_None)
{
	Parameter = nullptr;

#if WITH_EDITORONLY_DATA
	EditableProperties.Add(ValueName);
#endif
}
 
UDynamicMaterialModel* UDMMaterialValue::GetMaterialModel() const
{
	return Cast<UDynamicMaterialModel>(GetOuterSafe());
}
 
#if WITH_EDITOR
FText UDMMaterialValue::GetTypeName() const
{
	return UDMValueDefinitionLibrary::GetValueDefinition(Type).GetDisplayName();
}
 
FText UDMMaterialValue::GetDescription() const
{
	static const FText Template = LOCTEXT("ValueDescriptionTemplate", "{0} ({1})");
 
	return FText::Format(
		Template,
		Parameter ? FText::FromName(Parameter->GetParameterName()) : FText::AsNumber(FindIndex() + 1),
		GetTypeName()
	);
}

void UDMMaterialValue::PostCDOContruct()
{
	Super::PostCDOContruct();

	if (Type != EDMValueType::VT_None)
	{
		TypeClasses.Emplace(Type, TStrongObjectPtr<UClass>(GetClass()));
	}
}

void UDMMaterialValue::PostLoad()
{
	Super::PostLoad();

	if (Parameter)
	{
		if (GUndo)
		{
			Parameter->Modify();
		}

		Parameter->SetParentComponent(this);
	}
}

void UDMMaterialValue::PostEditImport()
{
	Super::PostEditImport();

	if (Parameter)
	{
		if (GUndo)
		{
			Parameter->Modify();
		}

		Parameter->SetParentComponent(this);
	}
}

void UDMMaterialValue::BeginDestroy()
{
	Super::BeginDestroy();
	PreviewMaterial = nullptr;
}
#endif // WITH_EDITOR

FName UDMMaterialValue::GetMaterialParameterName() const
{
	return Parameter ? FName(ParameterNamePrefix + Parameter->GetParameterName().ToString()) : GetFName();
}

UDMMaterialComponent* UDMMaterialValue::GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const
{
	if (InPathSegment.GetToken() == ParameterPathToken)
	{
		return Parameter;
	}

	return Super::GetSubComponentByPath(InPath, InPathSegment);
}
 
#if WITH_EDITOR
bool UDMMaterialValue::SetParameterName(FName InBaseName)
{
	if (Parameter && Parameter->GetParameterName() == InBaseName)
	{
		return false;
	}

	if (!IsComponentValid())
	{
		return false;
	}
 
	UDynamicMaterialModel* MaterialModel = GetMaterialModel();
	check(MaterialModel);

	if (GUndo && IsValid(Parameter))
	{
		Parameter->Modify();
		MaterialModel->Modify();
	}
 
	if (InBaseName.IsNone())
	{
		if (Parameter)
		{
			Parameter->SetParentComponent(nullptr);
			MaterialModel->FreeParameter(Parameter);
			Parameter = nullptr;
		}
 
		return true;
	}
 
	if (Parameter)
	{
		Parameter->RenameParameter(InBaseName);
	}
	else
	{
		Parameter = MaterialModel->CreateUniqueParameter(InBaseName);
		Parameter->SetParentComponent(this);
	}
 
	return true;
}

void UDMMaterialValue::OnComponentRemoved()
{
	if (Parameter)
	{
		if (GUndo)
		{
			Parameter->Modify();
		}

		Parameter->SetComponentState(EDMComponentLifetimeState::Removed);
	}
 
	Super::OnComponentRemoved();
}

int32 UDMMaterialValue::FindIndex() const
{
	UDynamicMaterialModel* MaterialModel = GetMaterialModel();
	check(MaterialModel);
 
	const TArray<UDMMaterialValue*>& Values = MaterialModel->GetValues();
 
	for (int32 ValueIdx = 0; ValueIdx < Values.Num(); ++ValueIdx)
	{
		if (Values[ValueIdx] == this)
		{
			return ValueIdx;
		}
	}
 
	check(false);
	return INDEX_NONE;
}
 
int32 UDMMaterialValue::FindIndexSafe() const
{
	UDynamicMaterialModel* MaterialModel = GetMaterialModel();
	check(MaterialModel);
 
	const TArray<UDMMaterialValue*>& Values = MaterialModel->GetValues();
 
	for (int32 ValueIdx = 0; ValueIdx < Values.Num(); ++ValueIdx)
	{
		if (Values[ValueIdx] == this)
		{
			return ValueIdx;
		}
	}
 
	return INDEX_NONE;
}
 
UMaterial* UDMMaterialValue::GetPreviewMaterial()
{
	if (!PreviewMaterial)
	{
		CreatePreviewMaterial();
 
		if (PreviewMaterial)
		{
			MarkComponentDirty();
		}
	}
 
	return PreviewMaterial;
}
#endif // WITH_EDITOR
 
void UDMMaterialValue::SetMIDParameter(UMaterialInstanceDynamic* InMID) const
{
	checkNoEntry();
}
 
#if WITH_EDITOR
void UDMMaterialValue::DoClean()
{
	if (!IsComponentValid())
	{
		return;
	}

	UpdatePreviewMaterial();

	Super::DoClean();
}

void UDMMaterialValue::PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent)
{
	if (GetOuter() != InMaterialModel)
	{
		FName OldParameterName = NAME_None;

		if (UDMMaterialParameter* Param = GetParameter())
		{
			// Reset this to null as it holds a copy of the parameter from the copied-from object.
			// This will not be in the model's parameter list and will share the same name as the old parameter.
			// Just null the reference and create a new parameter.
			if (InMaterialModel->ConditionalFreeParameter(Param))
			{
				OldParameterName = Param->GetParameterName();
				Parameter = nullptr;
			}
		}

		Super::PostEditorDuplicate(InMaterialModel, InParent);

		Rename(nullptr, InMaterialModel, UE::DynamicMaterial::RenameFlags);

		if (OldParameterName != NAME_None)
		{
			SetParameterName(OldParameterName);
		}
	}
	else
	{
		Super::PostEditorDuplicate(InMaterialModel, InParent);
	}

	PreviewMaterial = nullptr;
}

bool UDMMaterialValue::Modify(bool bInAlwaysMarkDirty)
{
	const bool bSaved = Super::Modify(bInAlwaysMarkDirty);

	if (Parameter)
	{
		Parameter->Modify(bInAlwaysMarkDirty);
	}

	return bSaved;
}

void UDMMaterialValue::PostEditUndo()
{
	Super::PostEditUndo();

	if (!IsComponentValid())
	{
		return;
	}
 
	MarkComponentDirty();
 
	OnValueUpdated(/*bForceStructureUpdate*/ true); // Just in case - Undos are not meant to be quick and easy.
}
 
void UDMMaterialValue::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!IsComponentValid())
	{
		return;
	}

	FName MemberPropertyName = PropertyChangedEvent.GetMemberPropertyName();

	if (MemberPropertyName == NAME_None)
	{
		return;
	}

	for (const FName& EditableProperty : EditableProperties)
	{
		if (EditableProperty == MemberPropertyName)
		{
			OnValueUpdated(/* bForceStructureUpdate */ EditableProperty != ValueName);
			return;
		}
	}
}
 
void UDMMaterialValue::EnsureDetailObjects()
{
	if (!IsComponentValid())
	{
		return;
	}

	if (PropertyRowGenerator.IsValid() && DetailTreeNode.IsValid() && PropertyHandle.IsValid())
	{
		return;
	}
 
	PropertyHandle.Reset();
	DetailTreeNode.Reset();
	PropertyRowGenerator.Reset();
 
	FPropertyEditorModule& PropertyEditor = FModuleManager::Get().LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor"));
 
	FPropertyRowGeneratorArgs RowGeneratorArgs;
	PropertyRowGenerator = PropertyEditor.CreatePropertyRowGenerator(RowGeneratorArgs);
	PropertyRowGenerator->SetObjects({this});
 
	for (const TSharedRef<IDetailTreeNode>& CategoryNode : PropertyRowGenerator->GetRootTreeNodes())
	{
		if (CategoryNode->GetNodeName() != TEXT("Material Designer"))
		{
			continue;
		}
 
		TArray<TSharedRef<IDetailTreeNode>> ChildNodes;
		CategoryNode->GetChildren(ChildNodes);
 
		for (const TSharedRef<IDetailTreeNode>& ChildNode : ChildNodes)
		{
			if (ChildNode->GetNodeType() != EDetailNodeType::Item)
			{
				continue;
			}
 
			if (ChildNode->GetNodeName() != ValueName)
			{
				continue;
			}
 
			DetailTreeNode = ChildNode;
			PropertyHandle = ChildNode->CreatePropertyHandle();
			return;
		}
	}
}

TSharedPtr<IDetailTreeNode> UDMMaterialValue::GetDetailTreeNode()
{
	EnsureDetailObjects();
	return DetailTreeNode;
}
 
TSharedPtr<IPropertyHandle> UDMMaterialValue::GetPropertyHandle()
{
	EnsureDetailObjects();
	return PropertyHandle;
}
#endif // WITH_EDITOR

UDMMaterialValue::UDMMaterialValue(EDMValueType InType)
	: Type(InType)
	, bLocal(false)
	, Parameter(nullptr)
#if WITH_EDITORONLY_DATA
	, PreviewMaterial(nullptr)
#endif
{
#if WITH_EDITORONLY_DATA
	EditableProperties.Add(ValueName);
#endif
}
 
void UDMMaterialValue::OnValueUpdated(bool bInForceStructureUpdate)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (FDMUpdateGuard::CanUpdate())
	{
		Update(bInForceStructureUpdate ? EDMUpdateType::Structure : EDMUpdateType::Value);
	}
}
 
void UDMMaterialValue::Update(EDMUpdateType InUpdateType)
{
	if (!IsComponentValid())
	{
		return;
	}

#if WITH_EDITOR
	if (HasComponentBeenRemoved())
	{
		return;
	}
 
	if (InUpdateType == EDMUpdateType::Structure)
	{
		MarkComponentDirty();
	}

	if (ParentComponent)
	{
		ParentComponent->Update(InUpdateType);
	}
#endif

	Super::Update(InUpdateType);

#if WITH_EDITOR
	UDynamicMaterialModel* MaterialModel = GetMaterialModel();
	check(MaterialModel);

	if (IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = MaterialModel->GetEditorOnlyData())
	{
		ModelEditorOnlyData->OnValueUpdated(this, InUpdateType);
	}
#endif
}

#if WITH_EDITOR
void UDMMaterialValue::CreatePreviewMaterial()
{
	if (!IsComponentValid())
	{
		return;
	}

	if (FDynamicMaterialModule::IsMaterialExportEnabled() == false)
	{
		UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
		check(MaterialFactory);
 
		PreviewMaterial = (UMaterial*)MaterialFactory->FactoryCreateNew(
			UMaterial::StaticClass(),
			GetTransientPackage(),
			NAME_None,
			RF_Transient,
			nullptr,
			GWarn
		);
 
		PreviewMaterial->bIsPreviewMaterial = true;
	}
	else
	{
		FString MaterialBaseName = GetName() + "-" + FGuid::NewGuid().ToString();
		const FString FullName = "/Game/DynamicMaterials/" + MaterialBaseName;
		UPackage* Package = CreatePackage(*FullName);
 
		UMaterialFactoryNew* MaterialFactory = NewObject<UMaterialFactoryNew>();
		check(MaterialFactory);
 
		PreviewMaterial = Cast<UMaterial>(MaterialFactory->FactoryCreateNew(
			UMaterial::StaticClass(),
			Package,
			*MaterialBaseName,
			RF_Standalone | RF_Public,
			nullptr,
			GWarn
		));
 
		FAssetRegistryModule::AssetCreated(PreviewMaterial);
		Package->FullyLoad();
	}
}
 
void UDMMaterialValue::UpdatePreviewMaterial()
{
	if (!IsComponentValid())
	{
		return;
	}

	if (!PreviewMaterial)
	{
		CreatePreviewMaterial();
 
		if (!PreviewMaterial)
		{
			return;
		}
	}
 
	UDynamicMaterialModel* MaterialModel = GetMaterialModel();

	if (!MaterialModel)
	{
		return;
	}

	IDynamicMaterialModelEditorOnlyDataInterface* ModelEditorOnlyData = MaterialModel->GetEditorOnlyData();

	if (!ModelEditorOnlyData)
	{
		return;
	}

	TSharedRef<IDMMaterialBuildStateInterface> BuildState = ModelEditorOnlyData->CreateBuildStateInterface(PreviewMaterial);

	UE_LOG(LogDynamicMaterial, Display, TEXT("Building Material Designer Value Preview (%s)..."), *GetName());

	GenerateExpression(BuildState);
	UMaterialExpression* ValueExpression = BuildState->GetLastValueExpression(this);
 
	BuildState->GetBuildUtils().UpdatePreviewMaterial(ValueExpression, 0, FDMMaterialStageConnectorChannel::WHOLE_CHANNEL, 32);
}
 
int32 UDMMaterialValue::GetInnateMaskOutput(int32 OutputChannels) const
{
	return INDEX_NONE;
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
