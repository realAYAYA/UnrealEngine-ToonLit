// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Reflection;
using System.Runtime.InteropServices;
using IntPtr = System.IntPtr;

namespace Jupiter.Utils
{

#pragma warning disable CA1008 // Add a member that has a value of zero with a suggested name of None
#pragma warning disable CA1027 // Mark enums with FlagsAttribute
#pragma warning disable CA1707 // Identifiers should not contain underscores. We use the same identifiers as Oodle uses in its source
#pragma warning disable CA1712 // Do not prefix enum values with the name of the enum type.  We use the same identifiers as Oodle uses in its source

    public enum OodleLZ_Compressor
    {
        OodleLZ_Compressor_Invalid = -1,
        OodleLZ_Compressor_None = 3, // None = memcpy, pass through uncompressed bytes

        // NEW COMPRESSORS :
        OodleLZ_Compressor_Kraken = 8, // Fast decompression and high compression ratios, amazing!
        OodleLZ_Compressor_Leviathan = 13, // Leviathan = Kraken's big brother with higher compression, slightly slower decompression.
        OodleLZ_Compressor_Mermaid = 9, // Mermaid is between Kraken & Selkie - crazy fast, still decent compression.
        OodleLZ_Compressor_Selkie = 11, // Selkie is a super-fast relative of Mermaid.  For maximum decode speed.
        OodleLZ_Compressor_Hydra = 12, // Hydra, the many-headed beast = Leviathan, Kraken, Mermaid, or Selkie (see $OodleLZ_About_Hydra)

        /* Deprecated compressors
        OodleLZ_Compressor_BitKnit = 10, // no longer supported as of Oodle 2.9.0
        OodleLZ_Compressor_LZB16 = 4, // DEPRECATED but still supported
        OodleLZ_Compressor_LZNA = 7,  // no longer supported as of Oodle 2.9.0
        OodleLZ_Compressor_LZH = 0,   // no longer supported as of Oodle 2.9.0
        OodleLZ_Compressor_LZHLW = 1, // no longer supported as of Oodle 2.9.0
        OodleLZ_Compressor_LZNIB = 2, // no longer supported as of Oodle 2.9.0
        OodleLZ_Compressor_LZBLW = 5, // no longer supported as of Oodle 2.9.0
        OodleLZ_Compressor_LZA = 6,   // no longer supported as of Oodle 2.9.0
         */

        OodleLZ_Compressor_Count = 14,
        OodleLZ_Compressor_Force32 = 0x40000000
    };

    public enum OodleLZ_CompressionLevel
    {
        OodleLZ_CompressionLevel_None=0,        // don't compress, just copy raw bytes
        OodleLZ_CompressionLevel_SuperFast=1,   // super fast mode, lower compression ratio
        OodleLZ_CompressionLevel_VeryFast=2,    // fastest LZ mode with still decent compression ratio
        OodleLZ_CompressionLevel_Fast=3,        // fast - good for daily use
        OodleLZ_CompressionLevel_Normal=4,      // standard medium speed LZ mode

        OodleLZ_CompressionLevel_Optimal1=5,    // optimal parse level 1 (faster optimal encoder)
        OodleLZ_CompressionLevel_Optimal2=6,    // optimal parse level 2 (recommended baseline optimal encoder)
        OodleLZ_CompressionLevel_Optimal3=7,    // optimal parse level 3 (slower optimal encoder)
        OodleLZ_CompressionLevel_Optimal4=8,    // optimal parse level 4 (very slow optimal encoder)
        OodleLZ_CompressionLevel_Optimal5=9,    // optimal parse level 5 (don't care about encode speed, maximum compression)

        OodleLZ_CompressionLevel_HyperFast1=-1, // faster than SuperFast, less compression
        OodleLZ_CompressionLevel_HyperFast2=-2, // faster than HyperFast1, less compression
        OodleLZ_CompressionLevel_HyperFast3=-3, // faster than HyperFast2, less compression
        OodleLZ_CompressionLevel_HyperFast4 =-4, // fastest, less compression
        
