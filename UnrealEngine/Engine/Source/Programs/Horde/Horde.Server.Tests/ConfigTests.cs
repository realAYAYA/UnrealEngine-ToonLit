// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.Json;
using System.Threading;
using Horde.Server.Configuration;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System.Threading.Tasks;
using System.Linq;
using EpicGames.Core;
using Moq;
using Horde.Server.Perforce;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging.Abstractions;
using System.IO;
using Horde.Server.Server;

namespace Horde.Server.Tests
{
	[TestClass]
	public class ConfigTests
	{
		class SubObject
		{
			public string? ValueA { get; set; }
			public int ValueB { get; set; }
			public SubObject? ValueC { get; set; }
		}

		[ConfigIncludeRoot]
		class ConfigObject
		{
			public List<ConfigInclude> Include { get; set; } = new List<ConfigInclude>();
			public string? TestString { get; set; }
			public List<string> TestList { get; set; } = new List<string>();
			public SubObject? TestObject { get; set; }
		}

		JsonSerializerOptions _jsonOptions = new JsonSerializerOptions { DefaultIgnoreCondition = System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingDefault };

		[TestMethod]
		public async Task IncludeTestAsync()
		{
			CancellationToken cancellationToken = CancellationToken.None;
			InMemoryConfigSource source = new InMemoryConfigSource();

			// memory:///bar
			Uri barUri = new Uri("memory:///bar");
			{
				ConfigObject obj = new ConfigObject();
				obj.TestList.Add("secondobj");
				obj.TestObject = new SubObject { ValueB = 123, ValueC = new SubObject { ValueB = 456 } };

				byte[] data2 = JsonSerializer.SerializeToUtf8Bytes(obj, _jsonOptions);
				source.Add(barUri, data2);
			}

			// memory:///foo
			Uri fooUri = new Uri("memory:///foo");
			{
				ConfigObject obj = new ConfigObject();
				obj.Include.Add(new ConfigInclude { Path = barUri.ToString() });
				obj.TestString = "hello";
				obj.TestList.Add("there");
				obj.TestList.Add("world");
				obj.TestObject = new SubObject { ValueA = "hi", ValueC = new SubObject { ValueA = "yo" } };

				byte[] json1 = JsonSerializer.SerializeToUtf8Bytes(obj, _jsonOptions);
				source.Add(fooUri, json1);
			}

			Dictionary<string, IConfigSource> sources = new Dictionary<string, IConfigSource>();
			sources["memory"] = source;

			ConfigContext options = new ConfigContext(_jsonOptions, sources, NullLogger.Instance);

			ConfigObject result = await ConfigType.ReadAsync<ConfigObject>(fooUri, options, cancellationToken);
			Assert.AreEqual(result.TestString, "hello");
			Assert.IsTrue(result.TestList.SequenceEqual(new[] { "secondobj", "there", "world" }));
			Assert.AreEqual(result.TestObject!.ValueA, "hi");
			Assert.AreEqual(result.TestObject!.ValueB, 123);
			Assert.AreEqual(result.TestObject!.ValueC!.ValueA, "yo");
		}

		[TestMethod]
		public async Task FileTestAsync()
		{
			CancellationToken cancellationToken = CancellationToken.None;

			DirectoryReference baseDir = new DirectoryReference("test");
			DirectoryReference.CreateDirectory(baseDir);

			FileConfigSource source = new FileConfigSource(baseDir);

			// file:test/foo
			Uri fooUri = new Uri($"file:///{FileReference.Combine(baseDir, "test.json")}");

			byte[] data;
			{
				ConfigObject obj = new ConfigObject();
				obj.TestString = "hello";
				data = JsonSerializer.SerializeToUtf8Bytes(obj, new JsonSerializerOptions { DefaultIgnoreCondition = System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingDefault });
			}
			await FileReference.WriteAllBytesAsync(new FileReference(fooUri.LocalPath), data, cancellationToken);

			Dictionary<string, IConfigSource> sources = new Dictionary<string, IConfigSource>();
			sources["file"] = source;

			ConfigContext context = new ConfigContext(_jsonOptions, sources, NullLogger.Instance);

			ConfigObject result = await ConfigType.ReadAsync<ConfigObject>(fooUri, context, cancellationToken);
			Assert.AreEqual(result.TestString, "hello");

			// Check it returns the same object if the timestamp hasn't changed
			IConfigFile file1 = await source.GetAsync(fooUri, cancellationToken);
			await Task.Delay(TimeSpan.FromSeconds(1));
			IConfigFile file2 = await source.GetAsync(fooUri, cancellationToken);
			Assert.IsTrue(ReferenceEquals(file1, file2));

			// Check it returns a new object if the timestamp HAS changed
			await Task.Delay(TimeSpan.FromSeconds(1));
			await FileReference.WriteAllBytesAsync(new FileReference(fooUri.LocalPath), data, cancellationToken);
			IConfigFile file3 = await source.GetAsync(fooUri, cancellationToken);
			Assert.IsTrue(!ReferenceEquals(file1, file3));
		}
	}
}
