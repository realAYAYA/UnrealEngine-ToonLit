// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UbaPathUtils.h"
#include "UbaBinaryReaderWriter.h"

namespace uba
{
	enum : u64 { InvalidTableOffset = 0 };

	class DirectoryTable
	{
	public:
		using EntryLookup = GrowingUnorderedMap<StringKey, u32>;
		struct Directory { Directory(MemoryBlock* block) : files(block) {} u32 tableOffset = InvalidTableOffset; u32 parseOffset = InvalidTableOffset; EntryLookup files; ReaderWriterLock lock; };

		void Init(u8* mem, u32 tableCount, u32 tableSize)
		{
			m_memory = mem;
			m_lookup.reserve(tableCount + 100);
			ParseDirectoryTable(tableSize);
		}

		void ParseDirectoryTable(u32 size)
		{
			SCOPED_WRITE_LOCK(m_lookupLock, lock);
			ParseDirectoryTableNoLock(size);
		}

		void ParseDirectoryTableNoLock(u32 size)
		{
			if (size <= m_memorySize)
				return;
			BinaryReader reader(m_memory, m_memorySize);
			while (true)
			{
				u64 pos = reader.GetPosition();
				if (pos == size)
					break;
				UBA_ASSERTF(pos < size, TC("Should never read past size (pos: %u, size: %u)"), pos, size);
				u64 storageSize = reader.Read7BitEncoded();
				StringKey dirKey = reader.ReadStringKey();
				auto insres = m_lookup.try_emplace(dirKey, m_memoryBlock); // Note that this is allowed to overwrite
				insres.first->second.tableOffset = u32(reader.GetPosition());
				reader.Skip(storageSize - sizeof(dirKey));
			}
			m_memorySize = size;
		}

		void PopulateDirectory(const StringKeyHasher& hasher, Directory& dir)
		{
			if (dir.parseOffset == dir.tableOffset)
				return;
			SCOPED_WRITE_LOCK(dir.lock, lock);
			PopulateDirectoryRecursive(hasher, dir.tableOffset, dir.parseOffset, dir.files);
			dir.parseOffset = dir.tableOffset;
		}

		void PopulateDirectoryRecursive(const StringKeyHasher& hasher, u32 tableOffset, u32 parseOffset, EntryLookup& files)
		{
			BinaryReader reader(m_memory, tableOffset);
			u32 prevTableOffset = u32(reader.Read7BitEncoded());

			u32 buffer[48*1024];
			u32 count = 0;
			u32* readerOffsets = buffer;
			readerOffsets[count++] = u32(reader.GetPosition());
			bool firstIsRoot = true;
			while (true)
			{
				if (prevTableOffset == InvalidTableOffset || prevTableOffset == parseOffset)
				{
					firstIsRoot = prevTableOffset == InvalidTableOffset;
					break;
				}
				reader.SetPosition(prevTableOffset);
				prevTableOffset = u32(reader.Read7BitEncoded());
				readerOffsets[count++] = u32(reader.GetPosition());
				if (count == sizeof_array(buffer))
				{
					readerOffsets = new u32[1024*1024]; // This sucks, but somethings the directory is huuuge. Ideally these files should be spread out over multiple directories
					memcpy(readerOffsets, buffer, sizeof(buffer));
				}
			}

			for (u32 i=count; i>0; --i)
			{
				reader.SetPosition(readerOffsets[i-1]);
				if (firstIsRoot)
				{
					firstIsRoot = false;
					u32 attr = reader.ReadU32();
					if (!attr)
						continue;
					reader.Skip(sizeof(u32) + sizeof(u64));
				}

				PopulateDirectoryWithFiles(reader, hasher, files);
			}

			if (readerOffsets != buffer)
				delete[] readerOffsets;
		}

		void PopulateDirectoryWithFiles(BinaryReader& reader, const StringKeyHasher& hasher, EntryLookup& files)
		{
			u64 itemCount = reader.Read7BitEncoded();

			files.reserve(files.size() + itemCount);
	
			StringBuffer<> filename;
			filename.Append(PathSeparator);

			while (itemCount--)
			{
				u32 offset = u32(reader.GetPosition());
				filename.Resize(1);
				reader.ReadString(filename);
				if (CaseInsensitiveFs)
					filename.MakeLower();
				reader.Skip(sizeof(u32)*2 + sizeof(u64)*3);

				StringKey filenameKey = ToStringKey(hasher, filename.data, filename.count);
				files[filenameKey] = offset; // Always write, since same file might have been added with new info
			}
		}

		enum Exists
		{
			Exists_Yes,
			Exists_No,
			Exists_Maybe,
		};

		Exists EntryExists(StringKey entryKey, const tchar* entryName, u64 entryNameLen, bool checkIfDir = false, u32* tableOffset = nullptr)
		{
			SCOPED_READ_LOCK(m_lookupLock, lock);
			return EntryExistsNoLock(entryKey, entryName, entryNameLen, checkIfDir, tableOffset);
		}

