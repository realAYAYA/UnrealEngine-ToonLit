#pragma once
#include "CoreMinimal.h"

class FCustomEditorToolbar
{
	
public:

	virtual ~FCustomEditorToolbar();
	FCustomEditorToolbar() : CommandList(new FUICommandList)
	{
		ContextObject = nullptr;
	}

	virtual void Initialize();
	
	void BindCommands();
	
	/** This function will be bound to Command. */
	void StartGameService_Executed();
	void StopGameService_Executed();
	void ShowGddInFileExplorer_Executed();
	void ShowExcelInFileExplorer_Executed();
	void UpdateGdd_Executed();
	void ReloadGdd_Executed();
	void UpdatePb_Executed();
	void GenerateWidgetTsFile_Executed();
	

	TSharedPtr<FUICommandList> GetCommandList() const
	{
		return CommandList;
	}

protected:

	static TSharedRef<SWidget> GenerateMenuContent(TSharedRef<FUICommandList> InCommandList, UObject* Context);

	const TSharedRef<FUICommandList> CommandList;

	void BuildToolbar(FToolBarBuilder& ToolbarBuilder, UObject* InContextObject);

	UObject* ContextObject = nullptr;// 当前操作文件（蓝图）上下文
};