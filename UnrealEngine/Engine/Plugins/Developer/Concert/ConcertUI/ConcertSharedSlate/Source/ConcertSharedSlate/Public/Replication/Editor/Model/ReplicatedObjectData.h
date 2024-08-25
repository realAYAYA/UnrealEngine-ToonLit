// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/SoftObjectPath.h"

namespace UE::ConcertSharedSlate
{
	/** Instanced for each object row in SObjectToPropertyView.*/
	class FReplicatedObjectData
	{
	public:

		FReplicatedObjectData(FSoftObjectPath ObjectPath)
			: ObjectPath(MoveTemp(ObjectPath))
		{}
		
		const FSoftObjectPath& GetObjectPath() const { return ObjectPath; }

	private:

		/** The replicated object */
		FSoftObjectPath ObjectPath;
	};
}