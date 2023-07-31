// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Linq;
using PerfReportTool;
using System.IO;

namespace PerfSummaries
{
	class TableUtil
	{
		public static string FormatStatName(string inStatName)
		{
			// We add a space so stats are split over multiple lines
			return inStatName.Replace("/", "/ ");
		}

		public static string SanitizeHtmlString(string str)
		{
			return str.Replace("<", "&lt;").Replace(">", "&gt;");
		}

		public static string SafeTruncateHtmlTableValue(string inValue, int maxLength)
		{
			if (inValue.StartsWith("<a") && inValue.EndsWith("</a>"))
			{
				// Links require special handling. Only truncate what's inside
				int openAnchorEndIndex = inValue.IndexOf(">");
				int closeAnchorStartIndex = inValue.IndexOf("</a>");
				if (openAnchorEndIndex > 2 && closeAnchorStartIndex > openAnchorEndIndex)
				{
					string anchor = inValue.Substring(0, openAnchorEndIndex + 1);
					string text = inValue.Substring(openAnchorEndIndex + 1, closeAnchorStartIndex - (openAnchorEndIndex + 1));
					if (text.Length > maxLength)
					{
						text = SanitizeHtmlString(text.Substring(0, maxLength)) + "...";
					}
					return anchor + text + "</a>";
				}
			}
			return SanitizeHtmlString(inValue.Substring(0, maxLength)) + "...";
		}
	}

	class SummarySectionBoundaryInfo
	{
		public SummarySectionBoundaryInfo(string inStatName, string inStartToken, string inEndToken, int inLevel, bool inInCollatedTable, bool inInFullTable)
		{
			statName = inStatName;
			startToken = inStartToken;
			endToken = inEndToken;
			level = inLevel;
			inCollatedTable = inInCollatedTable;
			inFullTable = inInFullTable;
		}
		public string statName;
		public string startToken;
		public string endToken;
		public int level;
		public bool inCollatedTable;
		public bool inFullTable;
	};

	class SummaryTableInfo
	{
		public SummaryTableInfo(XElement tableElement, Dictionary<string,string> substitutionsDict)
		{
			XAttribute rowSortAt = tableElement.Attribute("rowSort");
			if (rowSortAt != null)
			{
				rowSortList.AddRange(rowSortAt.Value.Split(','));
				ApplySubstitutionsToList(rowSortList, substitutionsDict);
			}
			XAttribute weightByColumnAt = tableElement.Attribute("weightByColumn");
			if (weightByColumnAt != null)
			{
				weightByColumn = weightByColumnAt.Value.ToLower();
			}

			XElement filterEl = tableElement.Element("filter");
			if (filterEl != null)
			{
				columnFilterList.AddRange(filterEl.Value.Split(','));
				ApplySubstitutionsToList(columnFilterList, substitutionsDict);
			}

			bReverseSortRows = tableElement.GetSafeAttibute<bool>("reverseSortRows", false);
			bScrollableFormatting = tableElement.GetSafeAttibute<bool>("scrollableFormatting", false);

			string colorizeModeStr = tableElement.GetSafeAttibute<string>("colorizeMode", "").ToLower();
			if (colorizeModeStr != "")
			{
				if (colorizeModeStr == "auto")
				{
					tableColorizeMode = TableColorizeMode.Auto;
				}
				else if (colorizeModeStr == "off")
				{
					tableColorizeMode = TableColorizeMode.Off;
				}
				else if (colorizeModeStr == "budget")
				{
					tableColorizeMode = TableColorizeMode.Budget;
				}
			}

			statThreshold = tableElement.GetSafeAttibute<float>("statThreshold", 0.0f);
			hideStatPrefix = tableElement.GetSafeAttibute<string>("hideStatPrefix");

			foreach (XElement sectionBoundaryEl in tableElement.Elements("sectionBoundary"))
			{
				if (sectionBoundaryEl != null)
				{
					string statName = ApplySubstitution(sectionBoundaryEl.GetSafeAttibute<string>("statName"), substitutionsDict);

					SummarySectionBoundaryInfo sectionBoundary = new SummarySectionBoundaryInfo(
						statName,
						sectionBoundaryEl.GetSafeAttibute<string>("startToken"),
						sectionBoundaryEl.GetSafeAttibute<string>("endToken"),
						sectionBoundaryEl.GetSafeAttibute<int>("level", 0),
						sectionBoundaryEl.GetSafeAttibute<bool>("inCollatedTable", true),
						sectionBoundaryEl.GetSafeAttibute<bool>("inFullTable", true)
						);
					sectionBoundaries.Add(sectionBoundary);
				}
			}
		}

		private void ApplySubstitutionsToList(List<string> list, Dictionary<string, string> substitutionsDict)
		{
			if (substitutionsDict == null)
			{
				return;
			}
			for (int i = 0; i < list.Count; i++)
			{
				list[i] = ApplySubstitution(list[i], substitutionsDict);
			}
		}

		private string ApplySubstitution(string str, Dictionary<string, string> substitutionsDict)
		{
			if (substitutionsDict == null)
			{
				return str;
			}
			if (substitutionsDict.TryGetValue(str, out string replaceStr))
			{
				return replaceStr;
			}
			return str;
		}


		public SummaryTableInfo(string filterListStr, string rowSortStr)
		{
			columnFilterList.AddRange(filterListStr.Split(','));
			rowSortList.AddRange(rowSortStr.Split(','));
		}

		public SummaryTableInfo()
		{
		}

		public List<string> rowSortList = new List<string>();
		public List<string> columnFilterList = new List<string>();
		public List<SummarySectionBoundaryInfo> sectionBoundaries = new List<SummarySectionBoundaryInfo>();
		public bool bReverseSortRows;
		public bool bScrollableFormatting;
		public TableColorizeMode tableColorizeMode = TableColorizeMode.Budget;
		public float statThreshold;
		public string hideStatPrefix = null;
		public string weightByColumn = null;
	}


	class SummaryTableColumnFormatInfoCollection
	{
		public SummaryTableColumnFormatInfoCollection(XElement element)
		{
			foreach (XElement child in element.Elements("columnInfo"))
			{
				columnFormatInfoList.Add(new SummaryTableColumnFormatInfo(child));
			}
		}

		public SummaryTableColumnFormatInfo GetFormatInfo(string columnName)
		{
			string lowerColumnName = columnName.ToLower();
			if (lowerColumnName.StartsWith("avg ") || lowerColumnName.StartsWith("min ") || lowerColumnName.StartsWith("max "))
			{
				lowerColumnName = lowerColumnName.Substring(4);
			}
			foreach (SummaryTableColumnFormatInfo columnInfo in columnFormatInfoList)
			{
				int wildcardIndex = columnInfo.name.IndexOf('*');
				if (wildcardIndex == -1)
				{
					if (columnInfo.name == lowerColumnName)
					{
						return columnInfo;
					}
				}
				else
				{
					string prefix = columnInfo.name.Substring(0, wildcardIndex);
					if (lowerColumnName.StartsWith(prefix))
					{
						return columnInfo;
					}
				}
			}
			return defaultColumnInfo;
		}

		public static SummaryTableColumnFormatInfo DefaultColumnInfo
		{
			get { return defaultColumnInfo; }
		}

		List<SummaryTableColumnFormatInfo> columnFormatInfoList = new List<SummaryTableColumnFormatInfo>();
		static SummaryTableColumnFormatInfo defaultColumnInfo = new SummaryTableColumnFormatInfo();
	};

	enum TableColorizeMode
	{
		Off,		// No coloring.
		Budget,     // Colorize based on defined budgets. No coloring for stats without defined budgets.
		Auto,		// Auto calculate based on other values in the column.
	};

	enum AutoColorizeMode
	{
		Off,
		HighIsBad,
		LowIsBad,
	};

