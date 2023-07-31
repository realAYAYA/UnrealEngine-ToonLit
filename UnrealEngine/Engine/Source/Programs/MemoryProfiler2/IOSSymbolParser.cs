// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Runtime.Serialization.Formatters.Binary;
using System.Windows.Forms;

namespace MemoryProfiler2
{
	public class FIOSSymbolParser : ISymbolParser
	{
        private const uint PACKAGE_FILE_TAG = 0x9E2A83C1;
        private const uint PACKAGE_FILE_TAG_SWAPPED = 0xC1832A9E;
        private const int LOADING_COMPRESSION_CHUNK_SIZE = 131072;

        protected struct FGenericPlatformSymbolInfo
        {
            public uint Line;
            public ulong Start;
            public ulong Length;
            public int PathIdx;
            public void Read(BinaryReader reader)
            {
                Line = reader.ReadUInt32();
                Start = reader.ReadUInt64();
                Length = reader.ReadUInt64();
                PathIdx = reader.ReadInt32();
            }
        }

        protected struct FGenericPlatformSymbolData
        {
            public ulong Start;
            public ulong Length;
            public int NameIdx;
            public FGenericPlatformSymbolInfo[] SymbolInfo;

            public void Read(BinaryReader reader)
            {
                Start = reader.ReadUInt64();
                Length = reader.ReadUInt64();
                NameIdx = reader.ReadInt32();
                int nrSymbols = reader.ReadInt32();
                SymbolInfo = new FGenericPlatformSymbolInfo[nrSymbols];
                for (int i = 0; i < nrSymbols; i++)
                {
                    SymbolInfo[i].Read(reader);
                }
            }
        }

        protected struct FGenericPlatformSymbolDatabase
        {
            string Signature;
            string Name;
            public FGenericPlatformSymbolData[] Symbols;
            public string[] StringTable;

            public void Read(BinaryReader reader)
            {
                int SignatureLen = reader.ReadInt32();
                Signature = new string(reader.ReadChars(SignatureLen));
                int NameLen = reader.ReadInt32();
                Name = new string(reader.ReadChars(NameLen));
                int nrSymbols = reader.ReadInt32();
                Symbols = new FGenericPlatformSymbolData[nrSymbols];
                for (int i = 0; i < nrSymbols; i++)
                {
                    Symbols[i].Read(reader);
                }

                int nrStrings = reader.ReadInt32();
                StringTable = new string[nrStrings];
                for (int i = 0; i < nrStrings; i++)
                {
                    int StringLen = reader.ReadInt32();
                    StringTable[i] = new string(reader.ReadChars(StringLen));
                }
            }
        }

        struct FCompressedChunkInfo
        {
            /** Holds the data's compressed size. */
            public long CompressedSize;

            /** Holds the data's uncompresses size. */
            public long UncompressedSize;
        };

        public static string StaticPlatformName()
        {
            return "IOS";
        }

        protected class FIOSSymbol : IComparable<FIOSSymbol>
        {
            public FIOSSymbol(ulong _Address, string _OutFileName, string _OutFunction, int _OutLineNumber)
            {
                OutFileName = _OutFileName;
                OutFunction = _OutFunction;
                OutLineNumber = _OutLineNumber;
                Address = _Address;
            }

            public int CompareTo(FIOSSymbol that)
            {
                if (that == null) return 1;
                if (this.Address > that.Address) return 1;
                if (this.Address < that.Address) return -1;
                return 0;
            }

            public string OutFileName;
            public string OutFunction;
            public int OutLineNumber;
            public ulong Address;
        }

        protected class CompareAdresses : IComparer<FIOSSymbol>
        {

            public int Compare(FIOSSymbol x, FIOSSymbol y)
            {
                if (x.Address > y.Address)
                    return 1;
                if (x.Address < y.Address)
                    return -1;
                return 0;
            }

        }

