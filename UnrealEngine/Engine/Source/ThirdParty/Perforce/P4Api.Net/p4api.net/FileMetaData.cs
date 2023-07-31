using System;
using System.Collections.Generic;

namespace Perforce.P4
{
	/// <summary>
	/// Describes the pending or completed action related to open,
	/// resolve, or integration for a specific file.
	/// </summary>
	public enum FileAction : long
	{
		/// <summary>
		/// None.
		/// </summary>
		None = 0x0000,
		/// <summary>
		/// Opened for add.
		/// </summary>
		Add = 1,
		/// <summary>
		/// Opened for branch.
		/// </summary>
		Branch = 2,
		/// <summary>
		/// Opened for edit.
		/// </summary>
		Edit = 3,
		/// <summary>
		/// Opened for integrate.
		/// </summary>
		Integrate = 4,
		/// <summary>
		/// File has been deleted.
		/// </summary>
		Delete = 5,
		/// <summary>
		/// file was integrated from partner-file, and partner-file
		/// had been previously deleted.
		/// </summary>
		DeleteFrom = 6,
		/// <summary>
		/// file was integrated into partner-file, and file had been
		/// previously deleted.
		/// </summary>
		DeleteInto = 7,
		/// <summary>
		/// File has been synced.
		/// </summary>
		Sync = 8,
		/// <summary>
		/// File has been updated.
		/// </summary>
		Updated = 9,
		/// <summary>
		/// File has been added.
		/// </summary>
		Added = 10,
		/// <summary>
		/// file was integrated into previously nonexistent partner-file,
		/// and partner-file was reopened for add before submission.
		/// </summary>
		AddInto = 11,
		/// <summary>
		/// File has been refreshed.
		/// </summary>
		Refreshed = 12,
		/// <summary>
		/// File was integrated from partner-file, accepting yours.
		/// </summary>
		Ignored = 13,
		/// <summary>
		/// File was integrated into partner-file, accepting yours.
		/// </summary>
		IgnoredBy = 14,
		/// <summary>
		/// File has been abandoned.
		/// </summary>
		Abandoned = 15,
		/// <summary>
		/// None.
		/// </summary>
		EditIgnored = 16,
		/// <summary>
		/// File is opened for move.
		/// </summary>
		Move = 17,
		/// <summary>
		/// File has been added as part of a move.
		/// </summary>
		MoveAdd = 18,
		/// <summary>
		/// File has been deleted as part of a move.
		/// </summary>
		MoveDelete = 19,
		/// <summary>
		/// File was integrated from partner-file, accepting theirs
		/// and deleting the original.
		/// </summary>
		MovedFrom = 20,
		/// <summary>
		/// File was integrated into partner-file, accepting merge.
		/// </summary>
		MovedInto = 21,
		/// <summary>
		/// File has not been resolved.
		/// </summary>
		Unresolved = 22,
		/// <summary>
		/// File was integrated from partner-file, accepting theirs.
		/// </summary>
		CopyFrom = 23,
		/// <summary>
		/// File was integrated into partner-file, accepting theirs.
		/// </summary>
		CopyInto = 24,
		/// <summary>
		/// File was integrated from partner-file, accepting merge.
		/// </summary>
		MergeFrom = 25,
		/// <summary>
		/// File was integrated into partner-file, accepting merge.
		/// </summary>
		MergeInto = 26,
		/// <summary>
		/// file was integrated from partner-file, and file was edited
		/// within the p4 resolve process. This allows you to determine
		/// whether the change should ever be integrated back; automated
		/// changes (merge from) needn't be, but original user edits
		/// (edit from) performed during the resolve should be.
		/// </summary>
		EditFrom = 27,
		/// <summary>
		/// File was integrated into partner-file, and partner-file was
		/// reopened for edit before submission.
		/// </summary>
		EditInto = 28,
		/// <summary>
		/// File was purged.
		/// </summary>
		Purge = 29,
		/// <summary>
		/// File was imported.
		/// </summary>
		Import = 30,
		/// <summary>
		/// File did not previously exist; it was created as a copy of
		/// partner-file.
		/// </summary>
		BranchFrom = 31,
		/// <summary>
		/// Partner-file did not previously exist; it was created as a
		/// copy of file.
		/// </summary>
		BranchInto = 32,
		/// <summary>
		/// File was reverted.
		/// </summary>
		Reverted = 33,
		/// <summary>
		/// File was archived.
		/// </summary>
		Archive = 34,
	}