        // aliases :
        OodleLZ_CompressionLevel_HyperFast =OodleLZ_CompressionLevel_HyperFast1, // alias hyperfast base level
        OodleLZ_CompressionLevel_Optimal = OodleLZ_CompressionLevel_Optimal2,   // alias optimal standard level
        OodleLZ_CompressionLevel_Max     = OodleLZ_CompressionLevel_Optimal5,   // maximum compression level
        OodleLZ_CompressionLevel_Min     = OodleLZ_CompressionLevel_HyperFast4, // fastest compression level

        OodleLZ_CompressionLevel_Force32 = 0x40000000,
        OodleLZ_CompressionLevel_Invalid = OodleLZ_CompressionLevel_Force32
    }

    public enum OodleLZ_Decode_ThreadPhase
    {
        OodleLZ_Decode_ThreadPhase1 = 1,
        OodleLZ_Decode_ThreadPhase2 = 2,
        OodleLZ_Decode_ThreadPhaseAll = 3,
        OodleLZ_Decode_Unthreaded = OodleLZ_Decode_ThreadPhaseAll
    }

    public enum OodleLZ_FuzzSafe
    {
        OodleLZ_FuzzSafe_No = 0,
        OodleLZ_FuzzSafe_Yes = 1
    }

    public enum OodleLZ_CheckCRC
    {
        OodleLZ_CheckCRC_No = 0,
        OodleLZ_CheckCRC_Yes = 1,
        OodleLZ_CheckCRC_Force32 = 0x40000000
    }

    public enum OodleLZ_Verbosity
    {
        OodleLZ_Verbosity_None = 0,
        OodleLZ_Verbosity_Minimal = 1,
        OodleLZ_Verbosity_Some = 2,
        OodleLZ_Verbosity_Lots = 3,
        OodleLZ_Verbosity_Force32 = 0x40000000
    }
#pragma warning restore CA1008 // Add a member that has a value of zero with a suggested name of None
#pragma warning restore CA1027 // Mark enums with FlagsAttribute
#pragma warning restore CA1707 // Identifiers should not contain underscores
#pragma warning restore CA1712 // Do not prefix enum values with the name of the enum type

    [StructLayout(LayoutKind.Sequential)]
    public class OodleConfigValues
    {
        public int m_OodleLZ_LW_LRM_step; // LZHLW LRM : bytes between LRM entries
        public int m_OodleLZ_LW_LRM_hashLength; // LZHLW LRM : bytes hashed for each LRM entries
        public int m_OodleLZ_LW_LRM_jumpbits; // LZHLW LRM : bits of hash used for jump table

        public int m_OodleLZ_Decoder_Max_Stack_Size;  // if OodleLZ_Decompress needs to allocator a Decoder object, and it's smaller than this size, it's put on the stack instead of the heap
        public int m_OodleLZ_Small_Buffer_LZ_Fallback_Size_Unused;  // deprecated
        public int m_OodleLZ_BackwardsCompatible_MajorVersion; // if you need to encode streams that can be read with an older version of Oodle, set this to the Oodle2 MAJOR version number that you need compatibility with.  eg to be compatible with oodle 2.7.3 you would put 7 here

        public int m_oodle_header_version;
    }

    public class OodleCompressor : IDisposable
    {
        private IntPtr _handle;
        private bool _initialized;
        private static bool s_resolverAdded;

        [DllImport("oo2core")]
        static extern long OodleLZ_Compress(OodleLZ_Compressor compressor, byte[] rawBuf, long rawLen, byte[] compBuf, OodleLZ_CompressionLevel level, long pOptions, byte[]? dictionaryBase, byte[]? lrm, long scratchMem, long scratchSize);

        [DllImport("oo2core")]
        static extern long OodleLZ_GetCompressedBufferSizeNeeded(OodleLZ_Compressor compressor, long rawSize);

        [DllImport("oo2core")]
        static extern int OodleLZDecoder_MemorySizeNeeded(OodleLZ_Compressor compressor, long rawLen = -1);

