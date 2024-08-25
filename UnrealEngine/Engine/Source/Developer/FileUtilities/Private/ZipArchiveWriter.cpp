// Copyright Epic Games, Inc. All Rights Reserved.

#include "FileUtilities/ZipArchiveWriter.h"

#if WITH_ENGINE

#include "GenericPlatform/GenericPlatformFile.h"
#include "ZipArchivePrivate.h"

DEFINE_LOG_CATEGORY(LogZipArchive);

FZipArchiveWriter::FZipArchiveWriter(IFileHandle* InFile)
	: File(InFile)
{
}

FZipArchiveWriter::~FZipArchiveWriter()
{
	// Zip File Format Specification:
	// https://www.loc.gov/preservation/digital/formats/digformatspecs/APPNOTE%2820120901%29_Version_6.3.3.txt

	UE_LOG(LogZipArchive, Display, TEXT("Closing zip file with %d entries."), Files.Num());

	// Write the file directory
	uint64 DirStartOffset = Tell();
	for (FFileEntry& Entry : Files)
	{
		const static uint8 Footer[] =
		{
			0x50, 0x4b, 0x01, 0x02, 0x3f, 0x00, 0x2d, 0x00,
			0x00, 0x00, 0x00, 0x00
		};
		Write((void*)Footer, sizeof(Footer));
		Write(Entry.Time);
		Write(Entry.Crc32);

		Write((uint64)0xffffffffffffffff);
		Write((uint16)Entry.Filename.Len());
		const static uint8 Fields[] =
		{
			0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x20, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff
		};
		Write((void*)Fields, sizeof(Fields));
		Write((void*)TCHAR_TO_UTF8(*Entry.Filename), Entry.Filename.Len());

		Write((uint16)0x01);
		Write((uint16)0x1c);

		Write((uint64)Entry.Length);
		Write((uint64)Entry.Length);
		Write((uint64)Entry.Offset);
		Write((uint32)0);

		Flush();
	}
	uint64 DirEndOffset = Tell();

	uint64 DirectorySizeInBytes = DirEndOffset - DirStartOffset;

	// Write ZIP64 end of central directory record
	const static uint8 Record[] =
	{
		0x50, 0x4b, 0x06, 0x06, 0x2c, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x2d, 0x00, 0x2d, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	};
	Write((void*)Record, sizeof(Record));
	Write((uint64)Files.Num());
	Write((uint64)Files.Num());
	Write(DirectorySizeInBytes);
	Write(DirStartOffset);

	// Write ZIP64 end of central directory locator
	const static uint8 Locator[] =
	{
		0x50, 0x4b, 0x06, 0x07, 0x00, 0x00, 0x00, 0x00,
	};
	Write((void*)Locator, sizeof(Locator));
	Write(DirEndOffset);
	Write((uint32)0x01);

	// Write normal end of central directory record
	const static uint8 EndRecord[] =
	{
		0x50, 0x4b, 0x05, 0x06, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0x00, 0x00
	};
	Write((void*)EndRecord, sizeof(EndRecord));

	Flush();

	if (File)
	{
		// Close the file
		delete File;
		File = nullptr;
	}
}

void FZipArchiveWriter::AddFile(const FString& Filename, TConstArrayView<uint8> Data, const FDateTime& Timestamp)
{
	if (!ensureMsgf(!Filename.IsEmpty(), TEXT("Failed to write data to zip file; filename is empty.")))
	{
		return;
	}
	uint32 Crc = FCrc::MemCrc32(Data.GetData(), Data.Num());

	// Convert the date-time to a zip file timestamp (2-second resolution).
	uint32 ZipTime =
		(Timestamp.GetSecond() / 2) |
		(Timestamp.GetMinute() << 5) |
		(Timestamp.GetHour() << 11) |
		(Timestamp.GetDay() << 16) |
		(Timestamp.GetMonth() << 21) |
		((Timestamp.GetYear() - 1980) << 25);

	uint64 FileOffset = Tell();

	FFileEntry* Entry = new (Files) FFileEntry(Filename, Crc, Data.Num(), FileOffset, ZipTime);

	static const uint8 Header[] =
	{
		0x50, 0x4b, 0x03, 0x04, 0x2d, 0x00, 0x00, 0x00,
		0x00, 0x00
	};
	Write((void*)Header, sizeof(Header));
	Write(ZipTime);
	Write(Crc);
	Write((uint64)0xffffffffffffffff);
	Write((uint16)Filename.Len());
	Write((uint16)0x20);

	Write((void*)TCHAR_TO_UTF8(*Entry->Filename), Filename.Len());

	Write((uint16)0x01);
	Write((uint16)0x1c);
	Write((uint64)Data.Num());
	Write((uint64)Data.Num());
	Write((uint64)FileOffset);
	Write((uint32)0);

	Write((void*)Data.GetData(), Data.Num());

	Flush();
}

void FZipArchiveWriter::AddFile(const FString& Filename, const TArray<uint8>& Data, const FDateTime& Timestamp)
{
	AddFile(Filename, TConstArrayView<uint8>(Data), Timestamp);
}

void FZipArchiveWriter::Flush()
{
	if (Buffer.Num())
	{
		if (File && !File->Write(Buffer.GetData(), Buffer.Num()))
		{
			UE_LOG(LogZipArchive, Error, TEXT("Failed to write to zip file. Zip file writing aborted."));
			delete File;
			File = nullptr;
		}

		Buffer.Reset(Buffer.Num());
	}
}

#endif