	/// <summary>
	/// Class summarizing the use of this file by another user.
	/// </summary>
	public class OtherFileUser
	{
		private string _client;

        /// <summary>
        /// Property for Client name
        /// </summary>
		public string Client
		{
			get { return _client; }
			set
			{
				_client = value;
				string[] parts = value.Split('@');
				if (parts.Length > 0)
					UserName = parts[0];
				if (parts.Length > 1)
					ClientName = parts[1];
			}
		}
        /// <summary>
        /// User Name
        /// </summary>
		public string  UserName {get; set;}
        /// <summary>
        /// File Action
        /// </summary>
		public FileAction Action { get; set; }
        /// <summary>
        /// Is File Locked?
        /// </summary>
		public bool hasLock { get; set; }
        /// <summary>
        /// Client Name
        /// </summary>
		public string ClientName { get; set; }
        /// <summary>
        /// Change List Number
        /// </summary>
		public int ChangelistId { get; set; }
	}
	
	/// <summary>
	/// Specifies other users who have a particular file open.
	/// </summary>
	public class OtherUsers : Dictionary<string, OtherFileUser>
	{
        /// <summary>
        /// Get OtherFileUser info from OtherUsers
        /// given a key
        /// </summary>
        /// <param name="key">name of user to return</param>
        /// <returns>OtherFileUser</returns>
      public new OtherFileUser this[string key]
		{
			get
			{
				if (base.ContainsKey(key) == false)
				{
					OtherFileUser newEntry = new OtherFileUser();
					newEntry.Client = key;
					base.Add(key, newEntry);
				}

				return base[key];
			}
			set
			{
				base[key] = value;
			}
		}

	}
	/// <summary>
	/// Metadata for a specific file stored in a Perforce repository.
	/// </summary>
	public class FileMetaData
	{
        /// <summary>
        /// Default constructor
        /// </summary>
		public FileMetaData() 
		{
			MovedFile = null;
			IsMapped = false;
			Shelved = false;
			HeadAction = FileAction.None;
			HeadChange = -1;
			HeadRev = -1;
			HeadType = null;
			HeadTime = DateTime.MinValue;
			HeadModTime = DateTime.MinValue;
			MovedRev = -1;
			HaveRev = -1;
			Desc = null;
			Digest = null;
			FileSize = -1;
			Action = FileAction.None;
			Type = null;
			ActionOwner = null;
			Change = -1;
			Resolved = false;
			Unresolved = false;
			Reresolvable = false;
			OtherOpen = 0;
			OtherOpenUserClients = null;
			OtherLock = false;
			OtherLockUserClients = null;
			OtherActions = null;
			OtherChanges = null;
			OurLock = false;
			ResolveRecords = null;
			Attributes = null;
            AttributesProp = null;
            AttributeDigests = null;
            OpenAttributes = null;
            OpenAttributesProp = null;
            TotalFileCount = -1;
		    Directory = null;
		}