	class SummaryTableColumnFormatInfo
	{
		public SummaryTableColumnFormatInfo()
		{
			name = "Default";
			maxStringLength = Int32.MaxValue;
			maxStringLengthCollated = Int32.MaxValue;
		}
		public SummaryTableColumnFormatInfo(XElement element)
		{
			name = element.Attribute("name").Value.ToLower();

			string autoColorizeStr = element.GetSafeAttibute<string>("autoColorize", "highIsBad").ToLower();
			var modeList = Enum.GetValues(typeof(AutoColorizeMode));
			foreach (AutoColorizeMode mode in modeList)
			{
				if (mode.ToString().ToLower() == autoColorizeStr)
				{
					autoColorizeMode = mode;
					break;
				}
			}
			numericFormat = element.GetSafeAttibute<string>("numericFormat");
			maxStringLength = element.GetSafeAttibute<int>("maxStringLength", Int32.MaxValue );
			maxStringLengthCollated = element.GetSafeAttibute<int>("maxStringLengthCollated", Int32.MaxValue );
			if (maxStringLengthCollated == Int32.MaxValue)
			{
				maxStringLengthCollated = maxStringLength;
			}

			includeValueWithBucketName = element.GetSafeAttibute<bool>("includeValueWithBucketName", true);
			string bucketNamesString = element.GetSafeAttibute<string>("valueBucketNames");
			if (bucketNamesString != null)
			{
				bucketNames = bucketNamesString.Split(',').ToList();
			}
			string bucketThresholdsString = element.GetSafeAttibute<string>("valueBucketThresholds");
			if (bucketThresholdsString != null)
			{
				bucketThresholds = bucketThresholdsString.Split(',').Select(valStr =>
				{
					if (float.TryParse(valStr, out float value))
					{
						return value;
					}
					return 0.0f;
				}).ToList();
			}

			colourThresholdList = ColourThresholdList.ReadColourThresholdListXML(element.Element("colourThresholds"));
		}

		public AutoColorizeMode autoColorizeMode = AutoColorizeMode.HighIsBad;
		public string name;
		public string numericFormat;
		public int maxStringLength;
		public int maxStringLengthCollated;
		// If we should display the actual value in parenthesis next to the bucket name (if a bucket exists).
		public bool includeValueWithBucketName = true;
		// The name of each bucket. The name is indexed by the threshold.
		public List<string> bucketNames = new List<string>();
		// The value thresholds that correspond to each bucket. If a name doesn't exist for a threshold, the last bucket name is used.
		public List<float> bucketThresholds = new List<float>();
		// Colour thresholds override for this column.
		public ColourThresholdList colourThresholdList = null;
	};

	class SummaryTableColumn
	{
		public string name;
		public bool isNumeric = false;
		public string displayName;
		public bool isRowWeightColumn = false;
		public bool hasDiffRows = false;
		List<double> doubleValues = new List<double>();
		List<string> stringValues = new List<string>();
		List<string> toolTips = new List<string>();
		public SummaryTableElement.Type elementType;
		public SummaryTableColumnFormatInfo formatInfo = null;

		List<ColourThresholdList> colourThresholds = new List<ColourThresholdList>();
		ColourThresholdList colourThresholdOverride = null;

		public SummaryTableColumn(string inName, bool inIsNumeric, string inDisplayName, bool inIsRowWeightColumn, SummaryTableElement.Type inElementType)
		{
			name = inName;
			isNumeric = inIsNumeric;
			displayName = inDisplayName;
			isRowWeightColumn = inIsRowWeightColumn;
			elementType = inElementType;
		}
		public SummaryTableColumn Clone()
		{
			SummaryTableColumn newColumn = new SummaryTableColumn(name, isNumeric, displayName, isRowWeightColumn, elementType);
			newColumn.doubleValues.AddRange(doubleValues);
			newColumn.stringValues.AddRange(stringValues);
			newColumn.colourThresholds.AddRange(colourThresholds);
			newColumn.toolTips.AddRange(toolTips);
			newColumn.hasDiffRows = hasDiffRows;
			return newColumn;
		}

		private double FilterInvalidValue(double value)
		{
			return value == double.MaxValue ? 0.0 : value;
		}

		public void AddDiffRows(bool bIsFirstColumn=false)
		{
			if (hasDiffRows)
			{
				throw new Exception("Column already has diff rows!");
			}
			// Add a diff row for every row after the first one
			int oldCount = GetCount();
			int diffRowCount = oldCount - 1;
			int newCount = oldCount + diffRowCount;

			// Create new lists with counts reserved
			List<double> newDoubleValues = new List<double>(doubleValues.Count > 0 ? newCount : 0);
			List<string> newStringValues = new List<string>(stringValues.Count > 0 ? newCount : 0);
			List<string> newToolTips = new List<string>(toolTips.Count > 0 ? newCount : 0);
			List<ColourThresholdList> newColourThresholds = new List<ColourThresholdList>(colourThresholds.Count > 0 ? newCount : 0);

			bool bComputeDiff = isNumeric && name != "Count";

			// Add diff rows to each of the arrays
			for (int i = 0; i < doubleValues.Count; i++)
			{
				newDoubleValues.Add(doubleValues[i]);
				if (i > 0)
				{
					if (bComputeDiff)
					{
						double thisValue = FilterInvalidValue(doubleValues[i]);
						double prevValue = FilterInvalidValue(doubleValues[i - 1]);
						newDoubleValues.Add(thisValue - prevValue);
					}
					else
					{
						newDoubleValues.Add(0.0);
					}
				}
			}
			for (int i = 0; i < stringValues.Count; i++)
			{
				newStringValues.Add(stringValues[i]);
				if (i > 0)
				{
					newStringValues.Add(bIsFirstColumn ? "Diff" : "");
				}
			}
			for (int i = 0; i < toolTips.Count; i++)
			{
				newToolTips.Add(toolTips[i]);
				if (i > 0)
				{
					newToolTips.Add("");
				}
			}
			for (int i = 0; i < colourThresholds.Count; i++)
			{
				newColourThresholds.Add(colourThresholds[i]);
				if (i > 0)
				{
					newColourThresholds.Add(null);
				}
			}

			doubleValues = newDoubleValues;
			stringValues = newStringValues;
			toolTips = newToolTips;
			colourThresholds = newColourThresholds;
			hasDiffRows = true;
		}

		public bool IsDiffRow(int rowIndex)
		{
			if (hasDiffRows == false || rowIndex < 2)
			{
				return false;
			}
			return ((rowIndex - 2) % 2) == 0;
		}

		public string GetDisplayName(string hideStatPrefix=null, bool bAddStatCategorySeparatorSpaces = true, bool bGreyOutStatCategories = false)
		{
			if (displayName != null)
			{
				return displayName;
			}
			// Trim the stat name suffix if necessary
			string statName = name;
			if (hideStatPrefix != null)
			{
				string baseStatName = SummaryTable.GetBaseStatNameWithPrefixAndSuffix(statName, out string prefix, out string suffix);
				if (baseStatName.ToLower().StartsWith(hideStatPrefix.ToLower() ) )
				{
					statName = prefix + baseStatName.Substring(hideStatPrefix.Length) + suffix;
				}
			}

			if (elementType == SummaryTableElement.Type.CsvStatAverage )
			{
				if (bGreyOutStatCategories)
				{
					string baseStatName = SummaryTable.GetBaseStatNameWithPrefixAndSuffix(statName, out string prefix, out _);
					int idx = baseStatName.LastIndexOf("/");
					if (idx >= 0)
					{
						statName = prefix + "<span class='greyText'>" + baseStatName.Substring(0, idx+1) + "</span><span class='blackText'>" + baseStatName.Substring(idx+1)+ "</span>";
					}
				}
				if (bAddStatCategorySeparatorSpaces)
				{
					return statName.Replace("/", "/ ");
				}
			}
			return statName;
		}

		public void SetValue(int index, double value)
		{
			if (!isNumeric)
			{
				// This is already a non-numeric column. Better treat this as a string value
				SetStringValue(index, value.ToString());
				return;
			}
			// Grow to fill if necessary
			if (index >= doubleValues.Count)
			{
				for (int i = doubleValues.Count; i <= index; i++)
				{
					doubleValues.Add(double.MaxValue);
				}
			}
			doubleValues[index] = value;
		}