        [DllImport("oo2core")]
        static extern long OodleLZ_Decompress(byte[] compBuf, long compBufSize, byte[] rawBuf, long rawLen, OodleLZ_FuzzSafe fuzzSafe = OodleLZ_FuzzSafe.OodleLZ_FuzzSafe_Yes, OodleLZ_CheckCRC checkCRC = OodleLZ_CheckCRC.OodleLZ_CheckCRC_No, OodleLZ_Verbosity verbosity = OodleLZ_Verbosity.OodleLZ_Verbosity_None, byte[]? decBufBase = null, long decBufSize = 0, long fpCallback = 0, long callbackUserData = 0, byte[]? decoderMemory = null, long decoderMemorySize = 0, OodleLZ_Decode_ThreadPhase threadPhase = OodleLZ_Decode_ThreadPhase.OodleLZ_Decode_Unthreaded);

        [DllImport("oo2core")]
        static extern void Oodle_GetConfigValues(OodleConfigValues configValues);

        [DllImport("oo2core")]
        static extern void Oodle_SetConfigValues(OodleConfigValues configValues);

        public void InitializeOodle()
        {
            if (!s_resolverAdded)
            {
                NativeLibrary.SetDllImportResolver(typeof(OodleCompressor).Assembly, ImportResolver);
                s_resolverAdded = true;
            }

            OodleConfigValues oodleConfig = new();
            Oodle_GetConfigValues(oodleConfig);
            oodleConfig.m_OodleLZ_BackwardsCompatible_MajorVersion = 9;
            Oodle_SetConfigValues(oodleConfig);

            _initialized = true;
        }

        private IntPtr ImportResolver(string libraryName, Assembly assembly, DllImportSearchPath? searchpath)
        {
            IntPtr libHandle = IntPtr.Zero;
            if (libraryName == "oo2core")
            {
                // we manually load the assembly as the file name is not consistent between platforms thus the automatic searching will not work
                string assemblyFilename;
                bool is64Bit = Environment.Is64BitProcess;

                if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
                {
                    assemblyFilename = $"oo2core_9_{(is64Bit ? "win64" : "win32")}.dll";
                } else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
                {
                    assemblyFilename = $"liboo2core{(is64Bit ? "linux64" : "linux")}.so.9";
                }
                else
                {
                    throw new NotImplementedException();
                }
                _handle = NativeLibrary.Load(assemblyFilename, assembly, searchpath);
                libHandle = _handle;
            }
            return libHandle;
        }

        public long Compress(OodleLZ_Compressor compressor, byte[] rawBuf, OodleLZ_CompressionLevel compressionLevel, out byte[] compBuf)
        {
            if (!_initialized)
            {
                throw new InvalidOperationException("Initialize Oodle before using it");
            }

            long bufferSizeNeeded = OodleLZ_GetCompressedBufferSizeNeeded(compressor, rawBuf.LongLength);
            compBuf = new byte[bufferSizeNeeded];

            long result = OodleLZ_Compress(compressor, rawBuf, rawBuf.LongLength, compBuf, compressionLevel, 0, null, null, 0, 0);
            return result;
        }
        public long CompressedBufferSizeNeeded(OodleLZ_Compressor compressor, long rawLength)
        {
            if (!_initialized)
            {
                throw new InvalidOperationException("Initialize Oodle before using it");
            }

            long bufferSizeNeeded = OodleLZ_GetCompressedBufferSizeNeeded(compressor, rawLength);
            return bufferSizeNeeded;
        }

        public long Decompress(byte[] compBuf, long uncompressedSize, out byte[] rawBuf)
        {
            if (!_initialized)
            {
                throw new InvalidOperationException("Initialize Oodle before using it");
            }

            rawBuf = new byte[uncompressedSize];

            long result = OodleLZ_Decompress(compBuf, compBuf.LongLength, rawBuf, rawBuf.LongLength);
            return result;
        }

        private void ReleaseUnmanagedResources()
        {
            if (_handle != IntPtr.Zero)
            {
                NativeLibrary.Free(_handle);
            }
        }

        public void Dispose()
        {
            Dispose(true);
            GC.SuppressFinalize(this);
        }

        protected virtual void Dispose(bool disposing)
        {
            if (disposing)
            {
                ReleaseUnmanagedResources();
            }
        }

        ~OodleCompressor()
        {
            Dispose(false);
        }
    }
}
