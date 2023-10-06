// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using EpicGames.Core;
using Horde.Server.Projects;
using Horde.Server.Server;
using Horde.Server.Streams;
using Horde.Server.Utilities;
using Microsoft.Extensions.Logging;

namespace Horde.Server.Commands.Config
{
	[Command("config", "docs", "Writes Markdown docs for server settings")]
	class DocsCommand : Command
	{
		[CommandLine]
		public DirectoryReference OutputDir { get; set; } = DirectoryReference.Combine(Program.AppDir, "Docs");

		[CommandLine]
		public FileReference? Agent { get; set; }

		class AppSettings
		{
			public ServerSettings Horde { get; set; } = new ServerSettings();
		}

		record class PageInfo(string Title, string FileName);

		public override async Task<int> ExecuteAsync(ILogger logger)
		{
			DirectoryReference.CreateDirectory(OutputDir);

			JsonSchema serverSchema = Schemas.CreateSchema(typeof(AppSettings));
			JsonSchema globalSchema = Schemas.CreateSchema(typeof(GlobalConfig));
			JsonSchema projectSchema = Schemas.CreateSchema(typeof(ProjectConfig));
			JsonSchema streamSchema = Schemas.CreateSchema(typeof(StreamConfig));

			Dictionary<JsonSchemaType, PageInfo> typeToPageInfo = new Dictionary<JsonSchemaType, PageInfo>
			{
				[serverSchema.RootType] = new PageInfo("appsettings.json (Server)", "Config-Schema-Server.md"),
				[globalSchema.RootType] = new PageInfo("Globals.json", "Config-Schema-Globals.md"),
				[projectSchema.RootType] = new PageInfo("*.project.json", "Config-Schema-Projects.md"),
				[streamSchema.RootType] = new PageInfo("*.stream.json", "Config-Schema-Streams.md"),
			};

			if (Agent == null)
			{
				logger.LogWarning("Not generating documentation for agent settings. Specify path to agent assembly with -Agent=...");
			}
			else
			{
				Assembly agentAssembly = Assembly.LoadFile(Agent.FullName);
				Type agentSettingsType = agentAssembly.GetType("Horde.Agent.AgentSettings")!;

				JsonSchema agentSchema = Schemas.CreateSchema(agentSettingsType);
				await WriteDocAsync(agentSchema.RootType, "appsettings.json (Agent)", "Config-Schema-Agent.md", new Dictionary<string, string>());
			}

			Dictionary<string, string> typeNameToPageName = typeToPageInfo.ToDictionary(x => x.Key.Name!, x => x.Value.FileName, StringComparer.Ordinal);
			foreach ((JsonSchemaType type, PageInfo pageInfo) in typeToPageInfo)
			{
				await WriteDocAsync(type, pageInfo.Title, pageInfo.FileName, typeNameToPageName);
			}

			return 0;
		}

		class ObjectQueue
		{
			readonly Stack<JsonSchemaType> _stack = new Stack<JsonSchemaType>();
			readonly HashSet<string> _visited = new HashSet<string>(StringComparer.Ordinal);

			public void Add(JsonSchemaType type)
			{
				if (type.Name != null && _visited.Add(type.Name))
				{
					_stack.Push(type);
				}
			}

			public void Ignore(JsonSchemaType type)
			{
				if (type.Name != null)
				{
					_visited.Add(type.Name);
				}
			}

			public bool TryPop([NotNullWhen(true)] out JsonSchemaType? obj) => _stack.TryPop(out obj);
		}

		async Task WriteDocAsync(JsonSchemaType rootType, string title, string fileName, Dictionary<string, string> typeNameToLink)
		{
			FileReference file = FileReference.Combine(OutputDir, fileName);
			using (FileStream stream = FileReference.Open(file, FileMode.Create, FileAccess.Write))
			{
				using (StreamWriter writer = new StreamWriter(stream))
				{
					await writer.WriteLineAsync($"[Horde](../README.md) > [Configuration](Config.md) > {title}");
					await writer.WriteLineAsync();
					await writer.WriteLineAsync($"# {title}");

					HashSet<string> visitedTypeNames = new HashSet<string>(typeNameToLink.Keys, StringComparer.Ordinal);
					visitedTypeNames.Remove(rootType.Name!);

					List<JsonSchemaType> types = new List<JsonSchemaType>();
					FindCustomTypes(rootType, types, visitedTypeNames);

					foreach (JsonSchemaType schemaType in types)
					{
						if (schemaType != rootType)
						{
							await writer.WriteLineAsync();
							await writer.WriteLineAsync($"## {GetHeadingName(schemaType)}");
						}

						await writer.WriteLineAsync();

						if (schemaType.Description != null)
						{
							string description = Regex.Replace(schemaType.Description, "\r?\n", Environment.NewLine);
							await writer.WriteLineAsync(description);
							await writer.WriteLineAsync();
						}

						if (schemaType is JsonSchemaObject schemaObj)
						{
							await writer.WriteLineAsync("Name | Type | Description");
							await writer.WriteLineAsync("---- | ---- | -----------");

							foreach (JsonSchemaProperty property in schemaObj.Properties)
							{
								string name = property.CamelCaseName;
								string type = GetMarkdownType(property.Type, typeNameToLink);
								string description = GetMarkdownDescription(property.Description);
								await writer.WriteLineAsync($"`{name}` | {type} | {description}");
							}
						}
						else if (schemaType is JsonSchemaEnum schemaEnum)
						{
							await writer.WriteLineAsync("Name | Description");
							await writer.WriteLineAsync("---- | -----------");

							for(int idx = 0; idx < schemaEnum.Values.Count; idx++)
							{
								string name = schemaEnum.Values[idx];
								string description = GetMarkdownDescription(schemaEnum.Descriptions[idx]);
								await writer.WriteLineAsync($"`{name}` | {description}");
							}
						}
						else
						{
							throw new NotImplementedException();
						}
					}
				}
			}
		}