		void convertToStrings()
		{
			if (isNumeric)
			{
				stringValues = new List<string>();
				foreach (float f in doubleValues)
				{
					stringValues.Add(f.ToString());
				}
				doubleValues = new List<double>();
				isNumeric = false;
			}
		}

		public void SetColourThresholds(int index, ColourThresholdList value)
		{
			// Grow to fill if necessary
			if (index >= colourThresholds.Count)
			{
				for (int i = colourThresholds.Count; i <= index; i++)
				{
					colourThresholds.Add(null);
				}
			}
			colourThresholds[index] = value;
		}

		public ColourThresholdList GetColourThresholds(int index)
		{
			if (index < colourThresholds.Count)
			{
				return colourThresholds[index];
			}
			return null;
		}

		public string GetBackgroundColor(int index)
		{
			ColourThresholdList thresholds = null;
			double value = GetValue(index);
			if (value == double.MaxValue || IsDiffRow(index))
			{
				return null;
			}

			if (formatInfo.colourThresholdList != null)
			{
				thresholds = formatInfo.colourThresholdList;
			}
			else if (colourThresholdOverride != null)
			{
				thresholds = colourThresholdOverride;
			}
			else
			{
				if (index < colourThresholds.Count)
				{
					thresholds = colourThresholds[index];
				}
				if (thresholds == null)
				{
					return null;
				}
			}
			return thresholds.GetColourForValue(value);
		}

		public string GetTextColor(int index)
		{
			const double absoluteIgnoreThreshold = 0.025;
			if (hasDiffRows && IsDiffRow(index) && isNumeric && index < doubleValues.Count )
			{
				// For simplicity, just negate the diff value if lowIsBad
				double diffValue = doubleValues[index];
				if (formatInfo.autoColorizeMode == AutoColorizeMode.LowIsBad)
				{
					diffValue *= -1.0;
				}

				// Diff absolute value is insignificant: output faded color
				if (Math.Abs(diffValue) < absoluteIgnoreThreshold)
				{
					// Very close to zero: just output grey
					if (Math.Abs(diffValue) < 0.001f)
					{
						return "#A0A0A0";
					}
					// Slight red/green
					return diffValue > 0.0 ? "#B8A0A0" : "#A0B8A0";
				}

				double prevValue = FilterInvalidValue(doubleValues[index - 2]);
				double thisValue = FilterInvalidValue(doubleValues[index - 1]);

				double maxValue = Math.Max(thisValue, prevValue);
				double percentOfMax = 100.0 * diffValue / maxValue;
				string red = "#B00000";
				string green = "#008000";
				// More than half a percent of max: output full colours
				if (percentOfMax >= 0.5)
				{
					return red;
				}
				if (percentOfMax <= -0.5)
				{
					return green;
				}
				// Output faded red/green
				return diffValue > 0.0 ? "#B8A0A0" : "#A0B8A0";
			}
			return null;
		}


		public void ComputeColorThresholds(TableColorizeMode tableColorizeMode)
		{
			if ( tableColorizeMode == TableColorizeMode.Budget )
			{
				return;
			}
			if (tableColorizeMode == TableColorizeMode.Off)
			{
				// Set empty color thresholds. This clears existing thresholds from summaries
				colourThresholds = new List<ColourThresholdList>();
				return;
			}

			AutoColorizeMode autoColorizeMode = formatInfo.autoColorizeMode;
			if (autoColorizeMode == AutoColorizeMode.Off || !isNumeric)
			{
				return;
			}

			// Set a single colour threshold list for the whole column
			colourThresholds = new List<ColourThresholdList>();
			double maxValue = -double.MaxValue;
			double minValue = double.MaxValue;
			double totalValue = 0.0f;
			double validCount = 0.0f;
			for (int i = 0; i < doubleValues.Count; i++)
			{
				if (IsDiffRow(i))
				{
					continue;
				}
				double val = doubleValues[i];
				if (val != double.MaxValue)
				{
					maxValue = Math.Max(val, maxValue);
					minValue = Math.Min(val, minValue);
					totalValue += val;
					validCount += 1.0f;
				}
			}
			if (minValue == maxValue)
			{
				return;
			}

			Colour green = new Colour(0.4f, 0.82f, 0.45f);
			Colour yellow = new Colour(1.0f, 1.0f, 0.5f);
			Colour red = new Colour(1.0f, 0.4f, 0.4f);

			double averageValue = totalValue / validCount; // TODO: Weighted average 
			colourThresholdOverride = new ColourThresholdList();
			colourThresholdOverride.Add(new ThresholdInfo(minValue, (autoColorizeMode == AutoColorizeMode.HighIsBad) ? green : red));
			colourThresholdOverride.Add(new ThresholdInfo(averageValue, yellow));
			colourThresholdOverride.Add(new ThresholdInfo(averageValue, yellow));
			colourThresholdOverride.Add(new ThresholdInfo(maxValue, (autoColorizeMode == AutoColorizeMode.HighIsBad) ? red : green));
		}

		public int GetCount()
		{
			return Math.Max(doubleValues.Count, stringValues.Count);
		}
		public double GetValue(int index)
		{
			if (index >= doubleValues.Count)
			{
				return double.MaxValue;
			}
			return doubleValues[index];
		}

		public bool AreAllValuesOverThreshold(double threshold)
		{
			if (!isNumeric)
			{
				return true;
			}
			foreach(double value in doubleValues)
			{
				if ( value > threshold && value != double.MaxValue)
				{
					return true;
				}
			}
			return false;
		}

		public void SetStringValue(int index, string value)
		{
			if (isNumeric)
			{
				// Better convert this to a string column, since we're trying to add a string to it
				convertToStrings();
			}
			// Grow to fill if necessary
			if (index >= stringValues.Count)
			{
				for (int i = stringValues.Count; i <= index; i++)
				{
					stringValues.Add("");
				}
			}
			stringValues[index] = value;
			isNumeric = false;
		}
		public string GetStringValue(int index, bool roundNumericValues = false, string forceNumericFormat = null)
		{
			if (isNumeric)
			{
				if (index >= doubleValues.Count || doubleValues[index] == double.MaxValue)
				{
					return "";
				}
				double val = doubleValues[index];

				string prefix = "";
				bool bIsDiffRow = hasDiffRows && IsDiffRow(index);
				if (bIsDiffRow)
				{
					if ( val == 0.0 )
					{
						return "";
					}
					if ( val > 0.0 )
					{
						prefix = "+";
					}
				}

				if (forceNumericFormat != null)
				{
					return prefix + val.ToString(forceNumericFormat);
				}
				else if (roundNumericValues)
				{
					double absVal = Math.Abs(val);
					double frac = absVal - (double)Math.Truncate(absVal);
					if (absVal >= 250.0f || frac < 0.0001f)
					{
						return prefix+val.ToString("0");
					}
					if (absVal >= 50.0f)
					{
						return prefix + val.ToString("0.0");
					}
					if (absVal >= 0.1)
					{
						return prefix + val.ToString("0.00");
					}
					if (bIsDiffRow)
					{
						// Filter out close to zero results in diff columns
						if (absVal < 0.000)
						{
							return "";
						}
						return prefix + val.ToString("0.00");
					}
					else
					{
						return val.ToString("0.000");
					}
				}
				return prefix + val.ToString();
			}
			else
			{
				if (index >= stringValues.Count)
				{
					return "";
				}
				if (forceNumericFormat != null)
				{
					// We're forcing a numeric format on something that's technically a string, but since we were asked, we'll try to do it anyway 
					// Note: this is not ideal, but it's useful for collated table columns, which might get converted to non-numeric during collation
					try
					{
						return Convert.ToDouble(stringValues[index], System.Globalization.CultureInfo.InvariantCulture).ToString(forceNumericFormat);
					}
					catch { } // Ignore. Just fall through...
				}
				return stringValues[index];
			}
		}
		public void SetToolTipValue(int index, string value)
		{
			// Grow to fill if necessary
			if (index >= toolTips.Count)
			{
				for (int i = toolTips.Count; i <= index; i++)
				{
					toolTips.Add("");
				}
			}
			toolTips[index] = value;
		}
		public string GetToolTipValue(int index)
		{
			if (index >= toolTips.Count)
			{
				return "";
			}
			return toolTips[index];
		}

	};



