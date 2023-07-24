// Copyright Epic Games, Inc. All Rights Reserved.

using System.Text;
using Jupiter.Implementation;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using OpenTelemetry.Trace;

namespace Jupiter.Tests.Unit
{
    [TestClass]
    public class CompressedBufferTests
    {

        [TestMethod]
        public void CompressAndDecompress()
        {
            byte[] bytes = Encoding.UTF8.GetBytes("this is a test string");

            CompressedBufferUtils bufferUtils = new(TracerProvider.Default.GetTracer("TestTracer"));

            byte[] compressedBytes = bufferUtils.CompressContent(OoodleCompressorMethod.Mermaid, OoodleCompressionLevel.VeryFast, bytes);

            byte[] roundTrippedBytes = bufferUtils.DecompressContent(compressedBytes);

            CollectionAssert.AreEqual(bytes, roundTrippedBytes);
        }
    }
}
