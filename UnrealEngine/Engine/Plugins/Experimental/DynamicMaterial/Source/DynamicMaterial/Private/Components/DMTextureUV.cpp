// Copyright Epic Games, Inc. All Rights Reserved.

#include "Components/DMTextureUV.h"
#include "Components/DMMaterialParameter.h"
#include "DMComponentPath.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Model/DynamicMaterialModel.h"
#include "Serialization/CustomVersion.h"

#if WITH_EDITOR
#include "IDetailTreeNode.h"
#include "IPropertyRowGenerator.h"
#include "Model/IDynamicMaterialModelEditorOnlyDataInterface.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#endif

namespace UE::DynamicMaterial::Private
{
	TMap<int32, FName> BaseParameterNames = {
		{ParamID::PivotX,   FName(TEXT("TextureUV_PivotX"))},
		{ParamID::PivotY,   FName(TEXT("TextureUV_PivotY"))},
		{ParamID::ScaleX,   FName(TEXT("TextureUV_ScaleX"))},
		{ParamID::ScaleY,   FName(TEXT("TextureUV_ScaleY"))},
		{ParamID::Rotation, FName(TEXT("TextureUV_Rotation"))},
		{ParamID::OffsetX,  FName(TEXT("TextureUV_OffsetX"))},
		{ParamID::OffsetY,  FName(TEXT("TextureUV_OffsetY"))},
	};
}

enum class EDMTextureUVVersion : int32
{
	Initial_Pre_20221102 = 0,
	Version_22021102 = 1,
	LatestVersion = Version_22021102
};

const FGuid UDMTextureUV::GUID(0xFCF57AFB, 0x50764284, 0xB9A9E659, 0xFFA02D33);
FCustomVersionRegistration GRegisterDMTextureUVVersion(UDMTextureUV::GUID, static_cast<int32>(EDMTextureUVVersion::LatestVersion), TEXT("DMTextureUV"));

const FString UDMTextureUV::OffsetXPathToken  = FString(TEXT("OffsetX"));
const FString UDMTextureUV::OffsetYPathToken  = FString(TEXT("OffsetY"));
const FString UDMTextureUV::PivotXPathToken   = FString(TEXT("PivotX"));
const FString UDMTextureUV::PivotYPathToken   = FString(TEXT("PivotY"));
const FString UDMTextureUV::RotationPathToken = FString(TEXT("Rotation"));
const FString UDMTextureUV::ScaleXPathToken   = FString(TEXT("ScaleX"));
const FString UDMTextureUV::ScaleYPathToken   = FString(TEXT("ScaleY"));

#if WITH_EDITOR
const FName UDMTextureUV::NAME_UVSource   = GET_MEMBER_NAME_CHECKED(UDMTextureUV, UVSource);
const FName UDMTextureUV::NAME_Offset     = GET_MEMBER_NAME_CHECKED(UDMTextureUV, Offset);
const FName UDMTextureUV::NAME_Pivot      = GET_MEMBER_NAME_CHECKED(UDMTextureUV, Pivot);
const FName UDMTextureUV::NAME_Rotation   = GET_MEMBER_NAME_CHECKED(UDMTextureUV, Rotation);
const FName UDMTextureUV::NAME_Scale      = GET_MEMBER_NAME_CHECKED(UDMTextureUV, Scale);
const FName UDMTextureUV::NAME_bMirrorOnX = GET_MEMBER_NAME_CHECKED(UDMTextureUV, bMirrorOnX);
const FName UDMTextureUV::NAME_bMirrorOnY = GET_MEMBER_NAME_CHECKED(UDMTextureUV, bMirrorOnY);

const TMap<FName, bool> UDMTextureUV::TextureProperties = {
	{NAME_UVSource,   false},
	{NAME_Offset,     true},
	{NAME_Pivot,      true},
	{NAME_Rotation,   true},
	{NAME_Scale,      true},
	{NAME_bMirrorOnX, false},
	{NAME_bMirrorOnY, false}
};
#endif

