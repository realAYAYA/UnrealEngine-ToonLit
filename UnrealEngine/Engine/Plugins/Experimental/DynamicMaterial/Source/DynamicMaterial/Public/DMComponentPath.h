// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/StringView.h"

struct FDMComponentPathSegment;

/** A path such as Name.Component.Component[2].Value */
struct DYNAMICMATERIAL_API FDMComponentPath
{
	static constexpr TCHAR Separator = '.';
	static constexpr TCHAR ParameterOpen = '[';
	static constexpr TCHAR ParameterClose = ']';

	FDMComponentPath() = delete;
	FDMComponentPath(FStringView InPath);
	FDMComponentPath(const FString& InPathString);

	bool IsLeaf() const;

	/**
	 * Extracts the first component and removes it from the path
	 */
	FDMComponentPathSegment GetFirstSegment();

protected:
	FStringView Path;
};

/** Represents a single part of a component path */
struct DYNAMICMATERIAL_API FDMComponentPathSegment
{
	friend struct FDMComponentPath;

	FDMComponentPathSegment(FStringView InToken, FStringView InParameter);

	FStringView GetToken() const { return Token; }

	bool HasParameter() const;

	bool GetParameter(int32& OutParameter) const;

	bool GetParameter(FString& OutParameter) const;

protected:
	FStringView Token;
	FStringView Parameter;
};
