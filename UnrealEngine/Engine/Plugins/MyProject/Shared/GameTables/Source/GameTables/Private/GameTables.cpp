#include "GameTables.h"

#include "ConfigLoadHelper.h"
#include "Excels/ExcelItemTable.h"

DEFINE_LOG_CATEGORY(LogGameTables);

#define INIT_TABLE(TableType, VarName) \
	do { \
		if (!VarName) { \
			VarName = NewObject<TableType>(); \
		} \
		if (VarName->Init(bLoadImmediately)) { \
			UE_LOG(LogGameTables, Warning, TEXT("[GameTables] %s加载成功 {}"), *VarName->GetConfigFileName()); \
		} \
		else { \
			UE_LOG(LogGameTables, Error, TEXT("[GameTables] %s加载失败 {}"), *VarName->GetConfigFileName()); \
		} \
	} while (false);

bool UGameTables::Init(bool bLoadImmediately)
{
	UE_LOG(LogGameTables, Display, TEXT("[GameTables] Init Beginning..."));

#if WITH_EDITOR
	bLoadImmediately = true;
#endif
	
	INIT_TABLE(UExcelItemTable, Item);
	
	// ==============================================================
	// 以下为手动配置，仅.JsonData文件需要手动配置
	
	{
		const FString FileName = GetGameDesignDataFullPath() / TEXT("GameServicesConfig.jsondata");
		bool bOk = LoadObjectFromJsonFile(FileName, &GameServicesConfig);
		if (bOk)
		{
			UE_LOG(LogGameTables, Display, TEXT("[GameTables] 加载成功 GameServicesConfig.jsondata"));
		}
		else
		{
			UE_LOG(LogGameTables, Error, TEXT("[GameTables] 加载失败 GameServicesConfig.jsondata"));
		}
		check(bOk);
	}

	{
		const FString FileName = GetGameDesignDataFullPath() / TEXT("GameClientConfig.jsondata");
		bool bOk = LoadObjectFromJsonFile(FileName, &GameClientConfig);
		if (bOk)
		{
			UE_LOG(LogGameTables, Display, TEXT("[GameTables] 加载成功 GameClientConfig.jsondata"));
		}
		else
		{
			UE_LOG(LogGameTables, Error, TEXT("[GameTables] 加载失败 GameClientConfig.jsondata"));
		}
		check(bOk);
	}

	UE_LOG(LogGameTables, Display, TEXT("[GameTables] Init End."));
	
	return true;
}