UDMTextureUV::UDMTextureUV()
{
#if WITH_EDITOR
	EditableProperties.Add(NAME_Offset);
	EditableProperties.Add(NAME_Pivot);
	EditableProperties.Add(NAME_Rotation);
	EditableProperties.Add(NAME_Scale);
	EditableProperties.Add(NAME_bMirrorOnX);
	EditableProperties.Add(NAME_bMirrorOnY);
#endif
}

#if WITH_EDITOR
void UDMTextureUV::SetUVSource(EDMUVSource InUVSource)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (UVSource == InUVSource)
	{
		return;
	}

	UVSource = InUVSource;

	Update(EDMUpdateType::Structure);
}
#endif

void UDMTextureUV::SetOffset(const FVector2D& InOffset)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (FMath::IsNearlyEqual(Offset.X, InOffset.X)
		&& FMath::IsNearlyEqual(Offset.Y, InOffset.Y))
	{
		return;
	}

	Offset = InOffset;

	Update(EDMUpdateType::Value);
}

void UDMTextureUV::SetPivot(const FVector2D& InPivot)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (FMath::IsNearlyEqual(Pivot.X, InPivot.X)
		&& FMath::IsNearlyEqual(Pivot.Y, InPivot.Y))
	{
		return;
	}

	Pivot = InPivot;

	Update(EDMUpdateType::Value);
}

void UDMTextureUV::SetRotation(float InRotation)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (FMath::IsNearlyEqual(Rotation, InRotation))
	{
		return;
	}

	Rotation = InRotation;

	Update(EDMUpdateType::Value);
}

void UDMTextureUV::SetScale(const FVector2D& InScale)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (FMath::IsNearlyEqual(Scale.X, InScale.X)
		&& FMath::IsNearlyEqual(Scale.Y, InScale.Y))
	{
		return;
	}

	Scale = InScale;

	Update(EDMUpdateType::Value);
}

#if WITH_EDITOR
void UDMTextureUV::SetMirrorOnX(bool bInMirrorOnX)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (bMirrorOnX == bInMirrorOnX)
	{
		return;
	}

	bMirrorOnX = bInMirrorOnX;

	Update(EDMUpdateType::Structure);
}

void UDMTextureUV::SetMirrorOnY(bool bInMirrorOnY)
{
	if (!IsComponentValid())
	{
		return;
	}

	if (bMirrorOnY == bInMirrorOnY)
	{
		return;
	}

	bMirrorOnY = bInMirrorOnY;

	Update(EDMUpdateType::Structure);
}

TSharedPtr<IDetailTreeNode> UDMTextureUV::GetDetailTreeNode(FName InProperty)
{
	EnsureDetailObjects();

	return DetailTreeNodes.FindChecked(InProperty);
}

TSharedPtr<IPropertyHandle> UDMTextureUV::GetPropertyHandle(FName InProperty)
{
	EnsureDetailObjects();

	return PropertyHandles.FindChecked(InProperty);
}

TArray<UDMMaterialParameter*> UDMTextureUV::GetParameters() const
{
	TArray<UDMMaterialParameter*> Parameters;
	Parameters.Reserve(MaterialParameters.Num());

	for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& Pair : MaterialParameters)
	{
		Parameters.Add(Pair.Value.Get());
	}

	return Parameters;
}

void UDMTextureUV::PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent)
{
	Super::PostEditorDuplicate(InMaterialModel, InParent);

	if (GetOuter() != InMaterialModel)
	{
		Rename(nullptr, InMaterialModel, UE::DynamicMaterial::RenameFlags);
	}

	// Reset this map as it holds copies of the parameters from the copied-from object.
	// They will not be in this model's parameter list and will share the same name as the old parameters.
	// Just empty the list and create new parameters.
	for (TMap<int32, TObjectPtr<UDMMaterialParameter>>::TIterator It(MaterialParameters); It; ++It)
	{
		UDMMaterialParameter* Parameter = It->Value.Get();

		if (!Parameter || InMaterialModel->ConditionalFreeParameter(Parameter))
		{
			It.RemoveCurrent();
		}
	}

	MaterialParameters.Empty();

	// Create new parameters.
	CreateParameterNames();
}
#endif

