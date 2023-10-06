// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"

#include "DMXControlConsoleEditorGlobalLayoutUser.generated.h"


/** A layout where Control Console data sorting order is customizable */
UCLASS()
class UDMXControlConsoleEditorGlobalLayoutUser
	: public UDMXControlConsoleEditorGlobalLayoutBase
{
	GENERATED_BODY()

public:
	/** Gets Layout's name identifier */
	FString GetLayoutName() const { return LayoutName; }

	/** Sets Layout's name identifier */
	void SetLayoutName(const FString& NewName) { LayoutName = NewName; }

	// Property Name getters
	FORCEINLINE static FName GetLayoutNamePropertyName() { return GET_MEMBER_NAME_CHECKED(UDMXControlConsoleEditorGlobalLayoutUser, LayoutName); }

private:
	/** Name identifier of this Layout */
	UPROPERTY()
	FString LayoutName;
};
