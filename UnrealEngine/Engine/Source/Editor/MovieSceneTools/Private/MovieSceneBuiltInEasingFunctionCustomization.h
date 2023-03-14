// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"

class IDetailLayoutBuilder;

enum class EMovieSceneBuiltInEasing : uint8;
class FReply;
class IPropertyHandle;

class FMovieSceneBuiltInEasingFunctionCustomization : public IDetailCustomization
{
public:

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	FReply SetType(EMovieSceneBuiltInEasing NewType);

private:

	TSharedPtr<IPropertyHandle> TypeProperty;
};