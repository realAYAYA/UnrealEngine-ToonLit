using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Perforce.P4
{
	/// <summary>
	/// Represents submitted or pending integrations. 
	/// </summary>
	public class FileIntegrationRecord
	{
		public FileSpec FromFile { get; set; }
		public FileSpec ToFile { get; set; }
		public IntegrateAction How { get; set; }
		public int ChangeId { get; set; }
		
		public FileIntegrationRecord() { }
		
		public FileIntegrationRecord
			(
			FileSpec fromfile,
			FileSpec tofile,
			IntegrateAction how,
			int changeid
			)
		{
			FromFile = fromfile;
			ToFile = tofile;
			How = how;
			ChangeId = changeid;
		}
		public void ParseIntegratedCmdTaggedData(TaggedObject obj)
		{
			DepotPath tp = null;
			VersionSpec starttorev = null;
			VersionSpec endtorev = null;

			if (obj.ContainsKey("toFile"))
			{
                string p = PathSpec.UnescapePath(obj["toFile"]);
                tp = new DepotPath(p);
			}

			if (obj.ContainsKey("startToRev"))
			{
				string str = obj["startToRev"];
				starttorev = new Revision(-1);

				if (str.StartsWith("#h")
					|
					str.StartsWith("#n"))
				{
					if (str.Contains("#none"))
					{
						starttorev = Revision.None;
					}

					if (str.Contains("#have"))
					{
						starttorev = Revision.Have;
					}

					if (str.Contains("#head"))
					{
						starttorev = Revision.Head;
					}
				}
				else
				{
					str = str.Trim('#');
					int rev = Convert.ToInt16(str);
					starttorev = new Revision(rev);
				}
			}

			if (obj.ContainsKey("endToRev"))
			{
				string etr = obj["endToRev"];
				endtorev = new Revision(-1);

				if (etr.StartsWith("#h")
					|
					etr.StartsWith("#n"))
				{
					if (etr.Contains("#none"))
					{
						endtorev = Revision.None;
					}

					if (etr.Contains("#have"))
					{
						endtorev = Revision.Have;
					}

					if (etr.Contains("#head"))
					{
						endtorev = Revision.Head;
					}
				}
				else
				{
					etr = etr.Trim('#');
					int rev = Convert.ToInt16(etr);
					endtorev = new Revision(rev);
				}
			}

			ToFile = new FileSpec(tp, null, null, new VersionRange(starttorev, endtorev));

			DepotPath fp = null;
			VersionSpec startfromrev = null;
			VersionSpec endfromrev = null;
			if (obj.ContainsKey("fromFile"))
			{
                string p = PathSpec.UnescapePath(obj["fromFile"]);
                fp = new DepotPath(p);
			}

			if (obj.ContainsKey("startFromRev"))
			{
				string sfr = obj["startFromRev"];
				startfromrev = new Revision(-1);

				if (sfr.StartsWith("#h")
					|
					sfr.StartsWith("#n"))
				{
					if (sfr.Contains("#none"))
					{
						startfromrev = Revision.None;
					}

					if (sfr.Contains("#have"))
					{
						startfromrev = Revision.Have;
					}

					if (sfr.Contains("#head"))
					{
						startfromrev = Revision.Head;
					}
				}
				else
				{
					sfr = sfr.Trim('#');
					int rev = Convert.ToInt16(sfr);
					startfromrev = new Revision(rev);
				}
			}

			if (obj.ContainsKey("endFromRev"))
			{
				string efr = obj["endFromRev"];
				endfromrev = new Revision(-1);

				if (efr.StartsWith("#h")
					|
					efr.StartsWith("#n"))
				{
					if (efr.Contains("#none"))
					{
						endfromrev = Revision.None;
					}

					if (efr.Contains("#have"))
					{
						endfromrev = Revision.Have;
					}

					if (efr.Contains("#head"))
					{
						endfromrev = Revision.Head;
					}
				}
				else
				{
					efr = efr.Trim('#');
					int rev = Convert.ToInt16(efr);
					endfromrev = new Revision(rev);
				}
			}

            FromFile = new FileSpec(fp, null, null, new VersionRange(startfromrev, endfromrev));

            if (obj.ContainsKey("how"))
            {
                How = (IntegrateAction) new StringEnum<IntegrateAction>(obj["how"], true, true);
            }   

			if (obj.ContainsKey("change"))
			{
				int change = -1;
				int.TryParse(obj["change"], out change);
				ChangeId = change;
			}
		}

			public static FileIntegrationRecord FromIntegratedCmdTaggedData(TaggedObject obj)
		{
			FileIntegrationRecord val = new FileIntegrationRecord();
			val.ParseIntegratedCmdTaggedData(obj);
			return val;
		}

		}
	}
	public enum IntegrateAction
	{
		BranchFrom, BranchInto, MergeFrom, MergeInto, MovedFrom, MovedInto,
		CopyFrom, CopyInto, Ignored, IgnoredBy, DeleteFrom, DeleteInto,
		EditFrom, EditInto, AddInto
	}
