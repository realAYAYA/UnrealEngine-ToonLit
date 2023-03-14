// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceGrid3DCollectionDetails.h"
#include "NiagaraDetailSourcedArrayBuilder.h"
#include "NiagaraDataInterfaceDetails.h"
#include "NiagaraDataInterfaceGrid3DCollection.h" 
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "NiagaraComponent.h"
#include "NiagaraConstants.h"
#include "NiagaraNodeInput.h"
#include "SNiagaraNamePropertySelector.h"

#define LOCTEXT_NAMESPACE "FNiagaraDataInterfaceGrid3DCollectionDetails"

FNiagaraDataInterfaceGrid3DCollectionDetails::~FNiagaraDataInterfaceGrid3DCollectionDetails()
{
}

void FNiagaraDataInterfaceGrid3DCollectionDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	static const FName Grid3DCollectionCategoryName = TEXT("Grid3DCollection");

	LayoutBuilder = &DetailBuilder;

	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	if (SelectedObjects.Num() != 1 || SelectedObjects[0]->IsA<UNiagaraDataInterfaceGrid3DCollection>() == false)
	{
		return;
	}

	UNiagaraDataInterfaceGrid3DCollection* Grid3DInterface = CastChecked<UNiagaraDataInterfaceGrid3DCollection>(SelectedObjects[0].Get());
	Grid3DInterfacePtr = Grid3DInterface;

	Grid3DInterface->OnChanged().RemoveAll(this);
	Grid3DInterface->OnChanged().AddSP(this, &FNiagaraDataInterfaceGrid3DCollectionDetails::OnInterfaceChanged);

	Grid3DCollectionCategory = &DetailBuilder.EditCategory(Grid3DCollectionCategoryName, LOCTEXT("Grid3DCollectionCat", "Grid3DCollection"));
	{
		TArray<TSharedRef<IPropertyHandle>> Properties;
		Grid3DCollectionCategory->GetDefaultProperties(Properties, true, true);

		TSharedPtr<IPropertyHandle> PreviewAttributeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceGrid3DCollection, PreviewAttribute));

		for (TSharedPtr<IPropertyHandle> Property : Properties)
		{
			FProperty* PropertyPtr = Property->GetProperty();

			if (PropertyPtr == PreviewAttributeProperty->GetProperty())
			{
				TArray<TSharedPtr<FName>> PossibleNames;
				GeneratePreviewAttributes(PossibleNames);
				PreviewAttributesBuilder = SNew(SNiagaraNamePropertySelector, Property.ToSharedRef(), PossibleNames);

				IDetailPropertyRow& PropertyRow = Grid3DCollectionCategory->AddProperty(Property);
				PropertyRow.CustomWidget(false)
					.NameContent()
					[
						Property->CreatePropertyNameWidget()
					]
					.ValueContent()
					.MaxDesiredWidth(TOptional<float>())
					[
						PreviewAttributesBuilder.ToSharedRef()
					];
			}
			else
			{
				Grid3DCollectionCategory->AddProperty(Property);
			}
		}
	}
}

TSharedRef<IDetailCustomization> FNiagaraDataInterfaceGrid3DCollectionDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceGrid3DCollectionDetails>();
}

void FNiagaraDataInterfaceGrid3DCollectionDetails::OnInterfaceChanged()
{
	OnDataChanged();
}

void FNiagaraDataInterfaceGrid3DCollectionDetails::OnDataChanged()
{
	if (PreviewAttributesBuilder)
	{
		TArray<TSharedPtr<FName>> PossibleNames;
		GeneratePreviewAttributes(PossibleNames);
		PreviewAttributesBuilder->SetSourceArray(PossibleNames);
	}
}

void FNiagaraDataInterfaceGrid3DCollectionDetails::GeneratePreviewAttributes(TArray<TSharedPtr<FName>>& SourceArray)
{
	SourceArray.Reset();
	SourceArray.Add(MakeShared<FName>(NAME_None));

	if ( UNiagaraDataInterfaceGrid3DCollection* Grid3DInterface = Grid3DInterfacePtr.Get() )
	{
		// Discover attributes by name
		FName VariableName;
		if (UNiagaraNodeInput* NodeInput = Cast<UNiagaraNodeInput>(Grid3DInterface->GetOuter()))
		{
			VariableName = NodeInput->Input.GetName();
		}

		if ( !VariableName.IsNone() )
		{
			// Resolve namespace
			if (UNiagaraEmitter* OwnerEmitter = Grid3DInterface->GetTypedOuter<UNiagaraEmitter>())
			{
				FNiagaraAliasContext ResolveAliasesContext(FNiagaraAliasContext::ERapidIterationParameterMode::EmitterOrParticleScript);
				ResolveAliasesContext.ChangeEmitterToEmitterName(OwnerEmitter->GetUniqueEmitterName());
				VariableName = FNiagaraUtilities::ResolveAliases(FNiagaraVariable(UNiagaraDataInterfaceGrid3DCollection::StaticClass(), VariableName), ResolveAliasesContext).GetName();
			}

			// Add named attributes
			TArray<FNiagaraVariableBase> FoundVariables;
			TArray<uint32> FoundVariableOffsets;
			int32 FoundNumAttribChannelsFound;
			Grid3DInterface->FindAttributesByName(VariableName, FoundVariables, FoundVariableOffsets, FoundNumAttribChannelsFound);

			for (const FNiagaraVariableBase& Variable : FoundVariables)
			{
				SourceArray.Add(MakeShared<FName>(FName(Variable.GetName())));
			}
		}

		// Add anonymous attributes
		for (int32 i = 0; i < Grid3DInterface->NumAttributes; ++i)
		{
			FString AttributeName = FString::Printf(TEXT("%s %d"), *UNiagaraDataInterfaceGrid3DCollection::AnonymousAttributeString, i);
			SourceArray.Add(MakeShared<FName>(FName(*AttributeName)));
		}
	}
}

#undef LOCTEXT_NAMESPACE
