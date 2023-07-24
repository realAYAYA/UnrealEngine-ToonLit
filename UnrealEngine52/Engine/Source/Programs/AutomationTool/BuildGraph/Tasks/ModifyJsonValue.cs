// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using EpicGames.BuildGraph;
using System.Collections.Generic;
using System.Linq;
using System.Xml;
using System.IO;
using System.Text;
using EpicGames.Core;
using UnrealBuildBase;
using System.Text.Json;
using System.Threading.Tasks;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a ModifyJsonValue task
	/// </summary>
	public class ModifyJsonValueParameters
	{
		/// <summary>
		/// json file paths which will be modified
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.FileSpec)]
		public string Files;

		/// <summary>
		/// json key path to find in each file
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.Default)]
		public string KeyPath;

		/// <summary>
		/// new value to apply
		/// </summary>
		[TaskParameter(ValidationType = TaskParameterValidationType.Default)]
		public int NewValue;
	}

	/// <summary>
	/// Modifies json files by setting a value specified in the key path
	/// </summary>
	[TaskElement("ModifyJsonValue", typeof(ModifyJsonValueParameters))]
	public class ModifyJsonValue : BgTaskImpl
	{
		ModifyJsonValueParameters Parameters;

		/// <summary>
		/// Create a new ModifyJsonValue.
		/// </summary>
		/// <param name="InParameters">Parameters for this task.</param>
		public ModifyJsonValue(ModifyJsonValueParameters InParameters)
		{
			Parameters = InParameters;
		}

		/// <summary>
		/// Placeholder comment
		/// </summary>
		public override async Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			string[] Keys = Parameters.KeyPath.Split('.');
			if (Keys.Length == 0)
            {
				return;
            }
			HashSet<FileReference> Files = ResolveFilespec(Unreal.RootDirectory, Parameters.Files, TagNameToFileSet);
			foreach (var JsonFile in Files.Select(f => f.FullName))
			{
				var OldContents = File.ReadAllText(JsonFile);
				var ParamObj = fastJSON.JSON.Instance.Parse(OldContents) as IDictionary<string, object>;
				var CurrObj = ParamObj;
				for (int i = 0; i < Keys.Length - 1; i++)
				{
					if (!CurrObj.TryGetValue(Keys[i], out object NextNode))
						CurrObj[Keys[i]] = NextNode = new Dictionary<string, object>();
					CurrObj = (IDictionary<string, object>)NextNode;
				}

				CurrObj[Keys[Keys.Length - 1]] = Parameters.NewValue;

				var NewContents = JsonSerializer.Serialize(ParamObj, new JsonSerializerOptions { WriteIndented = true });
				await File.WriteAllTextAsync(JsonFile, NewContents, new UTF8Encoding(encoderShouldEmitUTF8Identifier: false));
			}
		}

		/// <summary>
		/// Placeholder comment
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Placeholder comment
		/// </summary>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			foreach (string TagName in FindTagNamesFromFilespec(Parameters.Files))
			{
				yield return TagName;
			}
		}

		/// <summary>
		/// Placeholder comment
		/// </summary>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}
	}
}
