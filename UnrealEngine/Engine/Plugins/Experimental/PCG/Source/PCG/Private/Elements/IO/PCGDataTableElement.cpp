// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/IO/PCGDataTableElement.h"

#include "PCGComponent.h"
#include "PCGModule.h"
#include "Data/PCGPointData.h"
#include "Elements/IO/PCGExternalDataContext.h"

#include "Engine/DataTable.h"

#define LOCTEXT_NAMESPACE "PCGLoadDataTable"

#if WITH_EDITOR

FText UPCGLoadDataTableSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Load Data Table");
}

FText UPCGLoadDataTableSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Loads data from DataTable asset");
}

#endif // WITH_EDITOR

FPCGElementPtr UPCGLoadDataTableSettings::CreateElement() const
{
	return MakeShared<FPCGLoadDataTableElement>();
}

bool FPCGLoadDataTableElement::PrepareLoad(FPCGExternalDataContext* Context) const
{
	check(Context);
	const UPCGLoadDataTableSettings* Settings = Context->GetInputSettings<UPCGLoadDataTableSettings>();
	check(Settings);

	// 1. Load data table - if we do, we should root it (?)
	UDataTable* DataTable = Settings->DataTable.LoadSynchronous();
	if (!DataTable)
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("DataTableNotLoaded", "Provided Data Table ('{0}') could not be loaded."), FText::FromString(Settings->DataTable.ToString())));
		return true;
	}

	// 2. Make sure it has a row struct
	const UScriptStruct* RowStruct = DataTable->GetRowStruct();
	if (!RowStruct)
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("DataTableNoRowStruct", "Provided Data Table ('{0}') does not have a valid row definition."), FText::FromString(Settings->DataTable.ToString())));
		return true;
	}

	// 2.1 Validate that there are rows too
	if (DataTable->GetRowMap().IsEmpty())
	{
		PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("DataTableIsEmpty", "Provided Data Table ('{0}') is empty."), FText::FromString(Settings->DataTable.ToString())));
		return true;
	}

	// 3-e. Create point data, and the mapping object
	UPCGPointData* PointData = NewObject<UPCGPointData>();
	check(PointData);

	FPCGExternalDataContext::FPointDataAccessorsMapping& PointDataAccessorMapping = Context->PointDataAccessorsMapping.Emplace_GetRef();
	PointDataAccessorMapping.PointData = PointData;

	UPCGMetadata* PointMetadata = PointData->MutableMetadata();

	// 3. Build property to accessor mappings, including renamings, and create new attributes as needed
	for (TFieldIterator<const FProperty> FieldIt(RowStruct, EFieldIterationFlags::IncludeSuper); FieldIt; ++FieldIt)
	{
		FString FieldName = RowStruct->GetAuthoredNameForField(*FieldIt);

		// Try to create the read accessor
		TUniquePtr<IPCGAttributeAccessor> DataAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(*FieldIt);

		if (!DataAccessor)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("DataTableFieldTypeNotSupported", "Provided Data Table ('{0}') property '{1}' is not of a supported type."), FText::FromString(Settings->DataTable.ToString()), FText::FromString(RowStruct->GetAuthoredNameForField(*FieldIt))));
			continue;
		}

		// Try to match to a property (if the name maps to a property) or create attribute as needed
		FPCGAttributePropertyOutputSelector PointPropertySelector;

		if (const FPCGAttributePropertySelector* MappedField = Settings->AttributeMapping.Find(FieldName))
		{
			PointPropertySelector.ImportFromOtherSelector(*MappedField);
			FieldName = PointPropertySelector.GetName().ToString();
		}
		else
		{
			PointPropertySelector.Update(FieldName);
		}

		if (PointPropertySelector.GetSelection() == EPCGAttributePropertySelection::Attribute)
		{
			// Create attribute with the appropriate type
			const void* FirstRow = DataTable->GetRowMap().CreateConstIterator().Value();
			PointMetadata->CreateAttributeFromDataProperty(FName(FieldName), FirstRow, *FieldIt);
		}

		TUniquePtr<IPCGAttributeAccessor> PointPropertyAccessor = PCGAttributeAccessorHelpers::CreateAccessor(PointData, PointPropertySelector);

		if (!PointPropertyAccessor)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("UnableToCreatePointProperty", "Unable to create/access point property/attribute '{0}'."), FText::FromString(FieldName)));
			continue;
		}

		// Final verification, if we can put the value of input into output
		if (!PCG::Private::IsBroadcastable(DataAccessor->GetUnderlyingType(), PointPropertyAccessor->GetUnderlyingType()))
		{
			FText InputTypeName = FText::FromString(PCG::Private::GetTypeName(DataAccessor->GetUnderlyingType()));
			FText OutputTypeName = FText::FromString(PCG::Private::GetTypeName(PointPropertyAccessor->GetUnderlyingType()));

			PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("CannotBroadcastTypes", "Cannot convert input type '{0}' into output type '{1}'"), InputTypeName, OutputTypeName));
			continue;
		}

		PointDataAccessorMapping.RowToPointAccessors.Emplace(MoveTemp(DataAccessor), MoveTemp(PointPropertyAccessor), PointPropertySelector);
	}

	if (!Context->PointDataAccessorsMapping.IsEmpty())
	{
		FPCGTaggedData& OutData = Context->OutputData.TaggedData.Emplace_GetRef();
		OutData.Data = PointData;

		Context->bDataPrepared = true;
	}

	return true;
}


bool FPCGLoadDataTableElement::ExecuteLoad(FPCGExternalDataContext* Context) const
{
	check(Context);

	if (Context->PointDataAccessorsMapping.IsEmpty())
	{
		return true;
	}

	const UPCGLoadDataTableSettings* Settings = Context->GetInputSettings<UPCGLoadDataTableSettings>();
	check(Settings);

	//TODO: instead of trying to reload here, it would be better to make sure that this is rooted properly
	UDataTable* DataTable = Settings->DataTable.LoadSynchronous();
	check(DataTable);

	// Note that while this is prepared for execution in multiple calls, we need to make sure the data table is kept in memory at all times otherwise the row keys could become stale
	check(Context->PointDataAccessorsMapping.Num() == 1);
	FPCGExternalDataContext::FPointDataAccessorsMapping& PointDataAccessorMapping = Context->PointDataAccessorsMapping[0];

	// Preallocate the point array, and create the row keys
	UPCGPointData* PointData = PointDataAccessorMapping.PointData;
	check(PointData);

	TArray<FPCGPoint>& Points = PointDataAccessorMapping.PointData->GetMutablePoints();
	if (Points.IsEmpty())
	{
		// Dummy implementation - note that we do initialize the points here in cases where some of the properties would not be set
		Points.SetNum(DataTable->GetRowMap().Num());
	}

	TArray<void*> AllRows;
	AllRows.Reserve(DataTable->GetRowMap().Num());
	for (TMap<FName, uint8*>::TConstIterator RowMapIter(DataTable->GetRowMap().CreateConstIterator()); RowMapIter; ++RowMapIter)
	{
		AllRows.Add(RowMapIter.Value());
	}

	PointDataAccessorMapping.RowKeys = MakeUnique<FPCGAttributeAccessorKeysGenericPtrs>(AllRows);
	
	return FPCGExternalDataElement::ExecuteLoad(Context);
}

#undef LOCTEXT_NAMESPACE