	class SummaryTable
	{
		public SummaryTable()
		{
		}

		public SummaryTable CollateSortedTable(List<string> collateByList, bool addMinMaxColumns)
		{
			int numSubColumns = addMinMaxColumns ? 3 : 1;

			// Find all the columns in collateByList
			HashSet<SummaryTableColumn> collateByColumns = new HashSet<SummaryTableColumn>();
			foreach (string collateBy in collateByList)
			{
				string key = collateBy.ToLower();
				if (columnLookup.ContainsKey(key))
				{
					collateByColumns.Add(columnLookup[key]);
				}
			}
			if (collateByColumns.Count == 0)
			{
				throw new Exception("None of the metadata strings were found:" + string.Join(", ", collateByList));
			}

			// Add the new collateBy columns in the order they appear in the original column list
			List<SummaryTableColumn> newColumns = new List<SummaryTableColumn>();
			List<string> finalSortByList = new List<string>();
			foreach (SummaryTableColumn srcColumn in columns)
			{
				if (collateByColumns.Contains(srcColumn))
				{
					newColumns.Add(new SummaryTableColumn(srcColumn.name, false, srcColumn.displayName, false, srcColumn.elementType));
					finalSortByList.Add(srcColumn.name.ToLower());
					// Early out if we've found all the columns
					if (finalSortByList.Count == collateByColumns.Count)
					{
						break;
					}
				}
			}

			newColumns.Add(new SummaryTableColumn("Count", true, null, false, SummaryTableElement.Type.ToolMetadata));
			int countColumnIndex = newColumns.Count - 1;

			int numericColumnStartIndex = newColumns.Count;
			List<int> srcToDestBaseColumnIndex = new List<int>();
			foreach (SummaryTableColumn column in columns)
			{
				// Add avg/min/max columns for this column if it's numeric and we didn't already add it above 
				if (column.isNumeric && !collateByColumns.Contains(column))
				{
					srcToDestBaseColumnIndex.Add(newColumns.Count);
					newColumns.Add(new SummaryTableColumn("Avg " + column.name, true, null, false, column.elementType));
					if (addMinMaxColumns)
					{
						newColumns.Add(new SummaryTableColumn("Min " + column.name, true, null, false, column.elementType));
						newColumns.Add(new SummaryTableColumn("Max " + column.name, true, null, false, column.elementType));
					}
				}
				else
				{
					srcToDestBaseColumnIndex.Add(-1);
				}
			}

			List<double> RowMaxValues = new List<double>();
			List<double> RowTotals = new List<double>();
			List<double> RowMinValues = new List<double>();
			List<int> RowCounts = new List<int>();
			List<double> RowWeights = new List<double>();
			List<ColourThresholdList> RowColourThresholds = new List<ColourThresholdList>();

			// Set the initial sort key
			string CurrentRowSortKey = "";
			foreach (string collateBy in finalSortByList)
			{
				CurrentRowSortKey += "{" + columnLookup[collateBy].GetStringValue(0) + "}";
			}

			int destRowIndex = 0;
			bool reset = true;
			int mergedRowsCount = 0;
			for (int i = 0; i < rowCount; i++)
			{
				if (reset)
				{
					RowMaxValues.Clear();
					RowMinValues.Clear();
					RowTotals.Clear();
					RowCounts.Clear();
					RowWeights.Clear();
					RowColourThresholds.Clear();
					for (int j = 0; j < columns.Count; j++)
					{
						if (addMinMaxColumns)
						{
							RowMaxValues.Add(-double.MaxValue);
							RowMinValues.Add(double.MaxValue);
						}
						RowTotals.Add(0.0f);
						RowCounts.Add(0);
						RowWeights.Add(0.0);
						RowColourThresholds.Add(null);
					}
					mergedRowsCount = 0;
					reset = false;
				}

				// Compute min/max/total for all numeric columns
				for (int j = 0; j < columns.Count; j++)
				{
					SummaryTableColumn column = columns[j];
					if (column.isNumeric)
					{
						double value = column.GetValue(i);
						if (value != double.MaxValue)
						{
							if (addMinMaxColumns)
							{
								RowMaxValues[j] = Math.Max(RowMaxValues[j], value);
								RowMinValues[j] = Math.Min(RowMinValues[j], value);
							}
							RowColourThresholds[j] = column.GetColourThresholds(i);
							RowCounts[j]++;
							double rowWeight = (rowWeightings != null) ? rowWeightings[i] : 1.0;
							RowWeights[j] += rowWeight;
							RowTotals[j] += value * rowWeight;
						}
					}
				}
				mergedRowsCount++;

				// Are we done?
				string nextSortKey = "";
				if (i < rowCount - 1)
				{
					foreach (string collateBy in finalSortByList)
					{
						nextSortKey += "{" + columnLookup[collateBy].GetStringValue(i + 1) + "}";
					}
				}

				// If this is the last row or if the sort key is different then write it out
				if (nextSortKey != CurrentRowSortKey)
				{
					for (int j = 0; j < countColumnIndex; j++)
					{
						string key = newColumns[j].name.ToLower();
						newColumns[j].SetStringValue(destRowIndex, columnLookup[key].GetStringValue(i));
					}
					// Commit the row 
					newColumns[countColumnIndex].SetValue(destRowIndex, (double)mergedRowsCount);
					for (int j = 0; j < columns.Count; j++)
					{
						int destColumnBaseIndex = srcToDestBaseColumnIndex[j];
						if (destColumnBaseIndex != -1 && RowCounts[j] > 0)
						{
							newColumns[destColumnBaseIndex].SetValue(destRowIndex, RowTotals[j] / RowWeights[j]);
							if (addMinMaxColumns)
							{
								newColumns[destColumnBaseIndex + 1].SetValue(destRowIndex, RowMinValues[j]);
								newColumns[destColumnBaseIndex + 2].SetValue(destRowIndex, RowMaxValues[j]);
							}

							// Set colour thresholds based on the source column
							ColourThresholdList Thresholds = RowColourThresholds[j];
							for (int k = 0; k < numSubColumns; k++)
							{
								newColumns[destColumnBaseIndex + k].SetColourThresholds(destRowIndex, Thresholds);
							}
						}
					}
					reset = true;
					destRowIndex++;
				}
				CurrentRowSortKey = nextSortKey;
			}

			SummaryTable newTable = new SummaryTable();
			newTable.columns = newColumns;
			newTable.InitColumnLookup();
			newTable.rowCount = destRowIndex;
			newTable.firstStatColumnIndex = numericColumnStartIndex;
			newTable.isCollated = true;
			newTable.hasMinMaxColumns = addMinMaxColumns;
			return newTable;
		}

		public SummaryTable SortAndFilter(string customFilter, string customRowSort = "buildversion,deviceprofile", bool bReverseSort = false, string weightByColumnName = null)
		{
			return SortAndFilter(customFilter.Split(',').ToList(), customRowSort.Split(',').ToList(), bReverseSort, weightByColumnName);
		}

