// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PbGddGlobal.h"

#include "GameTables.generated.h"

class UExcelItemTable;


DECLARE_LOG_CATEGORY_EXTERN(LogGameTables, Verbose, All);

/*
 * 游戏表格配置
 */
UCLASS(BlueprintType, Blueprintable)
class GAMETABLES_API UGameTables : public UObject
{
	GENERATED_BODY()

public:
	
	virtual bool Init(bool bLoadImmediately = false);

	/** 道具表 */
	UPROPERTY(BlueprintReadOnly, Category = "Excel | ")
	UExcelItemTable* Item;
	
	/** Json常量配置: 服务器配置 */
	FPbGameServicesConfig GameServicesConfig;

	/** Json常量配置: 客户端配置 */
	FPbGameClientConfig GameClientConfig;

#if UE_SERVER

#endif
	
};
