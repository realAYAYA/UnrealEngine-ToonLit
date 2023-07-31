// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "SCSEditorExtensionContext.generated.h"

class SSCSEditor;
class SSubobjectBlueprintEditor;
class SSubobjectEditor;
class SSubobjectInstanceEditor;
class UE_DEPRECATED(5.0, "USCSEditorExtensionContext has been deprecated, use USubobjectEditorExtensionContext instead.") USCSEditorExtensionContext;

UCLASS()
class KISMET_API USCSEditorExtensionContext : public UObject
{
	GENERATED_BODY()

public:
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	UE_DEPRECATED(5.0, "GetSCSEditor has been deprecated. Use GetSubobjectEditor instead.")
	const TWeakPtr<SSCSEditor>& GetSCSEditor() const { return SCSEditor; }
	
	const TWeakPtr<SSubobjectEditor>& GetSubobjectEditor() const { return SubobjectEditor; }

private:
	
	friend SSCSEditor;
	friend SSubobjectEditor;
	friend SSubobjectInstanceEditor;
	friend SSubobjectBlueprintEditor;
	
	TWeakPtr<SSCSEditor> SCSEditor;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	TWeakPtr<SSubobjectEditor> SubobjectEditor;

};