        /// <summary>
        /// Parameterized constructor
        /// </summary>
        /// <param name="movedfile">was file moved?</param>
        /// <param name="ismapped">is file in workspace?</param>
        /// <param name="shelved">is file shelved</param>
        /// <param name="headaction">Last Action to File</param>
        /// <param name="headchange">Change ID</param>
        /// <param name="headrev">Head Revision</param>
        /// <param name="headtype">Type of File</param>
        /// <param name="headtime">Time file created</param>
        /// <param name="headmodtime">Last modified time</param>
        /// <param name="movedrev">Revision which was moved</param>
        /// <param name="haverev">Revision we have</param>
        /// <param name="desc">Description of File</param>
        /// <param name="digest">Digest for file</param>
        /// <param name="filesize">size of file</param>
        /// <param name="action">current action on file</param>
        /// <param name="type">file type</param>
        /// <param name="actionowner">owner of file</param>
        /// <param name="change">current change ID</param>
        /// <param name="resolved">Resolved</param>
        /// <param name="unresolved">Unresolved</param>
        /// <param name="reresolvable">Re-Resolvable</param>
        /// <param name="otheropen">How many others have file open?</param>
        /// <param name="otheropenuserclients">List of other clients with file open</param>
        /// <param name="otherlock">true if other user has file locked</param>
        /// <param name="otherlockuserclients">List of other clients locking this file</param>
        /// <param name="otheractions">List of other FileActions</param>
        /// <param name="otherchanges">List of other change ID's</param>
        /// <param name="ourlock">This file is locked by us</param>
        /// <param name="resolverecords">List of resolve records</param>
        /// <param name="attributes">Dictionary of attributes</param>
        /// <param name="attributesprop">Dictionary of attribute properties</param>
        /// <param name="attributedigests">Dictionary of attribute digests</param>
        /// <param name="openattributes">Dictionary of open attributes</param>
        /// <param name="openattributesprop">Dictionary of open attribute properties</param>
        /// <param name="totalfilecount">count of files</param>
        /// <param name="directory">location of file</param>
		public FileMetaData(	DepotPath movedfile,
								bool ismapped,
								bool shelved,
								FileAction headaction,
								int headchange,
								int headrev,
								FileType headtype,
								DateTime headtime,
								DateTime headmodtime,
								int movedrev,
								int haverev,
								string desc,
								string digest,
								int filesize,
								FileAction action,
								FileType type,
								string actionowner,
								int change,
								bool resolved,
								bool unresolved,
								bool reresolvable,
								int otheropen,
								List<string> otheropenuserclients,
								bool otherlock,
								List<string> otherlockuserclients,
								List<FileAction> otheractions,
								List<int> otherchanges,
								bool ourlock,
								List<FileResolveAction> resolverecords,
								Dictionary<String, Object> attributes,
                                Dictionary<String, Object> attributesprop,
                                Dictionary<String, Object> attributedigests,
                                Dictionary<String, Object> openattributes,
                                Dictionary<String, Object> openattributesprop,
                                long totalfilecount,
            string directory
								)
		{
			MovedFile = movedfile;
			IsMapped = ismapped;
			Shelved = shelved;
			HeadAction = headaction;
			HeadChange = headchange;
			HeadRev = headrev;
			HeadType = headtype;
			HeadTime = headtime;
			HeadModTime = headmodtime;
			MovedRev = movedrev;
			HaveRev = haverev;
			Desc = desc;
			Digest = digest;
			FileSize = filesize;
			Action = action;
			Type = type;
			ActionOwner = actionowner;
			Change = change;
			Resolved = resolved;
			Unresolved = unresolved;
			Reresolvable = reresolvable;
			OtherOpen = otheropen;
			OtherOpenUserClients = otheropenuserclients;
			OtherLock = otherlock;
			OtherLockUserClients = otherlockuserclients;
			OtherActions = otheractions;
			OtherChanges = otherchanges;
			OurLock = ourlock;
			ResolveRecords = resolverecords;
			Attributes = attributes;
            AttributesProp = attributesprop;
            AttributeDigests = attributedigests;
            OpenAttributes = openattributes;
            OpenAttributesProp = openattributesprop;
            TotalFileCount = totalfilecount;
		    Directory = directory;
		}

        /// <summary>
        /// Constructor for FileMetaData given a File
        /// This command is "lossy" not all FileMetaData will be valid
        /// </summary>
        /// <param name="f">File</param>
		public FileMetaData(File f)
		{
			MovedFile = null;
			IsMapped = false;
			Shelved = false;
			HeadAction = FileAction.None;
			HeadChange = -1;
			HeadRev = -1;
			HeadType = null;
			HeadTime = DateTime.MinValue;
			HeadModTime = DateTime.MinValue;
			MovedRev = -1;
			Revision rev = f.Version as Revision;
			if (rev != null)
			{
				HaveRev = rev.Rev;
			}
			else
			{
				HaveRev = -1;
			}
			Desc = null;
			Digest = null;
			FileSize = -1;
			Action = f.Action;
			Type = f.Type;
			ActionOwner = f.User;
			Change = f.ChangeId;
			Resolved = false;
			Unresolved = false;
			Reresolvable = false;
			OtherOpen = 0;
			OtherOpenUserClients = null;
			OtherLock = false;
			OtherLockUserClients = null;
			OtherActions = null;
			OtherChanges = null;
			OurLock = false;
			ResolveRecords = null;
			Attributes = null;
            AttributesProp = null;
            AttributeDigests = null;
            OpenAttributes = null;
            OpenAttributesProp = null;
			Directory = null;
            TotalFileCount = -1;
			DepotPath = f.DepotPath;
			ClientPath = f.ClientPath;
		}

