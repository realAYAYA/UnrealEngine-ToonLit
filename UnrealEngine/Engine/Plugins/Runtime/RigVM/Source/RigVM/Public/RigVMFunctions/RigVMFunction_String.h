// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RigVMCore/RigVMStruct.h"
#include "RigVMCore/RigVMDispatchFactory.h"
#include "RigVMFunction_String.generated.h"

USTRUCT(meta=(Abstract, Category="Core|String", NodeColor = "0.462745, 1,0, 0.329412"))
struct RIGVM_API FRigVMFunction_StringBase : public FRigVMStruct
{
	GENERATED_BODY()
};

/**
 * Concatenates two strings together to make a new string
 */
USTRUCT(meta = (DisplayName = "Concat", TemplateName = "Concat", Keywords = "Add,+,Combine,Merge,Append"))
struct RIGVM_API FRigVMFunction_StringConcat : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringConcat()
	{
		A = B = Result = FString();
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta=(Input, Aggregate))
	FString A;

	UPROPERTY(meta=(Input, Aggregate))
	FString B;

	UPROPERTY(meta=(Output, Aggregate))
	FString Result;
};

/**
 * Returns the left or right most characters from the string chopping the given number of characters from the start or the end
 */
USTRUCT(meta = (DisplayName = "Chop", TemplateName = "Chop", Keywords = "Truncate,-,Remove,Subtract,Split"))
struct RIGVM_API FRigVMFunction_StringTruncate : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringTruncate()
	{
		Name = FString();
		Count = 1;
		FromEnd = true;
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta=(Input))
	FString Name;

	// Number of characters to remove from left or right
	UPROPERTY(meta=(Input))
	int32 Count;

	// if set to true the characters will be removed from the end
	UPROPERTY(meta=(Input))
	bool FromEnd;

	// the part of the string without the chopped characters
	UPROPERTY(meta=(Output))
	FString Remainder;

	// the part of the name that has been chopped off
	UPROPERTY(meta = (Output))
	FString Chopped;
};

/**
 * Replace all occurrences of a substring in this string
 */
USTRUCT(meta = (DisplayName = "Replace", TemplateName = "Replace", Keywords = "Search,Emplace,Find"))
struct RIGVM_API FRigVMFunction_StringReplace : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringReplace()
	{
		Name = Old = New = FString();
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Name;

	UPROPERTY(meta = (Input))
	FString Old;

	UPROPERTY(meta = (Input))
	FString New;

	UPROPERTY(meta = (Output))
	FString Result;
};

/**
 * Tests whether this string ends with given string
 */
USTRUCT(meta = (DisplayName = "Ends With", TemplateName = "EndsWith", Keywords = "Right"))
struct RIGVM_API FRigVMFunction_StringEndsWith : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringEndsWith()
	{
		Name = Ending = FString();
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Name;

	UPROPERTY(meta = (Input))
	FString Ending;

	UPROPERTY(meta = (Output))
	bool Result;
};

/**
 * Tests whether this string starts with given string
 */
USTRUCT(meta = (DisplayName = "Starts With", TemplateName = "StartsWith", Keywords = "Left"))
struct RIGVM_API FRigVMFunction_StringStartsWith : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringStartsWith()
	{
		Name = Start = FString();
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Name;

	UPROPERTY(meta = (Input))
	FString Start;

	UPROPERTY(meta = (Output))
	bool Result;
};

/**
 * Returns true or false if a given name exists in another given name
 */
USTRUCT(meta = (DisplayName = "Contains", TemplateName = "Contains", Keywords = "Contains,Find,Has,Search"))
struct RIGVM_API FRigVMFunction_StringContains : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

		FRigVMFunction_StringContains()
	{
		Name = Search = FString();
		Result = false;
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Name;

	UPROPERTY(meta = (Input))
	FString Search;

	UPROPERTY(meta = (Output))
	bool Result;
};

/**
 * Returns the length of a string 
 */
USTRUCT(meta = (DisplayName = "Length"))
struct RIGVM_API FRigVMFunction_StringLength : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringLength()
	{
		Value = FString();
		Length = 0;
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Value;

	UPROPERTY(meta = (Output))
	int32 Length;
};

/**
 * Trims the whitespace from a string (start and end)
 */
USTRUCT(meta = (DisplayName = "Trim Whitespace", Keywords = "Space,WhiteSpace,Remove,Truncate"))
struct RIGVM_API FRigVMFunction_StringTrimWhitespace : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringTrimWhitespace()
	{
		Value = Result = FString();
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Value;

	UPROPERTY(meta = (Output))
	FString Result;
};

/**
 * Converts the string to upper case
 */
USTRUCT(meta = (DisplayName = "To Uppercase"))
struct RIGVM_API FRigVMFunction_StringToUppercase : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringToUppercase()
	{
		Value = Result = FString();
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Value;

	UPROPERTY(meta = (Output))
	FString Result;
};

/**
 * Converts the string to lower case
 */
