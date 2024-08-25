// Copyright Epic Games, Inc. All Rights Reserved.

using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using UnrealBuildTool.Artifacts;

#nullable enable

namespace UnrealBuildToolTests
{

	[TestClass]
	public class ArtifactsTest
	{
		static readonly Utf8String s_input1Data = new Utf8String("This is some sample input");
		static readonly Utf8String s_input2Data = new Utf8String("This is some more sample input");
		static readonly Utf8String s_output1Data = new Utf8String("This is the sample output");

		private static ArtifactFile MakeInput1()
		{
			return new ArtifactFile(ArtifactDirectoryTree.Absolute, new Utf8String("Input1"), IoHash.Compute(s_input1Data.Span));
		}

		private static ArtifactFile MakeInput2()
		{
			return new ArtifactFile(ArtifactDirectoryTree.Absolute, new Utf8String("Input2"), IoHash.Compute(s_input2Data.Span));
		}

		private static ArtifactFile MakeOutput1()
		{
			return new ArtifactFile(ArtifactDirectoryTree.Absolute, new Utf8String("Output1"), IoHash.Compute(s_output1Data.Span));
		}

		public static ArtifactAction MakeBundle1()
		{
			return new ArtifactAction(
				IoHash.Compute(new Utf8String("SampleKey")),
				IoHash.Compute(new Utf8String("SampleBundleKey")),
				new ArtifactFile[] { MakeInput1(), MakeInput2() },
				new ArtifactFile[] { MakeOutput1() }
			);
		}

		public static ArtifactAction MakeBundle2()
		{
			return new ArtifactAction(
				IoHash.Compute(new Utf8String("SampleKey")),
				IoHash.Compute(new Utf8String("SampleBundleKeyV2")),
				new ArtifactFile[] { MakeInput1(), MakeInput2() },
				new ArtifactFile[] { MakeOutput1() }
			);
		}

		[TestMethod]
		public async Task ArtifactBundleStorageTest1()
		{
			CancellationToken cancellationToken = default;

			IArtifactCache cache = HordeStorageArtifactCache.CreateMemoryCache(NullLogger.Instance);

			await cache.WaitForReadyAsync();
			Assert.AreEqual(ArtifactCacheState.Available, cache.State);

			ArtifactAction bundle1 = MakeBundle1();
			await cache.SaveArtifactActionsAsync(new ArtifactAction[] { bundle1 }, cancellationToken);

			ArtifactAction[] readBack1 = await cache.QueryArtifactActionsAsync(new IoHash[] { bundle1.Key }, cancellationToken);
			Assert.AreEqual(1, readBack1.Length);

			ArtifactAction bundle2 = MakeBundle2();
			await cache.SaveArtifactActionsAsync(new ArtifactAction[] { bundle2 }, cancellationToken);

			ArtifactAction[] readBack2 = await cache.QueryArtifactActionsAsync(new IoHash[] { bundle1.Key }, cancellationToken);
			Assert.AreEqual(2, readBack2.Length);

			//await cache.FlushChangesAsync(cancellationToken);
		}
	}
}
