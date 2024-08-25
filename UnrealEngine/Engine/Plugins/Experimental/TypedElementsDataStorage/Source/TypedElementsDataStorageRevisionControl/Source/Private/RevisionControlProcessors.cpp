// Copyright Epic Games, Inc. All Rights Reserved.

#include "RevisionControlProcessors.h"

#include "ISourceControlModule.h"
#include "SourceControlFileStatusMonitor.h"
#include "HAL/IConsoleManager.h"

#include "Elements/Columns/TypedElementMiscColumns.h"
#include "Elements/Columns/TypedElementPackageColumns.h"
#include "Elements/Columns/TypedElementRevisionControlColumns.h"
#include "Elements/Columns/TypedElementSelectionColumns.h"
#include "Elements/Columns/TypedElementViewportColumns.h"
#include "Elements/Framework/TypedElementQueryBuilder.h"
#include "Elements/Framework/TypedElementRegistry.h"

#include "MassActorSubsystem.h"

extern TYPEDELEMENTSDATASTORAGE_API FAutoConsoleVariableRef CVarAutoPopulateState;

static bool GEnableOutlines = false;
static FAutoConsoleVariableRef CVarEnableOutlines(
	TEXT("TEDS.RevisionControl.UseOutlines"),
	GEnableOutlines,
	TEXT("Use revision control status outlines in the viewport")
);

static uint8 DetermineOutlineColorIndex(const TypedElementDataStorage::ICommonQueryContext& Context)
{
	if (GEnableOutlines)
	{
		// TODO: Get this information from TEDS instead of hardcoded pointing into UEditorStyleSettings::AdditionalSelectionColors?
		constexpr uint8 BasicSelectionColorCount = 2;
		constexpr uint8 IndexBlue = BasicSelectionColorCount + 0;
		constexpr uint8 IndexPurple = BasicSelectionColorCount + 1;
		constexpr uint8 IndexPink = BasicSelectionColorCount + 2;
		constexpr uint8 IndexRed = BasicSelectionColorCount + 3;
		constexpr uint8 IndexYellow = BasicSelectionColorCount + 4;
		constexpr uint8 IndexGreen = BasicSelectionColorCount + 5;

		// Check if the package is outdated because there is a newer version available.
		if (Context.HasColumn<FSCCNotCurrentTag>())
		{
			return IndexYellow;
		}

		// Check if the package is locked by someone else.
		if (Context.HasColumn<FSCCExternallyLockedColumn>())
		{
			return IndexRed;
		}

		// Check if the package is added locally.
		if (Context.HasColumn<FSCCStatusColumn>())
		{
			const FSCCStatusColumn* StatusColumn = Context.GetColumn<FSCCStatusColumn>();
			if (StatusColumn->Modification == ESCCModification::Added)
			{
				return IndexGreen;
			}
		}

		// Check if the package is locked by self.
		if (Context.HasColumn<FSCCLockedTag>())
		{
			return IndexBlue;
		}
	}

	return 0; // Default outline color.
}

void UTypedElementRevisionControlFactory::RegisterTables(ITypedElementDataStorageInterface& DataStorage)
{
	DataStorage.RegisterTable(
		TTypedElementColumnTypeList<
			FTypedElementPackagePathColumn, FTypedElementPackageLoadedPathColumn,
			FSCCRevisionIdColumn, FSCCExternalRevisionIdColumn>(),
		FName("Editor_RevisionControlTable"));
}

void UTypedElementRevisionControlFactory::RegisterQueries(ITypedElementDataStorageInterface& DataStorage)
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;

	TypedElementQueryHandle ObjectToSCCQuery = DataStorage.RegisterQuery(
		Select()
			.ReadOnly<FTypedElementPackagePathColumn>()
		.Compile());

	DataStorage.RegisterQuery(
		Select(
			TEXT("Change selection outline colors based on SCC status"),
			// This is in PrePhysics because the outline->actor query is in DuringPhysics and contexts don't flush changes between tick groups
			FProcessor(DSI::EQueryTickPhase::PrePhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage)),
			[](DSI::IQueryContext& Context, TypedElementRowHandle ObjectRow, const FTypedElementPackageReference& PackageReference)
			{
				Context.RunSubquery(0, PackageReference.Row, CreateSubqueryCallbackBinding(
					[&Context, &ObjectRow](DSI::ISubqueryContext& SubQueryContext)
					{
						Context.AddColumn<FTypedElementViewportColorColumn>(ObjectRow, { .SelectionOutlineColorIndex = DetermineOutlineColorIndex(SubQueryContext) });
						Context.AddColumns<FTypedElementSyncBackToWorldTag>(ObjectRow);
					})
				);
			}
		)
		.DependsOn()
			.SubQuery(ObjectToSCCQuery)
		.Compile()
	);
	
	CVarAutoPopulateState->AsVariable()->OnChangedDelegate().AddLambda(
		[this, &DataStorage](IConsoleVariable* AutoPopulate)
		{
			if (AutoPopulate->GetBool())
			{
				RegisterFetchUpdates(DataStorage);
			}
			else
			{
				DataStorage.UnregisterQuery(FetchUpdates);
			}
		}
	); 
	
	if (CVarAutoPopulateState->GetBool())
	{
		RegisterFetchUpdates(DataStorage);
	}
}

void UTypedElementRevisionControlFactory::RegisterFetchUpdates(ITypedElementDataStorageInterface& DataStorage) const
{
	using namespace TypedElementQueryBuilder;
	using DSI = ITypedElementDataStorageInterface;
	
	FSourceControlFileStatusMonitor& FileStatusMonitor = ISourceControlModule::Get().GetSourceControlFileStatusMonitor();

	FetchUpdates = DataStorage.RegisterQuery(
		Select(
			TEXT("Gather source control statuses for objects with unresolved package paths"),
			FProcessor(DSI::EQueryTickPhase::DuringPhysics, DataStorage.GetQueryTickGroupName(DSI::EQueryTickGroups::SyncExternalToDataStorage))
				.ForceToGameThread(true),
			[this, &FileStatusMonitor](DSI::IQueryContext& Context, const FTypedElementPackageUnresolvedReference* InUnresolvedReferences)
			{
				TConstArrayView<TypedElementDataStorage::RowHandle> RowHandles = Context.GetRowHandles();
				TConstArrayView<FTypedElementPackageUnresolvedReference, int64> UnresolvedReferences { InUnresolvedReferences, Context.GetRowCount() };

				for (int64 UnresolvedReferenceIndex = 0; UnresolvedReferenceIndex < UnresolvedReferences.Num(); ++UnresolvedReferenceIndex)
				{
					const FTypedElementPackageUnresolvedReference& UnresolvedReference = UnresolvedReferences[UnresolvedReferenceIndex];
					if (UnresolvedReference.Index == 0)
					{
						Context.RemoveColumns<FTypedElementPackageUnresolvedReference>(RowHandles[UnresolvedReferenceIndex]);
						return;
					}
					static FSourceControlFileStatusMonitor::FOnSourceControlFileStatus EmptyDelegate{};
					
					FileStatusMonitor.StartMonitoringFile(
						reinterpret_cast<uintptr_t>(this),
						UnresolvedReference.PathOnDisk,
						EmptyDelegate
					);
				}
			}
		)
		.Compile()
	);
}
