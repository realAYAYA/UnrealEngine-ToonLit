// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SSCSEditorMenuContext.generated.h"

class SSCSEditor;
class SSubobjectEditor;

class UE_DEPRECATED(5.0, "USSCSEditorMenuContext has been deprecated, use USubobjectEditorMenuContext instead.") USSCSEditorMenuContext;
UCLASS()
class KISMET_API USSCSEditorMenuContext : public UObject
{
	GENERATED_BODY()
public:
	
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	
	TWeakPtr<SSCSEditor> SCSEditor;
	
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	
	TWeakPtr<SSubobjectEditor> SubobjectEditor;

	bool bOnlyShowPasteOption;
};
