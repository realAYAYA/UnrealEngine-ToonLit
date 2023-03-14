// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "IObjectNameEditSink.h"
#include "Templates/SharedPointer.h"

class UClass;

namespace UE::EditorWidgets
{
class IObjectNameEditSink;

class EDITORWIDGETS_API FObjectNameEditSinkRegistry
{
public:
	FObjectNameEditSinkRegistry();

	/** Registers an object name edit sink so it can provide a name editing interface for an object type. */
	void RegisterObjectNameEditSink(const TSharedRef<IObjectNameEditSink>& NewSink);

	/** Unregisters a name edit sink. It will no longer provide a name editing interface for an object type. */
	void UnregisterObjectNameEditSink(const TSharedRef<IObjectNameEditSink>& SinkToRemove);

	/** Gets the appropriate ObjectNameEditSink for the supplied class */
	TSharedPtr<IObjectNameEditSink> GetObjectNameEditSinkForClass(const UClass* Class) const;

private:
	/** The list of all registered ObjectNameEditSinks */
	TArray<TSharedRef<IObjectNameEditSink>> ObjectNameEditSinkList;
};

} // end namespace UE::EditorWidgets
