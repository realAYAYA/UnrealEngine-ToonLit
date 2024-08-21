#include "ConfigLoadHelper.h"
#include "GameTables.h"

const FString& GetGameDesignDataFullPath()
{
	static FString GddFullPath;
	if (GddFullPath.Len() == 0)
	{
		const FString GddRelativePath = TEXT("GDD/");
		GddFullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir() + GddRelativePath);
	}
	return GddFullPath;
}

// =========================================================

bool ParseConfigContent(
	const FString& InCfgName,
	const TSharedPtr<FJsonObject>& InObject,
	const UStruct* InStructDefine,
	void* InData);

bool ParseConfigContent(
	const FString& InCfgName,
	const TSharedPtr<FJsonValue>& InValue,
	FProperty* InProperty,
	void* InData)
{
	const FString PropertyName = InProperty->GetName();
	if (const FNumericProperty* NumProp = CastField<FNumericProperty>(InProperty))
	{
		if (NumProp->IsInteger())
		{
			int32 Val = 0;
			if (!InValue || !InValue->TryGetNumber(Val))
			{
				UE_LOG(LogGameTables, Warning, TEXT("%s 读取字段 %s 失败"), *InCfgName, *PropertyName);
				return false;
			}

			int32* Ptr = NumProp->ContainerPtrToValuePtr<int32>(InData);
			*Ptr = Val;
		}
		else
		{
			float Val = 0.0f;
			if (!InValue || !InValue->TryGetNumber(Val))
			{
				UE_LOG(LogGameTables, Warning, TEXT("%s 读取字段 %s 失败"), *InCfgName, *PropertyName);
				return false;
			}

			float* Ptr = NumProp->ContainerPtrToValuePtr<float>(InData);
			*Ptr = Val;
		}
	}
	else if (const FStrProperty* StrProp = CastField<FStrProperty>(InProperty))
	{
		FString Val;
		if (!InValue || !InValue->TryGetString(Val))
		{
			UE_LOG(LogGameTables, Warning, TEXT("%s 读取字段 %s 失败"), *InCfgName, *PropertyName);
			return false;
		}

		FString* Ptr = StrProp->ContainerPtrToValuePtr<FString>(InData);
		*Ptr = Val;
	}
	else if (const FStructProperty* StructProp = CastField<FStructProperty>(InProperty))
	{
		const TSharedPtr<FJsonObject>* Val = nullptr;
		if (!InValue || !InValue->TryGetObject(Val))
		{
			UE_LOG(LogGameTables, Warning, TEXT("%s 读取字段 %s 失败"), *InCfgName, *PropertyName);
			return false;
		}
		if (Val)
		{
			uint8* Ptr = StructProp->ContainerPtrToValuePtr<uint8>(InData);
			const bool bOk = ParseConfigContent(InCfgName, *Val, StructProp->Struct, Ptr);
			if (!bOk)
				return false;
		}
	}
	else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(InProperty))
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!InValue || !InValue->TryGetArray(Values))
		{
			UE_LOG(LogGameTables, Warning, TEXT("%s 读取字段 %s 失败"), *InCfgName, *PropertyName);
			return false;
		}
		if (Values)
		{
			const uint8* Ptr = ArrayProp->ContainerPtrToValuePtr<uint8>(InData);

			FScriptArrayHelper ArrayHelper(ArrayProp, Ptr);
			ArrayHelper.EmptyValues();
			for (auto& Elem : *Values)
			{
				const int32 Idx = ArrayHelper.AddValue();
				uint8* ArrayEntryData = ArrayHelper.GetRawPtr(Idx);
				const bool bOk = ParseConfigContent(InCfgName, Elem, ArrayProp->Inner, ArrayEntryData);
				if (!bOk)
					return false;
			}
		}
	}
	else
	{
		UE_LOG(LogGameTables, Warning, TEXT("%s 读取字段 %s 失败,不支持该类型"), *InCfgName, *PropertyName);
		return false;
	}

	return true;
}

