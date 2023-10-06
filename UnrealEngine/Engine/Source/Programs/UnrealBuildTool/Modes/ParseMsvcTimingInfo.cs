// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Parses an MSVC timing info file generated from cl-filter to turn it into a form that can be used by other tooling.
	/// This is implemented as a separate mode to allow it to be done as part of the action graph.
	/// </summary>
	[ToolMode("ParseMsvcTimingInfo", ToolModeOptions.None)]
	class ParseMsvcTimingInfoMode : ToolMode
	{
		const string TimingDataRegex = @"^\t\t(?<Indent>\t*)(?<Name>[^\t]+):\s*(?<Duration>[0-9\.]+)s$";

		public override Task<int> ExecuteAsync(CommandLineArguments Arguments, ILogger Logger)
		{
			FileReference InputFile = Arguments.GetFileReference("-TimingFile=");

			// If the tracing argument was passed, hand off to the logic to generate a JSON file compatible with 
			// chrome://tracing
			if (Arguments.HasOption("-Tracing"))
			{
				ParseTimingDataToTracingFiles(InputFile);
				return Task.FromResult(0);
			}

			// Break the input file into the various sections for processing.
			string[] AllLines = FileReference.ReadAllLines(InputFile);
			List<string> Includes = new List<string>();
			List<string> Classes = new List<string>();
			List<string> Functions = new List<string>();
			TimingDataType CurrentType = TimingDataType.None;
			foreach (string Line in AllLines)
			{
				if (String.IsNullOrWhiteSpace(Line))
				{
					continue;
				}

				// Check for a change of type.
				if (Line.StartsWith("Include Headers:", StringComparison.OrdinalIgnoreCase))
				{
					CurrentType = TimingDataType.Include;
					continue;
				}
				else if (Line.StartsWith("Class Definitions:", StringComparison.OrdinalIgnoreCase))
				{
					CurrentType = TimingDataType.Class;
					continue;
				}
				else if (Line.StartsWith("Function Definitions:", StringComparison.OrdinalIgnoreCase))
				{
					CurrentType = TimingDataType.Function;
					continue;
				}

				// Skip the count line, we don't need it.
				if (Regex.IsMatch(Line, @"^\tCount\:\s*\d*$"))
				{
					continue;
				}

				// If we didn't change types and this isn't the count line and it doesn't match the expected output,
				//  clear the current type and move on.
				Match TimingDataMatch = Regex.Match(Line, TimingDataRegex);
				if (!TimingDataMatch.Success)
				{
					CurrentType = TimingDataType.None;
					continue;
				}

				// If we get to here this is a line we want to parse. Add it to the correct collection.
				switch (CurrentType)
				{
					case TimingDataType.Include:
						{
							Includes.Add(Line);
							break;
						}

					case TimingDataType.Class:
						{
							Classes.Add(Line);
							break;
						}

					case TimingDataType.Function:
						{
							Functions.Add(Line);
							break;
						}
				}
			}

			// Build the summary.
			TimingData Summary = new TimingData(InputFile.FullName.Replace(".timing.txt", String.Empty), TimingDataType.Summary);
			Summary.AddChild(SummarizeParsedTimingData("IncludeTimings", TimingDataType.Include, Includes));
			Summary.AddChild(SummarizeParsedTimingData("ClassTimings", TimingDataType.Class, Classes));
			Summary.AddChild(SummarizeParsedTimingData("FunctionTimings", TimingDataType.Function, Functions));

			// Write out the timing binary file.
			using (BinaryWriter Writer = new BinaryWriter(File.Open(InputFile.ChangeExtension(".cta").FullName, FileMode.Create)))
			{
				Writer.Write(Summary);
			}

			return Task.FromResult(0);
		}

		TimingData SummarizeParsedTimingData(string SummaryName, TimingDataType TimingType, IEnumerable<string> Lines)
		{
			TimingData Summary = new TimingData(SummaryName, TimingDataType.Summary);
			List<TimingData> ParsedTimingData = ParseTimingDataFromLines(TimingType, Lines);
			foreach (TimingData Data in ParsedTimingData)
			{
				// See if we've already added a child that matches this data's name. If so, just add to the duration.
				TimingData? MatchedData;
				if (Summary.Children.TryGetValue(Data.Name, out MatchedData))
				{
					MatchedData.Count += 1;
					MatchedData.ExclusiveDuration += Data.ExclusiveDuration;
				}
				else
				{
					Summary.AddChild(Data);
				}
			}

			return Summary;
		}

		List<TimingData> ParseTimingDataFromLines(TimingDataType TimingType, IEnumerable<string> Lines)
		{
			List<TimingData> ParsedTimingData = new List<TimingData>();
			int LastDepth = 0;
			TimingData? LastTimingData = null;
			foreach (string Line in Lines)
			{
				int LineDepth;
				TimingData CurrentTimingData = ParseTimingDataFromLine(TimingType, Line, out LineDepth)!;
				if (LineDepth == 0)
				{
					ParsedTimingData.Add(CurrentTimingData);
				}
				else
				{
					while (LineDepth < LastDepth)
					{
						LastTimingData = LastTimingData!.Parent;
						--LastDepth;
					}

					// If this timing data would have a parent, add the data to that parent and reduce its exclusive
					// duration by this data's inclusive duration.
					TimingData? ParentData = null;
					if (LineDepth == LastDepth)
					{
						CurrentTimingData.Parent = LastTimingData!.Parent;
						ParentData = LastTimingData.Parent;

					}
					else if (LineDepth > LastDepth)
					{
						CurrentTimingData.Parent = LastTimingData;
						ParentData = LastTimingData;
					}

					if (ParentData != null)
					{
						ParentData.AddChild(CurrentTimingData);
						ParentData.ExclusiveDuration -= CurrentTimingData.InclusiveDuration;
					}
				}

				LastTimingData = CurrentTimingData;
				LastDepth = LineDepth;
			}

			return ParsedTimingData;
		}

		TimingData? ParseTimingDataFromLine(TimingDataType TimingType, string Line, out int LineDepth)
		{
			Match TimingDataMatch = Regex.Match(Line, TimingDataRegex);
			if (!TimingDataMatch.Success)
			{
				LineDepth = -1;
				return null;
			}

			LineDepth = TimingDataMatch.Groups["Indent"].Success ? TimingDataMatch.Groups["Indent"].Value.Count() : 0;

			TimingData ParsedTimingData = new TimingData(TimingDataMatch.Groups["Name"].Value, TimingType);
			ParsedTimingData.ExclusiveDuration = Single.Parse(TimingDataMatch.Groups["Duration"].Value);

			return ParsedTimingData;
		}

		#region "Chrome Tracing Parsing"

		void ParseTimingDataToTracingFiles(FileReference InputFile)
		{
			string[] Lines = FileReference.ReadAllLines(InputFile);
			for (int LineIdx = 0; LineIdx < Lines.Length;)
			{
				string Line = Lines[LineIdx];
				if (Line.StartsWith("Include Headers:", StringComparison.Ordinal))
				{
					LineIdx = ParseIncludeHeadersToTraces(Lines, LineIdx + 1, InputFile.ChangeExtension(".json"));
				}
				else if (Line.StartsWith("Class Definitions:", StringComparison.Ordinal))
				{
					LineIdx = ParseDefinitions(Lines, LineIdx + 1, InputFile.ChangeExtension(".classes.txt"));
				}
				else if (Line.StartsWith("Function Definitions:", StringComparison.Ordinal))
				{
					LineIdx = ParseDefinitions(Lines, LineIdx + 1, InputFile.ChangeExtension(".functions.txt"));
				}
				else
				{
					LineIdx++;
				}
			}
		}

		int ParseIncludeHeadersToTraces(string[] Lines, int LineIdx, FileReference OutputFile)
		{
			if (LineIdx < Lines.Length && Lines[LineIdx].StartsWith("\tCount:", StringComparison.Ordinal))
			{
				LineIdx++;
			}

			using (JsonWriter Writer = new JsonWriter(OutputFile))
			{
				Writer.WriteObjectStart();
				Writer.WriteArrayStart("traceEvents");

				Stack<float> FinishTimesForIndent = new Stack<float>();
				FinishTimesForIndent.Push(0.0f);

				float StartTime = 0.0f;
				for (; LineIdx < Lines.Length; LineIdx++)
				{
					Match Match = Regex.Match(Lines[LineIdx], "^\t\t(\t*)([^\t]+):\\s*([0-9\\.]+)s$");
					if (!Match.Success)
					{
						break;
					}

					int Indent = Match.Groups[1].Length;
					string FileName = Match.Groups[2].Value;
					float Duration = Single.Parse(Match.Groups[3].Value);

					while (Indent <= FinishTimesForIndent.Count - 1)
					{
						StartTime = FinishTimesForIndent.Pop();
					}

					Writer.WriteObjectStart();
					Writer.WriteValue("pid", 1);
					Writer.WriteValue("tid", 1);
					Writer.WriteValue("ts", (long)(StartTime * 1000.0f * 1000.0f));
					Writer.WriteValue("dur", (long)(Duration * 1000.0f * 1000.0f));
					Writer.WriteValue("ph", "X");
					Writer.WriteValue("name", Path.GetFileName(FileName));
					Writer.WriteObjectStart("args");
					Writer.WriteValue("path", FileName);
					Writer.WriteObjectEnd();
					Writer.WriteObjectEnd();

					while (Indent >= FinishTimesForIndent.Count)
					{
						FinishTimesForIndent.Push(StartTime + Duration);
					}
				}

				Writer.WriteArrayEnd();
				Writer.WriteObjectEnd();
			}
			return LineIdx;
		}

		int ParseDefinitions(string[] Lines, int LineIdx, FileReference OutputFile)
		{
			if (LineIdx < Lines.Length && Lines[LineIdx].StartsWith("\tCount:", StringComparison.Ordinal))
			{
				LineIdx++;
			}

			Dictionary<string, float> ClassNameToTime = new Dictionary<string, float>();
			for (; LineIdx < Lines.Length; LineIdx++)
			{
				Match Match = Regex.Match(Lines[LineIdx], "^\t\t\t*([^\t]+):\\s*([0-9\\.]+)s$");
				if (!Match.Success)
				{
					break;
				}

				string ClassName = Match.Groups[1].Value;

				int TemplateIdx = ClassName.IndexOf('<');
				if (TemplateIdx != -1)
				{
					ClassName = ClassName.Substring(0, TemplateIdx) + "<>";
				}

				float Time;
				ClassNameToTime.TryGetValue(ClassName, out Time);

				Time += Single.Parse(Match.Groups[2].Value);
				ClassNameToTime[ClassName] = Time;
			}

			using (StreamWriter Writer = new StreamWriter(OutputFile.FullName))
			{
				foreach (KeyValuePair<string, float> Pair in ClassNameToTime.OrderByDescending(x => x.Value))
				{
					Writer.WriteLine("{0,7:0.000}: {1}", Pair.Value, Pair.Key);
				}
			}

			return LineIdx;
		}

		#endregion
	}
}