USTRUCT(meta = (DisplayName = "To Lowercase"))
struct RIGVM_API FRigVMFunction_StringToLowercase : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringToLowercase()
	{
		Value = Result = FString();
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Value;

	UPROPERTY(meta = (Output))
	FString Result;
};

/**
 * Returns the reverse of the input string
 */
USTRUCT(meta = (DisplayName = "Reverse"))
struct RIGVM_API FRigVMFunction_StringReverse : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringReverse()
	{
		Value = Reverse = FString();
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Value;

	UPROPERTY(meta = (Output))
	FString Reverse;
};

/**
 * Returns the left most characters of a string
 */
USTRUCT(meta = (DisplayName = "Left", Keywords = "Start,Begin"))
struct RIGVM_API FRigVMFunction_StringLeft : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringLeft()
	{
		Value = Result = FString();
		Count = 1;
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Value;

	UPROPERTY(meta = (Input))
	int32 Count;

	UPROPERTY(meta = (Output))
	FString Result;
};

/**
 * Returns the right most characters of a string
 */
USTRUCT(meta = (DisplayName = "Right", Keywords = "End"))
struct RIGVM_API FRigVMFunction_StringRight : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringRight()
	{
		Value = Result = FString();
		Count = 1;
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Value;

	UPROPERTY(meta = (Input))
	int32 Count;

	UPROPERTY(meta = (Output))
	FString Result;
};

/**
 * Returns the middle section of a string
 */
USTRUCT(meta = (DisplayName = "Middle", Keywords = "Within,Center"))
struct RIGVM_API FRigVMFunction_StringMiddle : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringMiddle()
	{
		Value = Result = FString();
		Start = 0;
		Count = -1;
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Value;

	// the index of the first character
	UPROPERTY(meta = (Input))
	int32 Start;

	// if count is set to -1 all character from Start will be returned 
	UPROPERTY(meta = (Input))
	int32 Count;

	UPROPERTY(meta = (Output))
	FString Result;
};

/**
 * Finds a string within another string
 */
USTRUCT(meta = (DisplayName = "Find", Keywords = "IndexOf"))
struct RIGVM_API FRigVMFunction_StringFind : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringFind()
	{
		Value = Search = FString();
		Found = false;
		Index = INDEX_NONE;
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Value;

	UPROPERTY(meta = (Input))
	FString Search;

	UPROPERTY(meta = (Output))
    bool Found;

	UPROPERTY(meta = (Output))
	int32 Index;
};

/**
 * Splits a string into multiple sections given a separator
 */
USTRUCT(meta = (DisplayName = "Split"))
struct RIGVM_API FRigVMFunction_StringSplit : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringSplit()
	{
		Value = Separator = FString();
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	FString Value;

	UPROPERTY(meta = (Input))
	FString Separator;

	UPROPERTY(meta = (Output))
	TArray<FString> Result;
};

/**
 * Joins a string into multiple sections given a separator
 */
USTRUCT(meta = (DisplayName = "Join", Keywords = "Combine"))
struct RIGVM_API FRigVMFunction_StringJoin : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringJoin()
	{
		Result = Separator = FString();
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	TArray<FString> Values;

	UPROPERTY(meta = (Input))
	FString Separator;

	UPROPERTY(meta = (Output))
	FString Result;
};

/**
 * Converts an integer number to a string with padding
 */
USTRUCT(meta = (DisplayName = "Pad Integer", Keywords = "FromInt,Number,LeadingZeroes"))
struct RIGVM_API FRigVMFunction_StringPadInteger : public FRigVMFunction_StringBase
{
	GENERATED_BODY()

	FRigVMFunction_StringPadInteger()
	{
		Value = 0;
		Digits = 4;
		Result = FString();
	}

	RIGVM_METHOD()
	virtual void Execute();

	UPROPERTY(meta = (Input))
	int32 Value;

	UPROPERTY(meta = (Input))
	int32 Digits;

	UPROPERTY(meta = (Output))
	FString Result;
};

/*
 * Converts any value to string
 */
USTRUCT(meta=(DisplayName = "To String", NodeColor = "0.462745, 1,0, 0.329412"))
struct FRigDispatch_ToString : public FRigVMDispatchFactory
{
	GENERATED_BODY()

public:

	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;

protected:

	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override;
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};

/*
 * Converts a string into any value
 */
USTRUCT(meta=(DisplayName = "From String", NodeColor = "0.462745, 1,0, 0.329412"))
struct FRigDispatch_FromString : public FRigVMDispatchFactory
{
	GENERATED_BODY()

public:

	virtual const TArray<FRigVMTemplateArgumentInfo>& GetArgumentInfos() const override;
	virtual FRigVMTemplateTypeMap OnNewArgumentType(const FName& InArgumentName, TRigVMTypeIndex InTypeIndex) const override;

protected:

	virtual FRigVMFunctionPtr GetDispatchFunctionImpl(const FRigVMTemplateTypeMap& InTypes) const override;
	static void Execute(FRigVMExtendedExecuteContext& InContext, FRigVMMemoryHandleArray Handles, FRigVMPredicateBranchArray Predicates);
};