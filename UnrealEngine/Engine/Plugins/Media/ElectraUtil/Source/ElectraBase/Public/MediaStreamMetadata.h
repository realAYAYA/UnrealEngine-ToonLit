// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Misc/ScopeLock.h"
#include "Misc/Variant.h"

namespace Electra
{

class ELECTRABASE_API IMediaStreamMetadata
{
public:
	IMediaStreamMetadata() = default;
	virtual ~IMediaStreamMetadata() = default;
	IMediaStreamMetadata(const IMediaStreamMetadata& rhs) = delete;
	IMediaStreamMetadata& operator=(const IMediaStreamMetadata&rhs) = delete;

	class IItem
	{
	public:
		virtual ~IItem() = default;
		virtual const FString& GetLanguageCode() const = 0;
		virtual const FString& GetMimeType() const = 0;
		virtual const FVariant& GetValue() const = 0;
	};
};

} // namespace Electra
