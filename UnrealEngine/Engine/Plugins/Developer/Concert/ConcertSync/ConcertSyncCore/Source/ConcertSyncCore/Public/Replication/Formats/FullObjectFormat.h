// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IObjectReplicationFormat.h"

namespace UE::ConcertSyncCore
{
	/**
	 * Full object format is a blob of serialized UObject data. 
	 *
	 * Since there no additional property information in this format, the server cannot merge data. Every update the
	 * entire blob must be sent. This format is inefficient. The only upside it that it is straight-forward to understand.
	 * 
	 * This format is used for the initial Concert Replication prototype.
	 * TODO: After the prototype phase, more efficient formats should be developed.
	 *
	 * @see FFullObjectReplicationData
	 */
	class CONCERTSYNCCORE_API FFullObjectFormat : public IObjectReplicationFormat
	{
	public:

		//~ Begin FFullObjectFormat Interface
		virtual TOptional<FConcertSessionSerializedPayload> CreateReplicationEvent(UObject& Object, FAllowPropertyFunc IsPropertyAllowedFunc) override;
		virtual void ClearInternalCache(TArrayView<UObject> ObjectsToClear) override { /* There is no cache: Full object format has no smart building relying on past events - it always serializes everything */ }
		virtual void CombineReplicationEvents(FConcertSessionSerializedPayload& Base, const FConcertSessionSerializedPayload& Newer) override;
		virtual void ApplyReplicationEvent(UObject& Object, const FConcertSessionSerializedPayload& Payload) override;
		//~ End FFullObjectFormat Interface
	};
}
