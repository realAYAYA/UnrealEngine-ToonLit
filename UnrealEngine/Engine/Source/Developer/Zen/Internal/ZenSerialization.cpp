// Copyright Epic Games, Inc. All Rights Reserved.

#include "ZenSerialization.h"
#include "Compression/OodleDataCompression.h"
#include "Serialization/CompactBinary.h"
#include "Serialization/CompactBinaryPackage.h"
#include "Serialization/CompactBinarySerialization.h"
#include "Serialization/CompactBinaryValidation.h"
#include "Serialization/CompactBinaryWriter.h"

#if UE_WITH_ZEN

namespace UE::Zen
{

namespace OpLog
{
void SaveCbAttachment(const FCbAttachment& Attachment, FCbWriter& Writer)
{
	if (Attachment.IsCompressedBinary())
	{
		Writer.AddBinary(Attachment.AsCompressedBinary().GetCompressed());
		Writer.AddBinaryAttachment(Attachment.GetHash());
	}
	else if (Attachment.IsNull())
	{
		Writer.AddBinary(FMemoryView());
	}
	else
	{
		// NOTE: All attachments needs to be compressed
		checkNoEntry();
	}
}

void SaveCbPackage(const FCbPackage& Package, FCbWriter& Writer)
{
	if (const FCbObject& RootObject = Package.GetObject())
	{
		Writer.AddObject(RootObject);
		Writer.AddObjectAttachment(Package.GetObjectHash());
	}
	for (const FCbAttachment& Attachment : Package.GetAttachments())
	{
		SaveCbAttachment(Attachment, Writer);
	}
	Writer.AddNull();
}

void SaveCbPackage(const FCbPackage& Package, FArchive& Ar)
{
	FCbWriter Writer;
	SaveCbPackage(Package, Writer);
	Writer.Save(Ar);
}

bool TryLoadCbPackage(FCbPackage& Package, FArchive& Ar, FCbBufferAllocator Allocator)
{
	uint8 StackBuffer[64];
	const auto StackAllocator = [&Allocator, &StackBuffer](uint64 Size) -> FUniqueBuffer
	{
		return Size <= sizeof(StackBuffer) ? FUniqueBuffer::MakeView(StackBuffer, Size) : Allocator(Size);
	};

	Package = FCbPackage();
	for (;;)
	{
		FCbField ValueField = LoadCompactBinary(Ar, StackAllocator);
		if (!ValueField)
		{
			Ar.SetError();
			return false;
		}
		if (ValueField.IsNull())
		{
			return true;
		}
		else if (ValueField.IsBinary())
		{
			const FMemoryView View = ValueField.AsBinaryView();
			if (View.GetSize() > 0)
			{
				FSharedBuffer Buffer = FSharedBuffer::MakeView(View, ValueField.GetOuterBuffer()).MakeOwned();
				FCbField HashField = LoadCompactBinary(Ar, StackAllocator);
				const FIoHash& Hash = HashField.AsAttachment();
				if (HashField.HasError() || FIoHash::HashBuffer(Buffer) != Hash)
				{
					Ar.SetError();
					return false;
				}
				if (HashField.IsObjectAttachment())
				{
					Package.AddAttachment(FCbAttachment(FCbObject(MoveTemp(Buffer)), Hash));
				}
				else
				{
					Package.AddAttachment(FCbAttachment(FCompositeBuffer(MoveTemp(Buffer)), Hash));
				}
			}
		}
		else
		{
			FCbObject Object = ValueField.AsObject();
			if (ValueField.HasError())
			{
				Ar.SetError();
				return false;
			}

			if (Object)
			{
				FCbField HashField = LoadCompactBinary(Ar, StackAllocator);
				FIoHash ObjectHash = HashField.AsObjectAttachment();
				if (HashField.HasError() || Object.GetHash() != ObjectHash)
				{
					Ar.SetError();
					return false;
				}
				Package.SetObject(Object, ObjectHash);
			}
		}
	}
}
}

namespace Http
{
struct CbPackageHeader
{
	uint32	HeaderMagic;
	uint32	AttachmentCount;
	uint32	Reserved1;
	uint32	Reserved2;
};

struct CbAttachmentEntry
{
	uint64	AttachmentSize;
	uint32	Flags;
	FIoHash	AttachmentHash;