        protected byte[] LoadCompressedChunk(BinaryReader reader)
        {
            try
            {
                FCompressedChunkInfo PackageFileTag = new FCompressedChunkInfo();
                PackageFileTag.CompressedSize = reader.ReadInt64();
                PackageFileTag.UncompressedSize = reader.ReadInt64();
                FCompressedChunkInfo Summary = new FCompressedChunkInfo();
                Summary.CompressedSize = reader.ReadInt64();
                Summary.UncompressedSize = reader.ReadInt64();

                bool bWasByteSwapped = PackageFileTag.CompressedSize != PACKAGE_FILE_TAG;
                bool bHeaderWasValid = true;

                if (bWasByteSwapped)
                {
                    bHeaderWasValid = PackageFileTag.CompressedSize == PACKAGE_FILE_TAG_SWAPPED;
                    if (bHeaderWasValid)
                    {
                        // not supported
                        //Summary.CompressedSize = BYTESWAP_ORDER64(Summary.CompressedSize);
                        //Summary.UncompressedSize = BYTESWAP_ORDER64(Summary.UncompressedSize);
                        //PackageFileTag.UncompressedSize = BYTESWAP_ORDER64(PackageFileTag.UncompressedSize);
                    }
                }
                else
                {
                    bHeaderWasValid = PackageFileTag.CompressedSize == PACKAGE_FILE_TAG;
                }

                // Handle change in compression chunk size in backward compatible way.
                long LoadingCompressionChunkSize = PackageFileTag.UncompressedSize;
                if (LoadingCompressionChunkSize == PACKAGE_FILE_TAG)
                {
                    LoadingCompressionChunkSize = LOADING_COMPRESSION_CHUNK_SIZE;
                }

                // Figure out how many chunks there are going to be based on uncompressed size and compression chunk size.
                long TotalChunkCount = (Summary.UncompressedSize + LoadingCompressionChunkSize - 1) / LoadingCompressionChunkSize;

                // Allocate compression chunk infos and serialize them, keeping track of max size of compression chunks used.
                FCompressedChunkInfo[] CompressionChunks = new FCompressedChunkInfo[TotalChunkCount];
                long MaxCompressedSize = 0;
                for (int ChunkIndex = 0; ChunkIndex < TotalChunkCount; ChunkIndex++)
                {
                    CompressionChunks[ChunkIndex].CompressedSize = reader.ReadInt64();
                    CompressionChunks[ChunkIndex].UncompressedSize = reader.ReadInt64();
                    if (bWasByteSwapped)
                    {
                        // not supported
                        //CompressionChunks[ChunkIndex].CompressedSize = BYTESWAP_ORDER64(CompressionChunks[ChunkIndex].CompressedSize);
                        //CompressionChunks[ChunkIndex].UncompressedSize = BYTESWAP_ORDER64(CompressionChunks[ChunkIndex].UncompressedSize);
                    }
                    MaxCompressedSize = Math.Max(CompressionChunks[ChunkIndex].CompressedSize, MaxCompressedSize);
                }

                byte[] DeCompressedBuffer = new byte[Summary.UncompressedSize];

                int offset = 0;
                // Iterate over all chunks, serialize them into memory and decompress them directly into the destination pointer
                for (long ChunkIndex = 0; ChunkIndex < TotalChunkCount; ChunkIndex++)
                {
                    // Read compressed data.
                    byte[] initialSkip = reader.ReadBytes(2);
                    byte[] CompressedBuffer = reader.ReadBytes((int)CompressionChunks[ChunkIndex].CompressedSize - 6);
                    byte[] finalSkip = reader.ReadBytes(4);
                    MemoryStream ms = new MemoryStream(CompressedBuffer);
                    DeflateStream gzip = new DeflateStream(ms, CompressionMode.Decompress);
                    gzip.Read(DeCompressedBuffer, offset, (int)CompressionChunks[ChunkIndex].UncompressedSize);
                    // Decompress into dest pointer directly.
                    offset += (int)CompressionChunks[ChunkIndex].UncompressedSize;
                }
                return DeCompressedBuffer;
            }
            catch
            {
                return null;
            }
        }

        public static void AppendAllBytes(string path, byte[] bytes)
        {
            using (var stream = new FileStream(path, FileMode.Append))
            {
                stream.Write(bytes, 0, bytes.Length);
            }
        }

        public override bool InitializeSymbolService(string ExecutableName, FUIBroker UIBroker)
		{
            string FDsName = FStreamInfo.GlobalInstance.FileName;
            string FAdName;
            string FUcName;

            string FExtension = Path.GetExtension(FDsName).ToLower();

            if (FExtension == ".mprof")
            {
                FAdName = Path.GetDirectoryName(FDsName) + "\\" + ExecutableName + ".udebugsymbols";
                FUcName = Path.GetDirectoryName(FDsName) + "\\" + ExecutableName + ".udb";
            }
            else
            {
                return false;
            }
            if (!File.Exists(FAdName))
            {
                MessageBox.Show(String.Format("Failed to look up symbol file ({0}). Please check the path and try again!", FAdName), "Memory Profiler 2", MessageBoxButtons.OK, MessageBoxIcon.Error);
                return false;
            }

            byte[] uncompressed = new byte[0];
            bool NeedsDecompress = false;
            if (!File.Exists(FUcName))
            {
                NeedsDecompress = true;
            }
            else if (File.GetCreationTimeUtc(FAdName) > File.GetCreationTimeUtc(FUcName))
            {
                NeedsDecompress = true;
            }

            if (NeedsDecompress)
            {
                byte[] data = File.ReadAllBytes(FAdName);
                MemoryStream memory = new MemoryStream(data);
                BinaryReader reader = new BinaryReader(memory);


                byte[] DeCompressedBuffer = LoadCompressedChunk(reader);
                while (DeCompressedBuffer != null)
                {
                    AppendAllBytes(FUcName, DeCompressedBuffer);
                    DeCompressedBuffer = LoadCompressedChunk(reader);
                }
            }
            uncompressed = File.ReadAllBytes(FUcName);

            if (uncompressed != null)
            {
                MemoryStream memory = new MemoryStream(uncompressed);
                BinaryReader reader = new BinaryReader(memory);
                Database.Read(reader);
            }

            return true;
		}

        protected ulong ModuleOffset = 0;

        public override void SetModuleOffset(ulong offset)
        {
            ModuleOffset = offset;
        }

        public override bool ResolveAddressToSymboInfo(ESymbolResolutionMode SymbolResolutionMode, ulong Address, out string OutFileName, out string OutFunction, out int OutLineNumber)
		{
			OutFileName = null;
			OutFunction = null;
			OutLineNumber = 0;
            bool bOk = false;

            foreach (var Symbol in Database.Symbols)
            {
                if ((Symbol.Start <= (Address - ModuleOffset)) && ((Symbol.Start + Symbol.Length) >= (Address - ModuleOffset)))
                {
                    OutFunction = Database.StringTable[Symbol.NameIdx];

                    foreach(var SymbolInfo in Symbol.SymbolInfo)
                    {
                        if ((SymbolInfo.Start <= (Address - ModuleOffset)) && ((SymbolInfo.Start + SymbolInfo.Length) >= (Address - ModuleOffset)))
                        {
                            OutFileName = Database.StringTable[SymbolInfo.PathIdx];
                            OutLineNumber = (int)SymbolInfo.Line;
                            break;
                        }
                    }

                    bOk = true;

                    break;
                }
            }

            return bOk;
		}

        protected FGenericPlatformSymbolDatabase Database = new FGenericPlatformSymbolDatabase();

    }
}