        /// <summary>
        /// Constructor for FileMetaData given a FileSpec
        /// This command is "lossy" not all FileMetaData will be valid
        /// </summary>
        /// <param name="f">FileSpec</param>
        public FileMetaData(FileSpec f)
        {
            MovedFile = null;
            IsMapped = false;
            Shelved = false;
            HeadAction = FileAction.None;
            HeadChange = -1;
            HeadRev = -1;
            HeadType = null;
            HeadTime = DateTime.MinValue;
            HeadModTime = DateTime.MinValue;
            MovedRev = -1;
            Revision rev = f.Version as Revision;
            if (rev != null)
            {
                HaveRev = rev.Rev;
            }
            else
            {
                HaveRev = -1;
            }
            Desc = null;
            Digest = null;
            FileSize = -1;
            Action = FileAction.None;
            Type = null;
            ActionOwner = null;
            Change = -1;
            Resolved = false;
            Unresolved = false;
            Reresolvable = false;
            OtherOpen = 0;
            OtherOpenUserClients = null;
            OtherLock = false;
            OtherLockUserClients = null;
            OtherActions = null;
            OtherChanges = null;
            OurLock = false;
            ResolveRecords = null;
            Attributes = null;
            AttributesProp = null;
            AttributeDigests = null;
            OpenAttributes = null;
            OpenAttributesProp = null;
            Directory = null;
            TotalFileCount = -1;
            DepotPath = f.DepotPath;
            ClientPath = f.ClientPath;
        }

        /// <summary>
        ///  The location of the file in the depot
        /// </summary>
        public DepotPath DepotPath { get; set; }

        /// <summary>
        /// Check if there is a non-empty Depot Path
        /// </summary>
		public bool IsInDepot
		{
			get
			{ 
				return ((DepotPath !=null) && (string.IsNullOrEmpty(DepotPath.Path) == false)); 
			}
		}

		/// <summary>
		/// The location of the file in the client's file system,
		/// a LocalPath
		/// </summary>
		public LocalPath LocalPath { get; set; }

        /// <summary>
        /// The location of the file relative to client root
        /// a ClientPath
        /// </summary>
		public ClientPath ClientPath { get; set; }

        /// <summary>
        /// Check if there is a non empty ClientPath
        /// </summary>
		public bool IsInClient
		{
			get
			{
				return ((ClientPath != null) && (string.IsNullOrEmpty(ClientPath.Path) == false));
			}
		}

        /// <summary>
        /// Access the DepotPath of the Moved File
        /// </summary>
		public DepotPath MovedFile { get; set; }

        /// <summary>
        /// Is this file mapped in the client/workspace?
        /// </summary>
		public bool IsMapped { get; set; }

        /// <summary>
        /// Is this file shelved
        /// </summary>
		public bool Shelved { get; set; }

		private StringEnum<FileAction> _headAction = FileAction.None;

        /// <summary>
        /// What was the last action done on this file?
        /// </summary>
		public FileAction HeadAction 
		{
			get { return (_headAction == null)? FileAction.None : (FileAction) _headAction; }
			set {_headAction = value; }
		}

        /// <summary>
        /// What Change Id is associated with Head revision
        /// </summary>
		public int HeadChange { get; set; }

        /// <summary>
        /// What revision is the Head revision
        /// </summary>
		public int HeadRev { get; set; }

        /// <summary>
        /// What is the FileType of the Head Revision
        /// </summary>
		public FileType HeadType { get; set; }

        /// <summary>
        /// What is the creation time of the head revision
        /// </summary>
		public DateTime HeadTime { get; set; }

        /// <summary>
        /// What is the last modification time of the head revision
        /// </summary>
		public DateTime HeadModTime { get; set; }

        /// <summary>
        /// What revision was moved?
        /// </summary>
		public int MovedRev { get; set; }

        /// <summary>
        /// What revision do you have?
        /// </summary>
		public int HaveRev { get; set; }

        /// <summary>
        /// Description
        /// </summary>
		public string Desc { get; set; }

        /// <summary>
        /// MD5 Digest for this file
        /// </summary>
		public string Digest { get; set; }

        /// <summary>
        /// Size of the file
        /// </summary>
		public long FileSize { get; set; }

		private StringEnum<FileAction> _action;

        /// <summary>
        /// What is the current Action being performed on this file
        /// </summary>
		public FileAction Action
		{
			get { return (_action == null)? FileAction.None : (FileAction) _action; }
			set { _action = value; }
		}

        /// <summary>
        /// What is the File Type of this file
        /// </summary>
		public FileType Type { get; set; }

        /// <summary>
        /// Who is causing the current action to this file
        /// </summary>
		public string ActionOwner { get; set; }

        /// <summary>
        /// Change ID
        /// </summary>
		public int Change { get; set; }

        /// <summary>
        /// Is this file Resolved?
        /// </summary>
		public bool Resolved { get; set; }

        /// <summary>
        /// Is this file Unresolved?
        /// </summary>
		public bool Unresolved { get; set; }

