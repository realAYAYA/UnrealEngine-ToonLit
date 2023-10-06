// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Set.h"
#include "IO/IoHash.h"
#include "IRemoteMessage.h"
#include "Serialization/CompactBinary.h"


namespace UE::RemoteExecution
{
	class FPutBlobResponse : public IRemoteMessage
	{
	public:
		FIoHash Hash;

		// Inherited via IMessage
		virtual FCbObject Save() const override;
		virtual void Load(const FCbObjectView& CbObjectView) override;
	};

	class FExistsResponse : public IRemoteMessage
	{
	public:
		TSet<FIoHash> Id;

		// Inherited via IMessage
		virtual FCbObject Save() const override;
		virtual void Load(const FCbObjectView& CbObjectView) override;
	};

	class FGetObjectTreeRequest : public IRemoteMessage
	{
	public:
		TSet<FIoHash> Have;

		// Inherited via IMessage
		virtual FCbObject Save() const override;
		virtual void Load(const FCbObjectView& CbObjectView) override;
	};

	class FPutObjectResponse : public IRemoteMessage
	{
	public:
		FIoHash Id;

		// Inherited via IMessage
		virtual FCbObject Save() const override;
		virtual void Load(const FCbObjectView& CbObjectView) override;
	};
}
