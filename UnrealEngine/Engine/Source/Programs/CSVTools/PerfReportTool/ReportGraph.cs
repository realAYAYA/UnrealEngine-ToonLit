// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Xml.Linq;

namespace PerfReportTool
{

	class OptionalString
	{
		public OptionalString(string valueIn)
		{
			value = valueIn;
			isSet = true;
		}
		public OptionalString()
		{
			isSet = false;
		}
		public OptionalString(XElement element, string Name, bool IsElement = false)
		{
			isSet = false;
			if (IsElement)
			{
				XElement child = element.Element(Name);
				if (child != null)
				{
					value = child.Value;
					isSet = true;
				}
			}
			else
			{
				XAttribute child = element.Attribute(Name);
				if (child != null)
				{
					value = child.Value;
					isSet = true;
				}
			}
		}

		public void InheritFrom(OptionalString baseVersion) { if (!isSet) { isSet = baseVersion.isSet; value = baseVersion.value; } }
		public bool isSet;
		public string value;
	};

	class OptionalBool
	{
		public OptionalBool(bool valueIn)
		{
			value = valueIn;
			isSet = true;
		}
		public OptionalBool()
		{
			isSet = false;
		}
		public OptionalBool(XElement element, string AttributeName)
		{
			isSet = false;
			try
			{
				if (element.Attribute(AttributeName) != null)
				{
					value = Convert.ToInt32(element.Attribute(AttributeName).Value) == 1;
					isSet = true;
				}
			}
			catch { }
		}
		public void InheritFrom(OptionalBool baseVersion) { if (!isSet) { isSet = baseVersion.isSet; value = baseVersion.value; } }

		public bool isSet;
		public bool value;
	};

	class OptionalInt
	{
		public OptionalInt(int valueIn)
		{
			value = valueIn;
			isSet = true;
		}
		public OptionalInt()
		{
			isSet = false;
		}
		public OptionalInt(XElement element, string AttributeName)
		{
			isSet = false;
			try
			{
				if (element.Attribute(AttributeName) != null)
				{
					value = Convert.ToInt32(element.Attribute(AttributeName).Value);
					isSet = true;
				}
			}
			catch { }
		}
		public void InheritFrom(OptionalInt baseVersion) { if (!isSet) { isSet = baseVersion.isSet; value = baseVersion.value; } }

		public bool isSet;
		public int value;
	};

	class OptionalDouble
	{
		public OptionalDouble(int valueIn)
		{
			value = valueIn;
			isSet = true;
		}
		public OptionalDouble()
		{
			isSet = false;
		}
		public OptionalDouble(XElement element, string AttributeName)
		{
			isSet = false;
			try
			{
				if (element.Attribute(AttributeName) != null)
				{
					value = Convert.ToDouble(element.Attribute(AttributeName).Value, System.Globalization.CultureInfo.InvariantCulture);
					isSet = true;
				}
			}
			catch { }
		}

		public void InheritFrom(OptionalDouble baseVersion) { if (!isSet) { isSet = baseVersion.isSet; value = baseVersion.value; } }

		public bool isSet;
		public double value;
	};

	static class OptionalHelper
	{
		public static string GetDoubleSetting(OptionalDouble setting, string cmdline)
		{
			return (setting.isSet ? (cmdline + setting.value.ToString()) : "");
		}

		public static string GetStringSetting(OptionalString setting, string cmdline)
		{
			return (setting.isSet ? (cmdline + setting.value) : "");
		}
	};

	class ReportGraph
	{
		public ReportGraph(XElement element)
		{
			title = element.Attribute("title").Value;
			budget = new OptionalDouble(element, "budget");
			inSummary = element.GetSafeAttibute<bool>("inSummary", false);
			isExternal = element.GetSafeAttibute<bool>("external", false);

			minFilterStatValue = new OptionalDouble(element, "minFilterStatValue");
		}
		public string title;
		public OptionalDouble budget;
		public bool inSummary;
		public bool isExternal;
		public OptionalDouble minFilterStatValue;
		public GraphSettings settings;
	};