        /// <summary>
        /// Is this file Re Resolvable?
        /// </summary>
		public bool Reresolvable { get; set; }	
	
        /// <summary>
        /// How many others have this file open?
        /// </summary>
		public int OtherOpen { get; set; }

        /// <summary>
        /// List of other clients which have this file open
        /// </summary>
		public IList<String> OtherOpenUserClients { get; set; }

        /// <summary>
        /// Does someone else have this file locked?
        /// </summary>
		public bool OtherLock { get; set; }

        /// <summary>
        /// List of Other Locks Clients
        /// </summary>
		public IList<String> OtherLockUserClients { get; set; }

        /// <summary>
        /// List of Actions by other users
        /// </summary>
		public IList<FileAction> OtherActions { get; set; }

        /// <summary>
        /// List of other change IDs
        /// </summary>
		public IList<int> OtherChanges { get; set; }

        /// <summary>
        /// Do we have this file locked?
        /// </summary>
		public bool OurLock { get; set; }

        /// <summary>
        /// List of Resolve records
        /// </summary>
		public IList<FileResolveAction> ResolveRecords { get; set; }

        /// <summary>
        /// Dictionary of Attributes
        /// </summary>
		public Dictionary<string, object> Attributes { get; set; }

        /// <summary>
        /// Dictionary of Attribute Properties
        /// </summary>
        public Dictionary<string, object> AttributesProp { get; set; }

        /// <summary>
        /// Dictionary of Attribute MD5 Digests
        /// </summary>
        public Dictionary<string, object> AttributeDigests { get; set; }

        /// <summary>
        /// Dictionary of Open Attributes
        /// </summary>
        public Dictionary<string, object> OpenAttributes { get; set; }

        /// <summary>
        /// Dictionary of Open Attributes Properties
        /// </summary>
        public Dictionary<string, object> OpenAttributesProp { get; set; }

        /// <summary>
        /// Total File count
        /// </summary>
        public long TotalFileCount { get; set; }

        /// <summary>
        /// Other Users of this file
        /// </summary>
		public OtherUsers OtherUsers { get; set; }

        /// <summary>
        /// Directory containing this file
        /// </summary>
        public string Directory { get; set; }

