// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Xml.Linq;
using CSVStats;
using PerfSummaries;

namespace PerfReportTool
{
	class CsvEventStripInfo
	{
		public string beginName;
		public string endName;
	};

	class DerivedMetadataEntry
	{
		public DerivedMetadataEntry(string inSourceName, string inSourceValue, string inDestName, string inDestValue)
		{
			sourceName = inSourceName;
			sourceValue = inSourceValue;
			destName = inDestName;
			destValue = inDestValue;
		}
		public string sourceName;
		public string sourceValue;
		public string destName;
		public string destValue;
	};

	class DerivedMetadataMappings
	{
		public DerivedMetadataMappings()
		{
			entries = new List<DerivedMetadataEntry>();
		}
		public void ApplyMapping(CsvMetadata csvMetadata)
		{
			if (csvMetadata != null)
			{
				foreach (DerivedMetadataEntry entry in entries)
				{
					if (csvMetadata.Values.ContainsKey(entry.sourceName.ToLowerInvariant()))
					{
						if (csvMetadata.Values[entry.sourceName].ToLowerInvariant() == entry.sourceValue.ToLowerInvariant())
						{
							csvMetadata.Values.Add(entry.destName.ToLowerInvariant(), entry.destValue);
						}
					}
				}
			}
		}
		public List<DerivedMetadataEntry> entries;
	}