bool ParseConfigContent(
	const FString& InCfgName,
	const TSharedPtr<FJsonObject>& InObject,
	const UStruct* InStructDefine,
	void* InData)
{
	for (TFieldIterator<FProperty> It(InStructDefine); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop)
			continue;

		FString PropName = Prop->GetName();
		if (PropName.StartsWith(TEXT("ExtendField_")))
			continue; // 跳过使用 ExtendField_ 开头的成员
		
		TSharedPtr<FJsonValue> Value = InObject->TryGetField(PropName);
		const bool bOk = ParseConfigContent(FString::Printf(TEXT("%s/%s"), *InCfgName, *PropName), Value, Prop, InData);
		if (!bOk)
			return false;
	}
	return true;
}


bool LoadObjectFromJsonFileImpl(const FString& InCfgFileName, const UStruct* InStruct, void* OutCfg)
{
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*InCfgFileName))
	{
		UE_LOG(LogGameTables, Error, TEXT("LoadTable 失败,找不到指定文件 %s"), *InCfgFileName);
		return false;
	}

	FString JsonData;
	if (!FFileHelper::LoadFileToString(JsonData, *InCfgFileName))
	{
		UE_LOG(LogGameTables, Error, TEXT("LoadTable 失败,加载文件错误 %s"), *InCfgFileName);
		return false;
	}

	return LoadObjectFromJsonStringImpl(InCfgFileName, JsonData, InStruct, OutCfg);
}

bool LoadObjectFromJsonStringImpl(const FString& InCfgName, const FString& InData, const UStruct* InStruct, void* OutCfg)
{
	TSharedPtr<FJsonObject> OutObject;
	const auto JsonReader = TJsonReaderFactory<TCHAR>::Create(InData);
	if (!FJsonSerializer::Deserialize(JsonReader, OutObject))
	{
		UE_LOG(LogGameTables, Error, TEXT("LoadTable 失败,Json解析错误 %s"), *InCfgName);
		return false;
	}
	if (!OutObject)
	{
		return false;
	}

	const bool bOk = ParseConfigContent(InCfgName, OutObject, InStruct, OutCfg);

	return bOk;		
}

// =========================================================
template <typename ValueType>
void WriteJSONValueWithOptionalIdentifier(
	const TSharedPtr<TJsonWriter<TCHAR>>& InJsonWriter,
	const FString* InIdentifier,
	const ValueType InValue)
{
	if (InIdentifier)
	{
		InJsonWriter->WriteValue(*InIdentifier, InValue);
	}
	else
	{
		InJsonWriter->WriteValue(InValue);
	}
}

bool WriteConfigContent(
	const FString& InCfgName,
	const TSharedPtr<TJsonWriter<TCHAR>>& JsonWriter,
	const UStruct* InStructDefine,
	const void* InData,
	const FString* InIdentifier);

bool WriteConfigContent(
	const FString& InCfgName,
	const TSharedPtr<TJsonWriter<TCHAR>>& JsonWriter,
	const FProperty* InProperty,
	const void* InData,
	const FString* InIdentifier)
{
	const FString PropertyName = InProperty->GetName();
	if (const FNumericProperty* NumProp = CastField<FNumericProperty>(InProperty))
	{
		if (NumProp->IsInteger())
		{
			const int64 Value = NumProp->GetSignedIntPropertyValue(InData);
			WriteJSONValueWithOptionalIdentifier(JsonWriter, InIdentifier, Value); 
		}
		else
		{
			const double Value = NumProp->GetFloatingPointPropertyValue(InData);
			WriteJSONValueWithOptionalIdentifier(JsonWriter, InIdentifier, Value);	
		}
	}
	else if (const FStrProperty* StrProp = CastField<FStrProperty>(InProperty))
	{
		FString Value;
		StrProp->ExportText_Direct(Value, InData, InData, nullptr, PPF_ExternalEditor);
		WriteJSONValueWithOptionalIdentifier(JsonWriter, InIdentifier, Value);
	}
	else if (const FStructProperty* StructProp = CastField<FStructProperty>(InProperty))
	{
		if (InIdentifier)
		{
			JsonWriter->WriteObjectStart(*InIdentifier);
		}
		else
		{
			JsonWriter->WriteObjectStart();
		}
		WriteConfigContent(InCfgName, JsonWriter, StructProp->Struct, InData, nullptr);
		JsonWriter->WriteObjectEnd();
	}
	else if (const FArrayProperty* ArrayProp = CastField<FArrayProperty>(InProperty))
	{
		JsonWriter->WriteArrayStart(PropertyName);
		FScriptArrayHelper ArrayHelper(ArrayProp, InData);
		for (int32 ArrayEntryIndex = 0; ArrayEntryIndex < ArrayHelper.Num(); ++ArrayEntryIndex)
		{
			const uint8* ArrayEntryData = ArrayHelper.GetRawPtr(ArrayEntryIndex);
			WriteConfigContent(InCfgName,JsonWriter, ArrayProp->Inner, ArrayEntryData, nullptr);
		}		
		JsonWriter->WriteArrayEnd();
	}
	else
	{
		UE_LOG(LogGameTables, Warning, TEXT("%s 读取字段 %s 失败,不支持该类型"), *InCfgName, *PropertyName);
		return false;
	}

	return true;
}