		Exists EntryExistsNoLock(StringKey entryKey, const tchar* entryName, u64 entryNameLen, bool checkIfDir = false, u32* tableOffset = nullptr)
		{
			if (checkIfDir)
			{
				auto findIt = m_lookup.find(entryKey);
				if (findIt != m_lookup.end())
				{
					if (tableOffset)
						*tableOffset = u32(findIt->second.tableOffset) | 0x80000000; // Use significant bit to say that this is a dir
					return Exists_Yes;
				}
			}

			// scan backwards first
			const tchar* rit = entryName + entryNameLen - 2;
			bool inAncestor = false;
			while (rit > entryName)
			{
				if (*rit != PathSeparator)
				{
					--rit;
					continue;
				}

				u64 sublen = u64(rit - entryName);
				StringKeyHasher ancestorHasher;
				ancestorHasher.Update(entryName, sublen);
				StringKey ancestorKey = ToStringKey(ancestorHasher);
				auto dirIt = m_lookup.find(ancestorKey);
				if (dirIt != m_lookup.end())
				{
					DirectoryTable::Directory& parentDir = dirIt->second;
					if (parentDir.tableOffset == -1)
						return Exists_No;
					if (parentDir.parseOffset != parentDir.tableOffset)
					{
						SCOPED_WRITE_LOCK(parentDir.lock, lock);
						PopulateDirectoryRecursive(ancestorHasher, parentDir.tableOffset, parentDir.parseOffset, parentDir.files);
						parentDir.parseOffset = parentDir.tableOffset;
					}

					SCOPED_READ_LOCK(parentDir.lock, lock);
					auto entryIt = parentDir.files.find(entryKey);
					if (entryIt == parentDir.files.end())
						return Exists_No;
					if (inAncestor)
						return Exists_Maybe;
					if (tableOffset)
						*tableOffset = entryIt->second;
					return Exists_Yes;
				}

				entryKey = ancestorKey;
				--rit;
				inAncestor = true;
			}
			return Exists_Maybe;
		}

		struct EntryInformation
		{
			u32 attributes;
			u32 volumeSerial;
			u64 fileIndex;
			u64 size;
			u64 lastWrite;
		};

		u32 GetEntryInformation(EntryInformation& outInfo, u32 tableOffset, tchar* outFileName = nullptr, u32 fileNameCapacity = 0)
		{
			if (tableOffset & 0x80000000)
			{
				tableOffset = tableOffset & ~0x80000000;
				BinaryReader reader(m_memory, tableOffset);
				u64 prevTableOffset = reader.Read7BitEncoded();
				while (prevTableOffset != InvalidTableOffset)
				{
					reader.SetPosition(prevTableOffset);
					prevTableOffset = reader.Read7BitEncoded();
				}
				outInfo.attributes = reader.ReadU32();
				if (outInfo.attributes)
				{
					outInfo.volumeSerial = reader.ReadU32();
					outInfo.fileIndex = reader.ReadU64();
				}
				outInfo.size = 0;
				outInfo.lastWrite = 0;
				UBA_ASSERT(!outFileName);
				return ~u32(0);
			}

			BinaryReader reader(m_memory, tableOffset);
			if (outFileName)
				reader.ReadString(outFileName, fileNameCapacity);
			else
				reader.SkipString();
			outInfo.lastWrite = reader.ReadU64();
			outInfo.attributes = reader.ReadU32();
			outInfo.volumeSerial = reader.ReadU32();
			outInfo.fileIndex = reader.ReadU64();
			outInfo.size = reader.ReadU64();
			return u32(reader.GetPosition());
		}

		void GetFinalPath(StringBufferBase& out, const tchar* path)
		{
			UBA_ASSERT(path[1] == ':');

			Directory* directory = nullptr;
			const tchar* prevSlash = TStrchr(path+3, PathSeparator);
			if (!prevSlash)
			{
				UBA_ASSERTF(false, TC("GetFinalPath got path \"%s\" which has no backslash"), path);
				return;
			}
			out.Append(path, u64(prevSlash - path));
			const tchar* end = path + TStrlen(path);

			StringBuffer<> forHash;
			forHash.Append(path, u64(prevSlash - path));
			if (CaseInsensitiveFs)
				forHash.MakeLower();

			StringKeyHasher hasher;
			hasher.Update(forHash.data, forHash.count);

			SCOPED_READ_LOCK(m_lookupLock, lock);
			while (true)
			{
				const tchar* slash = TStrchr(prevSlash + 1, PathSeparator);
				if (!slash)
					slash = end;

				forHash.Clear().Append(prevSlash, u64(slash - prevSlash));
				if (CaseInsensitiveFs)
					forHash.MakeLower();
				hasher.Update(forHash.data, forHash.count);
				StringKey fileNameKey = ToStringKey(hasher);

				if (directory)
				{
					SCOPED_READ_LOCK(directory->lock, lock2);
					auto fileIt = directory->files.find(fileNameKey);
					if (fileIt != directory->files.end())
					{
						BinaryReader reader(m_memory, fileIt->second);
						StringBuffer<> fileName;
						reader.ReadString(fileName);
						out.Append(PathSeparator).Append(fileName);
					}
					else
						out.Append(prevSlash, u64(slash - prevSlash));
				}
				else
					out.Append(prevSlash, u64(slash - prevSlash));

				if (slash == end)
					return;

				prevSlash = slash;

				auto findIt = m_lookup.find(fileNameKey);
				if (findIt == m_lookup.end())
				{
					directory = nullptr;
					continue;
				}
				directory = &findIt->second;
			}
		}

		DirectoryTable(MemoryBlock* block) : m_memoryBlock(block), m_lookup(block) {}

		MemoryBlock* m_memoryBlock;
		ReaderWriterLock m_lookupLock;
		GrowingUnorderedMap<StringKey, Directory> m_lookup;

		ReaderWriterLock m_memoryLock;
		u8* m_memory = nullptr;
		u32 m_memorySize = 0;
	};

}