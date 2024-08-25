// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Rundown/Factories/Filters/IAvaRundownFilterSuggestionFactory.h"

class FAvaRundownFilterPathSuggestionFactory : public IAvaRundownFilterSuggestionFactory
{
public:
	//~ Begin IAvaFilterSuggestionFactory interface
	virtual FName GetSuggestionIdentifier() const override;
	virtual bool IsSimpleSuggestion() const override { return false; }
	virtual void AddSuggestion(const TSharedRef<FAvaRundownFilterSuggestionPayload>& InPayload) override;
	virtual bool SupportSuggestionType(EAvaRundownSearchListType InSuggestionType) const override;
	//~ End IAvaFilterSuggestionFactory interface
};