        /// <summary>
        /// Given tagged output from an "fstat" command instantiate this object.
        /// </summary>
        /// <param name="obj">Tagged output from fstat</param>
		public void FromFstatCmdTaggedData(TaggedObject obj)
		{
			if (obj.ContainsKey("clientFile"))
			{
				string path = obj["clientFile"];
				if (path.StartsWith("//"))
				{
					ClientPath = new ClientPath(obj["clientFile"]);
				}
				else
				{
					ClientPath = new ClientPath(obj["clientFile"]);
					LocalPath = new LocalPath(obj["clientFile"]);
				}
			}

			if (obj.ContainsKey("path"))
			{
				LocalPath = new LocalPath(obj["path"]);
			}

			if (obj.ContainsKey("depotFile"))
			{
				string p = PathSpec.UnescapePath(obj["depotFile"]);
				DepotPath = new DepotPath(p);
			}

			if (obj.ContainsKey("movedFile"))
			{
				MovedFile = new DepotPath(obj["movedFile"]);
				if (obj.ContainsKey("movedRev"))
				{
					int movedrev = -1;
					if (int.TryParse(obj["movedRev"], out movedrev))
					{
						MovedRev = movedrev;
					}
				}
			}

			if (obj.ContainsKey("isMapped"))
			{ IsMapped = true; }

			if (obj.ContainsKey("shelved"))
			{ Shelved = true; }

			if (obj.ContainsKey("headAction"))
			{ 
				_headAction = obj["headAction"]; 
			}

			if (obj.ContainsKey("headChange"))
			{
				int r = -1;
				if (int.TryParse(obj["headChange"], out r))
				{
					HeadChange = r;
				}
			}

			if (obj.ContainsKey("headRev"))
			{
				int r = -1;
				if (int.TryParse(obj["headRev"], out r))
				{
					HeadRev = r;
				}
			}

			if (obj.ContainsKey("headType"))
			{ 
				HeadType = new FileType(obj["headType"]); 
			}

			if (obj.ContainsKey("headTime"))
			{
				HeadTime = FormBase.ConvertUnixTime(obj["headTime"]);
			}

			if (obj.ContainsKey("headModTime"))
			{
				HeadModTime = FormBase.ConvertUnixTime(obj["headModTime"]);
			}

			if (obj.ContainsKey("haveRev"))
			{
				int r = -1;
				if ((int.TryParse(obj["haveRev"], out r)) && (r > 0))
				{
					HaveRev = r;
				}
			}

			if (obj.ContainsKey("desc"))
			{ Desc = obj["desc"]; }

			if (obj.ContainsKey("digest"))
			{ Digest = obj["digest"]; }

			if (obj.ContainsKey("fileSize"))
			{
				long s = -1;
				if (long.TryParse(obj["fileSize"], out s))
				{
					FileSize = s;
				}
			}

			if (obj.ContainsKey("action"))
			{ 
				_action = obj["action"]; 
			}

			if (obj.ContainsKey("type"))
			{
				Type = new FileType(obj["type"]);
			}
			else if (obj.ContainsKey("headType"))
			{
				// If not on mapped in current client, will not have
				//the Type filed so User the HeadType
				Type = new FileType(obj["headType"]);
			}
			else
			{
				Type = new FileType(BaseFileType.Text, FileTypeModifier.None);
			}

			if (obj.ContainsKey("actionOwner"))
			{ 
				ActionOwner = obj["actionOwner"]; 
			}

			if (obj.ContainsKey("change"))
			{
				int c = -1;
				if (int.TryParse(obj["change"], out c))
				{
					Change = c;
				}
				else
				{
					Change = 0;
				}
			}

			if (obj.ContainsKey("resolved"))
			{ Resolved = true; }

			if (obj.ContainsKey("unresolved"))
			{ Unresolved = true; }

			if (obj.ContainsKey("reresolvable"))
			{ Reresolvable = true; }


            if (obj.ContainsKey("otherLock"))
            {
                OtherLock = true;
            }

            if (obj.ContainsKey("otherOpen"))
            {
                int cnt = 0;
                if (int.TryParse(obj["otherOpen"], out cnt))
                {
                    OtherOpen = cnt;
                    OtherLockUserClients = new List<string>();
                }

                if (cnt > 0)
                {
                    OtherUsers = new OtherUsers();

                    OtherOpenUserClients = new List<String>();
                    OtherActions = new List<FileAction>();
                    OtherChanges = new List<int>();

                    for (int idx=0; idx < cnt; idx++)
                    {
                        string key = String.Format("otherOpen{0}", idx);
                        string otherClientName = null;
                        OtherFileUser ofi = null;

                        if (obj.ContainsKey(key))
                        {
                            otherClientName = obj[key];
                            OtherOpenUserClients.Add(otherClientName);
                        }

                        ofi = OtherUsers[otherClientName];
                        ofi.Client = otherClientName;

                        key = String.Format("otherAction{0}", idx);

                        if (obj.ContainsKey(key))
                        {
                            StringEnum<FileAction> otheraction = obj[key];
                            OtherActions.Add(otheraction);
                            ofi.Action = otheraction;
                        }

                        key = String.Format("otherChange{0}", idx);

                        if (obj.ContainsKey(key))
                        {
                            int otherchange;
                            if (!int.TryParse(obj[key], out otherchange))
                            {
                                otherchange = 0;
                            }
                            OtherChanges.Add(otherchange);

                            ofi.ChangelistId = otherchange;
                        }

                        key = String.Format("otherLock{0}", idx);

                        if (obj.ContainsKey(key))
                        {
                            string s = obj[key];
                            OtherLockUserClients.Add(s);

                            OtherUsers[s].hasLock = true;
                        }
                    }
                }
            }

			if (obj.ContainsKey("ourLock"))
			{ 
				OurLock = true; 
			}

			if (obj.ContainsKey("resolved")	||	obj.ContainsKey("unresolved"))
			{
				int idx = 0;
				StringEnum<ResolveAction> resolveaction = ResolveAction.Unresolved;
				FileSpec resolvebasefile = null;
				FileSpec resolvefromfile = null;
				int resolvestartfromrev = -1;
				int resolveendfromrev = -1;
				FileResolveAction resolverecord = null;

				ResolveRecords = new List<FileResolveAction>();

				while (true)
				{
					string key = String.Format("resolveAction{0}", idx);

					if (obj.ContainsKey(key))
					{ resolveaction = obj[key]; }
					else break;

					key = String.Format("resolveBaseFile{0}", idx);

					if (obj.ContainsKey(key))
					{
						string basefile = obj[key];
						int resolvebaserev = -1;
						int.TryParse(obj[String.Format("resolveBaseRev{0}",idx)], out resolvebaserev);
						resolvebasefile = new FileSpec(new DepotPath(basefile), new Revision(resolvebaserev));
					}
					else break;

					key = String.Format("resolveFromFile{0}", idx);

					if (obj.ContainsKey(key))
					{
						string fromfile = obj[key];
						int startfromrev, endfromrev = -1;
						int.TryParse(obj[String.Format("resolveStartFromRev{0}",idx)], out startfromrev);
						int.TryParse(obj[String.Format("resolveEndFromRev{0}",idx)], out endfromrev);
						resolvefromfile = new FileSpec(new DepotPath(fromfile),
							new VersionRange(new Revision(startfromrev), new Revision(endfromrev)));
					}
					else break;

					resolverecord = new FileResolveAction
						(resolveaction, resolvebasefile, resolvefromfile, resolvestartfromrev, resolveendfromrev);
					ResolveRecords.Add(resolverecord);

					idx++;
				}
			}

			Attributes = new Dictionary<string, object>();

			foreach (string key in obj.Keys)
			{
				if (key.StartsWith("attr-"))
				{
					object val = obj[key];
					string atrib = key.Replace("attr-", "");
					Attributes.Add(atrib, val);
				}
			}

            AttributesProp = new Dictionary<string, object>();

            foreach (string key in obj.Keys)
            {
                if (key.StartsWith("attrProp-"))
                {
                    object val = obj[key];
                    string atrib = key.Replace("attrProp-", "");
                    AttributesProp.Add(atrib, val);
                }
            }

            AttributeDigests = new Dictionary<string, object>();

            foreach (string key in obj.Keys)
            {
                if (key.StartsWith("attrDigest-"))
                {
                    object val = obj[key];
                    string atribDigest = key.Replace("attrDigest-", "");
                    AttributeDigests.Add(atribDigest, val);
                }
            }

            OpenAttributes = new Dictionary<string, object>();

            foreach (string key in obj.Keys)
            {
                if (key.StartsWith("openattr-"))
                {
                    object val = obj[key];
                    string atrib = key.Replace("openattr-", "");
                    OpenAttributes.Add(atrib, val);
                }
            }

            OpenAttributesProp = new Dictionary<string, object>();

            foreach (string key in obj.Keys)
            {
                if (key.StartsWith("openattrProp-"))
                {
                    object val = obj[key];
                    string atrib = key.Replace("openattrProp-", "");
                    OpenAttributesProp.Add(atrib, val);
                }
            }

            if (obj.ContainsKey("totalFileCount"))
            {
                long s = -1;
                if (long.TryParse(obj["totalFileCount"], out s))
                {
                    TotalFileCount = s;
                }
            }

            if (obj.ContainsKey("dir"))
            {
                Directory= PathSpec.UnescapePath(obj["dir"]);
            }
		}

