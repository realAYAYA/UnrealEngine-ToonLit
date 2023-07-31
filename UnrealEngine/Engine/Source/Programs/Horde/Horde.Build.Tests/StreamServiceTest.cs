// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Globalization;
using System.Threading.Tasks;
using Horde.Build.Streams;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Build.Tests
{
	[TestClass]
    public class StreamServiceTests : TestSetup
    {
        [TestMethod]
        public async Task Pausing()
        {
			Fixture fixture = await CreateFixtureAsync();

	        IStream stream = (await StreamService.GetStreamAsync(fixture!.Stream!.Id))!;
	        Assert.IsFalse(stream.IsPaused(DateTime.UtcNow));
	        Assert.IsNull(stream.PausedUntil);
	        Assert.IsNull(stream.PauseComment);

	        DateTime pausedUntil = DateTime.UtcNow.AddHours(1);
	        await StreamService.UpdatePauseStateAsync(stream, newPausedUntil: pausedUntil, newPauseComment: "mycomment");
	        stream = (await StreamService.GetStreamAsync(fixture!.Stream!.Id))!;
	        // Comparing by string to avoid comparing exact milliseconds as those are not persisted in MongoDB fields
	        Assert.IsTrue(stream.IsPaused(DateTime.UtcNow));
	        Assert.AreEqual(pausedUntil.ToString(CultureInfo.InvariantCulture), stream.PausedUntil!.Value.ToString(CultureInfo.InvariantCulture));
	        Assert.AreEqual("mycomment", stream.PauseComment);
	        
	        await StreamService.UpdatePauseStateAsync(stream, newPausedUntil: null, newPauseComment: null);
	        stream = (await StreamService.GetStreamAsync(fixture!.Stream!.Id))!;
	        Assert.IsFalse(stream.IsPaused(DateTime.UtcNow));
	        Assert.IsNull(stream.PausedUntil);
	        Assert.IsNull(stream.PauseComment);
        }
    }
}