void UDMTextureUV::SetMIDParameters(UMaterialInstanceDynamic* InMID)
{
	if (!IsComponentValid())
	{
		return;
	}

	check(InMID);
	check(MaterialParameters.IsEmpty() == false);

	using namespace UE::DynamicMaterial;

	auto UpdateMID = [InMID](FName InParamName, float InValue)
	{
		if (FMath::IsNearlyEqual(InValue, InMID->K2_GetScalarParameterValue(InParamName)) == false)
		{
			InMID->SetScalarParameterValue(InParamName, InValue);
		}
	};

	UpdateMID(MaterialParameters[ParamID::PivotX]->GetParameterName(), GetPivot().X);
	UpdateMID(MaterialParameters[ParamID::PivotY]->GetParameterName(), GetPivot().Y);
	UpdateMID(MaterialParameters[ParamID::ScaleX]->GetParameterName(), GetScale().X);
	UpdateMID(MaterialParameters[ParamID::ScaleY]->GetParameterName(), GetScale().Y);
	UpdateMID(MaterialParameters[ParamID::Rotation]->GetParameterName(), GetRotation());
	UpdateMID(MaterialParameters[ParamID::OffsetX]->GetParameterName(), GetOffset().X);
	UpdateMID(MaterialParameters[ParamID::OffsetY]->GetParameterName(), GetOffset().Y);
}

#if WITH_EDITOR
bool UDMTextureUV::Modify(bool bInAlwaysMarkDirty)
{
	const bool bSaved = Super::Modify(bInAlwaysMarkDirty);

	for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& Pair : MaterialParameters)
	{
		Pair.Value->Modify(bInAlwaysMarkDirty);
	}

	return bSaved;
}

void UDMTextureUV::EnsureDetailObjects()
{
	if (!IsComponentValid())
	{
		return;
	}

	bool bHasValidDetailObjects = true;

	if (!PropertyRowGenerator.IsValid() || (DetailTreeNodes.Num() != TextureProperties.Num() && PropertyHandles.Num() != TextureProperties.Num()))
	{
		bHasValidDetailObjects = false;
	}
	else
	{
		for (const TPair<FName, TSharedPtr<IDetailTreeNode>>& DetailTreeNode : DetailTreeNodes)
		{
			if (DetailTreeNode.Value.IsValid() == false)
			{
				bHasValidDetailObjects = false;
				break;
			}
		}

		if (bHasValidDetailObjects)
		{
			for (const TPair<FName, TSharedPtr<IPropertyHandle>>& PropertyHandle : PropertyHandles)
			{
				if (PropertyHandle.Value.IsValid() == false)
				{
					bHasValidDetailObjects = false;
					break;
				}
			}
		}
	}

	if (bHasValidDetailObjects)
	{
		return;
	}

	PropertyHandles.Reset();
	DetailTreeNodes.Reset();
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
			
			if (TextureProperties.Contains(ChildNode->GetNodeName()) == false)
			{
				continue;
			}

			DetailTreeNodes.Emplace(ChildNode->GetNodeName(), ChildNode);
			PropertyHandles.Emplace(ChildNode->GetNodeName(), ChildNode->CreatePropertyHandle());
		}

		return;
	}
}
#endif

void UDMTextureUV::Update(EDMUpdateType InUpdateType)
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
		ModelEditorOnlyData->OnTextureUVUpdated(this);
	}
#endif
}

UDynamicMaterialModel* UDMTextureUV::GetMaterialModel() const
{
	return Cast<UDynamicMaterialModel>(GetOuterSafe());
}