        /// <summary>
        /// Get the Filename by checking DepotPath, ClientPath and LocalPath as needed
        /// </summary>
        /// <returns>File Name</returns>
		public string GetFileName()
		{
			if ((DepotPath != null) && (string.IsNullOrEmpty(DepotPath.Path) == false))
			{
				return DepotPath.GetFileName();
			}
			else if ((ClientPath != null) && (string.IsNullOrEmpty(ClientPath.Path) == false))
			{
				return ClientPath.GetFileName();
			}
			else if ((LocalPath != null) && (string.IsNullOrEmpty(LocalPath.Path) == false))
			{
				return LocalPath.GetFileName();
			}
			return null;
		}

        /// <summary>
        /// Get the Directory this file is in by checking DepotPath, ClientPath and LocalPath as needed
        /// </summary>
        /// <returns>Directory name</returns>
		public string GetDirectoryName()
		{
			if ((DepotPath != null) && (string.IsNullOrEmpty(DepotPath.Path) == false))
			{
				return DepotPath.GetDirectoryName();
			}
			else if ((ClientPath != null) && (string.IsNullOrEmpty(ClientPath.Path) == false))
			{
				return ClientPath.GetDirectoryName();
			}
			else if ((LocalPath != null) && (string.IsNullOrEmpty(LocalPath.Path) == false))
			{
				return LocalPath.GetDirectoryName();
			}
			return null;
		}

