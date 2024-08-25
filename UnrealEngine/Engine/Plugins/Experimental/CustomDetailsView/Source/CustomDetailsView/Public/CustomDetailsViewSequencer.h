// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#include "Templates/SharedPointer.h"

class IDetailKeyframeHandler;
class IPropertyHandle;
struct FPropertyRowExtensionButton;

struct CUSTOMDETAILSVIEW_API FCustomDetailsViewSequencerUtils
{
	DECLARE_DELEGATE_RetVal(TSharedPtr<IDetailKeyframeHandler>, FGetKeyframeHandlerDelegate);

	static void CreateSequencerExtensionButton(const FGetKeyframeHandlerDelegate& InKeyframeHandlerDelegate, TSharedPtr<IPropertyHandle> InPropertyHandle,
		TArray<FPropertyRowExtensionButton>& OutExtensionButtons);

	static void CreateSequencerExtensionButton(TWeakPtr<IDetailKeyframeHandler> InKeyframeHandlerWeak, TSharedPtr<IPropertyHandle> InPropertyHandle,
		TArray<FPropertyRowExtensionButton>& OutExtensionButtons);
};
