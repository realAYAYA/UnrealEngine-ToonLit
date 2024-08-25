// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Internationalization/Text.h"
#include "Templates/SharedPointer.h"

class FString;
class UObject;
class UStruct;

struct FConcertPropertyChain;
struct FSlateIcon;
struct FSoftObjectPath;

namespace UE::ConcertSharedSlate
{
	class IObjectNameModel;
	class IReplicationStreamModel;
}

namespace UE::ConcertSharedSlate::DisplayUtils
{
	/** Unified version that reads from IObjectNameModel or defaults to extracting the name from the path. */
	FText GetObjectDisplayText(const FSoftObjectPath& Object, IObjectNameModel* Model = nullptr);
	inline FText GetObjectDisplayText(const FSoftObjectPath& Object, const TSharedPtr<IObjectNameModel>& Model = nullptr) { return GetObjectDisplayText(Object, Model.Get()); }
	
	/** @return The text to use for displaying this object's name */
	FText ExtractObjectDisplayTextFromPath(const FSoftObjectPath& Object);
	
	/** @return More lightweight version of GetObjectDisplayText which does not construct any FText. */
	FString GetObjectDisplayString(const UObject& Object);
	
	/** @return The text to use for displaying this object's type */
	FText GetObjectTypeText(const IReplicationStreamModel& Model, const FSoftObjectPath& Object);
	
	/** @return The icon to use for this object */
	FSlateIcon GetObjectIcon(const IReplicationStreamModel& Model, const FSoftObjectPath& Object);
	/** @return The icon to use for this object */
	FSlateIcon GetObjectIcon(UObject& Object);

	/** @return The text to use for displaying this property's name. Uses Class to determine class name if available. */
	FText GetPropertyDisplayText(const FConcertPropertyChain& Property, UStruct* Class = nullptr);
	/** @return More lightweight version of GetPropertyDisplayString which does not construct any FText. Uses Class to determine class name if available. */
	FString GetPropertyDisplayString(const FConcertPropertyChain& Property, UStruct* Class = nullptr);
}
