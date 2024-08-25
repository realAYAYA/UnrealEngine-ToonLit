// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SVGRawItem.h"
#include "Templates/SharedPointer.h"

struct FSVGPathCommand;
struct FSVGRawElement;

/**
 * Specialization of SVG Items, representing SVG Attributes
 */
struct FSVGRawAttribute : public FSVGRawItem
{
public:
	/** Create a Raw Attribute shared ref */
	static TSharedRef<FSVGRawAttribute> NewSVGAttribute(const TSharedPtr<FSVGRawElement>& InParentElement, const FName& InName, const FString& InValue);

	/** Converts this Attribute Value to a list of FVector2D points */
	void AsPoints(TArray<FVector2D>& OutPoints) const;

	/** Prints the name and value of this Attribute */
	void PrintDebugInfo() const;

	/** Convert this Attribute Value to a float */
	float AsFloat() const;

	/** Convert this Attribute Value to a float. Useful for numbers with a suffix (e.g. percentage for some gradients) */
	bool AsFloatWithSuffix(const FString& InSuffix, float& OutValue) const;

	/** Returns the plain String Attribute Value*/
	FString AsString() const;

	/** Parse the SVG String as a path. Returns the list of commands describing the path */
	TArray<TArray<FSVGPathCommand>> AsPathCommands() const;

private:
	struct FPrivateToken { explicit FPrivateToken() = default; };
public:
	FSVGRawAttribute(FPrivateToken, const TSharedRef<FSVGRawItem>& InParent, const FName& InName, const FString& InValue)
	{
		Parent = InParent;
		Name = InName;
		Value = InValue;
	}

	FSVGRawAttribute() = default;
};
