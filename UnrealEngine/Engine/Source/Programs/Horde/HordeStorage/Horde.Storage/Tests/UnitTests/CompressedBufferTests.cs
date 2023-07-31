// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text;
using Horde.Storage.Implementation;
using Jupiter.Utils;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Storage.Tests.Unit
{
    [TestClass]
    public class CompressedBufferTests
    {

        [TestMethod]
        public void CompressAndDecompress()
        {
            byte[] bytes = Encoding.UTF8.GetBytes("this is a test string");

            using OodleCompressor compressor = new OodleCompressor();
            compressor.InitializeOodle();
            CompressedBufferUtils bufferUtils = new CompressedBufferUtils(compressor);

            byte[] compressedBytes = bufferUtils.CompressContent(OoodleCompressorMethod.Mermaid, OoodleCompressionLevel.VeryFast, bytes);

            byte[] roundTrippedBytes = bufferUtils.DecompressContent(compressedBytes);

            CollectionAssert.AreEqual(bytes, roundTrippedBytes);
        }
    }
}
