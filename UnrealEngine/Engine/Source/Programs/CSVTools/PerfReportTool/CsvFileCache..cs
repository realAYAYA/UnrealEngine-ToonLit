// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Xml;
using System.Xml.Linq;
using System.IO;
using System.Diagnostics;
using CSVStats;
using System.Collections;
using System.Collections.Concurrent;
using System.Threading;
using System.Security.Cryptography;

using PerfSummaries;

namespace PerfReportTool
{
	class CachedCsvFile
	{
		enum FileType
		{
			TextCsv,
			BinaryCsv,
			SummaryTableCachePrc
		};
		public CachedCsvFile(string inFilename, bool useCacheFiles, DerivedMetadataMappings derivedMetadataMappings)
		{
			if (inFilename.ToLower().EndsWith(".csv"))
			{
				fileType = FileType.TextCsv;
			}
			else if (inFilename.ToLower().EndsWith(".csv.bin"))
			{
				fileType = FileType.BinaryCsv;
			}
			else if (inFilename.ToLower().EndsWith(".prc"))
			{
				fileType = FileType.SummaryTableCachePrc;
			}
			else
			{
				throw new Exception("File extension not supported for file " + inFilename);
			}
			string cacheFilename = inFilename + ".cache";
			if (useCacheFiles && File.Exists(cacheFilename))
			{
				string[] fileLines = File.ReadAllLines(cacheFilename);

				// Put the stats and metadata lines in the standard order
				if (fileLines.Length >= 3)
				{
					string metadataLine = fileLines[1];
					string statsLine = fileLines[2];
					fileLines[1] = statsLine;
					fileLines[2] = metadataLine;
				}
				dummyCsvStats = CsvStats.ReadCSVFromLines(fileLines, null, 0, true);
			}
			else
			{
				if (fileType == FileType.BinaryCsv)
				{
					dummyCsvStats = CsvStats.ReadBinFile(inFilename, null, 0, true);
				}
				else if (fileType == FileType.TextCsv)
				{
					textCsvLines = File.ReadAllLines(inFilename);
					if (textCsvLines.Length > 0)
					{
						dummyCsvStats = CsvStats.ReadCSVFromLines(textCsvLines, null, 0, true);
					}
					else
					{
						Console.WriteLine("CSV file " + inFilename + " is contains no lines!");
						dummyCsvStats = new CsvStats();
					}
				}
				else if (fileType == FileType.SummaryTableCachePrc)
				{
					// Read just the Csv metadata so we can filter
					metadata = null;
					cachedSummaryTableRowData = SummaryTableRowData.TryReadFromCacheFile(inFilename, true);
					if (cachedSummaryTableRowData != null)
					{
						foreach (KeyValuePair<string, SummaryTableElement> entry in cachedSummaryTableRowData.dict)
						{
							if (entry.Value.type == SummaryTableElement.Type.CsvMetadata)
							{
								if (metadata == null)
								{
									metadata = new CsvMetadata();
								}
								metadata.Values.Add(entry.Key, entry.Value.value);
							}
							if (entry.Value.type == SummaryTableElement.Type.CsvStatAverage)
							{
								break;
							}
						}
					}
					else
					{
						Console.WriteLine("Invalid PRC file detected: " + inFilename);
					}
				}
			}
			filename = inFilename;

			// Setup initial metadata variable mappings and derived metadata (which can read from the initial variableMappings). 
			// Note: we skip for PRCs (dummyCsvStats==null), since these already have mappings cached
			if (dummyCsvStats != null)
			{
				xmlVariableMappings = new XmlVariableMappings();

				// Note: We could apply the global variable set here, but that is slow and leads to complexity and correctness issues
				if (dummyCsvStats.metaData != null)
				{
					metadata = dummyCsvStats.metaData;
					xmlVariableMappings.SetMetadataVariables(metadata);
					derivedMetadataMappings.ApplyMapping(metadata, xmlVariableMappings);
				}
			}
		}

		public void PrepareCsvData()
		{
			if (fileType == FileType.BinaryCsv)
			{
				finalCsv = CsvStats.ReadBinFile(filename);
				// If we already have metadata then use it. This avoids the need to remap it multiple times
				if (metadata != null)
				{
					finalCsv.metaData = metadata;
				}

			}
			else
			{
				if (textCsvLines == null)
				{
					textCsvLines = File.ReadAllLines(filename);
				}
			}
		}

		public CsvStats GetFinalCsv()
		{
			if (finalCsv != null)
			{
				return finalCsv;
			}
			if (fileType == FileType.TextCsv)
			{
				finalCsv = CsvStats.ReadCSVFromLines(textCsvLines, null);

				// If we already have metadata then use it. This avoids the need to remap it multiple times
				if ( metadata != null )
				{
					finalCsv.metaData = metadata;
				}

				return finalCsv;
			}
			return null;
		}


		public bool DoesMetadataMatchQuery(QueryExpression metadataQuery)
		{
			if (metadataQuery == null)
			{
				return true;
			}
			if (metadata == null)
			{
				Console.WriteLine("CSV " + filename + " has no metadata");
				return false;
			}
			return metadataQuery.Evaluate(metadata);
		}