		public SummaryTable SortAndFilter(List<string> columnFilterList, List<string> rowSortList, bool bReverseSort, string weightByColumnName, float statThreshold = 0.0f)
		{
			SummaryTable newTable = SortRows(rowSortList, bReverseSort);

			// Make a list of all unique keys
			List<string> allMetadataKeys = new List<string>();
			Dictionary<string, SummaryTableColumn> nameLookup = new Dictionary<string, SummaryTableColumn>();
			foreach (SummaryTableColumn col in newTable.columns)
			{
				string key = col.name.ToLower();
				if (!nameLookup.ContainsKey(key))
				{
					nameLookup.Add(key, col);
					allMetadataKeys.Add(key);
				}
			}
			allMetadataKeys.Sort();

			// Generate the list of requested metadata keys that this table includes
			List<string> orderedKeysWithDupes = new List<string>();

			// Add metadata keys from the column filter list in the order they appear
			foreach (string filterStr in columnFilterList)
			{
				string filterStrLower = filterStr.Trim().ToLower();
				bool startWild = filterStrLower.StartsWith("*");
				bool endWild = filterStrLower.EndsWith("*");
				filterStrLower = filterStrLower.Trim('*');
				if (startWild && endWild)
                {
					orderedKeysWithDupes.AddRange(allMetadataKeys.Where(x => x.Contains(filterStrLower)));
                }
				else if(startWild)
				{
					orderedKeysWithDupes.AddRange(allMetadataKeys.Where(x => x.EndsWith(filterStrLower)));
				}
				else if(endWild)
				{
					// Linear search through the sorted key list
					bool bFound = false;
					for (int wildcardSearchIndex = 0; wildcardSearchIndex < allMetadataKeys.Count; wildcardSearchIndex++)
					{
						if (allMetadataKeys[wildcardSearchIndex].StartsWith(filterStrLower))
						{
							orderedKeysWithDupes.Add(allMetadataKeys[wildcardSearchIndex]);
							bFound = true;
						}
						else if (bFound)
						{
							// Early exit: already found one key. If the pattern no longer matches then we must be done
							break;
						}
					}
				}
				else
				{
					string key = filterStrLower;
					orderedKeysWithDupes.Add(key);
				}
			}

			// Compute row weights
			if (weightByColumnName != null && nameLookup.ContainsKey(weightByColumnName))
			{
				SummaryTableColumn rowWeightColumn = nameLookup[weightByColumnName];
				newTable.rowWeightings = new List<double>(rowWeightColumn.GetCount());
				for (int i = 0; i < rowWeightColumn.GetCount(); i++)
				{
					newTable.rowWeightings.Add(rowWeightColumn.GetValue(i));
				}
			}

			List<SummaryTableColumn> newColumnList = new List<SummaryTableColumn>();
			// Add all the ordered keys that exist, ignoring duplicates
			foreach (string key in orderedKeysWithDupes)
			{
				if (nameLookup.ContainsKey(key))
				{
					newColumnList.Add(nameLookup[key]);
					// Remove from the list so it doesn't get counted again
					nameLookup.Remove(key);
				}
			}


			// Filter out csv stat or metric columns below the specified threshold
			if (statThreshold > 0.0f)
			{
				List<SummaryTableColumn> oldColumnList = newColumnList;
				newColumnList = new List<SummaryTableColumn>();
				foreach (SummaryTableColumn column in oldColumnList)
				{
					if (!column.isNumeric || 
						( column.elementType != SummaryTableElement.Type.CsvStatAverage && column.elementType != SummaryTableElement.Type.SummaryTableMetric ) || 
						column.AreAllValuesOverThreshold((double)statThreshold))
					{
						newColumnList.Add(column);
					}
				}
			}

			newTable.columns = newColumnList;
			newTable.rowCount = rowCount;
			newTable.InitColumnLookup();

			return newTable;
		}

		public void AddDiffRows()
		{
			for (int i=0; i<columns.Count; i++)
			{
				columns[i].AddDiffRows(i == 0);
			}
			rowCount += rowCount - 1;

			// Just set rowWeightings to null for now. We shouldn't need it, since we should have already collated by this point
			rowWeightings = null;
		}

		public void ApplyDisplayNameMapping(Dictionary<string, string> statDisplaynameMapping)
		{
			// Convert to a display-friendly name
			foreach (SummaryTableColumn column in columns)
			{
				if (statDisplaynameMapping != null && column.displayName == null)
				{
					string name = column.name;
					string statName = GetBaseStatNameWithPrefixAndSuffix(name, out string prefix, out string suffix);
					if (statDisplaynameMapping.ContainsKey(statName.ToLower()))
					{
						column.displayName = prefix + statDisplaynameMapping[statName.ToLower()] + suffix;
					}
				}
			}
		}
		public static string GetBaseStatName(string inName)
		{
			return GetBaseStatNameWithPrefixAndSuffix(inName, out _, out _);
		}

		public static string GetBaseStatNameWithPrefixAndSuffix(string inName, out string prefix, out string suffix)
		{
			suffix = "";
			prefix = "";
			string statName = inName;
			if (inName.StartsWith("Avg ") || inName.StartsWith("Max ") || inName.StartsWith("Min "))
			{
				prefix = inName.Substring(0, 4);
				statName = inName.Substring(4);
			}
			if (statName.EndsWith(" Avg") || statName.EndsWith(" Max") || statName.EndsWith(" Min"))
			{
				suffix = statName.Substring(statName.Length - 4);
				statName = statName.Substring(0, statName.Length - 4);
			}
			return statName;
		}

		public void WriteToCSV(string csvFilename)
		{
			System.IO.StreamWriter csvFile = new System.IO.StreamWriter(csvFilename, false);
			List<string> headerRow = new List<string>();
			foreach (SummaryTableColumn column in columns)
			{
				headerRow.Add(column.name);
			}
			csvFile.WriteLine(string.Join(",", headerRow));

			for (int i = 0; i < rowCount; i++)
			{
				List<string> rowStrings = new List<string>();
				foreach (SummaryTableColumn column in columns)
				{
					string cell = column.GetStringValue(i, false);
					// Sanitize so it opens in a spreadsheet (e.g. for buildversion) 
					cell = cell.TrimStart('+');
					rowStrings.Add(cell);
				}
				csvFile.WriteLine(string.Join(",", rowStrings));
			}
			csvFile.Close();
		}


