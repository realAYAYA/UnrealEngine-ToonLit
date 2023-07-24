// Copyright Epic Games, Inc. All Rights Reserved.

#include "HordeMessages.h"
#include "Serialization/CompactBinaryWriter.h"


namespace UE::RemoteExecution
{
	FCbObject FPutBlobResponse::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddHash("h", Hash);
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FPutBlobResponse::Load(const FCbObjectView& CbObjectView)
	{
		Hash = CbObjectView["h"].AsHash();
	}

	FCbObject FExistsResponse::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		if (!Id.IsEmpty())
		{
			Writer.BeginArray("id");
			for (const FIoHash& Hash : Id)
			{
				Writer.AddHash(Hash);
			}
			Writer.EndArray();
		}
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FExistsResponse::Load(const FCbObjectView& CbObjectView)
	{
		Id.Empty();
		for (FCbFieldView& CbFieldView : CbObjectView["id"].AsArrayView())
		{
			Id.Add(CbFieldView.AsHash());
		}
	}

	FCbObject FGetObjectTreeRequest::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		if (!Have.IsEmpty())
		{
			Writer.BeginArray("h");
			for (const FIoHash& Hash : Have)
			{
				Writer.AddObjectAttachment(Hash);
			}
			Writer.EndArray();
		}
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FGetObjectTreeRequest::Load(const FCbObjectView& CbObjectView)
	{
		Have.Empty();
		for (FCbFieldView& CbFieldView : CbObjectView["h"].AsArrayView())
		{
			Have.Add(CbFieldView.AsObjectAttachment());
		}
	}

	FCbObject FPutObjectResponse::Save() const
	{
		FCbWriter Writer;
		Writer.BeginObject();
		Writer.AddObjectAttachment("id", Id);
		Writer.EndObject();
		return Writer.Save().AsObject();
	}

	void FPutObjectResponse::Load(const FCbObjectView& CbObjectView)
	{
		Id = CbObjectView["id"].AsObjectAttachment();
	}
}
