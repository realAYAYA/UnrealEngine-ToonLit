// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "IAvaTransitionExtension.h"

enum class EAvaTransitionComparisonResult : uint8;
struct FAvaTransitionScene;
struct FGuid;

class IAvaRCTransitionExtension : public IAvaTransitionExtension
{
public:
	static constexpr const TCHAR* ExtensionIdentifier = TEXT("IAvaRCTransitionExtension");

	virtual EAvaTransitionComparisonResult CompareControllers(const FGuid& InControllerId
		, const FAvaTransitionScene& InMyScene
		, const FAvaTransitionScene& InOtherScene) const = 0;
};
