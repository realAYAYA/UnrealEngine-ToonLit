// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Software version string in the form "X.Y.Z.C", ie Major.Minor.Patch.Changeset (as returned by GetPlasticScmVersion)
*/
struct FSoftwareVersion
{
	FSoftwareVersion() {}

	explicit FSoftwareVersion(const FString& InVersionString);
	explicit FSoftwareVersion(const int& InMajor, const int& InMinor, const int& InPatch, const int& InChangeset);

	FString String = TEXT("<unknown-version>");

	int Major = 0;
	int Minor = 0;
	int Patch = 0;
	int Changeset = 0;
};

bool operator==(const FSoftwareVersion& Rhs, const FSoftwareVersion& Lhs);
bool operator<(const FSoftwareVersion& Rhs, const FSoftwareVersion& Lhs);