		public void WriteToHTML(
			string htmlFilename, 
			string VersionString, 
			bool bSpreadsheetFriendlyStrings, 
			List<SummarySectionBoundaryInfo> sectionBoundaries, 
			bool bScrollableTable, 
			TableColorizeMode tableColorizeMode,
			bool bAddMinMaxColumns, 
			string hideStatPrefix,
			int maxColumnStringLength, 
			SummaryTableColumnFormatInfoCollection columnFormatInfoList, 
			string weightByColumnName, 
			string title, 
			bool bTranspose )
		{
			System.IO.StreamWriter htmlFile = new System.IO.StreamWriter(htmlFilename, false);
			int statColSpan = hasMinMaxColumns ? 3 : 1;
			int cellPadding = 2;
			if (isCollated)
			{
				cellPadding = 4;
			}

			// Generate an automatic title
			if (title==null)
			{
				title = htmlFilename.Replace("_Email.html", "").Replace(".html", "").Replace("\\", "/");
				title = title.Substring(title.LastIndexOf('/') + 1);
			}

			htmlFile.WriteLine("<html>");
			htmlFile.WriteLine("<head><title>Perf Summary: "+ title + "</title>");

			bool bAddStatNameSpacing = !bTranspose;
			bool bGreyOutStatPrefixes = bScrollableTable && bTranspose;

			// Figure out the sticky column count
			int stickyColumnCount = 0;
			if (bScrollableTable)
			{
				stickyColumnCount = 1;
				if (isCollated && !bTranspose)
				{
					for (int i = 0; i < columns.Count; i++)
					{
						if (columns[i].name == "Count")
						{
							stickyColumnCount = i + 1;
							break;
						}
					}
				}
			}

			// Get format info for the columns
			foreach (SummaryTableColumn column in columns)
			{
				column.formatInfo = (columnFormatInfoList != null) ? columnFormatInfoList.GetFormatInfo(column.name) : SummaryTableColumnFormatInfoCollection.DefaultColumnInfo;
			}

			// Automatically colorize the table if requested.
			// We run this even if colorize is off as we need to overwrite the values.
			if (tableColorizeMode == TableColorizeMode.Auto ||
				tableColorizeMode == TableColorizeMode.Off)
			{
				foreach (SummaryTableColumn column in columns)
				{
					column.ComputeColorThresholds(tableColorizeMode);
				}
			}

			if (bScrollableTable)
			{
				// Insert some javascript to make the columns sticky. It's not possible to do this for multiple columns with pure CSS, since you need to compute the X offset dynamically
				// We need to do this when the page is loaded or the window is resized
				htmlFile.WriteLine("<script>");

				htmlFile.WriteLine(
					"var originalStyleElement = null; \n" +
					"document.addEventListener('DOMContentLoaded', function(event) { regenerateStickyColumnCss(); }) \n" +
					"window.addEventListener('resize', function(event) { regenerateStickyColumnCss(); }) \n" +
					"\n" +
					"function regenerateStickyColumnCss() { \n" +
					"  var styleElement=document.getElementById('pageStyle'); \n" +
					"  var table=document.getElementById('mainTable'); \n" +
					"  if ( table.rows.length < 2 ) \n" +
					"	return; \n" +
					"  if (originalStyleElement == null) \n" +
					"    originalStyleElement = styleElement.textContent; \n" +
					"  else \n" +
					"    styleElement.textContent = originalStyleElement  \n"
				);

				// Make the columns Sticky and compute their X offsets
				htmlFile.WriteLine(
					"  var numStickyCols=" + stickyColumnCount + "; \n" +
					"  var xOffset=0; \n" +
					"  for (var i=0;i<numStickyCols;i++) \n" +
					"  { \n" +
					"	 var rBorderParam=(i==numStickyCols-1) ? 'border-right: 2px solid black;':''; \n" +
					"	 styleElement.textContent+='tr.lastHeaderRow th:nth-child('+(i+1)+') {  z-index: 8;  border-top: 2px solid black;  font-size: 11px;  left: '+xOffset+'px;'+rBorderParam+'}'; \n" +
					"	 styleElement.textContent+='td:nth-child('+(i+1)+') {  position: -webkit-sticky;  position: sticky; z-index: 7;  left: '+xOffset+'px; '+rBorderParam+'}'; \n" +
					"	 xOffset+=table.rows[1].cells[i].offsetWidth; \n" +
					"  } \n"+
					"} \n"
					);
				htmlFile.WriteLine("</script>");
			}

			htmlFile.WriteLine("<style type='text/css' id='pageStyle'>");
			htmlFile.WriteLine("p {  font-family: 'Verdana', Times, serif; font-size: 12px }");
			htmlFile.WriteLine("h3 {  font-family: 'Verdana', Times, serif; font-size: 14px }");
			htmlFile.WriteLine("h2 {  font-family: 'Verdana', Times, serif; font-size: 16px }");
			htmlFile.WriteLine("h1 {  font-family: 'Verdana', Times, serif; font-size: 20px }");
			string tableCss = "";


			if (bScrollableTable)
			{
				int headerMinWidth = bTranspose ? 50 : 75;
				int headerMaxWidth = bTranspose ? 165 : 220;
				int cellFontSize = bTranspose ? 12 : 10;
				int headerCellFontSize = bTranspose ? 10 : 9;
				int firstColVerticalPadding = bTranspose ? 5 : 0;
				string cellAlign = bTranspose ? "right" : "left";

				if (bAddMinMaxColumns)
				{
					headerMinWidth = 35;
				}

				tableCss =
					"table {table-layout: fixed;} \n" +
					"table, th, td { border: 0px solid black; border-spacing: 0; border-collapse: separate; padding: " + cellPadding + "px; vertical-align: center; font-family: 'Verdana', Times, serif; font-size: " + cellFontSize + "px;} \n" +
					"td {" +
					"  border-right: 1px solid black;" +
					"  max-width: 450;" +
					"} \n" +
					"td:first-child {" +
					"  padding-right:10px; padding-left:5px;" +
					"  padding-top:" + firstColVerticalPadding + "px;padding-bottom:" + firstColVerticalPadding + "px;" +
					"} \n" +
					"td:not(:first-child) {" +
					"  text-align: " + cellAlign + ";" +
					"} \n" +
					"tr:first-element { border-top: 2px; border-bottom: 2px } \n" +
					"th {" +
					"  width: auto;" +
					"  max-width: " + headerMaxWidth + "px;" +
					"  min-width: " + headerMinWidth + "px;" +
					"  position: -webkit-sticky;" +
					"  position: sticky;" +
					"  border-right: 1px solid black;" +
					"  border-top: 2px solid black;" +
					"  z-index: 5;" +
					"  background-color: #ffffff;" +
					"  top:0;" +
					"  font-size: " + headerCellFontSize + "px;" +
					"  word-wrap: break-word;" +
					"  overflow: hidden;" +
					"  height: 60;" +
					"} \n" +
					"span.greyText {" +
					"  color: #808080;" +
					"  display: inline-block;"+
					"} \n"+
					"span.blackText {" +
					"  color: #000000;" +
					"  display: inline-block;" +
					"} \n";

				// Top-left cell of the table is always on top, big font, thick border
				tableCss += "tr:first-child th:first-child { z-index: 100;  border-right: 2px solid black; border-top: 2px solid black; font-size: 11px; top:0; left: 0px; } \n";

				tableCss += "th:first-child, td:first-child { border-left: 2px solid black; white-space: nowrap; max-width:800px;} \n";

				if (bAddMinMaxColumns && isCollated)
				{
					tableCss += "tr.lastHeaderRow th { top:60px; height:20px; } \n";
				}

				if (!isCollated)
				{
					tableCss += "td { max-height: 40px; height:40px } \n";
				}
				tableCss += "tr:last-child td{border-bottom: 2px solid black;} \n";

			}
			else
			{
				tableCss =
					"table, th, td { border: 2px solid black; border-collapse: collapse; padding: " + cellPadding + "px; vertical-align: center; font-family: 'Verdana', Times, serif; font-size: 11px;} \n";
			}


			bool bOddRowsGray = !(!bAddMinMaxColumns || !isCollated);

			string oddColor = bOddRowsGray ? "#eaeaea" : "#ffffff";
			string evenColor = bOddRowsGray ? "#ffffff" : "#eaeaea";

			tableCss += "tr:nth-child(odd) {background-color: "+oddColor+";} \n";
			tableCss += "tr:nth-child(even) {background-color: "+evenColor+";} \n";

			tableCss += "tr:first-child {background-color: #ffffff;} \n";
			tableCss += "tr.lastHeaderRow th { border-bottom: 2px solid black; } \n";

			// Section start row styles
			tableCss += "tr.sectionStartLevel0 td { border-top: 2px solid black; } \n";
			tableCss += "tr.sectionStartLevel1 td { border-top: 1px solid black; } \n";
			tableCss += "tr.sectionStartLevel2 td { border-top: 1px dashed black; } \n";

			htmlFile.WriteLine(tableCss);

			htmlFile.WriteLine("</style>");
			htmlFile.WriteLine("</head><body>");


			HtmlTable htmlTable = new HtmlTable("id='mainTable'", 1, columns.Count);

			// Make a header row, but don't add it to the table yet
			HtmlTable.Row headerRow = new HtmlTable.Row();

			// Write the header
			if (isCollated)
			{
				HtmlTable.Row topHeaderRow = headerRow;

				// Add the special columns (up to Count) to the lower header row
				for (int i = 0; i < firstStatColumnIndex; i++)
				{
					headerRow.AddCell(columns[i].GetDisplayName(hideStatPrefix, bAddStatNameSpacing, bGreyOutStatPrefixes));
				}

				if (bAddMinMaxColumns)
				{
					topHeaderRow = htmlTable.CreateRow();
					htmlTable.numHeaderRows = 2;
					if (bScrollableTable)
					{
						topHeaderRow.AddCell("<h3>" + title + "</h3>", "colspan='" + firstStatColumnIndex + "'");
					}
					else
					{
						topHeaderRow.AddCell("", "colspan='" + firstStatColumnIndex + "'");
					}
					// Add the stat columns
					for (int i = firstStatColumnIndex; i < columns.Count; i++)
					{
						string statName = GetBaseStatNameWithPrefixAndSuffix(columns[i].GetDisplayName(hideStatPrefix, bAddStatNameSpacing, bGreyOutStatPrefixes), out string prefix, out string suffix);
						if ((i - 1) % statColSpan == 0)
						{
							topHeaderRow.AddCell(statName + suffix, "colspan='" + statColSpan + "'");
						}
						headerRow.AddCell(prefix.Trim());
					}
				}
				else
				{
					// Add the stat columns
					for (int i = firstStatColumnIndex; i < columns.Count; i++)
					{
						string statName = GetBaseStatNameWithPrefixAndSuffix(columns[i].GetDisplayName(hideStatPrefix, bAddStatNameSpacing, bGreyOutStatPrefixes), out _, out string suffix);
						headerRow.AddCell(statName + suffix);
					}
				}
			}
			else
			{
				foreach (SummaryTableColumn column in columns)
				{
					headerRow.AddCell(column.GetDisplayName(hideStatPrefix, bAddStatNameSpacing, bGreyOutStatPrefixes));
				}
			}
			htmlTable.AddRow(headerRow);

			// Work out which rows are major/minor section boundaries
			Dictionary<int, int> rowSectionBoundaryLevel = new Dictionary<int, int>();
			if (sectionBoundaries != null && !bTranspose)
			{
				foreach (SummarySectionBoundaryInfo sectionBoundaryInfo in sectionBoundaries)
				{
					// Skip this section boundary info if it's not in this table type
					if (isCollated && !sectionBoundaryInfo.inCollatedTable)
					{
						continue;
					}
					if (!isCollated && !sectionBoundaryInfo.inFullTable)
					{
						continue;
					}
					string prevSectionName = "";
					for (int i = 0; i < rowCount; i++)
					{
						int boundaryLevel = 0;
						if (sectionBoundaryInfo != null)
						{
							// Work out the section name if we have section boundary info. When it changes, apply the sectionStart CSS class
							string sectionName = "";
							if (sectionBoundaryInfo != null && columnLookup.ContainsKey(sectionBoundaryInfo.statName))
							{
								// Get the section name
								if (!columnLookup.ContainsKey(sectionBoundaryInfo.statName))
								{
									continue;
								}
								SummaryTableColumn col = columnLookup[sectionBoundaryInfo.statName];
								sectionName = col.GetStringValue(i);

								// if we have a start token then strip before it
								if (sectionBoundaryInfo.startToken != null)
								{
									int startTokenIndex = sectionName.IndexOf(sectionBoundaryInfo.startToken);
									if (startTokenIndex != -1)
									{
										sectionName = sectionName.Substring(startTokenIndex + sectionBoundaryInfo.startToken.Length);
									}
								}

								// if we have an end token then strip after it
								if (sectionBoundaryInfo.endToken != null)
								{
									int endTokenIndex = sectionName.IndexOf(sectionBoundaryInfo.endToken);
									if (endTokenIndex != -1)
									{
										sectionName = sectionName.Substring(0, endTokenIndex);
									}
								}
							}
							if (sectionName != prevSectionName && i > 0)
							{
								// Update the row's boundary type info
								boundaryLevel = sectionBoundaryInfo.level;
								if (rowSectionBoundaryLevel.ContainsKey(i))
								{
									// Lower level values override higher ones
									boundaryLevel = Math.Min(rowSectionBoundaryLevel[i], boundaryLevel);
								}
								rowSectionBoundaryLevel[i] = boundaryLevel;
							}
							prevSectionName = sectionName;
						}
					}
				}
			}

			// Add the rows to the table
			for (int rowIndex = 0; rowIndex < rowCount; rowIndex++)
			{
				string rowClassStr = "";

				// Is this a major/minor section boundary
				if (rowSectionBoundaryLevel.ContainsKey(rowIndex))
				{
					int sectionLevel = rowSectionBoundaryLevel[rowIndex];
					if (sectionLevel < 3)
					{
						rowClassStr = " class='sectionStartLevel" + sectionLevel + "'";
					}
				}

				HtmlTable.Row currentRow = htmlTable.CreateRow(rowClassStr);
				int columnIndex = 0;
				foreach (SummaryTableColumn column in columns)
				{
					List<string> attributes = new List<string>();

					// Add the tooltip for non-collated tables
					if (!isCollated)
					{
						string toolTip = column.GetToolTipValue(rowIndex);
						if (toolTip == "")
						{
							toolTip = column.GetDisplayName();
						}
						attributes.Add("title='" + toolTip + "'");
					}
					string bgColour = column.GetBackgroundColor(rowIndex);

					if (bgColour != null)
					{
						attributes.Add("bgcolor=" + bgColour);
					}
					string textColour = column.GetTextColor(rowIndex);
					if (textColour != null)
					{
						attributes.Add("style='color:" + textColour + "'");
					}

					bool bold = false;
					SummaryTableColumnFormatInfo columnFormat = column.formatInfo;
					int maxStringLength = Math.Min( isCollated ? columnFormat.maxStringLengthCollated : columnFormat.maxStringLength, maxColumnStringLength);

					string numericFormat = columnFormat.numericFormat;
					string stringValue = column.GetStringValue(rowIndex, true, numericFormat);

					// Check if we have any value buckets, if so lookup the bucket name for the value and display that with the value.
					if (column.isNumeric && stringValue.Length > 0 && columnFormat.bucketNames.Count > 0 && columnFormat.bucketThresholds.Count > 0)
					{
						double value = column.GetValue(rowIndex);
						int bucketIndex = 0;
						for (bucketIndex = 0; bucketIndex < columnFormat.bucketThresholds.Count; ++bucketIndex)
						{
							if (value <= columnFormat.bucketThresholds[bucketIndex])
							{
								break;
							}
						}

						bucketIndex = Math.Min(bucketIndex, columnFormat.bucketNames.Count-1);
						if (columnFormat.includeValueWithBucketName)
						{
							stringValue = columnFormat.bucketNames[bucketIndex] + " (" + stringValue + ")";
						}
						else
						{
							stringValue = columnFormat.bucketNames[bucketIndex];
						}
					}

					if (stringValue.Length > maxStringLength)
					{
						stringValue = TableUtil.SafeTruncateHtmlTableValue(stringValue, maxStringLength);
					}
					if (bSpreadsheetFriendlyStrings && !column.isNumeric)
					{
						stringValue = "'" + stringValue;
					}
					currentRow.AddCell( (bold ? "<b>" : "") + stringValue + (bold ? "</b>" : ""), attributes );
					columnIndex++;
				}
			}

			if (bTranspose)
			{
				htmlTable = htmlTable.Transpose();
				htmlTable.Set(0, 0, new HtmlTable.Cell(title, ""));

				// Add a section boundary where the stats start
				htmlTable.rows[htmlTable.numHeaderRows + firstStatColumnIndex - 1].attributes = " class='sectionStartLevel2'";
			}

			// Apply final formatting
			htmlTable.rows[htmlTable.numHeaderRows-1].attributes += " class='lastHeaderRow'";

			if (bScrollableTable)
			{
				// Apply stripe colors to the sticky columns to make them opaque (these need to render on top).	Can't use per-cell CSS because we don't want to override existing cell colors
				string[] stripeColors = { "bgcolor='"+oddColor+"'", "bgcolor='"+evenColor+"'" };
				for (int rowIndex = htmlTable.numHeaderRows; rowIndex < htmlTable.rows.Count; rowIndex++)
				{
					HtmlTable.Row row = htmlTable.rows[rowIndex];
					for (int colIndex = 0; colIndex<Math.Min(row.cells.Count,stickyColumnCount); colIndex++ )
					{
						HtmlTable.Cell cell = row.cells[colIndex];
						if (cell != null && !cell.attributes.Contains("bgcolor="))
						{
							cell.attributes += " " + stripeColors[rowIndex % 2];
						}
					}
				}
			}

			htmlTable.WriteToHtml(htmlFile);

			string extraString = "";
			if (isCollated && weightByColumnName != null)
			{
				extraString += " - weighted avg";
			}

			htmlFile.WriteLine("<p style='font-size:8'>Created with PerfReportTool " + VersionString + extraString + "</p>");
			htmlFile.WriteLine("</font></body></html>");

			htmlFile.Close();
		}

