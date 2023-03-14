// Copyright Epic Games, Inc. All Rights Reserved.

#include "NaniteDisplacedMeshCustomization.h"

#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/Text.h"
#include "Misc/AssertionMacros.h"
#include "ScopedTransaction.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "FNaniteDisplacedMeshDetails"

TSharedRef<IDetailCustomization> FNaniteDisplacedMeshDetails::MakeInstance()
{
	return MakeShared<FNaniteDisplacedMeshDetails>();
}

void FNaniteDisplacedMeshDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> ObjectsCustomized;
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	CustomizedPairs.Reserve(ObjectsCustomized.Num());

	for (const TWeakObjectPtr<UObject>& ObjectCustomized : ObjectsCustomized)
	{
		if (UNaniteDisplacedMesh* DisplacedMesh = Cast<UNaniteDisplacedMesh>(ObjectCustomized.Get()))
		{
			CustomizedPairs.Emplace(TWeakObjectPtr<UNaniteDisplacedMesh>(DisplacedMesh), DisplacedMesh->Parameters);
		}
	}

	{
		IDetailCategoryBuilder& ParametersCategory = DetailBuilder.EditCategory(TEXT("Parameters"));
		
		UStruct* NaniteDisplacedMeshStaticStruct = FNaniteDisplacedMeshParams::StaticStruct();
		for (int32 Index = 0; Index < CustomizedPairs.Num(); ++Index)
		{
			FNaniteDisplacedMeshParams& DisplacedMeshParams = CustomizedPairs[Index].Value;

			// The custom struct work well with minimum code added, but it doesn't support transaction like the existing stuff for the static meshes
			TArray<IDetailPropertyRow*> PropertyRows;
			ParametersCategory.AddAllExternalStructureProperties(MakeShared<FStructOnScope>(NaniteDisplacedMeshStaticStruct, static_cast<uint8*>(static_cast<void*>(&DisplacedMeshParams))), EPropertyLocation::Default, &PropertyRows);

			for (IDetailPropertyRow* PropertyRow : PropertyRows)
			{
				// Turned off the custom reset to default for now so that we can submit the rest until this is in a working state.
				/*
				const bool bPropagateToChildren = true;
				PropertyRow->OverrideResetToDefault(FResetToDefaultOverride::Create(
					FIsResetToDefaultVisible::CreateSP(this, &FNaniteDisplacedMeshDetails::DoesParamDifferFromOriginalValue, Index),
					FResetToDefaultHandler::CreateSP(this, &FNaniteDisplacedMeshDetails::ResetParamToOriginalValue, Index),
					bPropagateToChildren
					));
				*/

				PropertyRow->EditCondition(GetCanEditAttribute(Index), FOnBooleanValueChanged());
			}

			ParametersCategory.AddCustomRow(LOCTEXT("ApplyChanges", "Apply Changes"))
				.ValueContent()
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.OnClicked(this, &FNaniteDisplacedMeshDetails::ApplyNaniteDisplacedMeshParams)
					.IsEnabled(this, &FNaniteDisplacedMeshDetails::IsApplyNaniteDisplacedMeshParamsNeeded)
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("ApplyChanges", "Apply Changes"))
						.Font(DetailBuilder.GetDetailFont())
					]
				];
		}
	}

	TSharedRef<IPropertyHandle> ParametersPropertyHandle = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNaniteDisplacedMesh, Parameters));
	ParametersPropertyHandle->MarkHiddenByCustomization();
}

FReply FNaniteDisplacedMeshDetails::ApplyNaniteDisplacedMeshParams()
{
	FScopedTransaction Transaction(LOCTEXT("ApplyDisplacementParamaterToMesh", "Apply Parameters to Nanite Displaced Mesh"));
	FProperty* ParametersProperty = UNaniteDisplacedMesh::StaticClass()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(UNaniteDisplacedMesh, Parameters));
	for (TPair<TWeakObjectPtr<UNaniteDisplacedMesh>, FNaniteDisplacedMeshParams>& Pair : CustomizedPairs)
	{
		if (UNaniteDisplacedMesh* DisplacedMesh = Pair.Key.Get())
		{
			DisplacedMesh->PreEditChange(ParametersProperty);
			DisplacedMesh->Parameters = Pair.Value;
			FPropertyChangedEvent ChangedEvent(ParametersProperty, EPropertyChangeType::ValueSet);
			DisplacedMesh->PostEditChangeProperty(ChangedEvent);
		}
	}

	return FReply::Handled();
}

bool FNaniteDisplacedMeshDetails::IsApplyNaniteDisplacedMeshParamsNeeded() const
{
	for (const TPair<TWeakObjectPtr<UNaniteDisplacedMesh>, FNaniteDisplacedMeshParams>& Pair : CustomizedPairs)
	{
		if (UNaniteDisplacedMesh* DisplacedMesh = Pair.Key.Get())
		{
			if (DisplacedMesh->Parameters != Pair.Value)
			{
				return true;
			}
		}
	}

	return false;
}

bool FNaniteDisplacedMeshDetails::DoesParamDifferFromOriginalValue(TSharedPtr<IPropertyHandle> Handle, int32 ObjectIndex)
{
	/*
	TPair<TWeakObjectPtr<UNaniteDisplacedMesh>, FNaniteDisplacedMeshParams>& Pair = CustomizedPairs[ObjectIndex];
	if (UNaniteDisplacedMesh* DisplacedMesh = Pair.Key.Get())
	{
		// Check if the property value exist in the NaniteDisplacedMesh
		if (uint8* DestAddress = Handle->GetValueBaseAddress(static_cast<uint8*>(static_cast<void*>(&(DisplacedMesh->Parameters)))))
		{
			uint8* SourceAddress = Handle->GetValueBaseAddress(static_cast<uint8*>(static_cast<void*>(&(Pair.Value))));
			return !Handle->GetProperty()->Identical(DestAddress, SourceAddress);
		}
	}
	*/
	return false;
}

void FNaniteDisplacedMeshDetails::ResetParamToOriginalValue(TSharedPtr<IPropertyHandle> Handle, int32 ObjectIndex)
{
	/*
	TPair<TWeakObjectPtr<UNaniteDisplacedMesh>, FNaniteDisplacedMeshParams>& Pair = CustomizedPairs[ObjectIndex];
	if (UNaniteDisplacedMesh* DisplacedMesh = Pair.Key.Get())
	{
		// Check if the property value exist in the NaniteDisplacedMesh
		if (uint8* SourceAddress = Handle->GetValueBaseAddress(static_cast<uint8*>(static_cast<void*>(&(DisplacedMesh->Parameters)))))
		{
			uint8* DestAddress = Handle->GetValueBaseAddress(static_cast<uint8*>(static_cast<void*>(&(Pair.Value))));
			Handle->GetProperty()->CopyCompleteValue(DestAddress, SourceAddress);
		}
	}
	*/

}

TAttribute<bool> FNaniteDisplacedMeshDetails::GetCanEditAttribute(int32 ObjectIndex)
{
	TPair<TWeakObjectPtr<UNaniteDisplacedMesh>, FNaniteDisplacedMeshParams>& Pair = CustomizedPairs[ObjectIndex];
	return TAttribute<bool>::Create([WeakMesh = Pair.Key]
		{
			if (UNaniteDisplacedMesh* DisplacedMesh = WeakMesh.Get())
			{
				return DisplacedMesh->bIsEditable;
			}

			return false;
		});
}

#undef LOCTEXT_NAMESPACE
