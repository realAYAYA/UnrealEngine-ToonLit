using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Specifies whether this line match is an exact match,
	/// or a context (before or after) match.
	/// </summary>
	public enum MatchType
	{
		Match, Before, After
	}
	/// <summary>
	/// Specifies matching lines in Perforce managed files. 
	/// </summary>
	public class FileLineMatch
	{
		public MatchType Type;
		public string Line;
		public int LineNumber;
		public FileSpec FileSpec;

		public FileLineMatch() { }

		public FileLineMatch (MatchType type, string line, int linenumber, FileSpec filespec)
		{
			Type = type;
			Line = line;
			LineNumber = linenumber;
			FileSpec = filespec;
		}

		public void ParseGrepCmdTaggedData(TaggedObject obj)
		{
			if (obj.ContainsKey("depotFile"))
			{
				int rev = -1;
				if (obj.ContainsKey("rev"))
				{
					int.TryParse(obj["rev"], out rev);
					FileSpec = new FileSpec(new DepotPath(obj["depotFile"]), new Revision(rev));
				}
			}

			Type = MatchType.Match;
			if (obj.ContainsKey("type"))
			{
				StringEnum<MatchType> matchtype = obj["type"];
				Type = matchtype;
			}

			if (obj.ContainsKey("matchedLine"))
			{
				Line = obj["matchedLine"];
			}

			if (obj.ContainsKey("line"))
			{
				int v = -1;
				int.TryParse(obj["line"], out v);
				LineNumber = v;
			}
		}

		public static FileLineMatch FromGrepCmdTaggedData(TaggedObject obj)
		{
			FileLineMatch val = new FileLineMatch();
			val.ParseGrepCmdTaggedData(obj);
			return val;
		}
	}
}
