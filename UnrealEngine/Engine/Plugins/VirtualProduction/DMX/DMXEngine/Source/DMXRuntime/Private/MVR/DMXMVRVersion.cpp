// Copyright Epic Games, Inc. All Rights Reserved.

#include "MVR/DMXMVRVersion.h"


const FString FDMXMVRVersion::GetMajorVersionAsString() 
{ 
	return FString::FromInt(FDMXMVRVersion::MajorVersion); 
}

const FString FDMXMVRVersion::GetMinorVersionAsString() 
{ 
	return FString::FromInt(FDMXMVRVersion::MinorVersion); 
}