bool WriteConfigContent(
	const FString& InCfgName,
	const TSharedPtr<TJsonWriter<TCHAR>>& JsonWriter,
	const UStruct* InStructDefine,
	const void* InData,
	const FString* InIdentifier)
{
	for (TFieldIterator<FProperty> It(InStructDefine); It; ++It)
	{
		const FProperty* Prop = *It;
		if (!Prop)
			continue;

		const FString PropName = Prop->GetName();
		if (Prop->ArrayDim == 1)
		{
			const void* Data = Prop->ContainerPtrToValuePtr<void>(InData, 0);
			WriteConfigContent(InCfgName, JsonWriter, Prop, Data, &PropName);
		}
		else
		{
			JsonWriter->WriteArrayStart(PropName);
			for (int32 Idx = 0; Idx < Prop->ArrayDim; ++Idx)
			{
				const void* Data = Prop->ContainerPtrToValuePtr<void>(InData, Idx);
				WriteConfigContent(InCfgName, JsonWriter, Prop, Data, nullptr);
			}
			JsonWriter->WriteArrayEnd();
		}
	}
	return true;
}

bool SaveObjectToJsonFileImpl(const FString& InCfgFileName, const UStruct* InStruct, const void* InData)
{
	FString JsonOutputStr;
	{
		const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter
		= TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonOutputStr);

		JsonWriter->WriteObjectStart();
		WriteConfigContent(InCfgFileName, JsonWriter, InStruct, InData, nullptr);
		JsonWriter->WriteObjectEnd();

		JsonWriter->Close();
	}

	// 写入文件
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		const TSharedPtr<IFileHandle> FileHandle(PlatformFile.OpenWrite(*InCfgFileName));
		if (FileHandle)
		{
			FileHandle->Write((const uint8*)TCHAR_TO_UTF8(*JsonOutputStr), JsonOutputStr.Len());
		}
	}
	
	return true;
}

// =========================================================

UDataTable* LoadTableFromJsonFileImpl(const FString& InCfgFileName, const FString& InKeyFieldName, UScriptStruct* InStruct)
{
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*InCfgFileName))
	{
		UE_LOG(LogGameTables, Error, TEXT("LoadTable 失败,找不到指定文件 %s"), *InCfgFileName);
		return nullptr;
	}
    
	FString JsonData;
	if (!FFileHelper::LoadFileToString(JsonData, *InCfgFileName))
	{
		UE_LOG(LogGameTables, Error, TEXT("LoadTable 失败,加载文件错误 %s"), *InCfgFileName);
		return nullptr;
	}
    
	UDataTable* DataTable = NewObject<UDataTable>();
	DataTable->ImportKeyField = InKeyFieldName;
	DataTable->RowStruct = InStruct;
	auto Errors = DataTable->CreateTableFromJSONString(JsonData);

	for (const auto &Elem : Errors)
	{
		UE_LOG(LogGameTables, Error, TEXT("LoadTable %s 处理过程中出现错误 %s"), *InCfgFileName, *Elem);		
	}

	FString JsonOutputStr;
	TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> JsonWriter = TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&JsonOutputStr);
	
	return DataTable;	
}
