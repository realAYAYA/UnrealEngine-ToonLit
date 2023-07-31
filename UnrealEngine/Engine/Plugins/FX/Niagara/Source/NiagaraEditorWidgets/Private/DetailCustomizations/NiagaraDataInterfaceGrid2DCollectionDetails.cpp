// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceGrid2DCollectionDetails.h"
#include "NiagaraDetailSourcedArrayBuilder.h"
#include "NiagaraDataInterfaceDetails.h"
#include "NiagaraDataInterfaceGrid2DCollection.h" 
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "IDetailGroup.h"
#include "NiagaraComponent.h"
#include "NiagaraConstants.h"
#include "NiagaraNodeInput.h"
#include "SNiagaraNamePropertySelector.h"

#define LOCTEXT_NAMESPACE "FNiagaraDataInterfaceGrid2DCollectionDetails"

FNiagaraDataInterfaceGrid2DCollectionDetails::~FNiagaraDataInterfaceGrid2DCollectionDetails()
{
}

void FNiagaraDataInterfaceGrid2DCollectionDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	static const FName Grid2DCollectionCategoryName = TEXT("Grid2DCollection");

	LayoutBuilder = &DetailBuilder;

	TArray<TWeakObjectPtr<UObject>> SelectedObjects;
	DetailBuilder.GetObjectsBeingCustomized(SelectedObjects);
	if (SelectedObjects.Num() != 1 || SelectedObjects[0]->IsA<UNiagaraDataInterfaceGrid2DCollection>() == false)
	{
		return;
	}

	UNiagaraDataInterfaceGrid2DCollection* Grid2DInterface = CastChecked<UNiagaraDataInterfaceGrid2DCollection>(SelectedObjects[0].Get());
	Grid2DInterfacePtr = Grid2DInterface;

	Grid2DInterface->OnChanged().RemoveAll(this);
	Grid2DInterface->OnChanged().AddSP(this, &FNiagaraDataInterfaceGrid2DCollectionDetails::OnInterfaceChanged);

	Grid2DCollectionCategory = &DetailBuilder.EditCategory(Grid2DCollectionCategoryName, LOCTEXT("Grid2DCollectionCat", "Grid2DCollection"));
	{
		TArray<TSharedRef<IPropertyHandle>> Properties;
		Grid2DCollectionCategory->GetDefaultProperties(Properties, true, true);

		TSharedPtr<IPropertyHandle> PreviewAttributeProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceGrid2DCollection, PreviewAttribute));

		for (TSharedPtr<IPropertyHandle> Property : Properties)
		{
			FProperty* PropertyPtr = Property->GetProperty();

			if (PropertyPtr == PreviewAttributeProperty->GetProperty())
			{
				TArray<TSharedPtr<FName>> PossibleNames;
				GeneratePreviewAttributes(PossibleNames);
				PreviewAttributesBuilder = SNew(SNiagaraNamePropertySelector, Property.ToSharedRef(), PossibleNames);

				IDetailPropertyRow& PropertyRow = Grid2DCollectionCategory->AddProperty(Property);
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
				Grid2DCollectionCategory->AddProperty(Property);
			}
		}
	}
}

TSharedRef<IDetailCustomization> FNiagaraDataInterfaceGrid2DCollectionDetails::MakeInstance()
{
	return MakeShared<FNiagaraDataInterfaceGrid2DCollectionDetails>();
}

void FNiagaraDataInterfaceGrid2DCollectionDetails::OnInterfaceChanged()
{
	OnDataChanged();
}

void FNiagaraDataInterfaceGrid2DCollectionDetails::OnDataChanged()
{
	if (PreviewAttributesBuilder)
	{
		TArray<TSharedPtr<FName>> PossibleNames;
		GeneratePreviewAttributes(PossibleNames);
		PreviewAttributesBuilder->SetSourceArray(PossibleNames);
	}
}

void FNiagaraDataInterfaceGrid2DCollectionDetails::GeneratePreviewAttributes(TArray<TSharedPtr<FName>>& SourceArray)
{
	SourceArray.Reset();
	SourceArray.Add(MakeShared<FName>(NAME_None));

	if ( UNiagaraDataInterfaceGrid2DCollection* Grid2DInterface = Grid2DInterfacePtr.Get() )
	{
		// Discover attributes by name
		FName VariableName;
		if (UNiagaraNodeInput* NodeInput = Cast<UNiagaraNodeInput>(Grid2DInterface->GetOuter()))
		{
			VariableName = NodeInput->Input.GetName();
		}

		if ( !VariableName.IsNone() )
		{
			// Resolve namespace
			if (UNiagaraEmitter* OwnerEmitter = Grid2DInterface->GetTypedOuter<UNiagaraEmitter>())
			{
				FNiagaraAliasContext ResolveAliasesContext(FNiagaraAliasContext::ERapidIterationParameterMode::EmitterOrParticleScript);
				ResolveAliasesContext.ChangeEmitterToEmitterName(OwnerEmitter->GetUniqueEmitterName());
				VariableName = FNiagaraUtilities::ResolveAliases(FNiagaraVariable(UNiagaraDataInterfaceGrid2DCollection::StaticClass(), VariableName), ResolveAliasesContext).GetName();
			}

			// Add named attributes
			TArray<FNiagaraVariableBase> FoundVariables;
			TArray<uint32> FoundVariableOffsets;
			int32 FoundNumAttribChannelsFound;
			Grid2DInterface->FindAttributesByName(VariableName, FoundVariables, FoundVariableOffsets, FoundNumAttribChannelsFound);

			for (const FNiagaraVariableBase& Variable : FoundVariables)
			{
				SourceArray.Add(MakeShared<FName>(FName(Variable.GetName())));
			}
		}

		// Add anonymous attributes
		for (int32 i = 0; i < Grid2DInterface->NumAttributes; ++i)
		{
			FString AttributeName = FString::Printf(TEXT("%s %d"), *UNiagaraDataInterfaceGrid2DCollection::AnonymousAttributeString, i);
			SourceArray.Add(MakeShared<FName>(FName(*AttributeName)));
		}
	}
}

#undef LOCTEXT_NAMESPACE
