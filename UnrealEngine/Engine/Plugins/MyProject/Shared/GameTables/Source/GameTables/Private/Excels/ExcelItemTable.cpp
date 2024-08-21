#include "Excels/ExcelItemTable.h"
#include "ConfigLoadHelper.h"


// =============================================================================
// *** BEGIN WRITING YOUR CODE - CUSTOMIZE ***

// *** END WRITING YOUR CODE - CUSTOMIZE ***
// =============================================================================


bool UExcelItemConfig::Init()
{
    // =========================================================
    // *** BEGIN WRITING YOUR CODE - UExcelItemConfig::Init ***

    // *** END WRITING YOUR CODE - UExcelItemConfig::Init ***
    // =========================================================
    return true;
} 

bool UExcelItemTable::Init(bool bLoadImmediately)
{
    if (bLoadImmediately)
    {
        return TryLoadData(true);
    }

    FString Path = GetGameDesignDataFullPath() / GetConfigFileName();
    auto Table = LoadTableFromJsonFile<FExcelItemRow>(Path, TEXT("Id"));
    if (!Table)
        return false;
    
    return true;
}
    
bool UExcelItemTable::TryLoadData(bool bForce) const
{
    if (!bForce)
    {
        if (bInitDataDone)
        {
            return true;
        }
    }
    bInitDataDone = true;

    FString Path = GetGameDesignDataFullPath() / GetConfigFileName();
    auto Table = LoadTableFromJsonFile<FExcelItemRow>(Path, TEXT("Id"));
    if (!Table)
        return false;

    Table->ForeachRow<FExcelItemRow>(
        TEXT("UExcelItemTable::Init"), 
        [this](const FName& Key, const FExcelItemRow& Row)
        {
            bool bIsNew = true;
            UExcelItemConfig* Config = nullptr;
            {
                auto Ret = Data.Find(Row.Id);
                if (Ret && *Ret)
                {
                    bIsNew = false;
                    Config = *Ret;
                    for (TFieldIterator<FProperty> It(UExcelItemConfig::StaticClass()); It; ++It)
                    {
                        FProperty* Prop = *It;
                        if (!Prop)
                            continue;
                        void* ValuePtr = Prop->ContainerPtrToValuePtr<void>(Config, 0);
                        if (!ValuePtr)
                            continue;
                        Prop->InitializeValue(ValuePtr);
                    }
                }
                else
                {
                    Config = NewObject<UExcelItemConfig>();
                }
            }
            Config->Id = Row.Id;
            Config->Name = Row.Name;
            Config->Type = Row.Type;
            Config->Quality = Row.Quality;
            Config->MaxNum = Row.MaxNum;
            Config->SellingPrice = Row.SellingPrice;
            Config->Description = Row.Description;
            Config->Icon = Row.Icon;
            if (Config->Init())
            {
                if (bIsNew)
                {
                    Data.Emplace(Config->Id, Config);
                }
            }
        });

    // =========================================================
    // *** BEGIN WRITING YOUR CODE - UExcelItemTable::Init ***

    // *** END WRITING YOUR CODE - UExcelItemTable::Init ***
    // =========================================================
    return true;
}

void UExcelItemTable::Foreach(const TFunction<bool(const UExcelItemConfig*)>& Func) const
{
    TryLoadData();
    for (const auto& Elem : Data)
    {
        if (!Func(Elem.Value)) 
            break;
    }
}

void UExcelItemTable::MutableForeach(const TFunction<bool(UExcelItemConfig*)>& Func) const
{
    TryLoadData();
    for (const auto& Elem : Data)
    {
        if (!Func(Elem.Value)) 
            break;
    }
}
 
const UExcelItemConfig* UExcelItemTable::Get(const int32& InKey) const 
{
    TryLoadData();
    auto Ret = Data.Find(InKey);
    if (!Ret)
        return nullptr;
    return *Ret;
}

void UExcelItemTable::K2_Foreach(const FOnForeachExcelItemConfig& InCallback)
{
    TryLoadData();
    Foreach([InCallback](const UExcelItemConfig* Entry) -> bool {
        InCallback.Execute(Entry);
        return true;
    });
}

FString UExcelItemTable::GetConfigFileName()
{
    return TEXT("Item.jsondata");
}