		public void ComputeSummaryTableCacheId(string reportTypeId)
		{
			// If the CSV has an embedded ID, use that. Otherwise generate one. 
			string csvId;
			if (metadata != null && metadata.Values.ContainsKey("csvid"))
			{
				csvId = metadata.Values["csvid"];
			}
			else
			{
				// Fall back to absolute path if the CSVID metadata doesn't exist
				// Note that using the filename like this means moving the file will result in a new entry for each location
				StringBuilder sb = new StringBuilder();
				sb.Append("CSVFILENAME={" + Path.GetFullPath(filename).ToLower() + "}\n");
				if (metadata != null)
				{
					foreach (string key in metadata.Values.Keys)
					{
						sb.Append("{" + key + "}={" + metadata.Values[key] + "}\n");
					}
				}
				csvId = HashHelper.StringToHashStr(sb.ToString());
			}
			if (reportTypeId == null)
			{
				summaryTableCacheId = csvId;
			}
			else
			{
				summaryTableCacheId = csvId + "_" + reportTypeId;
			}
		}


		public string filename;
		public string summaryTableCacheId;
		public string[] textCsvLines;
		public CsvStats dummyCsvStats;
		public CsvStats finalCsv;
		public CsvMetadata metadata;
		public SummaryTableRowData cachedSummaryTableRowData;
		public ReportTypeInfo reportTypeInfo;
		public XmlVariableMappings xmlVariableMappings;
		FileType fileType;
	};

	class CsvFileCache
	{
		public CsvFileCache(
			string[] inCsvFilenames,
			int inLookaheadCount,
			int inThreadCount,
			bool inUseCacheFiles,
			QueryExpression inMetadataQuery,
			ReportXML inReportXml,
			ReportTypeParams inReportTypeParams,
			bool inBulkMode,
			bool inSummaryTableCacheOnlyMode,
			bool inSummaryTableCacheUseOnlyCsvID,
			bool inRemoveDuplicates,
			bool inRequireMetadata,
			string inSummaryTableCacheDir,
			bool inListFilesMode)
		{
			csvFileInfos = new CsvFileInfo[inCsvFilenames.Length];
			for (int i = 0; i < inCsvFilenames.Length; i++)
			{
				csvFileInfos[i] = new CsvFileInfo(inCsvFilenames[i]);
			}
			fileCache = this;
			writeIndex = 0;
			useCacheFiles = inUseCacheFiles;
			readIndex = 0;
			lookaheadCount = inLookaheadCount;
			countFreedSinceLastGC = 0;
			reportXml = inReportXml;
			bulkMode = inBulkMode;
			summaryTableCacheOnlyMode = inSummaryTableCacheOnlyMode;
			summaryTableCacheUseOnlyCsvID = inSummaryTableCacheUseOnlyCsvID;
			metadataQuery = inMetadataQuery;
			derivedMetadataMappings = inReportXml.derivedMetadataMappings;
			summaryTableCacheDir = inSummaryTableCacheDir;
			reportTypeParams = inReportTypeParams;
			bRemoveDuplicates = inRemoveDuplicates;
			bRequireMetadata = inRequireMetadata;
			bListFilesMode = inListFilesMode;

			csvIdToFilename = new Dictionary<string, string>();
			outCsvQueue = new BlockingCollection<CsvFileInfo>(lookaheadCount);

			// Kick off the workers (must be done last)
			if (inLookaheadCount > 0)
			{
				Console.WriteLine("Kicking off " + inThreadCount + " precache threads with lookahead " + lookaheadCount);
				precacheThreads = new Thread[inThreadCount];
				precacheJobs = new ThreadStart[inThreadCount];
				for (int i = 0; i < precacheThreads.Length; i++)
				{
					precacheJobs[i] = new ThreadStart(PrecacheThreadRun);
					precacheThreads[i] = new Thread(precacheJobs[i]);
					precacheThreads[i].Start();
				}
			}
		}

		public CachedCsvFile GetNextCachedCsvFile()
		{
			CachedCsvFile file = null;
			if (readIndex >= csvFileInfos.Length)
			{
				// We're done
				return null;
			}

			// Find the next valid fileinfo
			if (precacheThreads == null)
			{
				CsvFileInfo fileInfo = csvFileInfos[readIndex];
				file = new CachedCsvFile(fileInfo.filename, useCacheFiles, derivedMetadataMappings);
				if (!file.DoesMetadataMatchQuery(metadataQuery)) // TODO do we need to check this here and in the thread?
				{
					return null;
				}
			}
			else
			{
				CsvFileInfo fileInfo = outCsvQueue.Take();
				file = fileInfo.cachedFile;
				fileInfo.cachedFile = null;
				countFreedSinceLastGC++;
				// Periodically GC
				if (countFreedSinceLastGC > 16 && summaryTableCacheOnlyMode == false)
				{
					GC.Collect();
					GC.WaitForPendingFinalizers();
					countFreedSinceLastGC = 0;
				}
				if (!fileInfo.isValid)
				{
					file = null;
				}
			}
			readIndex++;
			return file;
		}