		static void FindCustomTypes(JsonSchemaType type, List<JsonSchemaType> types, HashSet<string> visitedTypeNames)
		{
			switch (type)
			{
				case JsonSchemaOneOf oneOf:
					foreach (JsonSchemaType oneOfType in oneOf.Types)
					{
						FindCustomTypes(oneOfType, types, visitedTypeNames);
					}
					break;
				case JsonSchemaArray array:
					FindCustomTypes(array.ItemType, types, visitedTypeNames);
					break;
				case JsonSchemaEnum _:
					if (type.Name != null && visitedTypeNames.Add(type.Name))
					{
						types.Add(type);
					}
					break;
				case JsonSchemaObject obj:
					if (type.Name != null)
					{
						if (visitedTypeNames.Add(type.Name))
						{
							types.Add(type);
						}
						else
						{
							break;
						}
					}
					if (obj.AdditionalProperties != null)
					{
						FindCustomTypes(obj.AdditionalProperties, types, visitedTypeNames);
					}
					foreach (JsonSchemaProperty property in obj.Properties)
					{
						FindCustomTypes(property.Type, types, visitedTypeNames);
					}
					break;
			}
		}

		static string GetMarkdownType(JsonSchemaType type, Dictionary<string, string> typeNameToLink)
		{
			switch (type)
			{
				case JsonSchemaBoolean _:
					return "`boolean`";
				case JsonSchemaInteger _:
					return "`integer`";
				case JsonSchemaNumber _:
					return "`number`";
				case JsonSchemaString _:
					return "`string`";
				case JsonSchemaOneOf oneOf:
					return String.Join("/", oneOf.Types.Select(x => GetMarkdownType(x, typeNameToLink)));
				case JsonSchemaArray array:
					string elementType = GetMarkdownType(array.ItemType, typeNameToLink);
					if (elementType.EndsWith("`", StringComparison.Ordinal))
					{
						return elementType.Insert(elementType.Length - 1, "[]");
					}
					else
					{
						return elementType + "`[]`";
					}

				case JsonSchemaEnum en:
					if (type.Name == null)
					{
						return String.Join("<br>", en.Values);
					}
					else
					{
						return $"[`{type.Name}`]({GetLink(type, typeNameToLink)})";
					}
				case JsonSchemaObject obj:
					if (obj.AdditionalProperties != null)
					{
						return "`string` `->` " + GetMarkdownType(obj.AdditionalProperties, typeNameToLink);
					}
					else if (type.Name == null)
					{
						return "`object`";
					}
					else
					{
						return $"[`{type.Name}`]({GetLink(type, typeNameToLink)})";
					}
				default:
					return type.GetType().Name;
			}
		}

		static string GetMarkdownDescription(string? description)
		{
			return (description ?? String.Empty).Replace("\n", "<br>", StringComparison.Ordinal);
		}

		static string GetHeadingName(JsonSchemaType type)
		{
			if (type.Name == null)
			{
				throw new NotImplementedException("Unknown type");
			}

			switch (type)
			{
				case JsonSchemaEnum _:
					return $"{type.Name} (Enum)";
				default:
					return type.Name;
			}
		}

		static string GetLink(JsonSchemaType type, Dictionary<string, string> typeNameToLink)
		{
			string? link;
			if (type.Name != null && typeNameToLink.TryGetValue(type.Name, out link))
			{
				return link;
			}
			else
			{
				return GetAnchorName(type);
			}
		}

		[SuppressMessage("Globalization", "CA1308:Normalize strings to uppercase")]
		static string GetAnchorName(JsonSchemaType type)
		{
			string anchor = GetHeadingName(type).ToLowerInvariant();
			anchor = Regex.Replace(anchor, @"[^a-z0-9]+", "-");
			return "#" + anchor.Trim('-');
		}
	}
}
