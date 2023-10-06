// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMStruct.h"
#include "RigVMFunction_Name.generated.h"

USTRUCT(meta=(Abstract, Category="Core|Name", NodeColor = "0.462745, 1,0, 0.329412"))
struct RIGVM_API FRigVMFunction_NameBase : public FRigVMStruct
{
	GENERATED_BODY()

	virtual void Execute() {}
};

/**
 * Concatenates two strings together to make a new string
 */
USTRUCT(meta = (DisplayName = "Concat", TemplateName = "Concat", Keywords = "Add,+,Combine,Merge,Append"))
struct RIGVM_API FRigVMFunction_NameConcat : public FRigVMFunction_NameBase
{
	GENERATED_BODY()

	FRigVMFunction_NameConcat()
	{
		A = B = Result = NAME_None;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input, Aggregate))
	FName A;

	UPROPERTY(meta=(Input, Aggregate))
	FName B;

	UPROPERTY(meta=(Output, Aggregate))
	FName Result;
};

/**
 * Returns the left or right most characters from the string chopping the given number of characters from the start or the end
 */
USTRUCT(meta = (DisplayName = "Chop", TemplateName = "Chop", Keywords = "Truncate,-,Remove,Subtract,Split"))
struct RIGVM_API FRigVMFunction_NameTruncate : public FRigVMFunction_NameBase
{
	GENERATED_BODY()

	FRigVMFunction_NameTruncate()
	{
		Name = NAME_None;
		Count = 1;
		FromEnd = true;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta=(Input))
	FName Name;

	// Number of characters to remove from left or right
	UPROPERTY(meta=(Input))
	int32 Count;

	// if set to true the characters will be removed from the end
	UPROPERTY(meta=(Input))
	bool FromEnd;

	// the part of the string without the chopped characters
	UPROPERTY(meta=(Output))
	FName Remainder;

	// the part of the name that has been chopped off
	UPROPERTY(meta = (Output))
	FName Chopped;
};

/**
 * Replace all occurrences of a substring in this string
 */
USTRUCT(meta = (DisplayName = "Replace", TemplateName = "Replace", Keywords = "Search,Emplace,Find"))
struct RIGVM_API FRigVMFunction_NameReplace : public FRigVMFunction_NameBase
{
	GENERATED_BODY()

	FRigVMFunction_NameReplace()
	{
		Name = Old = New = NAME_None;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FName Name;

	UPROPERTY(meta = (Input))
	FName Old;

	UPROPERTY(meta = (Input))
	FName New;

	UPROPERTY(meta = (Output))
	FName Result;
};

/**
 * Tests whether this string ends with given string
 */
USTRUCT(meta = (DisplayName = "Ends With", TemplateName = "EndsWith", Keywords = "Right"))
struct RIGVM_API FRigVMFunction_EndsWith : public FRigVMFunction_NameBase
{
	GENERATED_BODY()

	FRigVMFunction_EndsWith()
	{
		Name = Ending = NAME_None;
		Result = false;
	}

	RIGVM_METHOD()
		virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FName Name;

	UPROPERTY(meta = (Input))
	FName Ending;

	UPROPERTY(meta = (Output))
	bool Result;
};

/**
 * Tests whether this string starts with given string
 */
USTRUCT(meta = (DisplayName = "Starts With", TemplateName = "StartsWith", Keywords = "Left"))
struct RIGVM_API FRigVMFunction_StartsWith : public FRigVMFunction_NameBase
{
	GENERATED_BODY()

	FRigVMFunction_StartsWith()
	{
		Name = Start = NAME_None;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FName Name;

	UPROPERTY(meta = (Input))
	FName Start;

	UPROPERTY(meta = (Output))
	bool Result;
};

/**
 * Returns true or false if a given name exists in another given name
 */
USTRUCT(meta = (DisplayName = "Contains", TemplateName = "Contains", Keywords = "Contains,Find,Has,Search"))
struct RIGVM_API FRigVMFunction_Contains : public FRigVMFunction_NameBase
{
	GENERATED_BODY()

		FRigVMFunction_Contains()
	{
		Name = Search = NAME_None;
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute() override;

	UPROPERTY(meta = (Input))
	FName Name;

	UPROPERTY(meta = (Input))
	FName Search;

	UPROPERTY(meta = (Output))
	bool Result;
};