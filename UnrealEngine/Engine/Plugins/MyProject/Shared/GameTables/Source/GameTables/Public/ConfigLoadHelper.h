#pragma once

#include "Engine/DataTable.h"

// 获取 GDD 目录完整路径
GAMETABLES_API const FString& GetGameDesignDataFullPath();

// =========================================================
// 根据对象的反射信息，从 JSON 文件中读取数据

GAMETABLES_API bool LoadObjectFromJsonFileImpl(const FString& InCfgFileName, const UStruct* InStruct, void* OutCfg);
GAMETABLES_API bool LoadObjectFromJsonStringImpl(const FString& InCfgName, const FString& InData, const UStruct* InStruct, void* OutCfg);

template <typename T>
bool LoadObjectFromJsonFile(const FString& CfgFileName, T* OutCfg)
{
	return LoadObjectFromJsonFileImpl(CfgFileName, T::StaticStruct(), OutCfg);
}

template <typename T>
bool LoadObjectFromJsonString(const FString& InCfgName, const FString& InData, T* OutCfg)
{
	return LoadObjectFromJsonStringImpl(InCfgName, InData, T::StaticStruct(), OutCfg);
}


// =========================================================
// 根据对象的反射信息，将其序列化为 JSON 文件

GAMETABLES_API bool SaveObjectToJsonFileImpl(const FString& InCfgFileName, const UStruct* InStruct, const void* InData);

template <typename T>
bool SaveObjectToJsonFile(const FString& CfgFileName, T* InData)
{
	return SaveObjectToJsonFileImpl(CfgFileName, T::StaticStruct(), InData);
}

// =========================================================


GAMETABLES_API UDataTable* LoadTableFromJsonFileImpl(const FString& InCfgFileName, const FString& InKeyFieldName, UScriptStruct* InStruct);

template <typename T>
UDataTable* LoadTableFromJsonFile(const FString& InCfgFileName, const FString& InKeyFieldName)
{
	return LoadTableFromJsonFileImpl(InCfgFileName, InKeyFieldName, T::StaticStruct());
}