		public SummaryTable SortRows(List<string> rowSortList, bool reverseSort)
		{
			List<KeyValuePair<string, int>> columnRemapping = new List<KeyValuePair<string, int>>();
			for (int i = 0; i < rowCount; i++)
			{
				string key = "";
				foreach (string s in rowSortList)
				{
					if (columnLookup.ContainsKey(s.ToLower()))
					{
						SummaryTableColumn column = columnLookup[s.ToLower()];
						key += "{" + column.GetStringValue(i,false,"0.0000000000") + "}";
					}
					else
					{
						key += "{}";
					}
				}
				columnRemapping.Add(new KeyValuePair<string, int>(key, i));
			}

			columnRemapping.Sort(delegate (KeyValuePair<string, int> m1, KeyValuePair<string, int> m2)
			{
				return m1.Key.CompareTo(m2.Key);
			});

			// Reorder the metadata rows
			List<SummaryTableColumn> newColumns = new List<SummaryTableColumn>();
			foreach (SummaryTableColumn srcCol in columns)
			{
				SummaryTableColumn destCol = new SummaryTableColumn(srcCol.name, srcCol.isNumeric, null, false, srcCol.elementType);
				for (int i = 0; i < rowCount; i++)
				{
					int srcIndex = columnRemapping[i].Value;
					int destIndex = reverseSort ? rowCount - 1 - i : i;
					if (srcCol.isNumeric)
					{
						destCol.SetValue(destIndex, srcCol.GetValue(srcIndex));
					}
					else
					{
						destCol.SetStringValue(destIndex, srcCol.GetStringValue(srcIndex));
					}
					destCol.SetColourThresholds(destIndex, srcCol.GetColourThresholds(srcIndex));
					destCol.SetToolTipValue(destIndex, srcCol.GetToolTipValue(srcIndex));
				}
				newColumns.Add(destCol);
			}
			SummaryTable newTable = new SummaryTable();
			newTable.columns = newColumns;
			newTable.rowCount = rowCount;
			newTable.firstStatColumnIndex = firstStatColumnIndex;
			newTable.isCollated = isCollated;
			newTable.InitColumnLookup();
			return newTable;
		}