	class ReportXML
	{
		bool IsAbsolutePath(string path)
		{
			if (path.Length > 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
			{
				return true;
			}
			return false;
		}

		public ReportXML(string graphXMLFilenameIn, string reportXMLFilenameIn, string baseXMLDirectoryOverride, string additionalSummaryTableXmlFilename, string summaryTableXmlSubstStr)
		{
			
			string location = System.Reflection.Assembly.GetEntryAssembly().Location.ToLower();
			string baseDirectory = location.Replace("perfreporttool.exe", "");

			// Check if this is a debug build, and redirect base dir to binaries if so
			if (baseDirectory.Contains("\\engine\\source\\programs\\") && baseDirectory.Contains("\\csvtools\\") && baseDirectory.Contains("\\bin\\debug\\"))
			{
				baseDirectory = baseDirectory.Replace("\\engine\\source\\programs\\", "\\engine\\binaries\\dotnet\\");
				int csvToolsIndex = baseDirectory.LastIndexOf("\\csvtools\\");
				baseDirectory = baseDirectory.Substring(0, csvToolsIndex + "\\csvtools\\".Length);
			}

			// Check if the base directory is being overridden
			if (baseXMLDirectoryOverride.Length > 0)
			{
				baseDirectory = IsAbsolutePath(baseXMLDirectoryOverride) ? baseXMLDirectoryOverride : Path.Combine(baseDirectory, baseXMLDirectoryOverride);
			}
			Console.Out.WriteLine("BaseDir:   " + baseDirectory);

			baseXmlDirectory = baseDirectory;

			// Read the report type XML
			reportTypeXmlFilename = Path.Combine(baseDirectory, "reportTypes.xml");
			if (reportXMLFilenameIn.Length > 0)
			{
				reportTypeXmlFilename = IsAbsolutePath(reportXMLFilenameIn) ? reportXMLFilenameIn : Path.Combine(baseDirectory, reportXMLFilenameIn);
			}
			Console.Out.WriteLine("ReportXML: " + reportTypeXmlFilename);
			XDocument reportTypesDoc = XDocument.Load(reportTypeXmlFilename);
			rootElement = reportTypesDoc.Element("root");
			if (rootElement == null)
			{
				throw new Exception("No root element found in report XML " + reportTypeXmlFilename);
			}

			// Read the additional summary table XML (if supplied)
			if (additionalSummaryTableXmlFilename.Length > 0)
			{
				if (!IsAbsolutePath(additionalSummaryTableXmlFilename))
				{
					additionalSummaryTableXmlFilename = Path.Combine(baseDirectory, additionalSummaryTableXmlFilename);
				}
				XDocument summaryXmlDoc = XDocument.Load(additionalSummaryTableXmlFilename);
				XElement summaryTablesEl = summaryXmlDoc.Element("summaryTables");
				if (summaryTablesEl == null)
				{
					throw new Exception("No summaryTables element found in summaryTableXML file: " + additionalSummaryTableXmlFilename);
				}

				XElement destSummaryTablesEl = rootElement.Element("summaryTables");
				if (destSummaryTablesEl == null)
				{
					rootElement.Add(summaryTablesEl);
				}
				else
				{
					foreach (XElement child in summaryTablesEl.Elements())
					{
						destSummaryTablesEl.Add(child);
					}
				}
			}
			Console.Out.WriteLine("ReportXML: " + reportTypeXmlFilename);

			reportTypesElement = rootElement.Element("reporttypes");
			if (reportTypesElement == null)
			{
				throw new Exception("No reporttypes element found in report XML " + reportTypeXmlFilename);
			}

			// Read the graph XML
			string graphsXMLFilename;
			if (graphXMLFilenameIn.Length > 0)
			{
				graphsXMLFilename = IsAbsolutePath(graphXMLFilenameIn) ? graphXMLFilenameIn : Path.Combine(baseDirectory, graphXMLFilenameIn);
			}
			else
			{
				graphsXMLFilename = reportTypesElement.GetSafeAttibute<string>("reportGraphsFile");
				if (graphsXMLFilename != null)
				{
					graphsXMLFilename = Path.GetDirectoryName(reportTypeXmlFilename) + "\\" + graphsXMLFilename;
				}
				else
				{
					graphsXMLFilename = Path.Combine(baseDirectory, "reportGraphs.xml");
				}

			}

			Console.Out.WriteLine("GraphXML:  " + graphsXMLFilename+"\n");
			XDocument reportGraphsDoc = XDocument.Load(graphsXMLFilename);
			graphGroupsElement = reportGraphsDoc.Element("graphGroups");

			// Read the base settings - all other settings will inherit from this
			GraphSettings baseSettings = new GraphSettings(graphGroupsElement.Element("baseSettings"));
			if (reportTypesElement == null)
			{
				throw new Exception("No baseSettings element found in graph XML " + graphsXMLFilename);
			}

			graphs = new Dictionary<string, GraphSettings>();
			foreach (XElement graphGroupElement in graphGroupsElement.Elements())
			{
				if (graphGroupElement.Name == "graphGroup")
				{
					// Create the base settings
					XElement settingsElement = graphGroupElement.Element("baseSettings");
					GraphSettings groupSettings = new GraphSettings(settingsElement);
					groupSettings.InheritFrom(baseSettings);
					foreach (XElement graphElement in graphGroupElement.Elements())
					{
						if (graphElement.Name == "graph")
						{
							string title = graphElement.Attribute("title").Value.ToLower();
							GraphSettings graphSettings = new GraphSettings(graphElement);
							graphSettings.InheritFrom(groupSettings);
							graphs.Add(title, graphSettings);
						}
					}
				}
			}



			// Read the display name mapping
			statDisplayNameMapping = new Dictionary<string, string>();
			XElement displayNameElement = rootElement.Element("statDisplayNameMappings");
			if (displayNameElement != null)
			{
				foreach (XElement mapping in displayNameElement.Elements("mapping"))
				{
					string statName = mapping.GetSafeAttibute<string>("statName");
					string displayName = mapping.GetSafeAttibute<string>("displayName");
					if (statName != null && displayName != null)
					{
						statDisplayNameMapping.Add(statName.ToLower(), displayName);
					}
				}
			}

			XElement summaryTableColumnInfoListEl = rootElement.Element("summaryTableColumnFormatInfo");
			if (summaryTableColumnInfoListEl != null)
			{
				columnFormatInfoList = new SummaryTableColumnFormatInfoCollection(summaryTableColumnInfoListEl);
			}

			// Read the derived metadata mappings
			derivedMetadataMappings = new DerivedMetadataMappings();
			XElement derivedMetadataMappingsElement = rootElement.Element("derivedMetadataMappings");
			if (derivedMetadataMappingsElement != null)
			{
				foreach (XElement mapping in derivedMetadataMappingsElement.Elements("mapping"))
				{
					string sourceName = mapping.GetSafeAttibute<string>("sourceName");
					string sourceValue = mapping.GetSafeAttibute<string>("sourceValue");
					string destName = mapping.GetSafeAttibute<string>("destName");
					string destValue = mapping.GetSafeAttibute<string>("destValue");
					if (sourceName == null || sourceValue == null || destName == null || destValue == null)
					{
						throw new Exception("Derivedmetadata mapping is missing a required attribute!\nRequired attributes: sourceName, sourceValue, destName, destValue.\nXML: " + mapping.ToString());
					}
					derivedMetadataMappings.entries.Add(new DerivedMetadataEntry(sourceName, sourceValue, destName, destValue));
				}
			}

			// Read events to strip
			XElement eventsToStripEl = rootElement.Element("csvEventsToStrip");
			if (eventsToStripEl != null)
			{
				csvEventsToStrip = new List<CsvEventStripInfo>();
				foreach (XElement eventPair in eventsToStripEl.Elements("eventPair"))
				{
					CsvEventStripInfo eventInfo = new CsvEventStripInfo();
					eventInfo.beginName = eventPair.GetSafeAttibute<string>("begin");
					eventInfo.endName = eventPair.GetSafeAttibute<string>("end");

					if (eventInfo.beginName == null && eventInfo.endName == null)
					{
						throw new Exception("eventPair with no begin or end attribute found! Need to have one or the other.");
					}
					csvEventsToStrip.Add(eventInfo);
				}
			}

			summaryTablesElement = rootElement.Element("summaryTables");
			if (summaryTablesElement != null)
			{
				// Read the substitutions
				Dictionary<string, string> substitutionsDict = null;
				string[] substitutions = summaryTableXmlSubstStr.Split(',');
				if (substitutions.Length>0)
				{
					substitutionsDict = new Dictionary<string, string>();
					foreach (string substStr in substitutions)
					{
						string [] pair = substStr.Split('=');
						if (pair.Length == 2)
						{
							substitutionsDict[pair[0]] = pair[1];
						}
					}
				}


				summaryTables = new Dictionary<string, SummaryTableInfo>();
				foreach (XElement summaryElement in summaryTablesElement.Elements("summaryTable"))
				{
					SummaryTableInfo table = new SummaryTableInfo(summaryElement, substitutionsDict);
					summaryTables.Add(summaryElement.Attribute("name").Value.ToLower(), table);
				}
			}

			// Add any shared summaries
			XElement sharedSummariesElement = rootElement.Element("sharedSummaries");
			sharedSummaries = new Dictionary<string, XElement>();
			if (sharedSummariesElement != null)
			{
				foreach (XElement summaryElement in sharedSummariesElement.Elements("summary"))
				{
					sharedSummaries.Add(summaryElement.Attribute("refName").Value, summaryElement);
				}
			}

		}

		public ReportTypeInfo GetReportTypeInfo(string reportType, CachedCsvFile csvFile, bool bBulkMode, bool forceReportType)
		{
			ReportTypeInfo reportTypeInfo = null;
			if (reportType == "")
			{
				// Attempt to determine the report type automatically based on the stats
				foreach (XElement element in reportTypesElement.Elements("reporttype"))
				{
					if (IsReportTypeXMLCompatibleWithStats(element, csvFile.dummyCsvStats))
					{
						reportTypeInfo = new ReportTypeInfo(element, sharedSummaries, baseXmlDirectory);
						break;
					}
				}
				if (reportTypeInfo == null)
				{
					throw new Exception("Compatible report type for CSV " + csvFile.filename + " could not be found in" + reportTypeXmlFilename);
				}
			}
			else
			{
				XElement foundReportTypeElement = null;
				foreach (XElement element in reportTypesElement.Elements("reporttype"))
				{
					if (element.Attribute("name").Value.ToLower() == reportType)
					{
						foundReportTypeElement = element;
					}
				}
				if (foundReportTypeElement == null)
				{
					throw new Exception("Report type " + reportType + " not found in " + reportTypeXmlFilename);
				}

				if (!IsReportTypeXMLCompatibleWithStats(foundReportTypeElement, csvFile.dummyCsvStats))
				{
					if (forceReportType)
					{
						Console.Out.WriteLine("Report type " + reportType + " is not compatible with CSV " + csvFile.filename + ", but using it anyway");
					}
					else
					{
						throw new Exception("Report type " + reportType + " is not compatible with CSV " + csvFile.filename);
					}
				}
				reportTypeInfo = new ReportTypeInfo(foundReportTypeElement, sharedSummaries, baseXmlDirectory);
			}

			// Load the graphs
			foreach (ReportGraph graph in reportTypeInfo.graphs)
			{
				string key = graph.title.ToLower();
				if (graphs.ContainsKey(key))
				{
					graph.settings = graphs[key];
				}
				else
				{
					throw new Exception("Graph with title \"" + graph.title + "\" was not found in graphs XML");
				}
			}

			foreach (Summary summary in reportTypeInfo.summaries)
			{
				summary.PostInit(reportTypeInfo, csvFile.dummyCsvStats);
			}
			return reportTypeInfo;
		}

		bool IsReportTypeXMLCompatibleWithStats(XElement reportTypeElement, CsvStats csvStats)
		{
			XAttribute nameAt = reportTypeElement.Attribute("name");
			if (nameAt == null)
			{
				return false;
			}
			string reportTypeName = nameAt.Value;

			XElement autoDetectionEl = reportTypeElement.Element("autodetection");
			if (autoDetectionEl == null)
			{
				return false;
			}
			XAttribute requiredStatsAt = autoDetectionEl.Attribute("requiredstats");
			if (requiredStatsAt != null)
			{
				string[] requiredStats = requiredStatsAt.Value.Split(',');
				foreach (string stat in requiredStats)
				{
					if (csvStats.GetStatsMatchingString(stat).Count == 0)
					{
						return false;
					}
				}
			}

			foreach (XElement requiredMetadataEl in autoDetectionEl.Elements("requiredmetadata"))
			{
				XAttribute keyAt = requiredMetadataEl.Attribute("key");
				if (keyAt == null)
				{
					throw new Exception("Report type " + reportTypeName + " has no 'key' attribute!");
				}
				XAttribute allowedValuesAt = requiredMetadataEl.Attribute("allowedValues");
				if (allowedValuesAt == null)
				{
					throw new Exception("Report type " + reportTypeName + " has no 'allowedValues' attribute!");
				}

				if (csvStats.metaData == null)
				{
					// There was required metadata, but the CSV doesn't have any
					return false;
				}

				bool ignoreIfKeyNotFound = requiredMetadataEl.GetSafeAttibute("ignoreIfKeyNotFound", true);
				bool stopIfKeyFound = requiredMetadataEl.GetSafeAttibute("stopIfKeyFound", false);

				string key = keyAt.Value.ToLower();
				if (csvStats.metaData.Values.ContainsKey(key))
				{
					string value = csvStats.metaData.Values[key].ToLower();
					string[] allowedValues = allowedValuesAt.Value.ToString().ToLower().Split(',');
					if (!allowedValues.Contains(value))
					{
						return false;
					}
					if (stopIfKeyFound)
					{
						break;
					}
				}
				else if (ignoreIfKeyNotFound == false)
				{
					return false;
				}
			}

			//Console.Out.WriteLine("Autodetected report type: " + reportTypeName);

			return true;
		}


		public Dictionary<string, string> GetDisplayNameMapping() { return statDisplayNameMapping; }

		public SummaryTableInfo GetSummaryTable(string name)
		{
			if (summaryTables.ContainsKey(name.ToLower()))
			{
				return summaryTables[name.ToLower()];
			}
			else
			{
				throw new Exception("Requested summary table type '" + name + "' was not found in <summaryTables>");
			}
		}

		public List<CsvEventStripInfo> GetCsvEventsToStrip()
		{
			return csvEventsToStrip;
		}

		public void ApplyDerivedMetadata(CsvMetadata csvMetadata)
		{
			derivedMetadataMappings.ApplyMapping(csvMetadata);
		}

		public List<string> GetSummaryTableNames()
		{
			return summaryTables.Keys.ToList();
		}

		Dictionary<string, SummaryTableInfo> summaryTables;

		XElement reportTypesElement;
		XElement rootElement;
		XElement graphGroupsElement;
		XElement summaryTablesElement;
		Dictionary<string, XElement> sharedSummaries;
		Dictionary<string, GraphSettings> graphs;
		Dictionary<string, string> statDisplayNameMapping;
		public SummaryTableColumnFormatInfoCollection columnFormatInfoList;
		string baseXmlDirectory;

		List<CsvEventStripInfo> csvEventsToStrip;
		string reportTypeXmlFilename;
		public DerivedMetadataMappings derivedMetadataMappings;
	}
}