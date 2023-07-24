// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Framework/Commands/UIAction.h"

#include "ToolMenuMisc.generated.h"

struct FToolMenuContext;

UENUM(BlueprintType)
enum class EToolMenuStringCommandType : uint8
{
	Command,
	Python,
	Custom
};

USTRUCT(BlueprintType, meta=(HasNativeBreak="/Script/ToolMenus.ToolMenuEntryExtensions.BreakStringCommand", HasNativeMake="/Script/ToolMenus.ToolMenuEntryExtensions.MakeStringCommand"))
struct TOOLMENUS_API FToolMenuStringCommand
{
	GENERATED_BODY()

	FToolMenuStringCommand() : Type(EToolMenuStringCommandType::Command) {}

	FToolMenuStringCommand(EToolMenuStringCommandType InType, FName InCustomType, const FString& InString) :
		Type(InType),
		CustomType(InCustomType),
		String(InString)
	{
	}

	// Which command handler to use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	EToolMenuStringCommandType Type;

	// Which command handler to use when type is custom
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FName CustomType;

	// String to pass to command handler
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FString String;

private:

	friend class UToolMenus;

	bool IsBound() const { return String.Len() > 0; }

	FExecuteAction ToExecuteAction(const FToolMenuContext& Context) const;

	FName GetTypeName() const;
};

UENUM(BlueprintType)
enum class EToolMenuInsertType : uint8
{
	Default,
	Before,
	After,
	First
};

USTRUCT(BlueprintType)
struct FToolMenuInsert
{
	GENERATED_BODY()

	FToolMenuInsert() : Position(EToolMenuInsertType::Default) {}
	FToolMenuInsert(FName InName, EToolMenuInsertType InPosition) : Name(InName), Position(InPosition) {}

	// Where to insert
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	FName Name;

	// How to insert
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tool Menus")
	EToolMenuInsertType Position;

	FORCEINLINE bool operator==(const FToolMenuInsert& Other) const
	{
		return Other.Name == Name && Other.Position == Position;
	}

	FORCEINLINE bool operator!=(const FToolMenuInsert& Other) const
	{
		return Other.Name != Name || Other.Position != Position;
	}

	bool IsDefault() const
	{
		return Position == EToolMenuInsertType::Default;
	}

	bool IsBeforeOrAfter() const
	{
		return Position == EToolMenuInsertType::Before || Position == EToolMenuInsertType::After;
	}
};

