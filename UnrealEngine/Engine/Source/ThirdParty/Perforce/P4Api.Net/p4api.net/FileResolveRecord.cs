using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	public enum ResolveType
	{
		None = 0,
		Branch,
		Content,
		Delete,
		Move,
		Filetype,
		Attribute
	};

	public enum ResolveSubtype
	{
		None = 0,
		ThreeWayText,
		ThreeWayRaw,
		TwoWayRaw
	};
	[Flags]
	public enum ResolveOptions
	{
		None = 0,
		AcceptMerged = 0x0001,
		AcceptYours = 0x0002,
		AccecptTheirs = 0x0004,
		Skip = 0x0008,
		Merge = 0x0010,
		AutoSelect = 0x0020,
		All = 0x003F
	};

	public class ResolveAnalysis
	{
		public ResolveAnalysis()
		{
			ResolveType = ResolveType.None;

			TargetDiffCnt = -1;
			SourceDiffCnt = -1;
			CommonDiffCount = -1;
			ConflictCount = -1;

			Options = ResolveOptions.None;
		}

		public ResolveAnalysis(string str)
		{
			if (str == null)
			{
				return;
			}
			string[] parts = str.Split(new char[] { ' ', '+' }, StringSplitOptions.RemoveEmptyEntries);

			int v;
			if (int.TryParse(parts[2], out v))
			{
				TargetDiffCnt = v;
			}
			if (int.TryParse(parts[4], out v))
			{
				SourceDiffCnt = v;
			}
			if (int.TryParse(parts[6], out v))
			{
				CommonDiffCount = v;
			}
			if (int.TryParse(parts[8], out v))
			{
				ConflictCount = v;
			}
			// this is from a content resolve
			SetResolveType(ResolveType.Content);
		}

		StringEnum<ResolveType> _resolveType = ResolveType.None;

		public ResolveType ResolveType
		{
			get { return _resolveType; }
			set { _resolveType = value; }
		}

		public ResolveType SetResolveType(string sType)
		{
			try
			{
				_resolveType = sType;
				if (_resolveType == null)
				{
					_resolveType = ResolveType.None;
				}
			}
			catch
			{
				return P4.ResolveType.None;
			}
			return _resolveType;
		}

		public ResolveType SetResolveType(P4.ResolveType type)
		{
			try
			{
				ResolveType = type;
			}
			catch
			{
				return P4.ResolveType.None;
			}
			return type;
		}

		public ResolveOptions Options {get; set;}

		public int TargetDiffCnt { get; set; }
		public int SourceDiffCnt { get; set; }
		public int CommonDiffCount { get; set; }
		public int ConflictCount { get; set; }

		public string YoursAction { get; set; }
		public string TheirsAction { get; set; }
		public string MergeAction { get; set; }

		public P4ClientMerge.MergeStatus SuggestedAction = P4ClientMerge.MergeStatus.CMS_SKIP;
	}
	/// <summary>
	/// Specifies how file resolve operations were completed or will
	/// potentially be completed. 
	/// </summary>
	public class FileResolveRecord
	{
		private StringEnum<FileAction> _action = FileAction.None;
		public FileAction Action
		{
			get { return _action; }
			private set { _action = value; }
		}

		private StringEnum<ResolveType> _resolveType = ResolveType.None;
		public ResolveType ResolveType
		{
			get { return _resolveType; }
			private set { _resolveType = value; }
		}

		private StringEnum<ResolveSubtype> _resolveSubtype = ResolveSubtype.None;
		public ResolveSubtype ResolveSubtype
		{
			get { return _resolveSubtype; }
			private set { _resolveSubtype = value; }
		}

		public FileSpec BaseFileSpec { get; set; }
		public FileSpec FromFileSpec { get; set; }
		public LocalPath LocalFilePath { get; set; }

		public ResolveAnalysis Analysis { get; set; }

		public string Sumary { get; set; }
		public string Result { get; set; }

		public FileResolveRecord() { _action = FileAction.None; }
		public FileResolveRecord(FileAction action, FileSpec baseFile, FileSpec fromFile)
		{
			Action = action;
			BaseFileSpec = baseFile;
			FromFileSpec = fromFile;
		}

		public FileResolveRecord(TaggedObject obj)
		{
			FromTaggedOutput(obj);
		}

		public FileResolveRecord(string spec)
		{
			Parse(spec);
		}

		public void FromTaggedOutput(TaggedObject obj)
		{
		}

		public void Parse(string spec)
		{

		}

		public static FileResolveRecord FromResolveCmdTaggedOutput(TaggedObject obj)
		{
			FileResolveRecord frr = new FileResolveRecord();
			int startRev = -1;
			int endRev = -1;

			if (obj.ContainsKey("clientFile"))
			{
				frr.LocalFilePath = new LocalPath(obj["clientFile"]);
			}
			if (obj.ContainsKey("baseFile"))
			{
				int baseRev = -1;
				VersionSpec vs = null;
				if (obj.ContainsKey("baseRev"))
				{
					if (int.TryParse(obj["baseRev"], out baseRev))
					{
						vs = new Revision(baseRev);
					}					
				}
				frr.BaseFileSpec = new FileSpec(new DepotPath(obj["baseFile"]), vs);
			}

			if (obj.ContainsKey("startFromRev"))
			{
				int.TryParse(obj["startFromRev"], out startRev);
			}
			if (obj.ContainsKey("endFromRev"))
			{
				int.TryParse(obj["endFromRev"], out endRev);
			}
			if (obj.ContainsKey("fromFile"))
			{
				VersionRange versions = null;
				if ((startRev >= 0) && (endRev >= 0))
				{
					versions = new VersionRange(startRev, endRev);
				}
				frr.FromFileSpec = new FileSpec(new DepotPath(obj["fromFile"]), versions);
			}
			if (obj.ContainsKey("how"))
			{
				frr._action = obj["how"];
			}
			else
			{
				frr.Action = FileAction.None;
			}
			if (obj.ContainsKey("resolveType"))
			{
				frr._resolveType = obj["resolveType"];
			}
			else
			{
				frr.ResolveType = ResolveType.None;
			}
			if (obj.ContainsKey("contentResolveType"))
			{
				switch (obj["contentResolveType"])
				{
					case "3waytext":
						frr.ResolveSubtype = ResolveSubtype.ThreeWayText;
						break;
					case "3wayraw":
						frr.ResolveSubtype = ResolveSubtype.ThreeWayRaw;
						break;
					case "2wayraw":
						frr.ResolveSubtype = ResolveSubtype.TwoWayRaw;
						break;
					default:
						frr.ResolveSubtype = ResolveSubtype.None;
						break;
				}
			}
			else
			{
				frr.ResolveSubtype = ResolveSubtype.None;
			}
			return frr;
		}

		public static FileResolveRecord FromResolvedCmdTaggedOutput(TaggedObject obj)
		{
			FileResolveRecord frr = new FileResolveRecord();
			int startRev = -1;
			int endRev = -1;

			if (obj.ContainsKey("path"))
			{
				frr.LocalFilePath = new LocalPath(obj["path"]);
			}

			if (obj.ContainsKey("startToRev"))
			{
				int.TryParse(obj["startToRev"].Trim('#'), out startRev);
			}
			if (obj.ContainsKey("endToRev"))
			{
				int.TryParse(obj["endToRev"].Trim('#'), out endRev);
			}
			if (obj.ContainsKey("clientFile"))
			{
				VersionRange versions = null;
				if ((startRev >= 0) && (endRev >= 0))
				{
					versions = new VersionRange(startRev, endRev);
				}

				frr.BaseFileSpec = new FileSpec(new LocalPath(obj["clientFile"]), versions);
			}
			else if (obj.ContainsKey("toFile"))
			{
				VersionRange versions = null;
				if ((startRev >= 0) && (endRev >= 0))
				{
					versions = new VersionRange(startRev, endRev);
				}
				frr.BaseFileSpec = new FileSpec(new ClientPath(obj["toFile"]), versions);
			}

			if (obj.ContainsKey("startFromRev"))
			{
				int.TryParse(obj["startFromRev"].Trim('#'), out startRev);
			}
			if (obj.ContainsKey("endFromRev"))
			{
				int.TryParse(obj["endFromRev"].Trim('#'), out endRev);
			}
			if (obj.ContainsKey("fromFile"))
			{
				VersionRange versions = null;
				if ((startRev >= 0) && (endRev >= 0))
				{
					versions = new VersionRange(startRev, endRev);
				}
				frr.FromFileSpec = new FileSpec(new DepotPath(obj["fromFile"]), versions);
			}
			if (obj.ContainsKey("how"))
			{
				frr._action = obj["how"];
			}
			return frr;
		}

		public static FileResolveRecord FromMergeInfo(P4.P4ClientInfoMessageList info)
		{
			if (info.Count <= 0)
			{
				return null;
			}

			string l1 = null;
			string l2 = null;
			string l3 = null;

			if (info[0].Message.Contains("Diff chunks:"))
			{
				l2 = info[0].Message;
				if (info.Count >= 2)
				{
					l3 = info[1].Message;
				}
			}
			else if ((info.Count >= 2) && (info[1].Message.Contains("Diff chunks:")))
			{
				l1 = info[0].Message;
				l2 = info[1].Message;
				if (info.Count >= 2)
				{
					l3 = info[2].Message;
				}
			}
			else
			{
				if (info[0].Message.Contains(" - resolve "))
				{
					l3 = info[0].Message;
				}
				else
				{
					l1 = info[0].Message;
					if (info.Count >= 2)
					{
						l3 = info[1].Message;
					}
				}
			}
			return FromMergeInfo(l1, l2, l3);
		}
		public static FileResolveRecord FromMergeInfo(string l1, string l2, string l3)
		{
			try
			{
				FileResolveRecord frr = new FileResolveRecord();
				int idx1 = -1;
				int idx2 = -1;
				string path;

				if (l1 != null)
				{
					idx1 = l1.IndexOf(" - merging ");
					if (idx1 < 0)
					{
						idx1 = l1.IndexOf(" - resolving ");
					}
					if (idx1 < 0)
					{
						idx1 = l1.IndexOf(" - binary/binary merge ");
					}
					if (idx1 < 0)
					{
						idx1 = l1.IndexOf(" - text/binary merge ");
					}
					if (idx1 < 0)
					{
						return null;
					}
					path = l1.Substring(0, idx1);

					frr.LocalFilePath = new LocalPath(path);

					idx1 = l1.IndexOf("//");

					idx2 = l1.IndexOf(" using base ");
					if (idx2 < 0)
					{
						path = l1.Substring(idx1);
					}
					else
					{
						path = l1.Substring(idx1, idx2 - idx1);
					}
					string[] parts = path.Split('#');
					int rev = -1;
					int.TryParse(parts[1], out rev);

					frr.FromFileSpec = new FileSpec(new DepotPath(parts[0]), new Revision(rev));

					idx2 = l1.IndexOf(" using base ");
					if (idx2 > 0)
					{
						path = l1.Substring(idx2);

						parts = path.Split('#');
						rev = -1;
						int.TryParse(parts[1], out rev);

						frr.BaseFileSpec = new FileSpec(new DepotPath(parts[0]), new Revision(rev));
					}
				}
				if (l3 != null)
				{
					idx1 = l3.LastIndexOf(" - ") + 3;

					idx2 = l3.IndexOf('/', idx1) - 1;
					if (idx2 < 0)
					{
						idx2 = l3.Length - 1;
					}
					string actionStr = l3.Substring(idx1, idx2 - idx1);

					frr._action = actionStr;

					if (frr._action == null)
					{
						frr._action = FileAction.None;
					}
				}
				frr.Sumary = l1;
				frr.Analysis = new ResolveAnalysis(l2);
				frr.Result = l3;

				return frr;
			}
			catch
			{
				return null;
			}
		}
		public static void MergeRecords(FileResolveRecord Record1, FileResolveRecord Record2)
		{
			if (Record1.LocalFilePath == null)
			{
				Record1.LocalFilePath = Record2.LocalFilePath;
			}
			if (Record1.FromFileSpec == null)
			{
				Record1.FromFileSpec = Record2.FromFileSpec;
			}
			if (Record1.BaseFileSpec == null)
			{
				Record1.BaseFileSpec = Record2.BaseFileSpec;
			}
			if ((Record1._action == null) || (Record1._action == FileAction.None))
			{
				Record1._action = Record2.Action;
			}
			if (Record1._action == null)
			{
				Record1._action = FileAction.None;
			}
			if ((Record1._resolveType == null) || (Record1._resolveType == ResolveType.None))
			{
				Record1._resolveType = Record2._resolveType;
			}
			if (Record1._resolveType == null)
			{
				Record1._resolveType = ResolveType.None;
			}
			if ((Record1._resolveSubtype == null) || (Record1._resolveSubtype == ResolveSubtype.None))
			{
				Record1._resolveSubtype = Record2._resolveSubtype;
			}
			if (Record1._resolveSubtype == null)
			{
				Record1._resolveSubtype = ResolveSubtype.None;
			}
			if (Record1.Sumary == null)
			{
				Record1.Sumary = Record2.Sumary;
			}
			if (Record1.Analysis == null)
			{
				Record1.Analysis = Record2.Analysis;
			}
			if (Record1.Result == null)
			{
				Record1.Result = Record2.Result;
			}
		}
	}
}