		void InitColumnLookup()
		{
			columnLookup.Clear();
			foreach (SummaryTableColumn col in columns)
			{
				columnLookup.Add(col.name.ToLower(), col);
			}
		}

		public void AddRowData(SummaryTableRowData metadata, bool bIncludeCsvStatAverages, bool bIncludeHiddenStats)
		{
			foreach (string key in metadata.dict.Keys)
			{
				SummaryTableElement value = metadata.dict[key];
				if (value.type == SummaryTableElement.Type.CsvStatAverage && !bIncludeCsvStatAverages)
				{
					continue;
				}
				if (value.GetFlag(SummaryTableElement.Flags.Hidden) && !bIncludeHiddenStats)
				{
					continue;
				}
				SummaryTableColumn column = null;

				if (!columnLookup.ContainsKey(key))
				{
					column = new SummaryTableColumn(value.name, value.isNumeric, null, false, value.type);
					columnLookup.Add(key, column);
					columns.Add(column);
				}
				else
				{
					column = columnLookup[key];
				}

				if (value.isNumeric)
				{
					column.SetValue(rowCount, (double)value.numericValue);
				}
				else
				{
					column.SetStringValue(rowCount, value.value);
				}
				column.SetColourThresholds(rowCount, value.colorThresholdList);
				column.SetToolTipValue(rowCount, value.tooltip);
			}
			rowCount++;
		}

		public int Count
		{
			get { return rowCount; }
		}

		Dictionary<string, SummaryTableColumn> columnLookup = new Dictionary<string, SummaryTableColumn>();
		List<SummaryTableColumn> columns = new List<SummaryTableColumn>();
		List<double> rowWeightings = null;
		int rowCount = 0;
		int firstStatColumnIndex = 0;
		bool isCollated = false;
		bool hasMinMaxColumns = false;
	};

	class HtmlTable
	{
		public class Row
		{
			public Row(string inAttributes="", int reserveColumnCount=0)
			{
				attributes = inAttributes;
				if (reserveColumnCount > 0)
				{
					cells = new List<Cell>(reserveColumnCount);
				}
				else
				{
					cells = new List<Cell>();
				}
			}

			public void AddCell(string contents="", string attributes="")
			{
				cells.Add(new Cell(contents, attributes));
			}

			public void AddCell(string contents, List<string> attributes)
			{
				cells.Add(new Cell(contents, String.Join(" ",attributes)));
			}


			public void WriteToHtml(System.IO.StreamWriter htmlFile, bool bIsHeaderRow )
			{
				string attributesStr = attributes.Length == 0 ? "" : " " + attributes;
				htmlFile.Write("<tr"+ attributesStr + ">");
				foreach (Cell cell in cells)
				{
					if (cell == null)
					{
						htmlFile.Write("<td/>");
					}
					else
					{
						cell.WriteToHtml(htmlFile, bIsHeaderRow);
					}
				}
				htmlFile.WriteLine("</tr>");
			}

			public void Set(int ColIndex, Cell cell)
			{
				if (cells.Count < ColIndex + 1)
				{
					cells.Capacity = ColIndex + 1;
					while (cells.Count < ColIndex + 1)
					{
						cells.Add(null);
					}
				}
				cells[ColIndex] = cell;
			}

			public string attributes;
			public List<Cell> cells;
		}

		public class Cell
		{
			public Cell()
			{
			}
			public Cell(Cell srcCell)
			{
				contents = srcCell.contents;
				attributes = srcCell.attributes;
			}

			public Cell(string inContents, string inAttributes, bool bInIsHeader=false)
			{
				contents = inContents;
				attributes = inAttributes;
			}

			public void WriteToHtml(System.IO.StreamWriter htmlFile, bool bIsHeaderRow)
			{
				string AttributesStr = attributes.Length == 0 ? "" : " "+ attributes;
				string CellType = bIsHeaderRow ? "th" : "td";
				htmlFile.Write("<" + CellType + AttributesStr + ">" + contents + "</" + CellType + ">");
			}

			public string contents;
			public string attributes;
		}

		public HtmlTable(string inAttributes="", int inNumHeaderRows=1, int reserveColumnCount=0)
		{
			rows = new List<Row>();
			attributes = inAttributes;
			numHeaderRows = inNumHeaderRows;
			reserveColumnCount = 0;
		}

		public HtmlTable Transpose()
		{
			// NOTE: Row attributes are stripped when transposing!
			HtmlTable transposedTable = new HtmlTable(attributes, 1, rows.Count);
			for (int i=0; i<rows.Count; i++)
			{
				Row row = rows[i];
				for (int j=0; j< row.cells.Count; j++)
				{
					Cell newCell = new Cell(row.cells[j]);
					transposedTable.Set(j, i, newCell);
				}
			}
			return transposedTable;
		}

		public void WriteToHtml(System.IO.StreamWriter htmlFile)
		{
			string attributesStr = attributes.Length == 0 ? "" : " " + attributes;
			htmlFile.WriteLine("<table"+attributesStr+">");

			// Compute the number of columns, just in case not all rows are equal
			for (int i=0; i<rows.Count; i++)
			{
				rows[i].WriteToHtml(htmlFile, i<numHeaderRows);
			}
			htmlFile.WriteLine("</table>");
		}

		public void Set(int RowIndex, int ColIndex, Cell cell)
		{
			// Make sure we have enough rows
			if (rows.Count < RowIndex + 1)
			{
				rows.Capacity = RowIndex + 1;
				while (rows.Count < RowIndex + 1)
				{
					rows.Add(new Row("", reserveColumnCount));
				}
			}
			// Make sure the row is big enough
			rows[RowIndex].Set(ColIndex, cell);
		}

		public Row CreateRow(string attributes="")
		{
			Row newRow = new Row(attributes, reserveColumnCount);
			rows.Add(newRow);
			return newRow; 
		}
		public void AddRow(Row row)
		{
			rows.Add(row);
		}

		public List<Row> rows;
		public string attributes;
		public int numHeaderRows = 0;
		int reserveColumnCount = 0;
	};

}