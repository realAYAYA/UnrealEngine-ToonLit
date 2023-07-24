using System;

namespace Perforce.P4
{
	/// <summary>
	/// A versioned object that describes an individual file in a Perforce repository. 
	/// </summary>
	public class File : FileSpec
	{
        /// <summary>
        /// What change contains this file?
        /// </summary>
		public int ChangeId;

        /// <summary>
        /// Last Action taken on this file
        /// </summary>
		public FileAction Action;

        /// <summary>
        /// FileType of the file
        /// </summary>
		public FileType Type;

        /// <summary>
        /// DateTime when file was last submitted
        /// </summary>
		public DateTime SubmitTime;

        /// <summary>
        /// The revision of this file which is in the client/workspace
        /// </summary>
		public Revision HaveRev;

        /// <summary>
        /// The user which created this file
        /// </summary>
		public string User;

        /// <summary>
        /// The client/workspace name which contains this file.
        /// </summary>
		public string Client;

        /// <summary>
        /// Default constructor
        /// </summary>
		public File() { }

        /// <summary>
        /// Fully parameterized constructor
        /// </summary>
        /// <param name="depotPath">Server Depot Path</param>
        /// <param name="clientPath">Client workspace Path</param>
        /// <param name="rev">Latest Revision of this file</param>
        /// <param name="haveRev">Revision of this file in the workspace</param>
        /// <param name="change">Change ID which contains this file</param>
        /// <param name="action">Last Action taken on this file</param>
        /// <param name="type">The file type</param>
        /// <param name="submittime">The time when the file was submitted</param>
        /// <param name="user">the User which created this file</param>
        /// <param name="client">the name of the client/workspace which contains this file</param>
		public File(
			DepotPath depotPath,
			ClientPath clientPath,
			Revision rev,
			Revision haveRev,
			int change,
			FileAction action,
			FileType type,
			DateTime submittime,
			string user,
			string client)
			: base(depotPath, clientPath, null, rev) 
		{
			ChangeId = change;
			Action = action;
			Type = type;
			SubmitTime = submittime;
			HaveRev = haveRev;
			User = user;
			Client = client;
		}

        /// <summary>
        /// Given a Tagged object from the server, instantiate this File from the object
        /// </summary>
        /// <param name="obj">Tagged Object to Parse</param>
		public void ParseFilesCmdTaggedData(TaggedObject obj)
		{
			if (obj.ContainsKey("depotFile"))
			{
				base.DepotPath = new DepotPath(obj["depotFile"]);
			}

			if (obj.ContainsKey("clientFile"))
			{
				base.ClientPath = new ClientPath(obj["clientFile"]);
			}

			if (obj.ContainsKey("rev"))
			{
				int rev = -1;
				int.TryParse(obj["rev"], out rev);
				base.Version = new Revision(rev);
			}

			if (obj.ContainsKey("haveRev"))
			{
				int rev = -1;
				int.TryParse(obj["haveRev"], out rev);
				HaveRev = new Revision(rev);
			}

			if (obj.ContainsKey("change"))
			{
				int change = -1;
				int.TryParse(obj["change"], out change);
				ChangeId = change;
			}

			if (obj.ContainsKey("action"))
			{
				Action = (FileAction) new StringEnum<FileAction>(obj["action"], true, true);
			}

			if (obj.ContainsKey("type"))
			{
				Type = new FileType(obj["type"]);
			}

			if (obj.ContainsKey("time"))
			{
				SubmitTime = FormBase.ConvertUnixTime(obj["time"]);
			}

			if (obj.ContainsKey("user"))
			{
				User = obj["user"];
			}

			if (obj.ContainsKey("client"))
			{
				Client = obj["client"];
			}
		}

        /// <summary>
        /// Return a File from a parsed Tagged Object
        /// </summary>
        /// <param name="obj">Tagged object to parse</param>
        /// <returns>a new File</returns>
		public static File FromFilesCmdTaggedData(TaggedObject obj)
		{
			File val = new File();
			val.ParseFilesCmdTaggedData(obj);
			return val;
		}
	}
}
