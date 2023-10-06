// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Templates/SharedPointer.h"

class UDeviceProfile;

struct FNiagaraDeviceProfileViewModel
{
	UDeviceProfile* Profile;
	TArray<TSharedPtr<FNiagaraDeviceProfileViewModel>> Children;
};