UDMMaterialComponent* UDMTextureUV::GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const
{
	auto GetParameter = [this, &InPath, &InPathSegment](int32 InParamId) -> UDMMaterialComponent*
		{
			if (const TObjectPtr<UDMMaterialParameter>* ParameterPtr = MaterialParameters.Find(InParamId))
			{
				return *ParameterPtr;
			}

			return UDMMaterialLinkedComponent::GetSubComponentByPath(InPath, InPathSegment);
		};

	using namespace UE::DynamicMaterial;

	if (InPathSegment.GetToken() == OffsetXPathToken)
	{
		return GetParameter(ParamID::OffsetX);
	}

	if (InPathSegment.GetToken() == OffsetYPathToken)
	{
		return GetParameter(ParamID::OffsetY);
	}

	if (InPathSegment.GetToken() == PivotXPathToken)
	{
		return GetParameter(ParamID::PivotX);
	}

	if (InPathSegment.GetToken() == PivotYPathToken)
	{
		return GetParameter(ParamID::PivotY);
	}

	if (InPathSegment.GetToken() == RotationPathToken)
	{
		return GetParameter(ParamID::Rotation);
	}

	if (InPathSegment.GetToken() == ScaleXPathToken)
	{
		return GetParameter(ParamID::ScaleX);
	}

	if (InPathSegment.GetToken() == ScaleYPathToken)
	{
		return GetParameter(ParamID::ScaleY);
	}

	return Super::GetSubComponentByPath(InPath, InPathSegment);
}

#if WITH_EDITOR
void UDMTextureUV::GetComponentPathInternal(TArray<FString>& OutChildComponentPathComponents) const
{
	using namespace UE::DynamicMaterial::Private;

	// Replace parameter object names with the base parameter name
	if (OutChildComponentPathComponents.IsEmpty() == false)
	{
		for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& MaterialParameter : MaterialParameters)
		{
			if (OutChildComponentPathComponents.Last() == MaterialParameter.Value->GetComponentPathComponent())
			{
				OutChildComponentPathComponents.Last() = BaseParameterNames[MaterialParameter.Key].ToString();
				break;
			}
		}
	}

	Super::GetComponentPathInternal(OutChildComponentPathComponents);
}

void UDMTextureUV::CreateParameterNames()
{
	if (!IsComponentValid())
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = GetMaterialModel();
	check(MaterialModel);

	using namespace UE::DynamicMaterial;

	auto CreateParam = [this, MaterialModel](int32 ParamId)
	{
		if (!MaterialParameters.Contains(ParamId))
		{
			using namespace UE::DynamicMaterial::Private;

			MaterialParameters.Add(ParamId, MaterialModel->CreateUniqueParameter(BaseParameterNames[ParamId]));
			MaterialParameters[ParamId]->SetParentComponent(this);
		}
	};

	CreateParam(ParamID::PivotX);
	CreateParam(ParamID::PivotY);
	CreateParam(ParamID::ScaleX);
	CreateParam(ParamID::ScaleY);
	CreateParam(ParamID::Rotation);
	CreateParam(ParamID::OffsetX);
	CreateParam(ParamID::OffsetY);
}

void UDMTextureUV::RemoveParameterNames()
{
	if (!IsComponentValid())
	{
		return;
	}

	UDynamicMaterialModel* MaterialModel = GetMaterialModel();
	check(MaterialModel);

#if WITH_EDITOR
	if (GUndo)
	{
		MaterialModel->Modify();
	}
#endif

	for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& Pair : MaterialParameters)
	{
#if WITH_EDITOR
		if (GUndo)
		{
			Pair.Value->Modify();
		}
#endif

		MaterialModel->FreeParameter(Pair.Value);
	}
}

void UDMTextureUV::OnComponentAdded()
{
	if (!IsComponentValid())
	{
		return;
	}

	CreateParameterNames();
	
	Super::OnComponentAdded();
}

void UDMTextureUV::OnComponentRemoved()
{
	RemoveParameterNames();

	Super::OnComponentRemoved();
}

