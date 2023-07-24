// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraceServices/Model/TableImport.h"
#include "TraceServices/Containers/Tables.h"

#include "Insights/IUnrealInsightsModule.h"

class SDockTab;
class FSpawnTabArgs;

namespace TraceServices
{
	struct FTableImportCallbackParams;
}

namespace Insights
{

class SUntypedTableTreeView;

////////////////////////////////////////////////////////////////////////////////////////////////////

struct FOpenImportedTableTabData
{
	TSharedPtr<SDockTab> Tab;
	TSharedPtr<SUntypedTableTreeView> TableTreeView;

	bool operator ==(const FOpenImportedTableTabData& Other) const
	{
		return Tab == Other.Tab && TableTreeView == Other.TableTreeView;
	}
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FTableImportTool : public TSharedFromThis<FTableImportTool>, public IInsightsComponent
{
public:
	/** Default constructor. */
	FTableImportTool();

	/** Destructor. */
	virtual ~FTableImportTool();

	/** Creates an instance of the Table Import Tool. */
	static TSharedPtr<FTableImportTool> CreateInstance();

	static TSharedPtr<FTableImportTool> Get();

	// IInsightsComponent
	virtual void Initialize(IUnrealInsightsModule& InsightsModule) override {}
	virtual void Shutdown() override {}
	virtual void RegisterMajorTabs(IUnrealInsightsModule& InsightsModule) override {}
	virtual void UnregisterMajorTabs() override {}
	virtual void OnWindowClosedEvent() override;

	void StartImportProcess();
	void ImportFile(const FString& Filename);
	void StartDiffProcess();
	void DiffFiles(const FString& FilenameA, const FString& FilenameB);

	void CloseAllOpenTabs();

private:
	TSharedRef<SDockTab> SpawnTab_TableImportTreeView(const FSpawnTabArgs& Args, FName TableViewID, FText InDisplayName);
	TSharedRef<SDockTab> SpawnTab_TableDiffTreeView(const FSpawnTabArgs& Args, FName TableViewID, FText InDisplayName);
	void OnTableImportTreeViewTabClosed(TSharedRef<SDockTab> TabBeingClosed);

	void DisplayImportTable(FName TableViewID);
	void DisplayDiffTable(FName TableViewID);
	void TableImportServiceCallback(TSharedPtr<TraceServices::FTableImportCallbackParams> Params);

	FName GetTableID(const FString& Path);
	static TSharedPtr<FTableImportTool> Instance;
	TMap<FName, FOpenImportedTableTabData> OpenTablesMap;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