	enum
	{
		IsCompressed = (1u << 0),	// Is marshaled using compressed buffer storage format
		IsObject = (1u << 1),		// Is compact binary object
	};

};

void SaveCbPackage(const FCbPackage& Package, FArchive& Ar)
{
	TConstArrayView<FCbAttachment> Attachments = Package.GetAttachments();
	const FCbObject& Object = Package.GetObject();
	FCompressedBuffer ObjectBuffer = FCompressedBuffer::Compress(Object.GetBuffer(), FOodleDataCompression::ECompressor::NotSet, FOodleDataCompression::ECompressionLevel::None);

	CbPackageHeader Hdr;
	Hdr.HeaderMagic = kCbPkgMagic;
	Hdr.AttachmentCount = Attachments.Num();
	Hdr.Reserved1 = 0;
	Hdr.Reserved2 = 0;

	Ar.Serialize(&Hdr, sizeof Hdr);

	// Root object metadata

	{
		CbAttachmentEntry Entry;
		Entry.AttachmentHash = ObjectBuffer.GetRawHash();
		Entry.AttachmentSize = ObjectBuffer.GetCompressedSize();
		Entry.Flags = CbAttachmentEntry::IsObject | CbAttachmentEntry::IsCompressed;

		Ar.Serialize(&Entry, sizeof Entry);
	}

	// Attachment metadata

	for (const FCbAttachment& Attachment : Attachments)
	{
		CbAttachmentEntry Entry;
		Entry.AttachmentHash = Attachment.GetHash();
		Entry.Flags = 0;

		if (Attachment.IsCompressedBinary())
		{
			Entry.AttachmentSize = Attachment.AsCompressedBinary().GetCompressedSize();
			Entry.Flags |= CbAttachmentEntry::IsCompressed;
		}
		else if (Attachment.IsBinary())
		{
			Entry.AttachmentSize = Attachment.AsCompositeBinary().GetSize();
		}
		else if (Attachment.IsNull())
		{
			checkNoEntry();
		}
		else if (Attachment.IsObject())
		{
			Entry.AttachmentSize = Attachment.AsObject().GetSize();
			Entry.Flags |= CbAttachmentEntry::IsObject;
		}
		else
		{
			checkNoEntry();
		}

		Ar.Serialize(&Entry, sizeof Entry);
	}

	// Root object

	Ar << ObjectBuffer;

	// Payloads back-to-back

	for (const FCbAttachment& Attachment : Attachments)
	{
		if (Attachment.IsCompressedBinary())
		{
			FCompressedBuffer Payload = Attachment.AsCompressedBinary();
			Ar << Payload;
		}
		else if (Attachment.IsBinary())
		{
			const FCompositeBuffer& Buffer = Attachment.AsCompositeBinary();
			FSharedBuffer SharedBuffer = Buffer.ToShared();

			Ar.Serialize((void*)SharedBuffer.GetData(), SharedBuffer.GetSize());
		}
		else if (Attachment.IsNull())
		{
			checkNoEntry();
		}
		else if (Attachment.IsObject())
		{
			checkNoEntry();
		}
		else
		{
			checkNoEntry();
		}
	}
}

bool TryLoadCbPackage(FCbPackage& Package, FArchive& Ar, FCbBufferAllocator Allocator)
{
	CbPackageHeader Hdr;
	Ar.Serialize(&Hdr, sizeof Hdr);

	if (Hdr.HeaderMagic != kCbPkgMagic)
	{
		return false;
	}

	TArray<CbAttachmentEntry> AttachmentEntries;
	AttachmentEntries.SetNum(Hdr.AttachmentCount + 1);

	Ar.Serialize(AttachmentEntries.GetData(), (Hdr.AttachmentCount + 1) * sizeof(CbAttachmentEntry));

	int Index = 0;

	for (const CbAttachmentEntry& Entry : AttachmentEntries)
	{
		FUniqueBuffer AttachmentData = FUniqueBuffer::Alloc(Entry.AttachmentSize);
		Ar.Serialize(AttachmentData.GetData(), AttachmentData.GetSize());

		if (Entry.Flags & CbAttachmentEntry::IsCompressed)
		{
			FCompressedBuffer CompBuf(FCompressedBuffer::FromCompressed(AttachmentData.MoveToShared()));

			if (Entry.Flags & CbAttachmentEntry::IsObject)
			{
				checkf(Index == 0, TEXT("Object attachments are not currently supported"));

				Package.SetObject(FCbObject(CompBuf.Decompress()));
			}
			else
			{
				FCbAttachment Attachment(MoveTemp(CompBuf));
				Package.AddAttachment(Attachment);
			}
		}
		else /* not compressed */
		{
			if (Entry.Flags & CbAttachmentEntry::IsObject)
			{
				checkf(Index == 0, TEXT("Object attachments are not currently supported"));

				Package.SetObject(FCbObject(AttachmentData.MoveToShared()));
			}
			else
			{
				FCbAttachment Attachment(AttachmentData.MoveToShared());
				Package.AddAttachment(Attachment);
			}
		}

		++Index;
	}
	return true;
}
}

}
#endif // UE_WITH_ZEN