        /// <summary>
        /// Operator to Create a FileSpec from FileMetaData
        /// </summary>
        /// <param name="s">File Metadata</param>
        /// <returns>FileSpec</returns>
		public static implicit operator FileSpec(FileMetaData s)
		{
			Revision r = null;
			if (s.HaveRev > 0)
			{
				r = new Revision(s.HaveRev);
			}
			if ((s.DepotPath != null) && (string.IsNullOrEmpty(s.DepotPath.Path) == false))
			{
				return new FileSpec(s.DepotPath, r);
			}
			else if ((s.ClientPath != null) && (string.IsNullOrEmpty(s.ClientPath.Path) == false))
			{
				return new FileSpec(s.ClientPath, r);
			}
			else if ((s.LocalPath != null) && (string.IsNullOrEmpty(s.LocalPath.Path) == false))
			{
				return new FileSpec(s.LocalPath, r);
			}
			return null;
		}

		/// <summary>
		/// Cast a FileSpec to FileMetaData
		/// This command is "lossy" not all FileMetaData will be valid
		/// </summary>
		/// <param name="f">FileSpec</param>
		/// <returns>FileMetaData for FileSpec</returns>
		public static implicit operator FileMetaData(FileSpec f)
		{
			FileMetaData s = new FileMetaData();
			if (f.Version != null && f.Version is Revision)
			{
				s.HaveRev = ((Revision)f.Version).Rev;
			}
			s.DepotPath = f.DepotPath;
			s.ClientPath = f.ClientPath;
			s.LocalPath = f.LocalPath;
			
			return s;
		}

		/// <summary>
		/// Cast a FileSpec to FileMetatData
        /// This command is "lossy" not all FileMetaData will be valid
		/// </summary>
		/// <param name="f">File</param>
		/// <returns>FileMetaData for File</returns>
		public static implicit operator FileMetaData(File f)
		{
			FileMetaData s = new FileMetaData();
			if (f.Version != null && f.Version is Revision)
			{
				s.HeadRev = ((Revision)f.Version).Rev;
			}
			s.DepotPath = f.DepotPath;
			s.ClientPath = f.ClientPath;
			s.LocalPath = f.LocalPath;
			s.HaveRev = f.HaveRev.Rev;

			s.Change = f.ChangeId;
			s.Action = f.Action;
			s.Type = f.Type;
			
			return s;
		}

        /// <summary>
        /// Checks if the currently synced file is not at the latest revision
        /// </summary>
        /// <returns>true if file is stale</returns>
		public bool IsStale
		{
			get { return (HaveRev < HeadRev); }
		}
	}
	/// <summary>
	/// Describes how, or if a file has been resolved.
	/// </summary>
	public class FileResolveAction
	{
        /// <summary>
        /// Default Constructor
        /// </summary>
		public FileResolveAction()
		{
		}

        /// <summary>
        /// Parameterized Constructor
        /// </summary>
        /// <param name="resolveaction">Resolve Action</param>
        /// <param name="resolvebasefile">Base File</param>
        /// <param name="resolvefromfile">From File</param>
        /// <param name="resolvestartfromrev">Start From Revision</param>
        /// <param name="resolveendfromrev">End From Revision</param>
		public FileResolveAction
			(ResolveAction resolveaction, FileSpec resolvebasefile, FileSpec resolvefromfile,
			int resolvestartfromrev, int resolveendfromrev)
		{
			ResolveAction = resolveaction;
			ResolveBaseFile = resolvebasefile;
			ResolveFromFile = resolvefromfile;
			ResolveStartFromRev = resolvestartfromrev;
			ResolveEndFromRev = resolveendfromrev;
		}

        /// <summary>
        /// Action for the resolve
        /// </summary>
		public ResolveAction ResolveAction { get; set; }

        /// <summary>
        /// Base File
        /// </summary>
		public FileSpec ResolveBaseFile { get; set; }

        /// <summary>
        /// From File
        /// </summary>
		public FileSpec ResolveFromFile { get; set; }

        /// <summary>
        /// From File Start Revision
        /// </summary>
		public int ResolveStartFromRev { get; set; }

        /// <summary>
        /// From File End Revision
        /// </summary>
		public int ResolveEndFromRev { get; set; }
	}
	/// <summary>
	/// The action used in resolving the file.
	/// </summary>
	[Flags]
		public enum ResolveAction
		{
            /// <summary>
            /// No Action
            /// </summary>
			None = 0x0000,

            /// <summary>
            /// UnResolved Action
            /// </summary>
			Unresolved = 0x001,

            /// <summary>
            /// Copy From Action
            /// </summary>
			CopyFrom = 0x002,

            /// <summary>
            /// Merge From Action
            /// </summary>
			MergeFrom = 0x004,

            /// <summary>
            /// Edit From Action
            /// </summary>
			EditFrom = 0x008,

            /// <summary>
            /// Ignored Action
            /// </summary>
			Ignored = 0x010
		}
	
}
