// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Configuration;
using Horde.Server.Server;
using Microsoft.Extensions.Logging.Abstractions;
using Microsoft.VisualStudio.TestTools.UnitTesting;

namespace Horde.Server.Tests.Configuration
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

		readonly JsonSerializerOptions _jsonOptions = new JsonSerializerOptions { DefaultIgnoreCondition = System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingDefault };

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

		private static NetworkConfig? GetNetworkConfig(GlobalConfig gc, string ip)
		{
			bool result = gc.TryGetNetworkConfig(IPAddress.Parse(ip), out NetworkConfig? networkConfig);
			return result ? networkConfig : null;
		}

		[TestMethod]
		public void NetworkConfig()
		{
			GlobalConfig gc = new()
			{
				Networks = new List<NetworkConfig>()
				{
					new() { CidrBlock = "10.0.0.0/30", Id = "foo" },
					new() { CidrBlock = "10.0.0.4/30", Id = "bar" },
					new() { CidrBlock = "192.168.0.0/16", Id = "baz" },
				}
			};

			Assert.AreEqual(null, GetNetworkConfig(gc, "10.0.0.0"));
			Assert.AreEqual("foo", GetNetworkConfig(gc, "10.0.0.1")!.Id);
			Assert.AreEqual("foo", GetNetworkConfig(gc, "10.0.0.2")!.Id);
			Assert.AreEqual("foo", GetNetworkConfig(gc, "10.0.0.3")!.Id);

			Assert.AreEqual(null, GetNetworkConfig(gc, "10.0.0.4"));
			Assert.AreEqual("bar", GetNetworkConfig(gc, "10.0.0.5")!.Id);
			Assert.AreEqual("bar", GetNetworkConfig(gc, "10.0.0.6")!.Id);
			Assert.AreEqual("bar", GetNetworkConfig(gc, "10.0.0.7")!.Id);
			Assert.AreEqual(null, GetNetworkConfig(gc, "10.0.0.4"));

			Assert.AreEqual(null, GetNetworkConfig(gc, "192.168.0.0"));
			Assert.AreEqual("baz", GetNetworkConfig(gc, "192.168.0.1")!.Id);
			Assert.AreEqual("baz", GetNetworkConfig(gc, "192.168.255.254")!.Id);
			Assert.AreEqual(null, GetNetworkConfig(gc, "192.169.0.1"));

			Assert.AreEqual(null, GetNetworkConfig(gc, "11.0.0.1"));

			gc = new()
			{
				Networks = new List<NetworkConfig>() { new() { CidrBlock = "0.0.0.0/0", Id = "global" } }
			};
			Assert.AreEqual("global", GetNetworkConfig(gc, "15.3.4.5")!.Id);
		}

		class ObjectValue
		{
			public string Value { get; set; } = "";
		}

		[ConfigMacroScope]
		class BaseMacroScope
		{
			public List<ConfigMacro> Macros { get; set; } = new List<ConfigMacro>();
			public string Value { get; set; } = "";
			public List<string> ListValue { get; set; } = new List<string>();
			public ObjectValue ObjectValue { get; set; } = new ObjectValue();
			public BaseMacroScope? ChildScope { get; set; }
		}

		[TestMethod]
		public async Task MacroTestAsync()
		{
			CancellationToken cancellationToken = CancellationToken.None;
			InMemoryConfigSource source = new InMemoryConfigSource();

			// memory:///bar
			Uri fooUri = new Uri("memory:///foo");
			{
				BaseMacroScope obj = new BaseMacroScope();
				obj.Macros.Add(new ConfigMacro { Name = "MacroName", Value = "MacroValue" });
				obj.Value = "This is a macro $(MacroName)";
				obj.ListValue.Add("List element macro $(MacroName)");
				obj.ObjectValue = new ObjectValue { Value = "Object macro $(MacroName)" };

				byte[] data2 = JsonSerializer.SerializeToUtf8Bytes(obj, _jsonOptions);
				source.Add(fooUri, data2);
			}

			Dictionary<string, IConfigSource> sources = new Dictionary<string, IConfigSource>();
			sources["memory"] = source;

			ConfigContext options = new ConfigContext(_jsonOptions, sources, NullLogger.Instance);

			BaseMacroScope result = await ConfigType.ReadAsync<BaseMacroScope>(fooUri, options, cancellationToken);
			Assert.AreEqual("This is a macro MacroValue", result.Value);
			Assert.AreEqual("List element macro MacroValue", result.ListValue[0]);
			Assert.AreEqual("Object macro MacroValue", result.ObjectValue.Value);
		}

		[TestMethod]
		public async Task NestedMacroTestAsync()
		{
			CancellationToken cancellationToken = CancellationToken.None;
			InMemoryConfigSource source = new InMemoryConfigSource();

			// memory:///bar
			Uri fooUri = new Uri("memory:///foo");
			{
				BaseMacroScope obj = new BaseMacroScope();
				obj.Macros.Add(new ConfigMacro { Name = "MacroName", Value = "MacroValue" });

				obj.ChildScope = new BaseMacroScope();
				obj.ChildScope.Macros.Add(new ConfigMacro { Name = "MacroName2", Value = "MacroValue2" });
				obj.ChildScope.Value = "This is a macro $(MacroName) $(MacroName2)";

				obj.Value = "This is a macro $(MacroName) $(MacroName2)";

				byte[] data2 = JsonSerializer.SerializeToUtf8Bytes(obj, _jsonOptions);
				source.Add(fooUri, data2);
			}

			Dictionary<string, IConfigSource> sources = new Dictionary<string, IConfigSource>();
			sources["memory"] = source;

			ConfigContext options = new ConfigContext(_jsonOptions, sources, NullLogger.Instance);

			BaseMacroScope result = await ConfigType.ReadAsync<BaseMacroScope>(fooUri, options, cancellationToken);
			Assert.AreEqual("This is a macro MacroValue $(MacroName2)", result.Value);
			Assert.AreEqual("This is a macro MacroValue MacroValue2", result.ChildScope!.Value);
		}
	}
}
