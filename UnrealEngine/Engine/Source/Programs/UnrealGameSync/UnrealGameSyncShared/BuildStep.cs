// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealGameSync
{
	public enum BuildStepType
	{
		Compile,
		Cook,
		Other,
	}

	[DebuggerDisplay("{Description}")]
	public class BuildStep
	{
		public const string UniqueIdKey = "UniqueId";

		public Guid UniqueId;
		public int OrderIndex;
		public string? Description;
		public string? StatusText;
		public int EstimatedDuration;
		public BuildStepType Type;
		public string? Target;
		public string? Platform;
		public string? Configuration;
		public string? FileName;
		public string? WorkingDir;
		public string? Arguments;
		public bool UseLogWindow;
		public bool NormalSync;
		public bool ScheduledSync;
		public string? StatusPanelLink;
		public Guid[] Requires = Array.Empty<Guid>();
		public Guid ToolId;

		public BuildStep(Guid inUniqueId, int inOrderIndex, string? inDescription, string? inStatusText, int inEstimatedDuration, string? inFileName, string? inArguments, string? inWorkingDir, bool inUseLogWindow)
		{
			UniqueId = inUniqueId;
			OrderIndex = inOrderIndex;
			Description = inDescription;
			StatusText = inStatusText;
			EstimatedDuration = inEstimatedDuration;
			Type = BuildStepType.Other;
			FileName = inFileName;
			Arguments = inArguments;
			WorkingDir = inWorkingDir;
			UseLogWindow = inUseLogWindow;
		}

		public BuildStep(Guid inUniqueId, int inOrderIndex, string? inDescription, string? inStatusText, int inEstimatedDuration, string? inTarget, string? inPlatform, string? inConfiguration, string? inArguments, bool inSyncDefault)
		{
			UniqueId = inUniqueId;
			OrderIndex = inOrderIndex;
			Description = inDescription;
			StatusText = inStatusText;
			EstimatedDuration = inEstimatedDuration;
			Type = BuildStepType.Compile;
			Target = inTarget;
			Platform = inPlatform;
			Configuration = inConfiguration;
			Arguments = inArguments;
			UseLogWindow = true;
			NormalSync = inSyncDefault;
			ScheduledSync = inSyncDefault;
		}

		public BuildStep(ConfigObject obj)
		{
			if(!Guid.TryParse(obj.GetValue(UniqueIdKey, ""), out UniqueId))
			{
				UniqueId = Guid.NewGuid();
			}
			if(!Int32.TryParse(obj.GetValue("OrderIndex", ""), out OrderIndex))
			{
				OrderIndex = -1;
			}

			Description = obj.GetValue("Description", "Untitled");
			StatusText = obj.GetValue("StatusText", "Untitled");

			if(!int.TryParse(obj.GetValue("EstimatedDuration", ""), out EstimatedDuration) || EstimatedDuration < 1)
			{
				EstimatedDuration = 1;
			}

			if(!Enum.TryParse(obj.GetValue("Type", ""), true, out Type))
			{
				Type = BuildStepType.Other;
			}

			Target = obj.GetValue("Target");
			Platform = obj.GetValue("Platform");
			Configuration = obj.GetValue("Configuration");
			FileName = obj.GetValue("FileName");
			WorkingDir = obj.GetValue("WorkingDir");
			Arguments = obj.GetValue("Arguments");

			if(!Boolean.TryParse(obj.GetValue("bUseLogWindow", ""), out UseLogWindow))
			{
				UseLogWindow = true;
			}
			if(!Boolean.TryParse(obj.GetValue("bNormalSync", ""), out NormalSync))
			{
				NormalSync = true;
			}
			if(!Boolean.TryParse(obj.GetValue("bScheduledSync", ""), out ScheduledSync))
			{
				ScheduledSync = NormalSync;
			}

			StatusPanelLink = obj.GetValue("Link", null);
			if (String.IsNullOrEmpty(StatusPanelLink))
			{
				bool showAsTool;
				if (Boolean.TryParse(obj.GetValue("bShowAsTool", ""), out showAsTool) && showAsTool)
				{
					StatusPanelLink = $"More...|{Description}";
				}
			}

			List<Guid> requires = new List<Guid>();
			foreach (string requireString in obj.GetValue("Requires", String.Empty).Split(';', StringSplitOptions.RemoveEmptyEntries))
			{
				if (Guid.TryParse(requireString, out Guid require))
				{
					requires.Add(require);
				}
			}
			this.Requires = requires.ToArray();
	
			if (!Guid.TryParse(obj.GetValue("Tool", ""), out ToolId))
			{
				ToolId = Guid.Empty;
			}
		}

		public bool IsValid()
		{
			switch(Type)
			{
				case BuildStepType.Compile:
					return Target != null && Platform != null && Configuration != null;
				case BuildStepType.Cook:
					return FileName != null;
				case BuildStepType.Other:
					return FileName != null;
				default:
					return false;
			}
		}

		public static void MergeBuildStepObjects(Dictionary<Guid, ConfigObject> buildStepObjects, IEnumerable<ConfigObject> modifyObjects)
		{
			foreach(ConfigObject modifyObject in modifyObjects)
			{
				Guid uniqueId;
				if(Guid.TryParse(modifyObject.GetValue(UniqueIdKey, ""), out uniqueId))
				{
					ConfigObject? defaultObject;
					if(buildStepObjects.TryGetValue(uniqueId, out defaultObject))
					{
						modifyObject.SetDefaults(defaultObject);
					}
					buildStepObjects[uniqueId] = modifyObject;
				}
			}
		}

		public ConfigObject ToConfigObject()
		{
			ConfigObject result = new ConfigObject();
			result["UniqueId"] = UniqueId.ToString();
			result["Description"] = Description;
			result["StatusText"] = StatusText;
			result["EstimatedDuration"] = EstimatedDuration.ToString();
			result["Type"] = Type.ToString();
			switch(Type)
			{
				case BuildStepType.Compile:
					result["Target"] = Target;
					result["Platform"] = Platform;
					result["Configuration"] = Configuration;
					result["Arguments"] = Arguments;
					break;
				case BuildStepType.Cook:
					result["FileName"] = FileName;
					break;
				case BuildStepType.Other:
					result["FileName"] = FileName;
					result["WorkingDir"] = WorkingDir;
					result["Arguments"] = Arguments;
					result["bUseLogWindow"] = UseLogWindow.ToString();
					break;
			}
			result["OrderIndex"] = OrderIndex.ToString();
			result["bNormalSync"] = NormalSync.ToString();
			result["bScheduledSync"] = ScheduledSync.ToString();
			if (!String.IsNullOrEmpty(StatusPanelLink))
			{
				result["Link"] = StatusPanelLink;
			}
			if (ToolId != Guid.Empty)
			{
				result["Tool"] = ToolId.ToString();
			}
			return result;
		}

		public ConfigObject? ToConfigObject(ConfigObject? defaultObject)
		{
			ConfigObject result = new ConfigObject();
			result[UniqueIdKey] = UniqueId.ToString();
			result.AddOverrides(ToConfigObject(), defaultObject);
			return (result.Pairs.Count <= 1)? null : result;
		}
	}
}