void UDMTextureUV::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (!IsComponentValid())
	{
		return;
	}

	if (!PropertyChangedEvent.MemberProperty)
	{
		return;
	}

	if (PropertyChangedEvent.MemberProperty->GetFName() == NAME_Offset
		|| PropertyChangedEvent.MemberProperty->GetFName() == NAME_Pivot
		|| PropertyChangedEvent.MemberProperty->GetFName() == NAME_Rotation
		|| PropertyChangedEvent.MemberProperty->GetFName() == NAME_Scale)
	{
		Update(EDMUpdateType::Value);
	}
	else if (PropertyChangedEvent.MemberProperty->GetFName() == NAME_UVSource
		|| PropertyChangedEvent.MemberProperty->GetFName() == NAME_bMirrorOnX
		|| PropertyChangedEvent.MemberProperty->GetFName() == NAME_bMirrorOnY)
	{
		Update(EDMUpdateType::Structure);
	}
}

void UDMTextureUV::PreEditUndo()
{
	Super::PreEditUndo();

	UVSource_PreUndo = UVSource;
	bMirrorOnX_PreUndo = bMirrorOnX;
	bMirrorOnY_PreUndo = bMirrorOnY;
}

void UDMTextureUV::PostEditUndo()
{
	Super::PostEditUndo();

	if (UVSource != UVSource_PreUndo
		|| bMirrorOnX != bMirrorOnX_PreUndo
		|| bMirrorOnY != bMirrorOnY_PreUndo)
	{
		Update(EDMUpdateType::Structure);
	}
	else
	{
		Update(EDMUpdateType::Value);
	}
}

void UDMTextureUV::PostLoad()
{
	Super::PostLoad();

	if (!IsComponentValid())
	{
		return;
	}

	if (MaterialParameters.IsEmpty())
	{
		CreateParameterNames();
	}

	for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& Pair : MaterialParameters)
	{
		Pair.Value->SetParentComponent(this);
	}

	/*
	 * @TODO GetLinkerCustomVersion() isn't used here to trigger these updates because it always returns 
	 * the latest version regardless of what was saved to the archive.
	 * Inside the function, it is unable to find a Loader and thus fails in this way.
	 */

	if (bNeedsPostLoadStructureUpdate)
	{
		Update(EDMUpdateType::Structure);
	}
	else if (bNeedsPostLoadValueUpdate)
	{
		Update(EDMUpdateType::Value);
	}

	bNeedsPostLoadStructureUpdate = false;
	bNeedsPostLoadValueUpdate = false;
}

void UDMTextureUV::PostEditImport()
{
	Super::PostEditImport();

	if (!IsComponentValid())
	{
		return;
	}

	if (MaterialParameters.IsEmpty())
	{
		CreateParameterNames();
	}

	for (const TPair<int32, TObjectPtr<UDMMaterialParameter>>& Pair : MaterialParameters)
	{
		Pair.Value->SetParentComponent(this);
	}
}

UDMTextureUV* UDMTextureUV::CreateTextureUV(UObject* InOuter)
{
	UDMTextureUV* NewTextureUV = NewObject<UDMTextureUV>(InOuter, NAME_None, RF_Transactional);
	check(NewTextureUV);

	return NewTextureUV;
}
#endif

void UDMTextureUV::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(UDMTextureUV::GUID);

	Super::Serialize(Ar);

	// @See UDMTextureUV::PostLoad

	int32 TextureUVVersion = Ar.CustomVer(UDMTextureUV::GUID);

	while (TextureUVVersion != static_cast<int32>(EDMTextureUVVersion::LatestVersion))
	{
		switch (TextureUVVersion)
		{
			case INDEX_NONE:
			case static_cast<int32>(EDMTextureUVVersion::Initial_Pre_20221102):
				Offset.X *= -1;
				Rotation *= -360.f;
				Scale = FVector2D(1.f, 1.f) / Scale;
				TextureUVVersion = static_cast<int32>(EDMTextureUVVersion::LatestVersion);
#if WITH_EDITORONLY_DATA
				bNeedsPostLoadValueUpdate = true;
#endif
				break;

			case static_cast<int32>(EDMTextureUVVersion::LatestVersion):
				// Do nothing
				break;

			default:
				TextureUVVersion = static_cast<int32>(EDMTextureUVVersion::LatestVersion);
				break;
		}
	}
}
