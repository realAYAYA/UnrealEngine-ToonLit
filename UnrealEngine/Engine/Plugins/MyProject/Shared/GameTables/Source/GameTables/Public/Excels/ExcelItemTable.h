#pragma once
#include "Engine/DataTable.h"

//#include "***.h"

// =========================================================
// *** BEGIN WRITING YOUR CODE - INCLUDE ***

// *** END WRITING YOUR CODE - INCLUDE ***
// =========================================================
#include "ExcelItemTable.generated.h"


// =============================================================================
// *** BEGIN WRITING YOUR CODE - CUSTOMIZE ***

// *** END WRITING YOUR CODE - CUSTOMIZE ***
// =============================================================================


USTRUCT()
struct GAMETABLES_API FExcelItemRow : public FTableRowBase
{
    GENERATED_BODY()

    /** 道具Id */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameTables | Xlsx")
    int32 Id = int32();

    /** 道具名称 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameTables | Xlsx")
    FString Name;

    /** 分类 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameTables | Xlsx")
    int32 Type = int32();

    /** 品质 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameTables | Xlsx")
    int32 Quality = int32();

    /** 叠加上限 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameTables | Xlsx")
    int32 MaxNum = int32();

    /** 售价 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameTables | Xlsx")
    int32 SellingPrice = int32();

    /** 道具描述 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameTables | Xlsx")
    FString Description;

    /** 道具图标 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameTables | Xlsx")
    TSoftObjectPtr<UTexture2D> Icon;

};

UCLASS(BlueprintType)
class GAMETABLES_API UExcelItemConfig : public UObject
{
    GENERATED_BODY()
public: 

    bool Init();

    /** 道具Id */
    UPROPERTY(BlueprintReadOnly, Category = "GameTables | Xlsx")
    int32 Id = int32();

    /** 道具名称 */
    UPROPERTY(BlueprintReadOnly, Category = "GameTables | Xlsx")
    FString Name;

    /** 分类 */
    UPROPERTY(BlueprintReadOnly, Category = "GameTables | Xlsx")
    int32 Type = int32();

    /** 品质 */
    UPROPERTY(BlueprintReadOnly, Category = "GameTables | Xlsx")
    int32 Quality = int32();

    /** 叠加上限 */
    UPROPERTY(BlueprintReadOnly, Category = "GameTables | Xlsx")
    int32 MaxNum = int32();

    /** 售价 */
    UPROPERTY(BlueprintReadOnly, Category = "GameTables | Xlsx")
    int32 SellingPrice = int32();

    /** 道具描述 */
    UPROPERTY(BlueprintReadOnly, Category = "GameTables | Xlsx")
    FString Description;

    /** 道具图标 */
    UPROPERTY(BlueprintReadOnly, Category = "GameTables | Xlsx")
    TSoftObjectPtr<UTexture2D> Icon;

    // =========================================================
    // *** BEGIN WRITING YOUR CODE - UExcelItemConfig ***

    // *** END WRITING YOUR CODE - UExcelItemConfig ***
    // =========================================================
};

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnForeachExcelItemConfig, const UExcelItemConfig*, Entry);


UCLASS(BlueprintType)
class GAMETABLES_API UExcelItemTable : public UObject
{
    GENERATED_BODY()

public:

    typedef UExcelItemConfig EntryType;

    bool Init(bool bLoadImmediately = false);
    void Foreach(const TFunction<bool(const UExcelItemConfig*)>& Func) const;
    void MutableForeach(const TFunction<bool(UExcelItemConfig*)>& Func) const;

    /** 查找 */
    UFUNCTION(BlueprintPure, Category = "GameTables | Xlsx") 
    const UExcelItemConfig* Get(const int32& InKey) const; 

    /** 遍历 */
    UFUNCTION(BlueprintCallable, Category = "GameTables | Xlsx", DisplayName = "Foreach")
    void K2_Foreach(const FOnForeachExcelItemConfig& InCallback); 

    /** 配置文件名称 */
    UFUNCTION(BlueprintPure, Category = "GameTables | Xlsx")
    static FString GetConfigFileName();

    // =========================================================
    // *** BEGIN WRITING YOUR CODE - UExcelItemTable ***

    // *** END WRITING YOUR CODE - UExcelItemTable ***
    // =========================================================
    
private:

    bool TryLoadData(bool bForce = false) const;

    UPROPERTY()
    mutable TMap<int32, UExcelItemConfig*> Data; 
    
    mutable bool bInitDataDone = false;
};