		static void PrecacheThreadRun()
		{
			fileCache.ThreadRun();
		}

		void ThreadRun()
		{
			int threadWriteIndex = 0;
			while (true)
			{
				threadWriteIndex = Interlocked.Increment(ref writeIndex) - 1;
				if (threadWriteIndex >= csvFileInfos.Length)
				{
					// We're done
					break;
				}
				CsvFileInfo fileInfo = csvFileInfos[threadWriteIndex];

				// Process the file
				CachedCsvFile file = new CachedCsvFile(fileInfo.filename, useCacheFiles, derivedMetadataMappings);

				bool bProcessThisFile = true;
				if (bRequireMetadata && file.metadata == null)
				{
					Console.WriteLine("CSV has no metadata. Skipping: " + fileInfo.filename);
					bProcessThisFile = false;
				}
				else if (bRemoveDuplicates && file.metadata != null && file.metadata.Values.ContainsKey("csvid"))
				{
					string csvId = file.metadata.Values["csvid"];
					lock (csvIdToFilename)
					{
						if (csvIdToFilename.ContainsKey(csvId))
						{
							Console.WriteLine("Duplicate CSV found: " + fileInfo.filename);
							Console.WriteLine("   First version   : " + csvIdToFilename[csvId]);
							bProcessThisFile = false;
							duplicateCount++;
						}
						else
						{
							csvIdToFilename[csvId] = fileInfo.filename;
						}
					}
				}

				if (bProcessThisFile && file.DoesMetadataMatchQuery(metadataQuery))
				{
					if (bListFilesMode)
					{
						Console.WriteLine("File: " + fileInfo.filename);
					}
					else if (summaryTableCacheOnlyMode)
					{
						// Assume the report type is valid if we have cached metadata, since we don't actually need the reporttype in metadata only mode
						if (file.cachedSummaryTableRowData != null)
						{
							// Just read the full cached metadata
							file.cachedSummaryTableRowData = SummaryTableRowData.TryReadFromCacheFile(fileInfo.filename);
							fileInfo.isValid = true;
						}
					}
					else
					{
						file.reportTypeInfo = GetCsvReportTypeInfo(file, bulkMode);
						if (file.reportTypeInfo != null)
						{
							string reportTypeHash = summaryTableCacheUseOnlyCsvID ? null : file.reportTypeInfo.GetSummaryTableCacheID();
							file.ComputeSummaryTableCacheId(reportTypeHash);
							if (summaryTableCacheDir != null)
							{
								// If a summary metadata cache is specified, try reading from it instead of reading the whole CSV
								// Note that this will be disabled if we're not in bulk mode
								file.cachedSummaryTableRowData = SummaryTableRowData.TryReadFromCache(summaryTableCacheDir, file.summaryTableCacheId);
								if (file.cachedSummaryTableRowData == null)
								{
									Console.WriteLine("Failed to read summary metadata from cache for CSV: " + fileInfo.filename);
								}
							}
							if (file.cachedSummaryTableRowData == null)
							{
								file.PrepareCsvData();
							}

							// Only read the full file data if the metadata matches
							fileInfo.isValid = true;
						}
					}
				}
				fileInfo.cachedFile = file;
				// TODO: only add valid files to the queue? Need alternative stopping condition
				outCsvQueue.Add(fileInfo);
			}
		}

		ReportTypeInfo GetCsvReportTypeInfo(CachedCsvFile csvFile, bool bBulkMode)
		{
			try
			{
				return reportXml.GetReportTypeInfo(reportTypeParams.reportTypeOverride, csvFile, bBulkMode, reportTypeParams.forceReportType);
			}
			catch (Exception e)
			{
				if (bBulkMode)
				{
					Console.Out.WriteLine("[ERROR] : " + e.Message);
					return null;
				}
				else
				{
					// If we're not in bulk mode, exceptions are fatal
					throw;
				}
			}
		}


		ThreadStart[] precacheJobs;
		Thread[] precacheThreads;
		int writeIndex;
		int readIndex;
		int lookaheadCount;
		int countFreedSinceLastGC;
		bool useCacheFiles;
		bool bulkMode;
		bool bRequireMetadata;
		bool summaryTableCacheOnlyMode;
		bool summaryTableCacheUseOnlyCsvID;
		QueryExpression metadataQuery;
		string summaryTableCacheDir;
		ReportXML reportXml;
		ReportTypeParams reportTypeParams;
		DerivedMetadataMappings derivedMetadataMappings;

		Dictionary<string, string> csvIdToFilename;
		public int duplicateCount = 0;
		bool bRemoveDuplicates;
		bool bListFilesMode;

		static CsvFileCache fileCache;
		BlockingCollection<CsvFileInfo> outCsvQueue;

		class CsvFileInfo
		{
			public CsvFileInfo(string inFilename)
			{
				filename = inFilename;
				cachedFile = null;
				isValid = false;
			}
			public CachedCsvFile cachedFile;
			public string filename;
			public bool isValid;
		}
		CsvFileInfo[] csvFileInfos;

	}
}