	class GraphSettings
	{
		public GraphSettings(XElement element)
		{
			smooth = new OptionalBool(element, "smooth");
			thickness = new OptionalDouble(element, "thickness");
			miny = new OptionalDouble(element, "miny");
			maxy = new OptionalDouble(element, "maxy");
			threshold = new OptionalDouble(element, "threshold");
			averageThreshold = new OptionalDouble(element, "averageThreshold");
			minFilterStatValue = new OptionalDouble(element, "minFilterStatValue");
			minFilterStatName = new OptionalString(element, "minFilterStatName");
			smoothKernelPercent = new OptionalDouble(element, "smoothKernelPercent");
			smoothKernelSize = new OptionalDouble(element, "smoothKernelSize");
			compression = new OptionalDouble(element, "compression");
			width = new OptionalInt(element, "width");
			height = new OptionalInt(element, "height");
			stacked = new OptionalBool(element, "stacked");
			showAverages = new OptionalBool(element, "showAverages");
			filterOutZeros = new OptionalBool(element, "filterOutZeros");
			maxHierarchyDepth = new OptionalInt(element, "maxHierarchyDepth");
			hideStatPrefix = new OptionalString(element, "hideStatPrefix");
			mainStat = new OptionalString(element, "mainStat");
			showEvents = new OptionalString(element, "showEvents");
			requiresDetailedStats = new OptionalBool(element, "requiresDetailedStats");
			ignoreStats = new OptionalString(element, "ignoreStats");

			statString = new OptionalString(element, "statString", true);
			//additionalArgs = new OptionalString(element, "additionalArgs", true);
			statMultiplier = new OptionalDouble(element, "statMultiplier");
			legendAverageThreshold = new OptionalDouble(element, "legendAverageThreshold");
			snapToPeaks = new OptionalBool(element, "snapToPeaks");
			lineDecimalPlaces = new OptionalInt(element, "lineDecimalPlaces");
		}
		public void InheritFrom(GraphSettings baseSettings)
		{
			smooth.InheritFrom(baseSettings.smooth);
			statString.InheritFrom(baseSettings.statString);
			thickness.InheritFrom(baseSettings.thickness);
			miny.InheritFrom(baseSettings.miny);
			maxy.InheritFrom(baseSettings.maxy);
			threshold.InheritFrom(baseSettings.threshold);
			averageThreshold.InheritFrom(baseSettings.averageThreshold);
			minFilterStatValue.InheritFrom(baseSettings.minFilterStatValue);
			minFilterStatName.InheritFrom(baseSettings.minFilterStatName);
			smoothKernelSize.InheritFrom(baseSettings.smoothKernelSize);
			smoothKernelPercent.InheritFrom(baseSettings.smoothKernelPercent);
			compression.InheritFrom(baseSettings.compression);
			width.InheritFrom(baseSettings.width);
			height.InheritFrom(baseSettings.height);
			//additionalArgs.InheritFrom(baseSettings.additionalArgs);
			stacked.InheritFrom(baseSettings.stacked);
			showAverages.InheritFrom(baseSettings.showAverages);
			filterOutZeros.InheritFrom(baseSettings.filterOutZeros);
			maxHierarchyDepth.InheritFrom(baseSettings.maxHierarchyDepth);
			hideStatPrefix.InheritFrom(baseSettings.hideStatPrefix);
			mainStat.InheritFrom(baseSettings.mainStat);
			showEvents.InheritFrom(baseSettings.showEvents);
			requiresDetailedStats.InheritFrom(baseSettings.requiresDetailedStats);
			statMultiplier.InheritFrom(baseSettings.statMultiplier);
			ignoreStats.InheritFrom(baseSettings.ignoreStats);
			legendAverageThreshold.InheritFrom(baseSettings.legendAverageThreshold);
			snapToPeaks.InheritFrom(baseSettings.snapToPeaks);
			lineDecimalPlaces.InheritFrom(baseSettings.lineDecimalPlaces);

		}
		public OptionalBool smooth;
		public OptionalString statString;
		public OptionalDouble thickness;
		public OptionalDouble miny;
		public OptionalDouble maxy;
		public OptionalDouble threshold;
		public OptionalDouble averageThreshold;
		public OptionalDouble minFilterStatValue;
		public OptionalString minFilterStatName;
		public OptionalDouble smoothKernelSize;
		public OptionalDouble smoothKernelPercent;
		public OptionalDouble compression;
		public OptionalInt width;
		public OptionalInt height;
		//public OptionalString additionalArgs;
		public OptionalBool stacked;
		public OptionalBool showAverages;
		public OptionalBool filterOutZeros;
		public OptionalInt maxHierarchyDepth;
		public OptionalString hideStatPrefix;
		public OptionalString mainStat;
		public OptionalString showEvents;
		public OptionalString ignoreStats;
		public OptionalDouble statMultiplier;
		public OptionalDouble legendAverageThreshold;

		public OptionalBool requiresDetailedStats;
		public OptionalBool snapToPeaks;
		public OptionalInt lineDecimalPlaces;

	};

}