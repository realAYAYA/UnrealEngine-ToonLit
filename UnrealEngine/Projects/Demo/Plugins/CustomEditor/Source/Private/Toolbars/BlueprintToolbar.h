#pragma once
#include "CustomEditorToolbar.h"

class FBlueprintToolbar : FCustomEditorToolbar
{
	
public:
	
	virtual void Initialize() override;

	TSharedRef<FExtender> GetExtender(UObject* InContextObject);
};
