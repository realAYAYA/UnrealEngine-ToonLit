/*******************************************************************************

Copyright (c) 2011, Perforce Software, Inc.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1.  Redistributions of source code must retain the above copyright
	notice, this list of conditions and the following disclaimer.

2.  Redistributions in binary form must reproduce the above copyright
	notice, this list of conditions and the following disclaimer in the
	documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL PERFORCE SOFTWARE, INC. BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*******************************************************************************/

/*******************************************************************************
 * Name		: Client.cs
 *
 * Author	: dbb
 *
 * Description	: Class used to abstract a client in Perforce.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Linq;

namespace Perforce.P4
{
	/// <summary>
	/// Flags to configure the client behavior.
	/// </summary>
	[Flags]
	public enum ClientOption
	{
		/// <summary>
		/// No options.
		/// </summary>
		None		= 0x0000,
		/// <summary>
		/// Leaves all files writable on the client;
		/// by default, only files opened by 'p4 edit'
		/// are writable. If set, files might be clobbered
		/// as a result of ignoring the clobber option.
		/// </summary>
		AllWrite	= 0x0001,
		/// <summary>
		/// Permits 'p4 sync' to overwrite writable
		/// files on the client.  noclobber is ignored if
		/// allwrite is set.
		/// </summary>
		Clobber		= 0x0002,
		/// <summary>
		/// Compresses data sent between the client
		/// and server to speed up slow connections.
		/// </summary>
		Compress	= 0x0004,
		/// <summary>
		/// Allows only the client owner to use or change
		/// the client spec.  Prevents the client spec from
		/// being deleted.
		/// </summary>
		Locked		= 0x0008,
		/// <summary>
		/// Causes 'p4 sync' and 'p4 submit' to preserve
		/// file modification time, as with files with the
		/// +m type modifier. (See 'p4 help filetypes'.)
		/// With nomodtime, file timestamps are updated by
		/// sync and submit operations.
		/// </summary>
		ModTime		= 0x0010,
		/// <summary>
		/// Makes 'p4 sync' attempt to delete a workspace
		/// directory when all files in it are removed.
		/// </summary>
		RmDir		= 0x0020
	};

	internal class ClientOptionEnum : StringEnum<ClientOption>
	{

		public ClientOptionEnum(ClientOption v)
			: base(v)
		{
		}

		public ClientOptionEnum(string spec)
			: base(ClientOption.None)
		{
			Parse(spec);
		}

		public static implicit operator ClientOptionEnum(ClientOption v)
		{
			return new ClientOptionEnum(v);
		}

		public static implicit operator ClientOptionEnum(string s)
		{
			return new ClientOptionEnum(s);
		}

		public static implicit operator string(ClientOptionEnum v)
		{
			return v.ToString();
		}

		public override bool Equals(object obj)
		{
			if (obj.GetType() == typeof(ClientOption))
			{
				return value.Equals((ClientOption)obj);
			}
			if (obj.GetType() == typeof(ClientOptionEnum))
			{
				return value.Equals(((ClientOptionEnum)obj).value);
			}
			return false;
        }
        public override int GetHashCode()
        {
            return base.GetHashCode();
        }
        public static bool operator ==(ClientOptionEnum t1, ClientOptionEnum t2) { return t1.value.Equals(t2.value); }
		public static bool operator !=(ClientOptionEnum t1, ClientOptionEnum t2) { return !t1.value.Equals(t2.value); }

		public static bool operator ==(ClientOption t1, ClientOptionEnum t2) { return t1.Equals(t2.value); }
		public static bool operator !=(ClientOption t1, ClientOptionEnum t2) { return !t1.Equals(t2.value); }

		public static bool operator ==(ClientOptionEnum t1, ClientOption t2) { return t1.value.Equals(t2); }
		public static bool operator !=(ClientOptionEnum t1, ClientOption t2) { return !t1.value.Equals(t2); }

		/// <summary>
		/// Convert to a client spec formatted string
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			return String.Format("{0} {1} {2} {3} {4} {5}",
				((value & ClientOption.AllWrite) != 0) ? "allwrite" : "noallwrite",
				((value & ClientOption.Clobber) != 0) ? "clobber" : "noclobber",
				((value & ClientOption.Compress) != 0) ? "compress" : "nocompress",
				((value & ClientOption.Locked) != 0) ? "locked" : "unlocked",
				((value & ClientOption.ModTime) != 0) ? "modtime" : "nomodtime",
				((value & ClientOption.RmDir) != 0) ? "rmdir" : "normdir"
				);
		}
		/// <summary>
		/// Parse a client spec formatted string
		/// </summary>
		/// <param name="spec"></param>
		public void Parse(String spec)
		{
			value = ClientOption.None;

			if (!spec.Contains("noallwrite"))
				value |= ClientOption.AllWrite;

			if (!spec.Contains("noclobber"))
				value |= ClientOption.Clobber;

			if (!spec.Contains("nocompress"))
				value |= ClientOption.Compress;

			if (!spec.Contains("unlocked"))
				value |= ClientOption.Locked;

			if (!spec.Contains("nomodtime"))
				value |= ClientOption.ModTime;

			if (!spec.Contains("normdir"))
				value |= ClientOption.RmDir;
		}
	}

	/// <summary>
	/// Flags to change submit behavior.
	/// </summary>
	[Flags]
	public enum SubmitType
	{
		/// <summary>
		/// All open files are submitted (default).
		/// </summary>
		SubmitUnchanged = 0x000,
		/// <summary>
		/// Files that have content or type changes
		/// are submitted. Unchanged files are
		/// reverted.
		/// </summary>
		RevertUnchanged = 0x001,
		/// <summary>
		/// Files that have content or type changes
		/// are submitted. Unchanged files are moved
		/// to the default changelist.
		/// </summary>
		LeaveUnchanged = 0x002
	}

	/// <summary>
	/// Client options that define what to do with files upon submit. 
	/// </summary>
	public class ClientSubmitOptions
	{
		/// <summary>
		/// Determines if the files is reopened upon submit.
		/// </summary>
		public bool Reopen { get; set; }

		public ClientSubmitOptions() { }
		public ClientSubmitOptions(string spec)
		{
			Parse(spec);
		}
		public ClientSubmitOptions(bool reopen, SubmitType submitType)
		{
			Reopen = reopen;
			_submitType = submitType;
		}

		public static implicit operator ClientSubmitOptions(string s)
		{
			return new ClientSubmitOptions(s);
		}

		public static implicit operator string(ClientSubmitOptions v)
		{
			return v.ToString();
		}
		public override bool Equals(object obj)
		{
			if (obj is ClientSubmitOptions)
			{
				ClientSubmitOptions o = obj as ClientSubmitOptions;
				return ((this._submitType == o._submitType) && (this.Reopen == o.Reopen));
			}
			return false;
		}
        public override int GetHashCode() { return base.GetHashCode(); }
        private StringEnum<SubmitType> _submitType;
		public SubmitType SubmitType 
		{
			get { return _submitType; }
			set { _submitType = value; }
		}
		/// <summary>
		/// Convert to a client spec formatted string
		/// </summary>
		/// <returns></returns>
		public override string ToString()
		{
			String value = _submitType.ToString(StringEnumCase.Lower);

			if (Reopen)
				value += "+reopen";

			return value;
		}
		/// <summary>
		/// Parse a client spec formatted string
		/// </summary>
		/// <param name="spec"></param>
		public void Parse(String spec)
		{
			_submitType = SubmitType.SubmitUnchanged;
			Reopen = false;

			if (spec.Contains("revertunchanged"))
				_submitType = SubmitType.RevertUnchanged;

			if (spec.Contains("leaveunchanged"))
				_submitType = SubmitType.LeaveUnchanged;

			if (spec.Contains("+reopen"))
				Reopen = true;
		}
	}

	/// <summary>
	///  Sets line-ending character(s) for client text files.
	/// </summary>
	[Flags]
	public enum LineEnd
	{ 
		/// <summary>
		/// mode that is native to the client (default).
		/// </summary>
		Local = 0x0000,
		/// <summary>
		/// linefeed: UNIX style.
		/// </summary>
		Unix = 0x0001,
		/// <summary>
		/// carriage return: Macintosh style.
		/// </summary>
		Mac = 0x0002,
		/// <summary>
		/// carriage return-linefeed: Windows style.
		/// </summary>
		Win = 0x0003,
		/// <summary>
		/// hybrid: writes UNIX style but reads UNIX,
		/// Mac or Windows style.
		/// </summary>
		Share = 0x0004
	}

	/// <summary>
	///  Sets Type for client spec.
	/// </summary>
	[Flags]
	public enum ClientType
	{
		/// <summary>
		/// mode that is native to the client (default).
		/// </summary>
		writeable = 0x0000,
		/// <summary>
		/// Type: readonly.
		/// </summary>
		@readonly = 0x0001,
		/// <summary>
		/// Type: graph.
		/// </summary>
		graph = 0x0002,
		/// <summary>
		/// Type: paritioned.
		/// </summary>
		partitioned = 0x0003,	
	}

	/// <summary>
	/// A client specification in a Perforce repository. 
	/// </summary>
	public class Client
	{
        /// <summary>
        /// Property is true if this Client has been initialized from the server
        /// </summary>
		public bool Initialized { get; set; }

        /// <summary>
        /// Go to the server and instantiate this Client
        /// </summary>
        /// <param name="connection">Connection to server</param>
		public void Initialize(Connection connection)
		{
			Initialized = false;
			if ((connection == null) || String.IsNullOrEmpty(Name))
			{
				P4Exception.Throw(ErrorSeverity.E_FAILED, "Client cannot be initialized");
				return;
			}
			Connection = connection;

			if (!connection.connectionEstablished())
			{
				// not connected to the server yet
				return;
			}
			P4Command cmd = new P4Command(connection, "client", true, "-o", Name);

			P4CommandResult results = cmd.Run();
			if ((results.Success) && (results.TaggedOutput != null) && (results.TaggedOutput.Count > 0))
			{
 				FromClientCmdTaggedOutput(results.TaggedOutput[0]);
				Initialized = true;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
		}
		internal Connection Connection { get; private set; }
		internal FormBase _baseForm;

        /// <summary>
        /// Property to access Name of Client
        /// </summary>
		public string Name { get; set; }
        /// <summary>
        /// Property to access Owner of Client
        /// </summary>
		public string OwnerName { get; set; }
        /// <summary>
        /// Property to access Host of Client
        /// </summary>
		public string Host { get; set; }
        /// <summary>
        /// Property to access Description of Client
        /// </summary>
		public string Description { get; set; }
        /// <summary>
        /// Property for when this Client was updated
        /// </summary>
		public DateTime Updated { get; set; }
        /// <summary>
        /// Property for when the Client was last accessed
        /// </summary>
		public DateTime Accessed { get; set; }
        /// <summary>
        /// Property for the Client root directory
        /// </summary>
		public string Root { get; set; }
        /// <summary>
        /// A List of Alternate Roots
        /// </summary>
		public IList<string> AltRoots { get; set; }
		/// <summary>
		/// Ties client files to a particular point in time
		/// </summary>
		public IList<string> ChangeView { get; set; }

		private ClientOptionEnum _options = ClientOption.None;
        
        /// <summary>
        /// Options for the Client command
        /// </summary>
		public ClientOption Options
		{
			get { return _options; }
			set { _options = (ClientOptionEnum) value; }
		}

		private StringEnum<ClientType> _clientType = ClientType.writeable;

		/// <summary>
		/// Type for the Client command
		/// </summary>
		public ClientType ClientType
		{
			get { return _clientType; }
			set { _clientType = value; }
		}

		/// <summary>
		/// Options for the Client about submit behavior
		/// </summary>
		public ClientSubmitOptions SubmitOptions = new ClientSubmitOptions(false, SubmitType.SubmitUnchanged);

        private StringEnum<LineEnd> _lineEnd = LineEnd.Local;

        /// <summary>
        /// Property to access line ending settings
        /// </summary>
		public LineEnd LineEnd
		{
			get { return _lineEnd; }
			set { _lineEnd = value; }
		}

        /// <summary>
        /// Stream associated with client
        /// </summary>
		public string Stream { get; set; }

        /// <summary>
        /// Stream at a specific change
        /// </summary>
		public string StreamAtChange { get; set; }

        /// <summary>
        /// Associated Server
        /// </summary>
		public string ServerID { get; set; }

		/// <summary>
		/// View Mapping
		/// </summary>
		public ViewMap ViewMap { get; set; }

        /// <summary>
        /// Form Specification for the Client
        /// </summary>
		public FormSpec Spec { get; set; }


        #region fromTaggedOutput
        /// <summary>
        /// Parse the tagged output of a 'clients' command
        /// </summary>
        /// <param name="workspaceInfo">taggedobject</param>
		/// <param name="offset">offset within array</param>
        /// <param name="dst_mismatch">Daylight savings for converting dates</param>
        public void FromClientsCmdTaggedOutput(TaggedObject workspaceInfo, string offset, bool dst_mismatch)
		{
			Initialized = true;

			_baseForm = new FormBase();
			_baseForm.SetValues(workspaceInfo);

			if (workspaceInfo.ContainsKey("client"))
				Name = workspaceInfo["client"];
			else if (workspaceInfo.ContainsKey("Client"))
				Name = workspaceInfo["Client"];

            if (workspaceInfo.ContainsKey("Update"))
			{
				long unixTime = 0;
				if (Int64.TryParse(workspaceInfo["Update"], out unixTime))
				{
                    DateTime UTC = FormBase.ConvertUnixTime(unixTime);
                    DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                        DateTimeKind.Unspecified);
                    Updated = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
				}
			}

			if (workspaceInfo.ContainsKey("Access"))
			{
				long unixTime = 0;
				if (Int64.TryParse(workspaceInfo["Access"], out unixTime))
				{
                    DateTime UTC = FormBase.ConvertUnixTime(unixTime);
                    DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                        DateTimeKind.Unspecified);
                    Accessed = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
				}
			}

			if (workspaceInfo.ContainsKey("Owner"))
				OwnerName = workspaceInfo["Owner"];

			if (workspaceInfo.ContainsKey("Options"))
			{
				String optionsStr = workspaceInfo["Options"];
				_options = optionsStr;
			}

			if (workspaceInfo.ContainsKey("SubmitOptions"))
			{
				SubmitOptions = workspaceInfo["SubmitOptions"];
			}

			if (workspaceInfo.ContainsKey("LineEnd"))
			{
				_lineEnd = workspaceInfo["LineEnd"];
			}

			if (workspaceInfo.ContainsKey("Type"))
			{
				_clientType = workspaceInfo["Type"];
			}

			if (workspaceInfo.ContainsKey("Root"))
				Root = workspaceInfo["Root"];

			if (workspaceInfo.ContainsKey("Host"))
				Host = workspaceInfo["Host"];

			if (workspaceInfo.ContainsKey("Description"))
				Description = workspaceInfo["Description"];

			if (workspaceInfo.ContainsKey("Stream"))
				Stream = workspaceInfo["Stream"];

			if (workspaceInfo.ContainsKey("StreamAtChange"))
				StreamAtChange = workspaceInfo["StreamAtChange"];

			if (workspaceInfo.ContainsKey("ServerID"))
				ServerID = workspaceInfo["ServerID"];

			int idx = 0;
			String key = String.Format("AltRoots{0}", idx);
			if (workspaceInfo.ContainsKey(key))
			{
				AltRoots = new List<String>();
				while (workspaceInfo.ContainsKey(key))
				{
					AltRoots.Add(workspaceInfo[key]);
					idx++;
					key = String.Format("AltRoots{0}", idx);
				}
			}

			idx = 0;
			key = String.Format("View{0}", idx);
			if (workspaceInfo.ContainsKey(key))
			{
				ViewMap = new ViewMap();
				while (workspaceInfo.ContainsKey(key))
				{
					ViewMap.Add(workspaceInfo[key]);
					idx++;
					key = String.Format("View{0}", idx);
				}
			}
			else
			{
				ViewMap = null;
			}

			idx = 0;
			key = String.Format("ChangeView{0}", idx);
			if (workspaceInfo.ContainsKey(key))
			{
				ChangeView = new List<String>();
				while (workspaceInfo.ContainsKey(key))
				{
					ChangeView.Add(workspaceInfo[key]);
					idx++;
					key = String.Format("ChangeView{0}", idx);
				}
			}
		}


		/// <summary>
		/// Parse the tagged output of a 'client' command
		/// </summary>
		/// <param name="workspaceInfo"></param>
		public void FromClientCmdTaggedOutput(TaggedObject workspaceInfo)
		{
			Initialized = true;

			_baseForm = new FormBase();
			_baseForm.SetValues(workspaceInfo);

			if (workspaceInfo.ContainsKey("client"))
				Name = workspaceInfo["client"];
			else if (workspaceInfo.ContainsKey("Client"))
				Name = workspaceInfo["Client"];

			if (workspaceInfo.ContainsKey("Update"))
			{
				DateTime d;
				if (DateTime.TryParse(workspaceInfo["Update"], out d))
				{
					Updated = d;
				}
			}

			if (workspaceInfo.ContainsKey("Access"))
			{
				DateTime d;
				if (DateTime.TryParse(workspaceInfo["Access"], out d))
				{
					Accessed = d;
				}
			}

			if (workspaceInfo.ContainsKey("Owner"))
				OwnerName = workspaceInfo["Owner"];

			if (workspaceInfo.ContainsKey("Options"))
			{
				String optionsStr = workspaceInfo["Options"];
				_options = optionsStr;
			}

			if (workspaceInfo.ContainsKey("SubmitOptions"))
			{
				SubmitOptions = workspaceInfo["SubmitOptions"];
			}

			if (workspaceInfo.ContainsKey("LineEnd"))
			{
				_lineEnd = workspaceInfo["LineEnd"];
			}

			if (workspaceInfo.ContainsKey("Type"))
			{
				_clientType = workspaceInfo["Type"];
			}

			if (workspaceInfo.ContainsKey("Root"))
				Root = workspaceInfo["Root"];

			if (workspaceInfo.ContainsKey("Host"))
				Host = workspaceInfo["Host"];

			if (workspaceInfo.ContainsKey("Description"))
				Description = workspaceInfo["Description"];

			if (workspaceInfo.ContainsKey("Stream"))
				Stream = workspaceInfo["Stream"];

			if (workspaceInfo.ContainsKey("StreamAtChange"))
				StreamAtChange = workspaceInfo["StreamAtChange"];

			if (workspaceInfo.ContainsKey("ServerID"))
				ServerID = workspaceInfo["ServerID"];

			int idx = 0;
			String key = String.Format("AltRoots{0}", idx);
			if (workspaceInfo.ContainsKey(key))
			{
				AltRoots = new List<String>();
				while (workspaceInfo.ContainsKey(key))
				{
					AltRoots.Add(workspaceInfo[key]);
					idx++;
					key = String.Format("AltRoots{0}", idx);
				}
			}

			idx = 0;
			key = String.Format("View{0}", idx);
			if (workspaceInfo.ContainsKey(key))
			{
				ViewMap = new ViewMap();
				while (workspaceInfo.ContainsKey(key))
				{
					ViewMap.Add(workspaceInfo[key]);
					idx++;
					key = String.Format("View{0}", idx);
				}
			}
			else
			{
				ViewMap = null;
			}
			
			idx = 0;
			key = String.Format("ChangeView{0}", idx);
			if (workspaceInfo.ContainsKey(key))
			{
				ChangeView = new List<String>();
				while (workspaceInfo.ContainsKey(key))
				{
					ChangeView.Add(workspaceInfo[key]);
					idx++;
					key = String.Format("ChangeView{0}", idx);
				}
			}
		}
		#endregion
		#region client spec support
		/// <summary>
		/// Parse a client spec
		/// </summary>
		/// <param name="spec"></param>
		/// <returns>true if parse successful</returns>
		public bool Parse(String spec)
		{
			_baseForm = new FormBase();
			_baseForm.Parse(spec); // parse the values into the underlying dictionary

			if (_baseForm.ContainsKey("Client"))
			{
				Name = _baseForm["Client"] as string;
			}
			if (_baseForm.ContainsKey("Host"))
			{
				Host = _baseForm["Host"] as string;
			}
			if (_baseForm.ContainsKey("Owner"))
			{
				OwnerName = _baseForm["Owner"] as string;
			}
			if (_baseForm.ContainsKey("Root"))
			{
				Root = _baseForm["Root"] as string;
			}
			if (_baseForm.ContainsKey("Description"))
			{
                if (_baseForm["Description"] is IList<string>)
                {
                    IList<string> strList = _baseForm["Description"] as IList<string>;
                    Description = string.Empty;
                    for (int idx = 0; idx < strList.Count; idx++)
                    {
                        if (idx > 0)
                        {
                            Description += "\r\n";
                        }
                        Description += strList[idx];
                    }
                }
                else if (_baseForm["Description"] is SimpleList<string>)
                {
                    SimpleList<string> strList = _baseForm["Description"] as SimpleList<string>;
                    Description = string.Empty;
                    SimpleListItem<string> current = strList.Head;
                    bool addCRLF = false;
                    while (current != null)
                    {
                        if (addCRLF)
                        {
                            Description += "\r\n";
                        }
                        else
                        {
                            addCRLF = true;
                        }
                        Description += current.Item;
                        current = current.Next;
                    }
                }
			}
            if (_baseForm.ContainsKey("AltRoots"))
            {
                if (_baseForm["AltRoots"] is IList<string>)
                {
                    AltRoots = _baseForm["AltRoots"] as IList<string>;
                }
                else if (_baseForm["AltRoots"] is SimpleList<string>)
                {
                    AltRoots = (List<string>) ((SimpleList<string>) _baseForm["AltRoots"]);
                }
            }
			if (_baseForm.ContainsKey("View"))
			{
                if (_baseForm["View"] is IList<string>)
                {
                    IList<string> lines = _baseForm["View"] as IList<string>;
                    ViewMap = new ViewMap(lines.ToArray());
                }
                else if (_baseForm["View"] is SimpleList<string>)
                {
                    SimpleList<string> lines = _baseForm["View"] as SimpleList<string>;
                    ViewMap = new ViewMap(lines.ToArray());
                }
			}

			if (_baseForm.ContainsKey("ChangeView"))
			{
				if (_baseForm["ChangeView"] is IList<string>)
				{
					ChangeView = _baseForm["ChnageView"] as IList<string>;
				}
				else if (_baseForm["ChangeView"] is SimpleList<string>)
				{
					ChangeView = (List<string>)((SimpleList<string>)_baseForm["ChangeView"]);
				}
			}

			if (_baseForm.ContainsKey("Update"))
			{
				DateTime d;
				if (DateTime.TryParse(_baseForm["Update"] as string, out d))
				{
					Updated = d;
				}
			}
			if (_baseForm.ContainsKey("Access"))
			{
				DateTime d;
				if (DateTime.TryParse(_baseForm["Access"] as string, out d))
				{
					Accessed = d;
				}
			}
			if (_baseForm.ContainsKey("Options"))
			{
				_options = _baseForm["Options"] as string;
			}
			if (_baseForm.ContainsKey("SubmitOptions"))
			{
				SubmitOptions = _baseForm["SubmitOptions"] as string;
			}
			if (_baseForm.ContainsKey("LineEnd"))
			{
				_lineEnd = _baseForm["LineEnd"] as string;
			}
			if (_baseForm.ContainsKey("Type"))
			{
				_clientType = _baseForm["Type"] as string;
			}
			if (_baseForm.ContainsKey("Stream"))
			{
				Stream = _baseForm["Stream"] as string;
			}
			if (_baseForm.ContainsKey("StreamAtChange"))
			{
				StreamAtChange = _baseForm["StreamAtChange"] as string;
			}
			if (_baseForm.ContainsKey("ServerID"))
			{
				ServerID = _baseForm["ServerID"] as string;
			}
			return true;
		}

		private static String ClientSpecFormat =
													"Client:\t{0}\n" +
													"\n" +
													"Update:\t{1}\n" +
													"\n" +
													"Access:\t{2}\n" +
													"\n" +
													"Owner:\t{3}\n" +
													"\n" +
													"Host:\t{4}\n" +
													"\n" +
													"Description:\n" +
													"\t{5}\n" +
													"\n" +
													"Root:\t{6}\n" +
													"\n" +
													"AltRoots:\n" +
													"\t{7}\n" +
													"\n" +
													"Options:\t{8}\n" +
													"\n" +
													"SubmitOptions:\t{9}\n" +
													"\n" +
													"LineEnd:\t{10}\n" +
													"\n" +
													"Type:\t{11}\n" +
													"\n" +
													"{12}" +
													"{13}" +
													"{14}" +
													"{15}" +
													"View:\n" +
													"\t{16}\n";
		private String AltRootsStr
		{
			get
			{
				String value = String.Empty;
				if ((AltRoots != null) && (AltRoots.Count > 0))
				{
					for (int idx = 0; idx < AltRoots.Count; idx++)
					{
						value += AltRoots[idx] + "\r\n";
					}
				}
				return value;
			}
		}
		private String ChangeViewStr
		{
			get
			{
				String value = String.Empty;
				if ((ChangeView != null) && (ChangeView.Count > 0))
				{
					for (int idx = 0; idx < ChangeView.Count; idx++)
					{
						value += ChangeView[idx] + "\r\n";
					}
				}
				return value;
			}
		}
		/// <summary>
		/// Utility function to format a DateTime in the format expected in a spec
		/// </summary>
		/// <param name="dt"></param>
		/// <returns>formatted date string</returns>
		public static String FormatDateTime(DateTime dt)
		{
			if ((dt != null) && (DateTime.MinValue != dt))
				return dt.ToString("yyyy/MM/dd HH:mm:ss", CultureInfo.InvariantCulture);
			return string.Empty;
		}

		/// <summary>
		/// Format as a client spec
		/// </summary>
		/// <returns>String description of client </returns>
		override public String ToString()
		{
            String tmpDescStr = String.Empty;
            if (!String.IsNullOrEmpty(Description))
            {
                tmpDescStr = FormBase.FormatMultilineField(Description.ToString());
            }
            String tmpAltRootsStr = String.Empty;
			if (!String.IsNullOrEmpty(AltRootsStr))
			{
                tmpAltRootsStr = FormBase.FormatMultilineField(AltRootsStr.ToString());
			}
			String tmpChangeViewStr = String.Empty;
			if (!String.IsNullOrEmpty(ChangeViewStr))
			{
				tmpChangeViewStr = FormBase.FormatMultilineField(ChangeViewStr.ToString());
				tmpChangeViewStr = "ChangeView:\t" + tmpChangeViewStr + "\n" + "\n";
			}
			String tmpViewStr = String.Empty;
			if (ViewMap != null)
			{
                tmpViewStr = FormBase.FormatMultilineField(ViewMap.ToString());
			}
			String tmpStreamStr = String.Empty;
			if (Stream != null)
			{
                tmpStreamStr = FormBase.FormatMultilineField(Stream.ToString());
				tmpStreamStr = "Stream:\t"+tmpStreamStr+"\n" + "\n";
			}
			String tmpStreamAtChangeStr = String.Empty;
			if (StreamAtChange != null)
			{
                tmpStreamAtChangeStr = FormBase.FormatMultilineField(StreamAtChange.ToString());
				tmpStreamAtChangeStr = "StreamAtChange:\t" + tmpStreamAtChangeStr + "\n" + "\n";
			}
			String tmpServerIDStr = String.Empty;
			if (ServerID != null)
			{
				tmpServerIDStr = FormBase.FormatMultilineField(ServerID.ToString());
				tmpServerIDStr = "ServerID:\t" + tmpServerIDStr + "\n" + "\n";
			}
			String value = String.Format(ClientSpecFormat, Name,
				FormatDateTime(Updated),
				FormatDateTime(Accessed),
				OwnerName, Host, tmpDescStr, Root, tmpAltRootsStr,
				_options.ToString(),
				SubmitOptions.ToString(),
				_lineEnd.ToString(), _clientType.ToString(), tmpStreamStr, tmpStreamAtChangeStr,
				tmpServerIDStr, tmpChangeViewStr, tmpViewStr);
			return value;
		}
		#endregion

		#region operations
		internal List<FileSpec> runFileListCmd(string cmdName, Options options, params FileSpec[] files)
		{
			return runFileListCmd(cmdName, options, null, files);
		}
		internal List<FileSpec> runFileListCmd(string cmdName, Options options, string commandData, params FileSpec[] files)
		{
			string[] paths = null;
			P4Command cmd = null;
			if (files != null)
			{
				if (cmdName == "add")
				{
					paths = FileSpec.ToStrings(files);
				}
				else
				{
					paths = FileSpec.ToEscapedStrings(files);
				}

				cmd = new P4Command(Connection, cmdName, true, paths);
			}
			else
			{
				cmd = new P4Command(Connection, cmdName, true);
			}
			if (String.IsNullOrEmpty(commandData) == false)
			{
				cmd.DataSet = commandData;
			}
			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				List<FileSpec> newDepotFiles = new List<FileSpec>();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
					FileSpec spec = null;
					int rev = -1;
					string p;
                    string action;

					DepotPath dp = null;
					ClientPath cp = null;
					LocalPath lp = null;

                    if (obj.ContainsKey("workRev"))
                    {
                        int.TryParse(obj["workRev"], out rev);
                    }
                    else if (obj.ContainsKey("haveRev"))
					{
						int.TryParse(obj["haveRev"], out rev);
					}
					else if (obj.ContainsKey("rev"))
					{
						int.TryParse(obj["rev"], out rev);
					}
                    if (obj.ContainsKey("action"))
                    {
                        action = obj["action"];
                        // if the file is marked for add, some
                        // commands will return a tagged field
                        // of rev that is 1. This is not accurate
                        // and can cause other commands that use
                        // File to incorrectly run the command
                        // on a file #1, which can result in
                        // "no such file(s)." So, change rev to
                        // -1 for files marked for add.
                        if (action == "add")
                        {
                            rev = -1;
                        }
                    }
                    if (obj.ContainsKey("depotFile"))
					{
						p = obj["depotFile"];
						dp = new DepotPath(PathSpec.UnescapePath(p));
					}
					if (obj.ContainsKey("clientFile"))
					{
						p = obj["clientFile"];
						if (p.StartsWith("//"))
						{
							cp = new ClientPath(PathSpec.UnescapePath(p));
						}
						else
						{
							cp = new ClientPath(PathSpec.UnescapePath(p));
							lp = new LocalPath(PathSpec.UnescapePath(p));
						}
					}
					if (obj.ContainsKey("path"))
					{
						lp = new LocalPath(obj["path"]);
					}
                    spec = new FileSpec();// (dp, cp, lp, new Revision(rev));
                    spec.ClientPath= cp;
                    spec.DepotPath = dp;
                    spec.LocalPath = lp;
                    if (rev>=0)
                    {
                        spec.Version = new Revision(rev);
                    }

                    newDepotFiles.Add(spec);
				}
				return newDepotFiles;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}

			return null;
		}

		/// <summary>
		/// 
		/// </summary>
		/// <param name="files"></param>
		/// <param name="options"></param>
		/// <returns></returns>
		/// <remarks>
		/// <br/><b>p4 help add</b>
		/// <br/> 
		/// <br/>     add -- Open a new file to add it to the depot
		/// <br/> 
		/// <br/>     p4 add [-c changelist#] [-d -f -I -n] [-t filetype] file ...
		/// <br/> 
		/// <br/> 	Open a file for adding to the depot.  If the file exists on the
		/// <br/> 	client, it is read to determine if it is text or binary. If it does
		/// <br/> 	not exist, it is assumed to be text.  To be added, the file must not
		/// <br/> 	already reside in the depot, or it must be deleted at the current
		/// <br/> 	head revision.  Files can be deleted and re-added.
		/// <br/> 
		/// <br/> 	A 2012.1 client will ignore files that were to be added, if they
		/// <br/> 	match an exclusion line specified in a P4IGNORE file.
		/// <br/> 
		/// <br/> 	To associate the open files with a specific pending changelist, use
		/// <br/> 	the -c flag; if you omit the -c flag, the open files are associated
		/// <br/> 	with the default changelist.  If file is already open, it is moved
		/// <br/> 	into the specified pending changelist.  You cannot reopen a file for
		/// <br/> 	add unless it is already open for add.
		/// <br/> 
		/// <br/> 	As a shortcut to reverting and re-adding, you can use the -d
		/// <br/> 	flag to reopen currently-open files for add (downgrade) under
		/// <br/> 	the following circumstances:
		/// <br/> 
		/// <br/> 	    A file that is 'opened for edit' and is synced to the head
		/// <br/> 	    revision, and the head revision has been deleted (or moved).
		/// <br/> 
		/// <br/> 	    A file that is 'opened for move/add' can be downgraded to add,
		/// <br/> 	    which is useful when the source of the move has been deleted
		/// <br/> 	    or moved.  Typically, under these circumstances, your only
		/// <br/> 	    alternative is to revert.  In this case, breaking the move
		/// <br/> 	    connection enables you to preserve any content changes in the
		/// <br/> 	    new file and safely revert the source file (of the move).
		/// <br/> 
		/// <br/> 	To specify file type, use the -t flag.  By default, 'p4 add'
		/// <br/> 	determines file type using the name-to-type mapping table managed
		/// <br/> 	by 'p4 typemap' and by examining the file's contents and execute
		/// <br/> 	permission bit. If the file type specified by -t or configured in
		/// <br/> 	the typemap table is a partial filetype, the resulting modifier is
		/// <br/> 	applied to the file type that is determined by 'p4 add'. For more
		/// <br/> 	details, see 'p4 help filetypes'.
		/// <br/> 
		/// <br/> 	To add files with filenames that contain wildcard characters, specify
		/// <br/> 	the -f flag. Filenames that contain the special characters '@', '#',
		/// <br/> 	'%' or '*' are reformatted to encode the characters using ASCII
		/// <br/> 	hexadecimal representation.  After the files are added, you must
		/// <br/> 	refer to them using the reformatted file name, because Perforce
		/// <br/> 	does not recognize the local filesystem name.
		/// <br/> 
		/// <br/> 	The -I flag informs the client that it should not perform any ignore
		/// <br/> 	checking configured by P4IGNORE.
		/// <br/> 
		/// <br/> 	The -n flag displays a preview of the specified add operation without
		/// <br/> 	changing any files or metadata.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> AddFiles(Options options, params FileSpec[] files)
		{
			return runFileListCmd("add", options, files);
		}

        /// <summary>
        /// Get a List of FileSpecs from adding files with options
        /// </summary>
        /// <param name="toFiles">List of FileSpecs of files to add</param>
        /// <param name="options">Options for the add command</param>
        /// <returns></returns>
		public IList<FileSpec> AddFiles(IList<FileSpec> toFiles, Options options)
		{
			return AddFiles(options, toFiles.ToArray<FileSpec>());
		}

		/// <summary>
		/// Delete Files
		/// </summary>
		/// <param name="files">Files to delete</param>
		/// <param name="options">Options to the delete command</param>
		/// <returns>A list of FileSpecs for the deleted comand</returns>
		/// <remarks>
		/// <br/><b>p4 help delete</b>
		/// <br/> 
		/// <br/>     delete -- Open an existing file for deletion from the depot
		/// <br/> 
		/// <br/>     p4 delete [-c changelist#] [-n -v -k] file ...
		/// <br/> 
		/// <br/> 	Opens a depot file for deletion.
		/// <br/> 	If the file is synced in the client workspace, it is removed.  If a
		/// <br/> 	pending changelist number is specified using with the -c flag, the
		/// <br/> 	file is opened for delete in that changelist. Otherwise, it is opened
		/// <br/> 	in the default pending changelist.
		/// <br/> 
		/// <br/> 	Files that are deleted generally do not appear on the have list.
		/// <br/> 
		/// <br/> 	The -n flag displays a preview of the operation without changing any
		/// <br/> 	files or metadata.
		/// <br/> 
		/// <br/> 	The -k flag performs the delete on the server without modifying
		/// <br/> 	client files.  Use with caution, as an incorrect delete can cause
		/// <br/> 	discrepancies between the state of the client and the corresponding
		/// <br/> 	server metadata.
		/// <br/> 
		/// <br/> 	The -v flag enables you to delete files that are not synced to the
		/// <br/> 	client workspace. The files should be specified using depot syntax;
		/// <br/> 	since the files are not synced, client syntax or local syntax are
		/// <br/> 	inappropriate and could introduce ambiguities in the file list.
		/// <br/> 	Note, though, that if the files ARE synced, 'delete -v' will remove
		/// <br/> 	the files from the client in addition to opening them for delete.
		/// <br/> 	The preferred way to delete a set of files without transferring
		/// <br/> 	them to your machine is: 'sync -k file...', then 'delete -k file...'
		/// <br/> 
		/// <br/> 	'p4 delete' is not supported for files with propagating attributes
		/// <br/> 	from an edge server in a distributed environment.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> DeleteFiles(Options options, params FileSpec[] files)
		{
			return runFileListCmd("delete", options, files);
		}

        /// <summary>
        /// Delete Files
        /// </summary>
        /// <param name="toFiles">Files to delete</param>
        /// <param name="options">Options to the command</param>
        /// <returns>List of Files deleted</returns>
		public IList<FileSpec> DeleteFiles(IList<FileSpec> toFiles, Options options)
		{
			return DeleteFiles(options, toFiles.ToArray<FileSpec>());
		}

		/// <summary>
		/// Edit Files
		/// </summary>
		/// <param name="files">Array of Files to Edit</param>
		/// <param name="options">Options to Edit command</param>
		/// <returns>List of edited files</returns>
		/// <remarks>
		/// <br/><b>p4 help edit</b>
		/// <br/> 
		/// <br/>     edit -- Open an existing file for edit
		/// <br/> 
		/// <br/>     p4 edit [-c changelist#] [-k -n] [-t filetype] file ...
		/// <br/> 
		/// <br/> 	Open an existing file for edit.  The server records the fact that
		/// <br/> 	the current user has opened the file in the current workspace, and
		/// <br/> 	changes the file permission from read-only to read/write.
		/// <br/> 
		/// <br/> 	If -c changelist# is included, the file opened in the specified
		/// <br/> 	pending changelist.  If changelist number is omitted, the file is
		/// <br/> 	opened in the 'default' changelist.
		/// <br/> 
		/// <br/> 	If -t filetype is specified, the file is assigned that Perforce
		/// <br/> 	filetype. Otherwise, the filetype of the previous revision is reused.
		/// <br/> 	If a partial filetype is specified, it is combined with the current
		/// <br/> 	filetype.For details, see 'p4 help filetypes'.
		/// <br/> 	Using a filetype of 'auto' will cause the filetype to be chosen
		/// <br/> 	as if the file were being added, that is the typemap will be
		/// <br/> 	considered and the file contents may be examined.
		/// <br/> 
		/// <br/> 	The -n flag previews the operation without changing any files or
		/// <br/> 	metadata.
		/// <br/> 
		/// <br/> 	The -k flag updates metadata without transferring files to the
		/// <br/> 	workspace. This option can be used to tell the server that files in
		/// <br/> 	a client workspace are already editable, even if they are not in the
		/// <br/> 	client view. Typically this flag is used to correct the Perforce
		/// <br/> 	server when it is wrong about the state of files in the client
		/// <br/> 	workspace, but incorrect use of this option can result in inaccurate
		/// <br/> 	file status information.
		/// <br/> 
		/// <br/> 	'p4 edit' is not supported for files with propagating attributes
		/// <br/> 	from an edge server in a distributed environment.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> EditFiles(Options options, params FileSpec[] files)
		{
			return runFileListCmd("edit", options, files);
		}

        /// <summary>
        /// Edit Files
        /// </summary>
        /// <param name="toFiles">List of Files to Edit</param>
        /// <param name="options">Options to Edit command</param>
        /// <returns>List of edited files</returns>
		public IList<FileSpec> EditFiles(IList<FileSpec> toFiles, Options options)
		{
			return EditFiles(options, toFiles.ToArray<FileSpec>());
		}

		/// <summary>
		/// List files and revisions we have in workspace
		/// </summary>
		/// <param name="files">Array of files to check</param>
		/// <param name="options">options to the command</param>
		/// <returns>List of files we have</returns>
		/// <remarks>
		/// <br/><b>p4 help have</b>
		/// <br/> 
		/// <br/>     have -- List the revisions most recently synced to the current workspace
		/// <br/> 
		/// <br/>     p4 have [file ...]
		/// <br/> 
		/// <br/> 	List revision numbers of the currently-synced files. If file name is
		/// <br/> 	omitted, list all files synced to this client workspace.
		/// <br/> 
		/// <br/> 	The format is:  depot-file#revision - client-file
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> GetSyncedFiles(Options options, params FileSpec[] files)
		{
			if (options != null)
			{
				throw new ArgumentException("GetSynchedFiles has no valid options", "options");
			}
			return runFileListCmd("have", options, files);
		}

        /// <summary>
        /// List files and revisions we have in workspace
        /// </summary>
        /// <param name="toFiles">List of files to check</param>
        /// <param name="options">options to the command</param>
        /// <returns>List of files we have</returns>
		public IList<FileSpec> GetSyncedFiles(IList<FileSpec> toFiles, Options options)
		{
			return GetSyncedFiles(options, toFiles.ToArray<FileSpec>());
		}

		/// <summary>
		/// Integrate Files
		/// </summary>
		/// <param name="files">Array of files to integrate</param>
		/// <param name="options">options to the command</param>
		/// <returns>List of integrated Files</returns>
		/// <remarks>
		/// <br/><b>p4 help integrate</b>
		/// <br/> 
		/// <br/>     integrate -- Integrate one set of files into another
		/// <br/> 
		/// <br/>     p4 integrate [options] fromFile[revRange] toFile
		/// <br/>     p4 integrate [options] -b branch [-r] [toFile[revRange] ...]
		/// <br/>     p4 integrate [options] -b branch -s fromFile[revRange] [toFile ...]
		/// <br/>     p4 integrate [options] -S stream [-r] [-P parent] [file[revRange] ...]
		/// <br/> 
		/// <br/> 	options: -c changelist# -Di -f -h -O&lt;flags&gt; -n -m max -R&lt;flags&gt; -q -v
		/// <br/> 
		/// <br/> 	'p4 integrate' integrates one set of files (the 'source') into
		/// <br/> 	another (the 'target').
		/// <br/> 
		/// <br/> 	(See also 'p4 merge' and 'p4 copy', variants of 'p4 integrate' that
		/// <br/> 	may be easier and more effective for the task at hand.) 
		/// <br/> 
		/// <br/> 	Using the client workspace as a staging area, 'p4 integrate' adds and
		/// <br/> 	deletes target files per changes in the source, and schedules all
		/// <br/> 	other affected target files to be resolved.  Target files outside of
		/// <br/> 	the current client view are not affected. Source files need not be
		/// <br/> 	within the client view.
		/// <br/> 
		/// <br/> 	'p4 resolve' must be used to merge file content, and to resolve
		/// <br/> 	filename and filetype changes. 'p4 submit' commits integrated files
		/// <br/> 	to the depot.  Unresolved files may not be submitted.  Integrations
		/// <br/> 	can be shelved with 'p4 shelve' and abandoned with 'p4 revert'.  The
		/// <br/> 	commands 'p4 integrated' and 'p4 filelog' display integration history.
		/// <br/> 
		/// <br/> 	When 'p4 integrate' schedules a workspace file to be resolved, it
		/// <br/> 	leaves it read-only. 'p4 resolve' can operate on a read-only file.
		/// <br/> 	For other pre-submit changes, 'p4 edit' must be used to make the
		/// <br/> 	file writable.
		/// <br/> 
		/// <br/> 	Source and target files can be specified either on the 'p4 integrate'
		/// <br/> 	command line or through a branch view. On the command line, fromFile
		/// <br/> 	is the source file set and toFile is the target file set.  With a
		/// <br/> 	branch view, one or more toFile arguments can be given to limit the
		/// <br/> 	scope of the target file set.
		/// <br/> 
		/// <br/> 	revRange is a revision or a revision range that limits the span of
		/// <br/> 	source history to be probed for unintegrated revisions.  revRange
		/// <br/> 	can be used on fromFile, or on toFile, but not on both.  When used on
		/// <br/> 	toFile, it refers to source revisions, not to target revisions.  For
		/// <br/> 	details about revision specifiers, see 'p4 help revisions'.
		/// <br/> 
		/// <br/> 	The -S flag makes 'p4 integrate' use a generated branch view that maps
		/// <br/> 	a stream (or its underlying real stream) to its parent.  With -r, the
		/// <br/> 	direction of the mapping is reversed.  -P can be used to generate the
		/// <br/> 	branch view using a parent stream other than the stream's actual
		/// <br/> 	parent.  Note that to submit integrated stream files, the current
		/// <br/> 	client must be switched to the target stream, or to a virtual child
		/// <br/> 	stream of the target stream.
		/// <br/> 
		/// <br/> 	The -b flag makes 'p4 integrate' use a user-defined branch view.
		/// <br/> 	(See 'p4 help branch'.) The source is the left side of the branch view
		/// <br/> 	and the target is the right side. With -r, the direction is reversed.
		/// <br/> 
		/// <br/> 	The -s flag can be used with -b to cause fromFile to be treated as
		/// <br/> 	the source, and both sides of the branch view to be treated as the
		/// <br/> 	target, per the branch view mapping.  Optional toFile arguments may
		/// <br/> 	be given to further restrict the scope of the target file set.  The
		/// <br/> 	-r flag is ignored when -s is used.
		/// <br/> 
		/// <br/> 	Note that 'p4 integrate' automatically adusts source-to-target
		/// <br/> 	mappings for moved and renamed files.  (Adjustment occurs only if
		/// <br/> 	the 'p4 move' command was used to move/rename files.) The scope of
		/// <br/> 	source and target file sets must include both the old-named and the
		/// <br/> 	new-named files for mappings to be adjusted.  A filename resolve is
		/// <br/> 	scheduled for each remapped file to allow the target to be moved to
		/// <br/> 	match the source.
		/// <br/> 
		/// <br/> 	The -f flag forces integrate to ignore integration history and treat
		/// <br/> 	all source revisions as unintegrated. It is meant to be used with
		/// <br/> 	revRange to force reintegration of specific, previously integrated
		/// <br/> 	revisions. 
		/// <br/> 
		/// <br/> 	The -O flags cause more information to be output for each file opened:
		/// <br/> 
		/// <br/> 		-Ob	Show the base revision for the merge (if any).
		/// <br/> 		-Or	Show the resolve(s) that are being scheduled.
		/// <br/> 
		/// <br/> 	The -R flags modify the way resolves are scheduled:
		/// <br/> 
		/// <br/> 		-Rb	Schedules 'branch resolves' instead of branching new
		/// <br/> 			target files automatically.
		/// <br/> 
		/// <br/> 		-Rd	Schedules 'delete resolves' instead of deleting
		/// <br/> 			target files automatically.
		/// <br/> 
		/// <br/> 		-Rs	Skips cherry-picked revisions already integrated.
		/// <br/> 			This can improve merge results, but can also cause
		/// <br/> 			multiple resolves per file to be scheduled.
		/// <br/> 
		/// <br/> 	The -Di flag modifies the way deleted revisions are treated.  If the
		/// <br/> 	source file has been deleted and re-added, revisions that precede
		/// <br/> 	the deletion will be considered to be part of the same source file.
		/// <br/> 	By default, re-added files are considered to be unrelated to the
		/// <br/> 	files of the same name that preceded them.
		/// <br/> 
		/// <br/> 	The -h flag leaves the target files at the revision currently synced
		/// <br/> 	to the client (the '#have' revision). By default, target files are
		/// <br/> 	automatically synced to the head revision by 'p4 integrate'.
		/// <br/> 
		/// <br/> 	The -m flag limits integration to the first 'max' number of files.
		/// <br/> 
		/// <br/> 	The -n flag displays a preview of integration, without actually
		/// <br/> 	doing anything.
		/// <br/> 
		/// <br/> 	The -q flag suppresses normal output messages. Messages regarding
		/// <br/> 	errors or exceptional conditions are displayed.
		/// <br/> 
		/// <br/> 	If -c changelist# is specified, the files are opened in the
		/// <br/> 	designated numbered pending changelist instead of the 'default'
		/// <br/> 	changelist.
		/// <br/> 
		/// <br/> 	The -v flag causes a 'virtual' integration that does not modify
		/// <br/> 	client workspace files unless target files need to be resolved.
		/// <br/> 	After submitting a virtual integration, 'p4 sync' can be used to
		/// <br/> 	update the workspace.
		/// <br/> 
		/// <br/> 	Integration is not supported for files with propagating attributes
		/// <br/> 	from an edge server in a distributed environment. Depending on the
		/// <br/> 	integration action, target, and source, either the integration or
		/// <br/> 	resolve command will fail.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> IntegrateFiles(Options options, params FileSpec[] files)
		{
			return runFileListCmd("integrate", options, files);
		}

        /// <summary>
        /// Integrate Files
        /// </summary>
        /// <param name="toFiles">List of files to integrate</param>
        /// <param name="options">options to the command</param>
        /// <returns>List of integrated Files</returns>
		public IList<FileSpec> IntegrateFiles(IList<FileSpec> toFiles, Options options)
		{
			return IntegrateFiles(options, toFiles.ToArray<FileSpec>());
		}

		/// <summary>
		/// Integrate one set of files into another
		/// </summary>
		/// <param name="fromFile">File to integrate from</param>
		/// <param name="toFiles">Array of files to integrate into</param>
		/// <param name="options">Options for the command</param>
		/// <returns>List of integrated files</returns>
		/// <remarks>
		/// <br/><b>p4 help integrate</b>
		/// <br/> 
		/// <br/>     integrate -- Integrate one set of files into another
		/// <br/> 
		/// <br/>     p4 integrate [options] fromFile[revRange] toFile
		/// <br/>     p4 integrate [options] -b branch [-r] [toFile[revRange] ...]
		/// <br/>     p4 integrate [options] -b branch -s fromFile[revRange] [toFile ...]
		/// <br/>     p4 integrate [options] -S stream [-r] [-P parent] [file[revRange] ...]
		/// <br/> 
		/// <br/> 	options: -c changelist# -Di -f -h -O&lt;flags&gt; -n -m max -R&lt;flags&gt; -q -v
		/// <br/> 
		/// <br/> 	'p4 integrate' integrates one set of files (the 'source') into
		/// <br/> 	another (the 'target').
		/// <br/> 
		/// <br/> 	(See also 'p4 merge' and 'p4 copy', variants of 'p4 integrate' that
		/// <br/> 	may be easier and more effective for the task at hand.) 
		/// <br/> 
		/// <br/> 	Using the client workspace as a staging area, 'p4 integrate' adds and
		/// <br/> 	deletes target files per changes in the source, and schedules all
		/// <br/> 	other affected target files to be resolved.  Target files outside of
		/// <br/> 	the current client view are not affected. Source files need not be
		/// <br/> 	within the client view.
		/// <br/> 
		/// <br/> 	'p4 resolve' must be used to merge file content, and to resolve
		/// <br/> 	filename and filetype changes. 'p4 submit' commits integrated files
		/// <br/> 	to the depot.  Unresolved files may not be submitted.  Integrations
		/// <br/> 	can be shelved with 'p4 shelve' and abandoned with 'p4 revert'.  The
		/// <br/> 	commands 'p4 integrated' and 'p4 filelog' display integration history.
		/// <br/> 
		/// <br/> 	When 'p4 integrate' schedules a workspace file to be resolved, it
		/// <br/> 	leaves it read-only. 'p4 resolve' can operate on a read-only file.
		/// <br/> 	For other pre-submit changes, 'p4 edit' must be used to make the
		/// <br/> 	file writable.
		/// <br/> 
		/// <br/> 	Source and target files can be specified either on the 'p4 integrate'
		/// <br/> 	command line or through a branch view. On the command line, fromFile
		/// <br/> 	is the source file set and toFile is the target file set.  With a
		/// <br/> 	branch view, one or more toFile arguments can be given to limit the
		/// <br/> 	scope of the target file set.
		/// <br/> 
		/// <br/> 	revRange is a revision or a revision range that limits the span of
		/// <br/> 	source history to be probed for unintegrated revisions.  revRange
		/// <br/> 	can be used on fromFile, or on toFile, but not on both.  When used on
		/// <br/> 	toFile, it refers to source revisions, not to target revisions.  For
		/// <br/> 	details about revision specifiers, see 'p4 help revisions'.
		/// <br/> 
		/// <br/> 	The -S flag makes 'p4 integrate' use a generated branch view that maps
		/// <br/> 	a stream (or its underlying real stream) to its parent.  With -r, the
		/// <br/> 	direction of the mapping is reversed.  -P can be used to generate the
		/// <br/> 	branch view using a parent stream other than the stream's actual
		/// <br/> 	parent.  Note that to submit integrated stream files, the current
		/// <br/> 	client must be switched to the target stream, or to a virtual child
		/// <br/> 	stream of the target stream.
		/// <br/> 
		/// <br/> 	The -b flag makes 'p4 integrate' use a user-defined branch view.
		/// <br/> 	(See 'p4 help branch'.) The source is the left side of the branch view
		/// <br/> 	and the target is the right side. With -r, the direction is reversed.
		/// <br/> 
		/// <br/> 	The -s flag can be used with -b to cause fromFile to be treated as
		/// <br/> 	the source, and both sides of the branch view to be treated as the
		/// <br/> 	target, per the branch view mapping.  Optional toFile arguments may
		/// <br/> 	be given to further restrict the scope of the target file set.  The
		/// <br/> 	-r flag is ignored when -s is used.
		/// <br/> 
		/// <br/> 	Note that 'p4 integrate' automatically adusts source-to-target
		/// <br/> 	mappings for moved and renamed files.  (Adjustment occurs only if
		/// <br/> 	the 'p4 move' command was used to move/rename files.) The scope of
		/// <br/> 	source and target file sets must include both the old-named and the
		/// <br/> 	new-named files for mappings to be adjusted.  A filename resolve is
		/// <br/> 	scheduled for each remapped file to allow the target to be moved to
		/// <br/> 	match the source.
		/// <br/> 
		/// <br/> 	The -f flag forces integrate to ignore integration history and treat
		/// <br/> 	all source revisions as unintegrated. It is meant to be used with
		/// <br/> 	revRange to force reintegration of specific, previously integrated
		/// <br/> 	revisions. 
		/// <br/> 
		/// <br/> 	The -O flags cause more information to be output for each file opened:
		/// <br/> 
		/// <br/> 		-Ob	Show the base revision for the merge (if any).
		/// <br/> 		-Or	Show the resolve(s) that are being scheduled.
		/// <br/> 
		/// <br/> 	The -R flags modify the way resolves are scheduled:
		/// <br/> 
		/// <br/> 		-Rb	Schedules 'branch resolves' instead of branching new
		/// <br/> 			target files automatically.
		/// <br/> 
		/// <br/> 		-Rd	Schedules 'delete resolves' instead of deleting
		/// <br/> 			target files automatically.
		/// <br/> 
		/// <br/> 		-Rs	Skips cherry-picked revisions already integrated.
		/// <br/> 			This can improve merge results, but can also cause
		/// <br/> 			multiple resolves per file to be scheduled.
		/// <br/> 
		/// <br/> 	The -Di flag modifies the way deleted revisions are treated.  If the
		/// <br/> 	source file has been deleted and re-added, revisions that precede
		/// <br/> 	the deletion will be considered to be part of the same source file.
		/// <br/> 	By default, re-added files are considered to be unrelated to the
		/// <br/> 	files of the same name that preceded them.
		/// <br/> 
		/// <br/> 	The -h flag leaves the target files at the revision currently synced
		/// <br/> 	to the client (the '#have' revision). By default, target files are
		/// <br/> 	automatically synced to the head revision by 'p4 integrate'.
		/// <br/> 
		/// <br/> 	The -m flag limits integration to the first 'max' number of files.
		/// <br/> 
		/// <br/> 	The -n flag displays a preview of integration, without actually
		/// <br/> 	doing anything.
		/// <br/> 
		/// <br/> 	The -q flag suppresses normal output messages. Messages regarding
		/// <br/> 	errors or exceptional conditions are displayed.
		/// <br/> 
		/// <br/> 	If -c changelist# is specified, the files are opened in the
		/// <br/> 	designated numbered pending changelist instead of the 'default'
		/// <br/> 	changelist.
		/// <br/> 
		/// <br/> 	The -v flag causes a 'virtual' integration that does not modify
		/// <br/> 	client workspace files unless target files need to be resolved.
		/// <br/> 	After submitting a virtual integration, 'p4 sync' can be used to
		/// <br/> 	update the workspace.
		/// <br/> 
		/// <br/> 	Integration is not supported for files with propagating attributes
		/// <br/> 	from an edge server in a distributed environment. Depending on the
		/// <br/> 	integration action, target, and source, either the integration or
		/// <br/> 	resolve command will fail.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> IntegrateFiles(FileSpec fromFile, Options options, params FileSpec[] toFiles)
		{
			FileSpec[] newParams = new FileSpec[toFiles.Length + 1];
			newParams[0] = fromFile;
			for (int idx = 0; idx < toFiles.Length; idx++)
			{
				newParams[idx + 1] = toFiles[idx];
			}
			return runFileListCmd("integrate", options, newParams);
		}

		/// <summary>
		/// Integrate a file into others
		/// </summary>
		/// <param name="toFiles">List of files to integrate into</param>
		/// <param name="fromFile">File to integrate from</param>
		/// <param name="options">options to the command</param>
		/// <returns>List of integrated files</returns>
		/// <remarks>
		/// <br/><b>p4 help integrate</b>
		/// <br/> 
		/// <br/>     integrate -- Integrate one set of files into another
		/// <br/> 
		/// <br/>     p4 integrate [options] fromFile[revRange] toFile
		/// <br/>     p4 integrate [options] -b branch [-r] [toFile[revRange] ...]
		/// <br/>     p4 integrate [options] -b branch -s fromFile[revRange] [toFile ...]
		/// <br/>     p4 integrate [options] -S stream [-r] [-P parent] [file[revRange] ...]
		/// <br/> 
		/// <br/> 	options: -c changelist# -Di -f -h -O&lt;flags&gt; -n -m max -R&lt;flags&gt; -q -v
		/// <br/> 
		/// <br/> 	'p4 integrate' integrates one set of files (the 'source') into
		/// <br/> 	another (the 'target').
		/// <br/> 
		/// <br/> 	(See also 'p4 merge' and 'p4 copy', variants of 'p4 integrate' that
		/// <br/> 	may be easier and more effective for the task at hand.) 
		/// <br/> 
		/// <br/> 	Using the client workspace as a staging area, 'p4 integrate' adds and
		/// <br/> 	deletes target files per changes in the source, and schedules all
		/// <br/> 	other affected target files to be resolved.  Target files outside of
		/// <br/> 	the current client view are not affected. Source files need not be
		/// <br/> 	within the client view.
		/// <br/> 
		/// <br/> 	'p4 resolve' must be used to merge file content, and to resolve
		/// <br/> 	filename and filetype changes. 'p4 submit' commits integrated files
		/// <br/> 	to the depot.  Unresolved files may not be submitted.  Integrations
		/// <br/> 	can be shelved with 'p4 shelve' and abandoned with 'p4 revert'.  The
		/// <br/> 	commands 'p4 integrated' and 'p4 filelog' display integration history.
		/// <br/> 
		/// <br/> 	When 'p4 integrate' schedules a workspace file to be resolved, it
		/// <br/> 	leaves it read-only. 'p4 resolve' can operate on a read-only file.
		/// <br/> 	For other pre-submit changes, 'p4 edit' must be used to make the
		/// <br/> 	file writable.
		/// <br/> 
		/// <br/> 	Source and target files can be specified either on the 'p4 integrate'
		/// <br/> 	command line or through a branch view. On the command line, fromFile
		/// <br/> 	is the source file set and toFile is the target file set.  With a
		/// <br/> 	branch view, one or more toFile arguments can be given to limit the
		/// <br/> 	scope of the target file set.
		/// <br/> 
		/// <br/> 	revRange is a revision or a revision range that limits the span of
		/// <br/> 	source history to be probed for unintegrated revisions.  revRange
		/// <br/> 	can be used on fromFile, or on toFile, but not on both.  When used on
		/// <br/> 	toFile, it refers to source revisions, not to target revisions.  For
		/// <br/> 	details about revision specifiers, see 'p4 help revisions'.
		/// <br/> 
		/// <br/> 	The -S flag makes 'p4 integrate' use a generated branch view that maps
		/// <br/> 	a stream (or its underlying real stream) to its parent.  With -r, the
		/// <br/> 	direction of the mapping is reversed.  -P can be used to generate the
		/// <br/> 	branch view using a parent stream other than the stream's actual
		/// <br/> 	parent.  Note that to submit integrated stream files, the current
		/// <br/> 	client must be switched to the target stream, or to a virtual child
		/// <br/> 	stream of the target stream.
		/// <br/> 
		/// <br/> 	The -b flag makes 'p4 integrate' use a user-defined branch view.
		/// <br/> 	(See 'p4 help branch'.) The source is the left side of the branch view
		/// <br/> 	and the target is the right side. With -r, the direction is reversed.
		/// <br/> 
		/// <br/> 	The -s flag can be used with -b to cause fromFile to be treated as
		/// <br/> 	the source, and both sides of the branch view to be treated as the
		/// <br/> 	target, per the branch view mapping.  Optional toFile arguments may
		/// <br/> 	be given to further restrict the scope of the target file set.  The
		/// <br/> 	-r flag is ignored when -s is used.
		/// <br/> 
		/// <br/> 	Note that 'p4 integrate' automatically adusts source-to-target
		/// <br/> 	mappings for moved and renamed files.  (Adjustment occurs only if
		/// <br/> 	the 'p4 move' command was used to move/rename files.) The scope of
		/// <br/> 	source and target file sets must include both the old-named and the
		/// <br/> 	new-named files for mappings to be adjusted.  A filename resolve is
		/// <br/> 	scheduled for each remapped file to allow the target to be moved to
		/// <br/> 	match the source.
		/// <br/> 
		/// <br/> 	The -f flag forces integrate to ignore integration history and treat
		/// <br/> 	all source revisions as unintegrated. It is meant to be used with
		/// <br/> 	revRange to force reintegration of specific, previously integrated
		/// <br/> 	revisions. 
		/// <br/> 
		/// <br/> 	The -O flags cause more information to be output for each file opened:
		/// <br/> 
		/// <br/> 		-Ob	Show the base revision for the merge (if any).
		/// <br/> 		-Or	Show the resolve(s) that are being scheduled.
		/// <br/> 
		/// <br/> 	The -R flags modify the way resolves are scheduled:
		/// <br/> 
		/// <br/> 		-Rb	Schedules 'branch resolves' instead of branching new
		/// <br/> 			target files automatically.
		/// <br/> 
		/// <br/> 		-Rd	Schedules 'delete resolves' instead of deleting
		/// <br/> 			target files automatically.
		/// <br/> 
		/// <br/> 		-Rs	Skips cherry-picked revisions already integrated.
		/// <br/> 			This can improve merge results, but can also cause
		/// <br/> 			multiple resolves per file to be scheduled.
		/// <br/> 
		/// <br/> 	The -Di flag modifies the way deleted revisions are treated.  If the
		/// <br/> 	source file has been deleted and re-added, revisions that precede
		/// <br/> 	the deletion will be considered to be part of the same source file.
		/// <br/> 	By default, re-added files are considered to be unrelated to the
		/// <br/> 	files of the same name that preceded them.
		/// <br/> 
		/// <br/> 	The -h flag leaves the target files at the revision currently synced
		/// <br/> 	to the client (the '#have' revision). By default, target files are
		/// <br/> 	automatically synced to the head revision by 'p4 integrate'.
		/// <br/> 
		/// <br/> 	The -m flag limits integration to the first 'max' number of files.
		/// <br/> 
		/// <br/> 	The -n flag displays a preview of integration, without actually
		/// <br/> 	doing anything.
		/// <br/> 
		/// <br/> 	The -q flag suppresses normal output messages. Messages regarding
		/// <br/> 	errors or exceptional conditions are displayed.
		/// <br/> 
		/// <br/> 	If -c changelist# is specified, the files are opened in the
		/// <br/> 	designated numbered pending changelist instead of the 'default'
		/// <br/> 	changelist.
		/// <br/> 
		/// <br/> 	The -v flag causes a 'virtual' integration that does not modify
		/// <br/> 	client workspace files unless target files need to be resolved.
		/// <br/> 	After submitting a virtual integration, 'p4 sync' can be used to
		/// <br/> 	update the workspace.
		/// <br/> 
		/// <br/> 	Integration is not supported for files with propagating attributes
		/// <br/> 	from an edge server in a distributed environment. Depending on the
		/// <br/> 	integration action, target, and source, either the integration or
		/// <br/> 	resolve command will fail.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> IntegrateFiles(IList<FileSpec> toFiles, FileSpec fromFile, Options options)
		{
			return IntegrateFiles(fromFile, options, toFiles.ToArray<FileSpec>());
		}

		/// <summary>
		/// Labelsync - create a label from workspace contents
		/// </summary>
		/// <param name="files">Array of files in the label</param>
		/// <param name="labelName">Name of label</param>
		/// <param name="options">options for this command</param>
		/// <returns>List of files labeled</returns>
		/// <remarks>
		/// <br/><b>p4 help labelsync</b>
		/// <br/> 
		/// <br/>     labelsync -- Apply the label to the contents of the client workspace
		/// <br/> 
		/// <br/>     p4 labelsync [-a -d -g -n -q] -l label [file[revRange] ...]
		/// <br/> 
		/// <br/> 	Labelsync causes the specified label to reflect the current contents
		/// <br/> 	of the client.  It records the revision of each file currently synced.
		/// <br/> 	The label's name can subsequently be used in a revision specification
		/// <br/> 	as @label to refer to the revision of a file as stored in the label.
		/// <br/> 
		/// <br/> 	Without a file argument, labelsync causes the label to reflect the
		/// <br/> 	contents of the whole client, by adding, deleting, and updating the
		/// <br/> 	label.  If a file is specified, labelsync updates the specified file.
		/// <br/> 
		/// <br/> 	If the file argument includes a revision specification, that revision
		/// <br/> 	is used instead of the revision synced by the client. If the specified
		/// <br/> 	revision is a deleted revision, the label includes that deleted
		/// <br/> 	revision.  See 'p4 help revisions' for details about specifying
		/// <br/> 	revisions.
		/// <br/> 
		/// <br/> 	If the file argument includes a revision range specification,
		/// <br/> 	only files selected by the revision range are updated, and the
		/// <br/> 	highest revision in the range is used.
		/// <br/> 
		/// <br/> 	The -a flag adds the specified file to the label.
		/// <br/> 
		/// <br/> 	The -d deletes the specified file from the label, regardless of
		/// <br/> 	revision.
		/// <br/> 
		/// <br/> 	The -n flag previews the operation without altering the label.
		/// <br/> 
		/// <br/> 	Only the owner of a label can run labelsync on that label. A label
		/// <br/> 	that has its Options: field set to 'locked' cannot be updated. A
		/// <br/> 	label without an owner can be labelsync'd by any user.
		/// <br/> 
		/// <br/> 	The -q flag suppresses normal output messages. Messages regarding
		/// <br/> 	errors or exceptional conditions are displayed.
		/// <br/> 
		/// <br/> 	The -g flag should be used on an Edge Server to update a global
		/// <br/> 	label. Note that in this case, the client should be a global client.
		/// <br/> 	Configuring rpl.labels.global=1 reverses this default and causes this
		/// <br/> 	flag to have the opposite meaning.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> LabelSync(Options options, string labelName, params FileSpec[] files)
		{
			if (String.IsNullOrEmpty(labelName))
			{
				throw new ArgumentNullException("labelName");
			}
			else
			{
				if (options == null)
				{
					options = new Options();
				}
				options["-l"] = labelName;
			}
			return runFileListCmd("labelsync", options, files);
		}

		/// <summary>
		/// Create a label from files in a client/workspace
		/// </summary>
		/// <param name="toFiles">list of Files to label</param>
		/// <param name="labelName">Name of the label</param>
		/// <param name="options">command options</param>
		/// <returns>list of files in the label</returns>
		/// <remarks>
		/// <br/><b>p4 help labelsync</b>
		/// <br/> 
		/// <br/>     labelsync -- Apply the label to the contents of the client workspace
		/// <br/> 
		/// <br/>     p4 labelsync [-a -d -g -n -q] -l label [file[revRange] ...]
		/// <br/> 
		/// <br/> 	Labelsync causes the specified label to reflect the current contents
		/// <br/> 	of the client.  It records the revision of each file currently synced.
		/// <br/> 	The label's name can subsequently be used in a revision specification
		/// <br/> 	as @label to refer to the revision of a file as stored in the label.
		/// <br/> 
		/// <br/> 	Without a file argument, labelsync causes the label to reflect the
		/// <br/> 	contents of the whole client, by adding, deleting, and updating the
		/// <br/> 	label.  If a file is specified, labelsync updates the specified file.
		/// <br/> 
		/// <br/> 	If the file argument includes a revision specification, that revision
		/// <br/> 	is used instead of the revision synced by the client. If the specified
		/// <br/> 	revision is a deleted revision, the label includes that deleted
		/// <br/> 	revision.  See 'p4 help revisions' for details about specifying
		/// <br/> 	revisions.
		/// <br/> 
		/// <br/> 	If the file argument includes a revision range specification,
		/// <br/> 	only files selected by the revision range are updated, and the
		/// <br/> 	highest revision in the range is used.
		/// <br/> 
		/// <br/> 	The -a flag adds the specified file to the label.
		/// <br/> 
		/// <br/> 	The -d deletes the specified file from the label, regardless of
		/// <br/> 	revision.
		/// <br/> 
		/// <br/> 	The -n flag previews the operation without altering the label.
		/// <br/> 
		/// <br/> 	Only the owner of a label can run labelsync on that label. A label
		/// <br/> 	that has its Options: field set to 'locked' cannot be updated. A
		/// <br/> 	label without an owner can be labelsync'd by any user.
		/// <br/> 
		/// <br/> 	The -q flag suppresses normal output messages. Messages regarding
		/// <br/> 	errors or exceptional conditions are displayed.
		/// <br/> 
		/// <br/> 	The -g flag should be used on an Edge Server to update a global
		/// <br/> 	label. Note that in this case, the client should be a global client.
		/// <br/> 	Configuring rpl.labels.global=1 reverses this default and causes this
		/// <br/> 	flag to have the opposite meaning.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> LabelSync(IList<FileSpec> toFiles, string labelName, Options options)
		{
			return LabelSync(options, labelName, toFiles.ToArray<FileSpec>());
		}

		/// <summary>
		/// Lock files
		/// </summary>
		/// <param name="files">Array of files to lock</param>
		/// <param name="options">command options</param>
		/// <returns>list of locked files</returns>
		/// <remarks>
		/// <br/><b>p4 help lock</b>
		/// <br/> 
		/// <br/>     lock -- Lock an open file to prevent it from being submitted
		/// <br/> 
		/// <br/>     p4 lock [-c changelist#] [file ...]
		/// <br/> 
		/// <br/> 	The specified files are locked in the depot, preventing any user
		/// <br/> 	other than the current user on the current client from submitting
		/// <br/> 	changes to the files.  If a file is already locked, the lock request
		/// <br/> 	is rejected.  If no file names are specified, all files in the
		/// <br/> 	specified changelist are locked. If changelist number is omitted,
		/// <br/> 	files in the default changelist are locked.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> LockFiles(Options options, params FileSpec[] files)
		{
			return runFileListCmd("lock", options, files);
		}

        /// <summary>
        /// Lock files
        /// </summary>
        /// <param name="files">List of files to lock</param>
        /// <param name="options">command options</param>
        /// <returns>files which were locked</returns>
		public IList<FileSpec> LockFiles(IList<FileSpec> files, Options options)
		{
			return LockFiles(options, files.ToArray<FileSpec>());
		}

        /// <summary>
        /// Move (Rename) files
        /// </summary>
        /// <param name="fromFile">Original name</param>
        /// <param name="toFile">New name</param>
        /// <param name="options">options/flags</param>
        /// <returns>List of files moved</returns>
        /// <remarks>
        /// <br/><b>p4 help move</b>
        /// <br/> 
        /// <br/>     move -- move file(s) from one location to another
        /// <br/>     rename -- synonym for 'move'
        /// <br/> 
        /// <br/>     p4 move [-c changelist#] [-f -n -k] [-t filetype] fromFile toFile
        /// <br/> 
        /// <br/> 	Move takes an already opened file and moves it from one client
        /// <br/> 	location to another, reopening it as a pending depot move.  When
        /// <br/> 	the file is submitted with 'p4 submit', its depot file is moved
        /// <br/> 	accordingly.
        /// <br/> 
        /// <br/> 	Wildcards in fromFile and toFile must match. The fromFile must be
        /// <br/> 	a file open for add or edit.
        /// <br/> 
        /// <br/> 	'p4 opened' lists pending moves. 'p4 diff' can compare a moved
        /// <br/> 	client file with its depot original, 'p4 sync' can schedule an 
        /// <br/> 	update of a moved file, and 'p4 resolve' can resolve the update.
        /// <br/> 
        /// <br/> 	A client file can be moved many times before it is submitted.
        /// <br/> 	Moving a file back to its original location will undo a pending
        /// <br/> 	move, leaving unsubmitted content intact.  Using 'p4 revert'
        /// <br/> 	undoes the move and reverts the unsubmitted content.
        /// <br/> 
        /// <br/> 	If -c changelist# is specified, the file is reopened in the
        /// <br/> 	specified pending changelist as well as being moved.
        /// <br/> 
        /// <br/> 	The -f flag forces a move to an existing target file. The file
        /// <br/> 	must be synced and not opened.  The originating source file will
        /// <br/> 	no longer be synced to the client.
        /// <br/> 
        /// <br/> 	If -t filetype is specified, the file is assigned that filetype.
        /// <br/> 	If the filetype is a partial filetype, the partial filetype is
        /// <br/> 	combined with the current filetype.  See 'p4 help filetypes'.
        /// <br/> 
        /// <br/> 	The -n flag previews the operation without moving files.
        /// <br/> 
        /// <br/> 	The -k flag performs the rename on the server without modifying
        /// <br/> 	client files. Use with caution, as an incorrect move can cause
        /// <br/> 	discrepancies between the state of the client and the corresponding
        /// <br/> 	server metadata.
        /// <br/> 
        /// <br/> 	The 'move' command requires a release 2009.1 or newer client. The
        /// <br/> 	'-f' flag requires a 2010.1 client.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        public IList<FileSpec> MoveFiles(FileSpec fromFile, FileSpec toFile, Options options)
		{
			return runFileListCmd("move", options, fromFile, toFile);
		}

		/// <summary>
		/// Reopen - Change Filetype or move to different changelist
		/// </summary>
		/// <param name="options">command options</param>
		/// <param name="files">Array of files</param>
		/// <returns>list of files reopened</returns>
		/// <remarks>
		/// <br/><b>p4 help reopen</b>
		/// <br/> 
		/// <br/>     reopen -- Change the filetype of an open file or move it to
		/// <br/>               another changelist
		/// <br/> 
		/// <br/>     p4 reopen [-c changelist#] [-t filetype] file ...
		/// <br/> 
		/// <br/> 	Reopen an open file for the current user in order to move it to a
		/// <br/> 	different changelist or change its filetype.
		/// <br/> 
		/// <br/> 	The target changelist must exist; you cannot create a changelist by
		/// <br/> 	reopening a file. To move a file to the default changelist, use
		/// <br/> 	'p4 reopen -c default'.
		/// <br/> 
		/// <br/> 	If -t filetype is specified, the file is assigned that filetype. If
		/// <br/> 	a partial filetype is specified, it is combined with the current
		/// <br/> 	filetype.  For details, see 'p4 help filetypes'.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> ReopenFiles(Options options, params FileSpec[] files)
		{
			FileSpec[] temp = new P4.FileSpec[files.Length];
			for (int idx = 0; idx < files.Length; idx++)
			{
				temp[idx] = new P4.FileSpec(files[idx]);
				temp[idx].Version = null;
			}

			return runFileListCmd("reopen", options, temp);
		}

        /// <summary>
        /// Reopen Files - Change their type or move to another change
        /// </summary>
        /// <param name="files">List of files to reopen</param>
        /// <param name="options">command options</param>
        /// <returns>List of reopened files</returns>
		public IList<FileSpec> ReopenFiles(IList<FileSpec> files, Options options)
		{
			return ReopenFiles(options, files.ToArray<FileSpec>());
		}

        /// <summary>
        /// Resolve integrations and updates to workspace files
        /// </summary>
        /// <param name="options"><seealso cref="ResolveCmdOptions"/></param>
        /// <param name="files">Array of files to resolve</param>
        /// <returns>List of ResolveRecords</returns>
        /// <remarks>
        /// <br/><b>p4 help resolve</b>
        /// <br/> 
        /// <br/>     resolve -- Resolve integrations and updates to workspace files
        /// <br/> 
        /// <br/>     p4 resolve [options] [file ...]
        /// <br/> 
        /// <br/> 	options: -A&lt;flags&gt; -a&lt;flags&gt; -d&lt;flags&gt; -f -n -N -o -t -v
        /// <br/> 		 -c changelist#
        /// <br/> 
        /// <br/> 	'p4 resolve' resolves changes to files in the client workspace.
        /// <br/> 	
        /// <br/> 	'p4 resolve' works only on files that have been scheduled to be 
        /// <br/> 	resolved.  The commands that can schedule resolves are: 'p4 sync',
        /// <br/> 	'p4 update', 'p4 submit', 'p4 merge', and 'p4 integrate'.  Files must
        /// <br/> 	be resolved before they can be submitted.
        /// <br/> 
        /// <br/> 	Resolving involves two sets of files, a source and a target.  The
        /// <br/> 	target is a set of depot files that maps to opened files in the
        /// <br/> 	client workspace.  When resolving an integration, the source is a
        /// <br/> 	different set of depot files than the target.  When resolving an
        /// <br/> 	update, the source is the same set of depot files as the target,
        /// <br/> 	at a different revision.
        /// <br/> 
        /// <br/> 	The 'p4 resolve' file argument specifies the target.  If the file
        /// <br/> 	argument is omitted, all unresolved files are resolved.
        /// <br/> 
        /// <br/> 	Resolving can modify workspace files. To back up files, use 'p4
        /// <br/> 	shelve' before using 'p4 resolve'.
        /// <br/> 
        /// <br/> 	The resolve process is a classic three-way merge. The participating
        /// <br/> 	files are referred to as follows:
        /// <br/> 
        /// <br/> 	  'yours'       The target file open in the client workspace
        /// <br/> 	  'theirs'      The source file in the depot
        /// <br/> 	  'base'        The common ancestor; the highest revision of the
        /// <br/> 	                source file already accounted for in the target.
        /// <br/> 	  'merged'      The merged result.
        /// <br/> 
        /// <br/> 	Filenames, filetypes, and text file content can be resolved by 
        /// <br/> 	accepting 'yours', 'theirs', or 'merged'.  Branching, deletion, and
        /// <br/> 	binary file content can be resolved by accepting either 'yours' or
        /// <br/> 	'theirs'.
        /// <br/> 
        /// <br/> 	When resolving integrated changes, 'p4 resolve' distinguishes among
        /// <br/> 	four results: entirely yours, entirely theirs, a pure merge, or an
        /// <br/> 	edited merge.  The distinction is recorded when resolved files are
        /// <br/> 	submitted, and will be used by future commands to determine whether
        /// <br/> 	integration is needed.
        /// <br/> 
        /// <br/> 	In all cases, accepting 'yours' leaves the target file in its current
        /// <br/> 	state.  The result of accepting 'theirs' is as follows:
        /// <br/> 
        /// <br/> 	   Content:     The target file content is overwritten.
        /// <br/> 	   Attribute:   The target's attributes are replaced.
        /// <br/>  	   Branching:	A new target is branched.
        /// <br/>  	   Deletion:    The target file is deleted.
        /// <br/>  	   Filename:	The target file is moved or renamed.
        /// <br/>  	   Filetype:    The target file's type is changed.
        /// <br/> 
        /// <br/> 	For each unresolved change, the user is prompted to accept a result.
        /// <br/> 	Content and non-content changes are resolved separately.  For content,
        /// <br/> 	'p4 resolve' places the merged result into a temporary file in the
        /// <br/> 	client workspace.  If there are any conflicts, the merged file contains
        /// <br/> 	conflict markers that must be removed by the user.
        /// <br/> 
        /// <br/> 	'p4 resolve' displays a count of text diffs and conflicts, and offers
        /// <br/> 	the following prompts:
        /// <br/> 
        /// <br/> 	  Accept:
        /// <br/> 	     at              Keep only changes to their file.
        /// <br/> 	     ay              Keep only changes to your file.
        /// <br/> 	   * am              Keep merged file.
        /// <br/> 	   * ae              Keep merged and edited file.
        /// <br/> 	   * a               Keep autoselected file.
        /// <br/> 
        /// <br/> 	  Diff:
        /// <br/> 	   * dt              See their changes alone.
        /// <br/> 	   * dy              See your changes alone.
        /// <br/> 	   * dm              See merged changes.
        /// <br/> 	     d               Diff your file against merged file.
        /// <br/> 
        /// <br/> 	  Edit:
        /// <br/> 	     et              Edit their file (read only).
        /// <br/> 	     ey              Edit your file (read/write).
        /// <br/> 	   * e               Edit merged file (read/write).
        /// <br/> 
        /// <br/> 	  Misc:
        /// <br/> 	   * m               Run '$P4MERGE base theirs yours merged'.
        /// <br/> 			     (Runs '$P4MERGEUNICODE charset base theirs
        /// <br/> 			      yours merged' if set and the file is a
        /// <br/> 			      unicode file.)
        /// <br/> 	     s               Skip this file.
        /// <br/> 	     h               Print this help message.
        /// <br/> 	     ^C              Quit the resolve operation.
        /// <br/> 
        /// <br/> 	Options marked (*) appear only for text files. The suggested action
        /// <br/> 	will be displayed in brackets. 
        /// <br/> 
        /// <br/> 	The 'merge' (m) option enables you to invoke your own merge program, if
        /// <br/> 	one is configured using the $P4MERGE environment variable.  Four files
        /// <br/> 	are passed to the program: the base, yours, theirs, and the temporary
        /// <br/> 	file. The program is expected to write merge results to the temporary
        /// <br/> 	file.
        /// <br/> 
        /// <br/> 	The -A flag can be used to limit the kind of resolving that will be
        /// <br/> 	attempted; without it, everything is attempted:
        /// <br/> 
        /// <br/> 	    -Aa		Resolve attributes.
        /// <br/> 	    -Ab		Resolve file branching.
        /// <br/> 	    -Ac		Resolve file content changes.
        /// <br/> 	    -Ad		Resolve file deletions.
        /// <br/> 	    -Am		Resolve moved and renamed files.
        /// <br/> 	    -At		Resolve filetype changes.
        /// <br/> 	    -AQ		Resolve charset changes.
        /// <br/> 
        /// <br/> 	The -a flag puts 'p4 resolve' into automatic mode. The user is not
        /// <br/> 	prompted, and files that can't be resolved automatically are skipped:
        /// <br/> 
        /// <br/> 	    -as		'Safe' resolve; skip files that need merging.
        /// <br/> 	    -am 	Resolve by merging; skip files with conflicts.
        /// <br/> 	    -af		Force acceptance of merged files with conflicts.
        /// <br/> 	    -at		Force acceptance of theirs; overwrites yours.
        /// <br/> 	    -ay		Force acceptance of yours; ignores theirs.
        /// <br/> 
        /// <br/> 	The -as flag causes the workspace file to be replaced with their file
        /// <br/> 	only if theirs has changed and yours has not.
        /// <br/> 
        /// <br/> 	The -am flag causes the workspace file to be replaced with the result
        /// <br/> 	of merging theirs with yours. If the merge detected conflicts, the
        /// <br/> 	file is left untouched and unresolved.
        /// <br/> 
        /// <br/> 	The -af flag causes the workspace file to be replaced with the result
        /// <br/> 	of merging theirs with yours, even if there were conflicts.  This can
        /// <br/> 	leave conflict markers in workspace files.
        /// <br/> 
        /// <br/> 	The -at flag resolves all files by copying theirs into yours. It 
        /// <br/> 	should be used with care, as it overwrites any changes made to the
        /// <br/> 	file in the client workspace.
        /// <br/> 
        /// <br/> 	The -ay flag resolves all files by accepting yours and ignoring 
        /// <br/> 	theirs. It preserves the content of workspace files.
        /// <br/> 
        /// <br/> 	The -d flags can be used to control handling of whitespace and line
        /// <br/> 	endings when merging files:
        /// <br/> 
        /// <br/> 	    -db		Ignore whitespace changes.
        /// <br/> 	    -dw		Ignore whitespace altogether.
        /// <br/> 	    -dl 	Ignores line endings. 
        /// <br/> 
        /// <br/> 	The -d flags are also passed to the diff options in the 'p4 resolve'
        /// <br/> 	dialog. Additional -d flags that modify the diff output but do not 
        /// <br/> 	modify merge behavior include -dn (RCS), -dc (context), -ds (summary),
        /// <br/> 	and -du (unified). Note that 'p4 resolve' uses text from the client
        /// <br/> 	file if the files differ only in whitespace.
        /// <br/> 
        /// <br/> 	The -f flag enables previously resolved content to be resolved again.
        /// <br/> 	By default, after files have been resolved, 'p4 resolve' does not
        /// <br/> 	process them again.
        /// <br/> 
        /// <br/> 	The -n flag previews the operation without altering files.
        /// <br/> 
        /// <br/> 	The -N flag previews the operation with additional information about
        /// <br/> 	any non-content resolve actions that are scheduled.
        /// <br/> 
        /// <br/> 	The -o flag displays the base file name and revision to be used
        /// <br/> 	during the the merge.
        /// <br/> 
        /// <br/> 	The -t flag forces 'p4 resolve' to attempt a textual merge, even for
        /// <br/> 	files with non-text (binary) types.
        /// <br/> 
        /// <br/> 	The -v flag causes 'p4 resolve' to insert markers for all changes,
        /// <br/> 	not just conflicts.
        /// <br/> 
        /// <br/> 	The -c flag limits 'p4 resolve' to the files in changelist#.
        /// <br/> 
        /// <br/> 	'p4 resolve' is not supported for files with propagating attributes
        /// <br/> 	from an edge server in a distributed environment.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        public IList<FileResolveRecord> ResolveFiles(Options options, params FileSpec[] files)
		{
			return ResolveFiles(null, options, files);
		}

		List<FileResolveRecord> CurrentResolveRecords = null;

		FileResolveRecord CurrentResolveRecord = null;

        /// <summary>
        /// Delegate used for AutoResolve
        /// </summary>
        /// <param name="mergeForce">options for merge</param>
        /// <returns>A MergeStatus enum</returns>
		public delegate P4.P4ClientMerge.MergeStatus AutoResolveDelegate(P4.P4ClientMerge.MergeForce mergeForce);

        /// <summary>
        /// Delegate used for detailed Resolve
        /// </summary>
        /// <param name="resolveRecord">Instructions for the resolve</param>
        /// <param name="AutoResolve">Delegate for AutoResolve</param>
        /// <param name="sourcePath">source path</param>
        /// <param name="targetPath">target path</param>
        /// <param name="basePath">base path</param>
        /// <param name="resultsPath">results path</param>
        /// <returns>A MergeStatus enum</returns>
		public delegate P4.P4ClientMerge.MergeStatus ResolveFileDelegate(FileResolveRecord resolveRecord,
			AutoResolveDelegate AutoResolve, string sourcePath, string targetPath, string basePath, string resultsPath);

		private ResolveFileDelegate ResolveFileHandler = null;

		private P4.P4ClientMerge.MergeStatus HandleResolveFile(uint cmdId, P4.P4ClientMerge cm)
		{
			if (CurrentResolveRecord.Analysis == null)
			{
				CurrentResolveRecord.Analysis = new ResolveAnalysis();
			}
			// this is from a content resolve
			CurrentResolveRecord.Analysis.SetResolveType(ResolveType.Content);

			CurrentResolveRecord.Analysis.SourceDiffCnt = cm.GetYourChunks();
			CurrentResolveRecord.Analysis.TargetDiffCnt = cm.GetTheirChunks();
			CurrentResolveRecord.Analysis.CommonDiffCount = cm.GetBothChunks();
			CurrentResolveRecord.Analysis.ConflictCount = cm.GetConflictChunks();

			CurrentResolveRecord.Analysis.SuggestedAction = cm.AutoResolve(P4ClientMerge.MergeForce.CMF_AUTO);

			if ((ResolveFileHandler != null) && (CurrentResolveRecord != null))
			{
				try
				{
					return ResolveFileHandler(CurrentResolveRecord, new AutoResolveDelegate(cm.AutoResolve),
						cm.GetTheirFile(), cm.GetYourFile(), cm.GetBaseFile(), cm.GetResultFile());
				}
				catch (Exception ex)
				{
					LogFile.LogException("Error", ex);

					return P4ClientMerge.MergeStatus.CMS_SKIP;
				}
			}
			return P4ClientMerge.MergeStatus.CMS_SKIP;
		}

		private P4.P4ClientMerge.MergeStatus HandleResolveAFile(uint cmdId, P4.P4ClientResolve cr)
		{
			string strType = cr.ResolveType;

			if (strType.Contains("resolve"))
			{
				strType = strType.Replace("resolve", string.Empty).Trim();
				if ((strType == "Rename") || (strType == "Filename"))
				{
					strType = "Move";
				}
			}
			if (CurrentResolveRecord.Analysis == null)
			{
				CurrentResolveRecord.Analysis = new ResolveAnalysis();
			}
			// this is likely from an action resolve
			CurrentResolveRecord.Analysis.SetResolveType(strType);

			CurrentResolveRecord.Analysis.Options = ResolveOptions.Skip; // can always skip
			
			if (string.IsNullOrEmpty(cr.MergeAction) == false)
			{
				CurrentResolveRecord.Analysis.Options |= ResolveOptions.Merge;
				CurrentResolveRecord.Analysis.MergeAction = cr.MergeAction;
			}

			if (string.IsNullOrEmpty(cr.TheirAction) == false)
			{
				CurrentResolveRecord.Analysis.Options |= ResolveOptions.AccecptTheirs;
				CurrentResolveRecord.Analysis.TheirsAction = cr.TheirAction;
			}

			if (string.IsNullOrEmpty(cr.YoursAction) == false)
			{
				CurrentResolveRecord.Analysis.Options |= ResolveOptions.AcceptYours;
				CurrentResolveRecord.Analysis.YoursAction = cr.YoursAction;
			}

			// this is likely from an action resolve
			CurrentResolveRecord.Analysis.SetResolveType(strType);

			CurrentResolveRecord.Analysis.SuggestedAction = cr.AutoResolve(P4ClientMerge.MergeForce.CMF_AUTO);

			if ((ResolveFileHandler != null) && (CurrentResolveRecord != null))
			{
				try
				{
					return ResolveFileHandler(CurrentResolveRecord, new AutoResolveDelegate(cr.AutoResolve),
						null, null, null, null);
				}
				catch (Exception ex)
				{
					LogFile.LogException("Error", ex);

					return P4ClientMerge.MergeStatus.CMS_SKIP;
				}
			}
			return P4ClientMerge.MergeStatus.CMS_SKIP;
		}

		private P4.P4Server.TaggedOutputDelegate ResultsTaggedOutputHandler = null;

		private void ResultsTaggedOutputReceived(uint cmdId, int ObjId, TaggedObject Obj)
		{
			if (CurrentResolveRecord != null)
			{
				CurrentResolveRecords.Add(CurrentResolveRecord);
				CurrentResolveRecord = null;
			}
			//Create a record for this file resolve results
			CurrentResolveRecord = FileResolveRecord.FromResolveCmdTaggedOutput(Obj);

		}

		/// <summary>
		/// Resolve files
		/// </summary>
		/// <param name="resolveHandler">Delegate to handle the resolve</param>
		/// <param name="options">command options</param>
		/// <param name="files">Files to resolve</param>
		/// <returns>List of FileResolve records</returns>
		/// <remarks>
		/// The caller must either 
		/// 1) set an automatic resolution (-as, -am.-af, -at, or -ay), 
		/// 2) provide a callback function of type <see cref="P4Server.PromptHandlerDelegate"/> to
		/// respond to the prompts, or 3) provide a dictionary which contains responses to the prompts.
		/// <br/>
		/// <br/><b>p4 help resolve</b>
		/// <br/> 
		/// <br/>     resolve -- Resolve integrations and updates to workspace files
		/// <br/> 
		/// <br/>     p4 resolve [options] [file ...]
		/// <br/> 
		/// <br/> 	options: -A&lt;flags&gt; -a&lt;flags&gt; -d&lt;flags&gt; -f -n -N -o -t -v
		/// <br/> 		 -c changelist#
		/// <br/> 
		/// <br/> 	'p4 resolve' resolves changes to files in the client workspace.
		/// <br/> 	
		/// <br/> 	'p4 resolve' works only on files that have been scheduled to be 
		/// <br/> 	resolved.  The commands that can schedule resolves are: 'p4 sync',
		/// <br/> 	'p4 update', 'p4 submit', 'p4 merge', and 'p4 integrate'.  Files must
		/// <br/> 	be resolved before they can be submitted.
		/// <br/> 
		/// <br/> 	Resolving involves two sets of files, a source and a target.  The
		/// <br/> 	target is a set of depot files that maps to opened files in the
		/// <br/> 	client workspace.  When resolving an integration, the source is a
		/// <br/> 	different set of depot files than the target.  When resolving an
		/// <br/> 	update, the source is the same set of depot files as the target,
		/// <br/> 	at a different revision.
		/// <br/> 
		/// <br/> 	The 'p4 resolve' file argument specifies the target.  If the file
		/// <br/> 	argument is omitted, all unresolved files are resolved.
		/// <br/> 
		/// <br/> 	Resolving can modify workspace files. To back up files, use 'p4
		/// <br/> 	shelve' before using 'p4 resolve'.
		/// <br/> 
		/// <br/> 	The resolve process is a classic three-way merge. The participating
		/// <br/> 	files are referred to as follows:
		/// <br/> 
		/// <br/> 	  'yours'       The target file open in the client workspace
		/// <br/> 	  'theirs'      The source file in the depot
		/// <br/> 	  'base'        The common ancestor; the highest revision of the
		/// <br/> 	                source file already accounted for in the target.
		/// <br/> 	  'merged'      The merged result.
		/// <br/> 
		/// <br/> 	Filenames, filetypes, and text file content can be resolved by 
		/// <br/> 	accepting 'yours', 'theirs', or 'merged'.  Branching, deletion, and
		/// <br/> 	binary file content can be resolved by accepting either 'yours' or
		/// <br/> 	'theirs'.
		/// <br/> 
		/// <br/> 	When resolving integrated changes, 'p4 resolve' distinguishes among
		/// <br/> 	four results: entirely yours, entirely theirs, a pure merge, or an
		/// <br/> 	edited merge.  The distinction is recorded when resolved files are
		/// <br/> 	submitted, and will be used by future commands to determine whether
		/// <br/> 	integration is needed.
		/// <br/> 
		/// <br/> 	In all cases, accepting 'yours' leaves the target file in its current
		/// <br/> 	state.  The result of accepting 'theirs' is as follows:
		/// <br/> 
		/// <br/> 	   Content:     The target file content is overwritten.
		/// <br/> 	   Attribute:   The target's attributes are replaced.
		/// <br/>  	   Branching:	A new target is branched.
		/// <br/>  	   Deletion:    The target file is deleted.
		/// <br/>  	   Filename:	The target file is moved or renamed.
		/// <br/>  	   Filetype:    The target file's type is changed.
		/// <br/> 
		/// <br/> 	For each unresolved change, the user is prompted to accept a result.
		/// <br/> 	Content and non-content changes are resolved separately.  For content,
		/// <br/> 	'p4 resolve' places the merged result into a temporary file in the
		/// <br/> 	client workspace.  If there are any conflicts, the merged file contains
		/// <br/> 	conflict markers that must be removed by the user.
		/// <br/> 
		/// <br/> 	'p4 resolve' displays a count of text diffs and conflicts, and offers
		/// <br/> 	the following prompts:
		/// <br/> 
		/// <br/> 	  Accept:
		/// <br/> 	     at              Keep only changes to their file.
		/// <br/> 	     ay              Keep only changes to your file.
		/// <br/> 	   * am              Keep merged file.
		/// <br/> 	   * ae              Keep merged and edited file.
		/// <br/> 	   * a               Keep autoselected file.
		/// <br/> 
		/// <br/> 	  Diff:
		/// <br/> 	   * dt              See their changes alone.
		/// <br/> 	   * dy              See your changes alone.
		/// <br/> 	   * dm              See merged changes.
		/// <br/> 	     d               Diff your file against merged file.
		/// <br/> 
		/// <br/> 	  Edit:
		/// <br/> 	     et              Edit their file (read only).
		/// <br/> 	     ey              Edit your file (read/write).
		/// <br/> 	   * e               Edit merged file (read/write).
		/// <br/> 
		/// <br/> 	  Misc:
		/// <br/> 	   * m               Run '$P4MERGE base theirs yours merged'.
		/// <br/> 			     (Runs '$P4MERGEUNICODE charset base theirs
		/// <br/> 			      yours merged' if set and the file is a
		/// <br/> 			      unicode file.)
		/// <br/> 	     s               Skip this file.
		/// <br/> 	     h               Print this help message.
		/// <br/> 	     ^C              Quit the resolve operation.
		/// <br/> 
		/// <br/> 	Options marked (*) appear only for text files. The suggested action
		/// <br/> 	will be displayed in brackets. 
		/// <br/> 
		/// <br/> 	The 'merge' (m) option enables you to invoke your own merge program, if
		/// <br/> 	one is configured using the $P4MERGE environment variable.  Four files
		/// <br/> 	are passed to the program: the base, yours, theirs, and the temporary
		/// <br/> 	file. The program is expected to write merge results to the temporary
		/// <br/> 	file.
		/// <br/> 
		/// <br/> 	The -A flag can be used to limit the kind of resolving that will be
		/// <br/> 	attempted; without it, everything is attempted:
		/// <br/> 
		/// <br/> 	    -Aa		Resolve attributes.
		/// <br/> 	    -Ab		Resolve file branching.
		/// <br/> 	    -Ac		Resolve file content changes.
		/// <br/> 	    -Ad		Resolve file deletions.
		/// <br/> 	    -Am		Resolve moved and renamed files.
		/// <br/> 	    -At		Resolve filetype changes.
		/// <br/> 	    -AQ		Resolve charset changes.
		/// <br/> 
		/// <br/> 	The -a flag puts 'p4 resolve' into automatic mode. The user is not
		/// <br/> 	prompted, and files that can't be resolved automatically are skipped:
		/// <br/> 
		/// <br/> 	    -as		'Safe' resolve; skip files that need merging.
		/// <br/> 	    -am 	Resolve by merging; skip files with conflicts.
		/// <br/> 	    -af		Force acceptance of merged files with conflicts.
		/// <br/> 	    -at		Force acceptance of theirs; overwrites yours.
		/// <br/> 	    -ay		Force acceptance of yours; ignores theirs.
		/// <br/> 
		/// <br/> 	The -as flag causes the workspace file to be replaced with their file
		/// <br/> 	only if theirs has changed and yours has not.
		/// <br/> 
		/// <br/> 	The -am flag causes the workspace file to be replaced with the result
		/// <br/> 	of merging theirs with yours. If the merge detected conflicts, the
		/// <br/> 	file is left untouched and unresolved.
		/// <br/> 
		/// <br/> 	The -af flag causes the workspace file to be replaced with the result
		/// <br/> 	of merging theirs with yours, even if there were conflicts.  This can
		/// <br/> 	leave conflict markers in workspace files.
		/// <br/> 
		/// <br/> 	The -at flag resolves all files by copying theirs into yours. It 
		/// <br/> 	should be used with care, as it overwrites any changes made to the
		/// <br/> 	file in the client workspace.
		/// <br/> 
		/// <br/> 	The -ay flag resolves all files by accepting yours and ignoring 
		/// <br/> 	theirs. It preserves the content of workspace files.
		/// <br/> 
		/// <br/> 	The -d flags can be used to control handling of whitespace and line
		/// <br/> 	endings when merging files:
		/// <br/> 
		/// <br/> 	    -db		Ignore whitespace changes.
		/// <br/> 	    -dw		Ignore whitespace altogether.
		/// <br/> 	    -dl 	Ignores line endings. 
		/// <br/> 
		/// <br/> 	The -d flags are also passed to the diff options in the 'p4 resolve'
		/// <br/> 	dialog. Additional -d flags that modify the diff output but do not 
		/// <br/> 	modify merge behavior include -dn (RCS), -dc (context), -ds (summary),
		/// <br/> 	and -du (unified). Note that 'p4 resolve' uses text from the client
		/// <br/> 	file if the files differ only in whitespace.
		/// <br/> 
		/// <br/> 	The -f flag enables previously resolved content to be resolved again.
		/// <br/> 	By default, after files have been resolved, 'p4 resolve' does not
		/// <br/> 	process them again.
		/// <br/> 
		/// <br/> 	The -n flag previews the operation without altering files.
		/// <br/> 
		/// <br/> 	The -N flag previews the operation with additional information about
		/// <br/> 	any non-content resolve actions that are scheduled.
		/// <br/> 
		/// <br/> 	The -o flag displays the base file name and revision to be used
		/// <br/> 	during the the merge.
		/// <br/> 
		/// <br/> 	The -t flag forces 'p4 resolve' to attempt a textual merge, even for
		/// <br/> 	files with non-text (binary) types.
		/// <br/> 
		/// <br/> 	The -v flag causes 'p4 resolve' to insert markers for all changes,
		/// <br/> 	not just conflicts.
		/// <br/> 
		/// <br/> 	The -c flag limits 'p4 resolve' to the files in changelist#.
		/// <br/> 
		/// <br/> 	'p4 resolve' is not supported for files with propagating attributes
		/// <br/> 	from an edge server in a distributed environment.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileResolveRecord> ResolveFiles(
			ResolveFileDelegate resolveHandler,
			Options options,
			params FileSpec[] files)
		{
            string change=null;
            options.TryGetValue("-c",out change);
            GetOpenedFilesOptions openedOptions = new GetOpenedFilesOptions((GetOpenedFilesCmdFlags.None),
                change,null,null,-1);
			CurrentResolveRecords = new List<FileResolveRecord>();

			try
			{
				string[] paths = null;
				try
				{
                    IList<FileSpec> clientFiles = runFileListCmd("opened", openedOptions, files);

					paths = FileSpec.ToEscapedPaths(clientFiles.ToArray());
				}
				catch
				{
						paths = FileSpec.ToEscapedPaths(files);
				}
				ResultsTaggedOutputHandler = new P4Server.TaggedOutputDelegate(ResultsTaggedOutputReceived);

				Connection.getP4Server().TaggedOutputReceived += ResultsTaggedOutputHandler;

				ResolveFileHandler = resolveHandler;

				foreach (string path in paths)
				{
					P4Command cmd = new P4Command(Connection, "resolve", true, path);

					cmd.CmdResolveHandler = new P4Server.ResolveHandlerDelegate(HandleResolveFile);
					cmd.CmdResolveAHandler = new P4Server.ResolveAHandlerDelegate(HandleResolveAFile);

					CurrentResolveRecord = null;

					P4CommandResult results = cmd.Run(options);
					if (results.Success)
					{
						if (CurrentResolveRecord != null)
						{
							CurrentResolveRecords.Add(CurrentResolveRecord);
							CurrentResolveRecord = null;
						}
						else
						{
                            if ((results.ErrorList != null) && (results.ErrorList.Count > 0) && 
                                (results.ErrorList[0].ErrorMessage.Contains("no file(s) to resolve")))
                            {
								continue;
							}
							// not in interactive mode
							FileResolveRecord  record = null;

							if ((results.TaggedOutput != null) && (results.TaggedOutput.Count > 0))
							{
								foreach (TaggedObject obj in results.TaggedOutput)
								{
									record = new FileResolveRecord(obj);
									if (record != null)
									{
										CurrentResolveRecords.Add(record);
									}
								}
							}
						}
					}
					else
					{
						P4Exception.Throw(results.ErrorList);
					}
				}
				if (CurrentResolveRecords.Count > 0)
				{
					return CurrentResolveRecords;
				}
				return null;
			}
			finally
			{
				if (ResultsTaggedOutputHandler != null)
				{
					Connection.getP4Server().TaggedOutputReceived -= ResultsTaggedOutputHandler;
				}
			}
		}

		[Obsolete("This version of resolve is superseded ")]
		internal List<FileResolveRecord> ResolveFiles(P4Server.ResolveHandlerDelegate resolveHandler,
															P4Server.PromptHandlerDelegate promptHandler,
															Dictionary<String, String> promptResponses,
															Options options,
															params FileSpec[] files)
		{
			string[] paths = FileSpec.ToEscapedPaths(files);
			P4Command cmd = new P4Command(Connection, "resolve", true, paths);

			if (resolveHandler != null)
			{
				cmd.CmdResolveHandler = resolveHandler;
			}
			else if (promptHandler != null)
			{
				cmd.CmdPromptHandler = promptHandler;
			}
			else if (promptResponses != null)
			{
				cmd.Responses = promptResponses;
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				Dictionary<string, FileResolveRecord> recordMap = new Dictionary<string, FileResolveRecord>();
				List<FileResolveRecord> records = new List<FileResolveRecord>();
				if ((results.TaggedOutput != null) && (results.TaggedOutput.Count > 0))
				{
					foreach (TaggedObject obj in results.TaggedOutput)
					{
						FileResolveRecord record1 = FileResolveRecord.FromResolveCmdTaggedOutput(obj);
						records.Add(record1);
						if (record1.LocalFilePath != null)
						{
							recordMap[record1.LocalFilePath.Path.ToLower()] = record1;
						}
					}
				}
				if ((results.InfoOutput != null) && (results.InfoOutput.Count > 0))
				{
					string l1 = null;
					string l2 = null;
					string l3 = null;

					FileResolveRecord record2 = null;
					int RecordsPerItem = results.InfoOutput.Count / files.Length;
					for (int idx = 0; idx < results.InfoOutput.Count; idx += RecordsPerItem)
					{
						l1 = results.InfoOutput[idx].Message;
						if (RecordsPerItem == 3)
						{
							l2 = results.InfoOutput[idx + 1].Message;
							l3 = results.InfoOutput[idx + 2].Message;
						}
						if (RecordsPerItem == 2)
						{
							l2 = null;
							l3 = results.InfoOutput[idx + 1].Message;
						}
						record2 = FileResolveRecord.FromMergeInfo(l1, l2, l3);
						if ((record2 != null) && (recordMap.ContainsKey(record2.LocalFilePath.Path.ToLower())))
						{
							FileResolveRecord record1 = recordMap[record2.LocalFilePath.Path.ToLower()];
							FileResolveRecord.MergeRecords(record1, record2);
						}
						else
						{
							records.Add(record2);
						}
					}
				}
				return records;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}

			return null;
		}
		public IList<FileResolveRecord> ResolveFiles(IList<FileSpec> files, Options options)
		{
			return ResolveFiles(options, files.ToArray<FileSpec>());
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="options"></param>
		/// <param name="file">Optional file to submit</param>
		/// <returns></returns>
		/// <remarks>
		/// <br/><b>p4 help submit</b>
		/// <br/> 
		/// <br/>     submit -- Submit open files to the depot
		/// <br/> 
		/// <br/>     p4 submit [-r -s -f option --noretransfer 0|1]
		/// <br/>     p4 submit [-r -s -f option] file
		/// <br/>     p4 submit [-r -f option] -d description
		/// <br/>     p4 submit [-r -f option] -d description file
		/// <br/>     p4 submit [-r -f option --noretransfer 0|1] -c changelist#
		/// <br/>     p4 submit -e shelvedChange#
		/// <br/>     p4 submit -i [-r -s -f option]
		/// <br/>               --parallel=threads=N[,batch=N][,min=N]
		/// <br/> 
		/// <br/> 	'p4 submit' commits a pending changelist and its files to the depot.
		/// <br/> 
		/// <br/> 	By default, 'p4 submit' attempts to submit all files in the 'default'
		/// <br/> 	changelist.  Submit displays a dialog where you enter a description
		/// <br/> 	of the change and, optionally, delete files from the list of files
		/// <br/> 	to be checked in. 
		/// <br/> 
		/// <br/> 	To add files to a changelist before submitting, use any of the 
		/// <br/> 	commands that open client workspace files: 'p4 add', 'p4 edit',
		/// <br/> 	etc.
		/// <br/> 
		/// <br/> 	If the file parameter is specified, only files in the default
		/// <br/> 	changelist that match the pattern are submitted.
		/// <br/> 
		/// <br/> 	Files in a stream path can be submitted only by client workspaces
		/// <br/> 	dedicated to the stream. See 'p4 help client'.
		/// <br/> 
		/// <br/> 	Before committing a changelist, 'p4 submit' locks all the files being
		/// <br/> 	submitted. If any file cannot be locked or submitted, the files are 
		/// <br/> 	left open in a numbered pending changelist. By default, the files in
		/// <br/> 	a failed submit operation are left locked unless the
		/// <br/> 	submit.unlocklocked configurable is set. Files are unlocked even if
		/// <br/> 	they were manually locked prior to submit if submit fails when
		/// <br/> 	submit.unlocklocked is set. 'p4 opened' shows unsubmitted files
		/// <br/> 	and their changelists.
		/// <br/> 
		/// <br/> 	Submit is atomic: if the operation succeeds, all files are updated
		/// <br/> 	in the depot. If the submit fails, no depot files are updated.
		/// <br/> 
		/// <br/> 	If submit fails, some or all of the files may have been copied to
		/// <br/> 	the server. By default, retrying a failed submit will transfer all of
		/// <br/> 	the files again unless the submit.noretransfer configurable is set.
		/// <br/> 	If submit.noretransfer is set to 1, submit uses digest comparisons to
		/// <br/> 	to detect if the files have already been transferred in order to
		/// <br/> 	avoid file re-transfer when retrying a failed submit.
		/// <br/> 
		/// <br/> 	The --noretransfer flag is used to override the submit.noretransfer
		/// <br/> 	configurable so the user can choose his preferred re-transfer
		/// <br/> 	behavior during the current submit operation.
		/// <br/> 
		/// <br/> 	The -c flag submits the specified pending changelist instead of the
		/// <br/> 	default changelist. Additional changelists can be created manually, 
		/// <br/> 	using the 'p4 change' command, or automatically as the result of a 
		/// <br/> 	failed attempt to submit the default changelist.
		/// <br/> 
		/// <br/> 	The -e flag submits a shelved changelist without transferring files
		/// <br/> 	or modifying the workspace. The shelved change must be owned by
		/// <br/> 	the person submitting the change, but the client may be different.
		/// <br/> 	However, files shelved to a stream target may only be submitted by
		/// <br/> 	a stream client that is mapped to the target stream. In addition,
		/// <br/> 	files shelved to a non-stream target cannot be submitted by a stream
		/// <br/> 	client. To submit a shelved change, all files in the shelved change
		/// <br/> 	must be up to date and resolved. No files may be open in any workspace
		/// <br/> 	at the same change number. Client submit options (ie revertUnchanged,
		/// <br/> 	etc) will be ignored. If the submit is successful, the shelved change
		/// <br/> 	and files are cleaned up, and are no longer available to be unshelved
		/// <br/> 	or submitted.
		/// <br/> 
		/// <br/> 	The -d flag passes a description into the specified changelist rather
		/// <br/> 	than displaying the changelist dialog for manual editing. This option
		/// <br/> 	is useful for scripting, but does not allow you to add jobs or modify
		/// <br/> 	the default changelist.
		/// <br/> 
		/// <br/> 	The -f flag enables you to override submit options that are configured
		/// <br/> 	for the client that is submitting the changelist.  This flag overrides
		/// <br/> 	the -r (reopen)flag, if it is specified.  See 'p4 help client' for
		/// <br/> 	details about submit options.
		/// <br/> 
		/// <br/> 	The -i flag reads a changelist specification from the standard input.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -r flag reopens submitted files in the default changelist after
		/// <br/> 	submission.
		/// <br/> 
		/// <br/> 	The -s flag extends the list of jobs to include the fix status
		/// <br/> 	for each job, which becomes the job's status when the changelist
		/// <br/> 	is committed.  See 'p4 help change' for details.
		/// <br/> 
		/// <br/> 	The --parallel flag specifies options for parallel file transfer. If
		/// <br/> 	parallel file transfer has been enabled by setting the
		/// <br/> 	net.parallel.max configurable, and if there are sufficient resources
		/// <br/> 	across the system, a submit command may execute more rapidly by
		/// <br/> 	transferring multiple files in parallel. Specify threads=N to request
		/// <br/> 	files be sent concurrently, using N independent network connections.
		/// <br/> 	The N threads grab work in batches; specify batch=N to control the
		/// <br/> 	number of files in a batch. A submit that is too small will not
		/// <br/> 	initiate parallel file transfers; specify min=N to control the
		/// <br/> 	minimum number of files in a parallel submit. Requesting progress
		/// <br/> 	indicators causes the --parallel flag to be ignored.
		/// <br/> 
		/// <br/> 	Using --parallel from an edge server allows parallel file transfer
		/// <br/> 	from the edge server to the commit server. This uses standard pull
		/// <br/> 	threads to transfer the files. Note that an adminstrator must insure
		/// <br/> 	that pull threads can be run on the commit server. The address
		/// <br/> 	used by the commit server to connect to the edge server must
		/// <br/> 	be specified in the ExternalAddress field of the edge server spec.
		/// <br/> 
		/// <br/> 	Only 'submit -e' is supported for files with propagating attributes
		/// <br/> 	from an edge server in a distributed environment.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public SubmitResults SubmitFiles(Options options, FileSpec file)
		{
			P4Command cmd = null;
			if (file != null)
			{
				cmd = new P4Command(Connection, "submit", true, file.ToEscapedString());
			}
			else
			{
				cmd = new P4Command(Connection, "submit", true);
			}
			if (options != null&&!options.ContainsKey("-e"))
			{
				//the new Changelist Spec is passed using the command dataset
				if (options.ContainsKey("-i"))
				{
					cmd.DataSet =  options["-i"];
					options["-i"] = null;
				}
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				SubmitResults returnVal = new SubmitResults();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
					if (obj.ContainsKey("submittedChange"))
					{
						int i = -1;
						// the changelist number after the submit
						if (int.TryParse(obj["submittedChange"], out i))
							returnVal.ChangeIdAfterSubmit = i;
					}
					else if (obj.ContainsKey("change"))
					{
						// The changelist use by the submit
						int i = -1;
						if (int.TryParse(obj["change"], out i))
							returnVal.ChangeIdBeforeSubmit = i;
						if (obj.ContainsKey("locked"))
						{
							if (int.TryParse(obj["locked"], out i))
							{
								returnVal.FilesLockedBySubmit = i;
							}
						}
					}
					else
					{
						// a file in the submit
						StringEnum<FileAction> action = null;
						if (obj.ContainsKey("action"))
						{
							action = obj["action"];
						}
						int rev = -1;
						string p;

						DepotPath dp = null;
						ClientPath cp = null;
						LocalPath lp = null;

						if (obj.ContainsKey("rev"))
						{
							int.TryParse(obj["rev"], out rev);
						}
						if (obj.ContainsKey("depotFile"))
						{
							p = obj["depotFile"];
							dp = new DepotPath(p);
						}
						if (obj.ContainsKey("clientFile"))
						{
							p = obj["clientFile"];
							if (p.StartsWith("//"))
							{
								cp = new ClientPath(p);
							}
							else
							{
								cp = new ClientPath(p);
								lp = new LocalPath(p);
							}
						}
						if (obj.ContainsKey("path"))
						{
							lp = new LocalPath(obj["path"]);
						}
						FileSpec fs = new FileSpec(dp, cp, lp, new Revision(rev));
						returnVal.Files.Add(new FileSubmitRecord(action, fs));
					}
				}
				return returnVal;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}

			return null;
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="options"></param>
		/// <param name="files"></param>
		/// <returns></returns>
		/// <remarks>
		/// <br/><b>p4 help resolved</b>
		/// <br/> 
		/// <br/>     resolved -- Show files that have been resolved but not submitted
		/// <br/> 
		/// <br/>     p4 resolved [-o] [file ...]
		/// <br/> 
		/// <br/> 	'p4 resolved' lists file updates and integrations that have been 
		/// <br/> 	resolved but not yet submitted.  To see unresolved integrations, 
		/// <br/> 	use 'p4 resolve -n'.  To see already submitted integrations, use 
		/// <br/> 	'p4 integrated'.
		/// <br/> 
		/// <br/> 	If a depot file path is specified, the output lists resolves for
		/// <br/> 	'theirs' files that match the specified path.  If a client file
		/// <br/> 	path is specified, the output lists resolves for 'yours' files
		/// <br/> 	that match the specified path.
		/// <br/> 
		/// <br/> 	The -o flag reports the revision used as the base during the
		/// <br/> 	resolve.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileResolveRecord> GetResolvedFiles(Options options, params FileSpec[] files)
		{
			P4Command cmd = new P4Command(Connection, "resolved", true, FileSpec.ToStrings(files));

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				List<FileResolveRecord> fileList = new List<FileResolveRecord>();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
					fileList.Add(FileResolveRecord.FromResolvedCmdTaggedOutput(obj));
				}
				return fileList;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}

			return null;

		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="Files"></param>
		/// <param name="options"></param>
		/// <returns></returns>
		public IList<FileResolveRecord> GetResolvedFiles(IList<FileSpec> Files, Options options)
		{
			return GetResolvedFiles(options, Files.ToArray());
		}
        /// <summary>
        /// 
        /// </summary>
        /// <param name="options"><cref>ReconcileFilesOptions</cref></param>
        /// <param name="files"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help reconcile</b>
        /// <br/> 
        /// <br/>     reconcile -- Open files for add, delete, and/or edit to reconcile
        /// <br/>     client with workspace changes made outside of Perforce
        /// <br/>     
        /// <br/>     rec         -- synonym for 'reconcile'
        /// <br/>     status      -- 'reconcile -n + opened' (output uses local paths)
        /// <br/>     status -A   -- synonym for 'reconcile -ead' (output uses local paths)
        /// <br/>     
        /// <br/>     clean       -- synonym for 'reconcile -w'
        /// <br/>     
        /// <br/>     p4 reconcile [-c change#] [-e -a -d -f -I -l -m -n -w] [file ...]
        /// <br/>     p4 status [-c change#] [-A | [-e -a -d] | [-s]] [-f -I -m] [file ...]
        /// <br/>     p4 clean [-e -a -d -I -l -n] [file ...]
        /// <br/>     p4 reconcile -k [-l -n] [file ...]
        /// <br/>     p4 status -k [file ...]
        /// <br/> 	
        /// <br/> 	'p4 reconcile' finds unopened files in a client's workspace and
        /// <br/> 	detects the following:
        /// <br/> 	
        /// <br/> 	1. files in depot missing from workspace, but still on have list
        /// <br/> 	2. files on workspace that are not in depot
        /// <br/> 	3. files modified in workspace that are not opened for edit
        /// <br/> 	
        /// <br/> 	By default, the files matching each condition above in the path
        /// <br/> 	are reconciled by opening files for delete (scenario 1), add
        /// <br/> 	(scenario 2), and/or edit (scenario 3). The -e, -a, and -d flags
        /// <br/> 	may be used to limit to a subset of these operations. If no file
        /// <br/> 	arguments are given, reconcile and status default to the current
        /// <br/> 	working directory.
        /// <br/> 	
        /// <br/> 	If the list of files to be opened includes both adds and deletes,
        /// <br/> 	the missing and added files will be compared and converted to pairs
        /// <br/> 	of move/delete and move/add operations if they are similar enough.
        /// <br/> 	
        /// <br/> 	In addition to opening unopened files, reconcile will detect files
        /// <br/> 	that are currently opened for edit but missing from the workspace
        /// <br/> 	and reopen them for delete. Reconcile will also detect files opened
        /// <br/> 	for delete that are present on the workspace and reopen them for
        /// <br/> 	edit.
        /// <br/> 	
        /// <br/> 	The -n flag previews the operation without performing any action.
        /// <br/> 	Although metadata updates from reconcile require open permission,
        /// <br/> 	the preview commands only require read access.
        /// <br/> 	
        /// <br/> 	If -c changelist# is included, the files are opened in the specified
        /// <br/> 	pending changelist.
        /// <br/> 	
        /// <br/> 	The -e flag allows the user to reconcile files that have been
        /// <br/> 	modified outside of Perforce. The reconcile command will open
        /// <br/> 	these files for edit.
        /// <br/> 	
        /// <br/> 	The -a flag allows the user to reconcile files that are in the
        /// <br/> 	user's directory that are not under Perforce source control. These
        /// <br/> 	files are opened for add.
        /// <br/> 	
        /// <br/> 	The -f flag allows the user to add files with filenames that contain
        /// <br/> 	wildcard characters. Filenames that contain the special characters
        /// <br/> 	'@', '#', '%' or '*' are reformatted to encode the characters using
        /// <br/> 	ASCII hexadecimal representation.  After the files are added, you
        /// <br/> 	must refer to them using the reformatted file name, because Perforce
        /// <br/> 	does not recognize the local filesystem name.
        /// <br/> 	
        /// <br/> 	The -I flag informs the client that it should not perform any ignore
        /// <br/> 	checking configured by P4IGNORE.
        /// <br/> 	
        /// <br/> 	The -d flag allows the user to reconcile files that have been
        /// <br/> 	removed from the user's directory but are still in the depot.
        /// <br/> 	These files will be opened for delete only if they are still on the
        /// <br/> 	user's have list.
        /// <br/> 	
        /// <br/> 	The -l flag requests output in local file syntax using relative
        /// <br/> 	paths, similar to the workspace-centric view provided by 'status'.
        /// <br/> 	
        /// <br/> 	The -m flag used in conjunction with -e can be used to minimize
        /// <br/> 	costly digest computation on the client by checking file modification
        /// <br/> 	times before checking digests to determine if files have been
        /// <br/> 	modified outside of Perforce.
        /// <br/> 	
        /// <br/> 	The -w flag forces the workspace files to be updated to match the
        /// <br/> 	depot rather than opening them so that the depot can be updated to
        /// <br/> 	match the workspace.  Files that are not under source control will
        /// <br/> 	be deleted, and modified or deleted files will be refreshed.  Note
        /// <br/> 	that this operation will result in the loss of any changes made to
        /// <br/> 	unopened files. This option requires read permission.
        /// <br/> 	
        /// <br/> 	The -k flag updates the have list when files in the workspace but
        /// <br/> 	not on the have list match content of corresponding files in the
        /// <br/> 	depot. In this case, the client's have list is updated to reflect
        /// <br/> 	the matching revisions. This option is used to reconcile the have
        /// <br/> 	list with the workspace.
        /// <br/> 	
        /// <br/> 	The -s flag (only used with 'p4 status') requests summarized
        /// <br/> 	output for the files to be opened for 'add'. Files in the current
        /// <br/> 	directory are listed as usual, but subdirectories containing files
        /// <br/> 	to be opened for 'add' are displayed instead of each file. This
        /// <br/> 	optimized option doesn't support move detection. Files to open
        /// <br/> 	for 'delete' and 'edit' are still listed individually.
        /// <br/> 	
        /// <br/> 	The status command displays preview output which includes files
        /// <br/> 	which are already opened in addition to the files that need to
        /// <br/> 	be reconciled. Opened files are not shown with options -A/-a/-e/-d.
        /// <br/> 	
        /// <br/> 	'p4 reconcile' is not supported for files with propagating attributes
        /// <br/> 	from an edge server in a distributed environment.
        /// <br/> 
        /// </remarks>
        public IList<FileSpec> ReconcileFiles(Options options, params FileSpec[] files)
        {
            return runFileListCmd("reconcile", options, files);
        }
        /// <summary>
        /// 
        /// </summary>
        /// <param name="Files"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        public IList<FileSpec> ReconcileFiles(IList<FileSpec> Files, Options options)
        {
            return ReconcileFiles(options, Files.ToArray());
        }
        /// <summary>
        /// 
        /// </summary>
        /// <param name="options"><cref>ReconcileFilesOptions</cref></param>
        /// <param name="files"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help reconcile</b>
        /// <br/> 
        /// <br/>     reconcile -- Open files for add, delete, and/or edit to reconcile
        /// <br/>     client with workspace changes made outside of Perforce
        /// <br/>     
        /// <br/>     rec         -- synonym for 'reconcile'
        /// <br/>     status      -- 'reconcile -n + opened' (output uses local paths)
        /// <br/>     status -A   -- synonym for 'reconcile -ead' (output uses local paths)
        /// <br/>     
        /// <br/>     clean       -- synonym for 'reconcile -w'
        /// <br/>     
        /// <br/>     p4 reconcile [-c change#] [-e -a -d -f -I -l -m -n -w] [file ...]
        /// <br/>     p4 status [-c change#] [-A | [-e -a -d] | [-s]] [-f -I -m] [file ...]
        /// <br/>     p4 clean [-e -a -d -I -l -n] [file ...]
        /// <br/>     p4 reconcile -k [-l -n] [file ...]
        /// <br/>     p4 status -k [file ...]
        /// <br/> 	
        /// <br/> 	'p4 reconcile' finds unopened files in a client's workspace and
        /// <br/> 	detects the following:
        /// <br/> 	
        /// <br/> 	1. files in depot missing from workspace, but still on have list
        /// <br/> 	2. files on workspace that are not in depot
        /// <br/> 	3. files modified in workspace that are not opened for edit
        /// <br/> 	
        /// <br/> 	By default, the files matching each condition above in the path
        /// <br/> 	are reconciled by opening files for delete (scenario 1), add
        /// <br/> 	(scenario 2), and/or edit (scenario 3). The -e, -a, and -d flags
        /// <br/> 	may be used to limit to a subset of these operations. If no file
        /// <br/> 	arguments are given, reconcile and status default to the current
        /// <br/> 	working directory.
        /// <br/> 	
        /// <br/> 	If the list of files to be opened includes both adds and deletes,
        /// <br/> 	the missing and added files will be compared and converted to pairs
        /// <br/> 	of move/delete and move/add operations if they are similar enough.
        /// <br/> 	
        /// <br/> 	In addition to opening unopened files, reconcile will detect files
        /// <br/> 	that are currently opened for edit but missing from the workspace
        /// <br/> 	and reopen them for delete. Reconcile will also detect files opened
        /// <br/> 	for delete that are present on the workspace and reopen them for
        /// <br/> 	edit.
        /// <br/> 	
        /// <br/> 	The -n flag previews the operation without performing any action.
        /// <br/> 	Although metadata updates from reconcile require open permission,
        /// <br/> 	the preview commands only require read access.
        /// <br/> 	
        /// <br/> 	If -c changelist# is included, the files are opened in the specified
        /// <br/> 	pending changelist.
        /// <br/> 	
        /// <br/> 	The -e flag allows the user to reconcile files that have been
        /// <br/> 	modified outside of Perforce. The reconcile command will open
        /// <br/> 	these files for edit.
        /// <br/> 	
        /// <br/> 	The -a flag allows the user to reconcile files that are in the
        /// <br/> 	user's directory that are not under Perforce source control. These
        /// <br/> 	files are opened for add.
        /// <br/> 	
        /// <br/> 	The -f flag allows the user to add files with filenames that contain
        /// <br/> 	wildcard characters. Filenames that contain the special characters
        /// <br/> 	'@', '#', '%' or '*' are reformatted to encode the characters using
        /// <br/> 	ASCII hexadecimal representation.  After the files are added, you
        /// <br/> 	must refer to them using the reformatted file name, because Perforce
        /// <br/> 	does not recognize the local filesystem name.
        /// <br/> 	
        /// <br/> 	The -I flag informs the client that it should not perform any ignore
        /// <br/> 	checking configured by P4IGNORE.
        /// <br/> 	
        /// <br/> 	The -d flag allows the user to reconcile files that have been
        /// <br/> 	removed from the user's directory but are still in the depot.
        /// <br/> 	These files will be opened for delete only if they are still on the
        /// <br/> 	user's have list.
        /// <br/> 	
        /// <br/> 	The -l flag requests output in local file syntax using relative
        /// <br/> 	paths, similar to the workspace-centric view provided by 'status'.
        /// <br/> 	
        /// <br/> 	The -m flag used in conjunction with -e can be used to minimize
        /// <br/> 	costly digest computation on the client by checking file modification
        /// <br/> 	times before checking digests to determine if files have been
        /// <br/> 	modified outside of Perforce.
        /// <br/> 	
        /// <br/> 	The -w flag forces the workspace files to be updated to match the
        /// <br/> 	depot rather than opening them so that the depot can be updated to
        /// <br/> 	match the workspace.  Files that are not under source control will
        /// <br/> 	be deleted, and modified or deleted files will be refreshed.  Note
        /// <br/> 	that this operation will result in the loss of any changes made to
        /// <br/> 	unopened files. This option requires read permission.
        /// <br/> 	
        /// <br/> 	The -k flag updates the have list when files in the workspace but
        /// <br/> 	not on the have list match content of corresponding files in the
        /// <br/> 	depot. In this case, the client's have list is updated to reflect
        /// <br/> 	the matching revisions. This option is used to reconcile the have
        /// <br/> 	list with the workspace.
        /// <br/> 	
        /// <br/> 	The -s flag (only used with 'p4 status') requests summarized
        /// <br/> 	output for the files to be opened for 'add'. Files in the current
        /// <br/> 	directory are listed as usual, but subdirectories containing files
        /// <br/> 	to be opened for 'add' are displayed instead of each file. This
        /// <br/> 	optimized option doesn't support move detection. Files to open
        /// <br/> 	for 'delete' and 'edit' are still listed individually.
        /// <br/> 	
        /// <br/> 	The status command displays preview output which includes files
        /// <br/> 	which are already opened in addition to the files that need to
        /// <br/> 	be reconciled. Opened files are not shown with options -A/-a/-e/-d.
        /// <br/> 	
        /// <br/> 	'p4 reconcile' is not supported for files with propagating attributes
        /// <br/> 	from an edge server in a distributed environment.
        /// <br/> 
        /// </remarks>
        public IList<FileSpec> ReconcileStatus(Options options, params FileSpec[] files)
        {
            return runFileListCmd("status", options, files);
        }
        /// <summary>
        /// 
        /// </summary>
        /// <param name="Files"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        public IList<FileSpec> ReconcileStatus(IList<FileSpec> Files, Options options)
        {
            return ReconcileStatus(options, Files.ToArray());
        }
        /// <summary>
        /// 
        /// </summary>
        /// <param name="options"><cref>ReconcileFilesOptions</cref></param>
        /// <param name="files"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help reconcile</b>
        /// <br/> 
        /// <br/>     reconcile -- Open files for add, delete, and/or edit to reconcile
        /// <br/>     client with workspace changes made outside of Perforce
        /// <br/>     
        /// <br/>     rec         -- synonym for 'reconcile'
        /// <br/>     status      -- 'reconcile -n + opened' (output uses local paths)
        /// <br/>     status -A   -- synonym for 'reconcile -ead' (output uses local paths)
        /// <br/>     
        /// <br/>     clean       -- synonym for 'reconcile -w'
        /// <br/>     
        /// <br/>     p4 reconcile [-c change#] [-e -a -d -f -I -l -m -n -w] [file ...]
        /// <br/>     p4 status [-c change#] [-A | [-e -a -d] | [-s]] [-f -I -m] [file ...]
        /// <br/>     p4 clean [-e -a -d -I -l -n] [file ...]
        /// <br/>     p4 reconcile -k [-l -n] [file ...]
        /// <br/>     p4 status -k [file ...]
        /// <br/> 	
        /// <br/> 	'p4 reconcile' finds unopened files in a client's workspace and
        /// <br/> 	detects the following:
        /// <br/> 	
        /// <br/> 	1. files in depot missing from workspace, but still on have list
        /// <br/> 	2. files on workspace that are not in depot
        /// <br/> 	3. files modified in workspace that are not opened for edit
        /// <br/> 	
        /// <br/> 	By default, the files matching each condition above in the path
        /// <br/> 	are reconciled by opening files for delete (scenario 1), add
        /// <br/> 	(scenario 2), and/or edit (scenario 3). The -e, -a, and -d flags
        /// <br/> 	may be used to limit to a subset of these operations. If no file
        /// <br/> 	arguments are given, reconcile and status default to the current
        /// <br/> 	working directory.
        /// <br/> 	
        /// <br/> 	If the list of files to be opened includes both adds and deletes,
        /// <br/> 	the missing and added files will be compared and converted to pairs
        /// <br/> 	of move/delete and move/add operations if they are similar enough.
        /// <br/> 	
        /// <br/> 	In addition to opening unopened files, reconcile will detect files
        /// <br/> 	that are currently opened for edit but missing from the workspace
        /// <br/> 	and reopen them for delete. Reconcile will also detect files opened
        /// <br/> 	for delete that are present on the workspace and reopen them for
        /// <br/> 	edit.
        /// <br/> 	
        /// <br/> 	The -n flag previews the operation without performing any action.
        /// <br/> 	Although metadata updates from reconcile require open permission,
        /// <br/> 	the preview commands only require read access.
        /// <br/> 	
        /// <br/> 	If -c changelist# is included, the files are opened in the specified
        /// <br/> 	pending changelist.
        /// <br/> 	
        /// <br/> 	The -e flag allows the user to reconcile files that have been
        /// <br/> 	modified outside of Perforce. The reconcile command will open
        /// <br/> 	these files for edit.
        /// <br/> 	
        /// <br/> 	The -a flag allows the user to reconcile files that are in the
        /// <br/> 	user's directory that are not under Perforce source control. These
        /// <br/> 	files are opened for add.
        /// <br/> 	
        /// <br/> 	The -f flag allows the user to add files with filenames that contain
        /// <br/> 	wildcard characters. Filenames that contain the special characters
        /// <br/> 	'@', '#', '%' or '*' are reformatted to encode the characters using
        /// <br/> 	ASCII hexadecimal representation.  After the files are added, you
        /// <br/> 	must refer to them using the reformatted file name, because Perforce
        /// <br/> 	does not recognize the local filesystem name.
        /// <br/> 	
        /// <br/> 	The -I flag informs the client that it should not perform any ignore
        /// <br/> 	checking configured by P4IGNORE.
        /// <br/> 	
        /// <br/> 	The -d flag allows the user to reconcile files that have been
        /// <br/> 	removed from the user's directory but are still in the depot.
        /// <br/> 	These files will be opened for delete only if they are still on the
        /// <br/> 	user's have list.
        /// <br/> 	
        /// <br/> 	The -l flag requests output in local file syntax using relative
        /// <br/> 	paths, similar to the workspace-centric view provided by 'status'.
        /// <br/> 	
        /// <br/> 	The -m flag used in conjunction with -e can be used to minimize
        /// <br/> 	costly digest computation on the client by checking file modification
        /// <br/> 	times before checking digests to determine if files have been
        /// <br/> 	modified outside of Perforce.
        /// <br/> 	
        /// <br/> 	The -w flag forces the workspace files to be updated to match the
        /// <br/> 	depot rather than opening them so that the depot can be updated to
        /// <br/> 	match the workspace.  Files that are not under source control will
        /// <br/> 	be deleted, and modified or deleted files will be refreshed.  Note
        /// <br/> 	that this operation will result in the loss of any changes made to
        /// <br/> 	unopened files. This option requires read permission.
        /// <br/> 	
        /// <br/> 	The -k flag updates the have list when files in the workspace but
        /// <br/> 	not on the have list match content of corresponding files in the
        /// <br/> 	depot. In this case, the client's have list is updated to reflect
        /// <br/> 	the matching revisions. This option is used to reconcile the have
        /// <br/> 	list with the workspace.
        /// <br/> 	
        /// <br/> 	The -s flag (only used with 'p4 status') requests summarized
        /// <br/> 	output for the files to be opened for 'add'. Files in the current
        /// <br/> 	directory are listed as usual, but subdirectories containing files
        /// <br/> 	to be opened for 'add' are displayed instead of each file. This
        /// <br/> 	optimized option doesn't support move detection. Files to open
        /// <br/> 	for 'delete' and 'edit' are still listed individually.
        /// <br/> 	
        /// <br/> 	The status command displays preview output which includes files
        /// <br/> 	which are already opened in addition to the files that need to
        /// <br/> 	be reconciled. Opened files are not shown with options -A/-a/-e/-d.
        /// <br/> 	
        /// <br/> 	'p4 reconcile' is not supported for files with propagating attributes
        /// <br/> 	from an edge server in a distributed environment.
        /// <br/> 
        /// </remarks>
        public IList<FileSpec> ReconcileClean(Options options, params FileSpec[] files)
        {
            return runFileListCmd("clean", options, files);
        }
        /// <summary>
        /// 
        /// </summary>
        /// <param name="Files"></param>
        /// <param name="options"></param>
        /// <returns></returns>
        public IList<FileSpec> ReconcileClean(IList<FileSpec> Files, Options options)
        {
            return ReconcileClean(options, Files.ToArray());
        }

        /// <summary>
        /// 
        /// </summary>
        /// <param name="options"><cref>RevertFilesOptions</cref></param>
        /// <param name="files"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help revert</b>
        /// <br/> 
        /// <br/>     revert -- Discard changes from an opened file
        /// <br/> 
        /// <br/>     p4 revert [-a -n -k -w -c changelist# -C client] file ...
        /// <br/> 
        /// <br/> 	Revert an open file to the revision that was synced from the depot,
        /// <br/> 	discarding any edits or integrations that have been made.  You must
        /// <br/> 	explicitly specify the files to be reverted.  Files are removed from
        /// <br/> 	the changelist in which they are open.  Locked files are unlocked.
        /// <br/> 
        /// <br/> 	The -a flag reverts only files that are open for edit, add, or
        /// <br/> 	integrate and are unchanged or missing. Files with pending
        /// <br/> 	integration records are left open. The file arguments are optional
        /// <br/> 	when -a is specified.
        /// <br/> 
        /// <br/> 	The -n flag displays a preview of the operation.
        /// <br/> 
        /// <br/> 	The -k flag marks the file as reverted in server metadata without
        /// <br/> 	altering files in the client workspace.
        /// <br/> 
        /// <br/> 	The -w flag causes files that are open for add to be deleted from the
        /// <br/> 	workspace when they are reverted.
        /// <br/> 
        /// <br/> 	The -c flag reverts files that are open in the specified changelist.
        /// <br/> 
        /// <br/> 	The -C flag allows a user to specify the workspace that has the file
        /// <br/> 	opened rather than defaulting to the current client workspace. When
        /// <br/> 	this option is used, the '-k' flag is also enabled and the check for
        /// <br/> 	matching user is disabled. The -C flag requires 'admin' access, which
        /// <br/> 	is granted by 'p4 protect'.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        public IList<FileSpec> RevertFiles(Options options, params FileSpec[] files)
		{
			return runFileListCmd("revert", options, files);
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="Files"></param>
		/// <param name="options"></param>
		/// <returns></returns>
		public IList<FileSpec> RevertFiles(IList<FileSpec> Files, Options options)
		{
			return RevertFiles(options, Files.ToArray());
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="options"></param>
		/// <param name="files"></param>
		/// <returns></returns>
		/// <remarks>
		/// <br/><b>p4 help shelve</b>
		/// <br/> 
		/// <br/>     shelve -- Store files from a pending changelist into the depot
		/// <br/> 
		/// <br/>     p4 shelve [-p] [files]
		/// <br/>     p4 shelve [-a option] [-p] -i [-f | -r]
		/// <br/>     p4 shelve [-a option] [-p] -r -c changelist#
		/// <br/>     p4 shelve [-a option] [-p] -c changelist# [-f] [file ...]
		/// <br/>     p4 shelve -d -c changelist# [-f] [file ...]
		/// <br/> 
		/// <br/> 	'p4 shelve' creates, modifies or deletes shelved files in a pending
		/// <br/> 	changelist. Shelved files remain in the depot until they are deleted
		/// <br/> 	(using 'p4 shelve -d') or replaced by subsequent shelve commands.
		/// <br/> 	After 'p4 shelve', the user can revert the files and restore them
		/// <br/> 	later using 'p4 unshelve'.  Other users can 'p4 unshelve' the stored
		/// <br/> 	files into their own workspaces.
		/// <br/> 
		/// <br/> 	Files that have been shelved can be accessed by the 'p4 diff',
		/// <br/> 	'p4 diff2', 'p4 files' and 'p4 print' commands using the revision
		/// <br/> 	specification '@=change', where 'change' is the pending changelist
		/// <br/> 	number.
		/// <br/> 
		/// <br/> 	By default, 'p4 shelve' creates a changelist, adds files from the
		/// <br/> 	user's default changelist, then shelves those files in the depot.
		/// <br/> 	The user is presented with a text changelist form displayed using
		/// <br/> 	the editor configured using the $P4EDITOR environment variable.
		/// <br/> 
		/// <br/> 	If a file pattern is specified, 'p4 shelve' shelves the files that
		/// <br/> 	match the pattern.
		/// <br/> 
		/// <br/> 	The -i flag reads the pending changelist specification with shelved
		/// <br/> 	files from the standard input.  The user's editor is not invoked.
		/// <br/> 	To modify an existing changelist with shelved files, specify the
		/// <br/> 	changelist number using the -c flag.
		/// <br/> 
		/// <br/> 	The -c flag specifies the pending changelist that contains shelved
		/// <br/> 	files to be created, deleted, or modified. Only the user and client
		/// <br/> 	of the pending changelist can add or modify its shelved files. Any
		/// <br/> 	files specified by the file pattern must already be opened in the
		/// <br/> 	specified changelist; use 'p4 reopen' to move an opened file from
		/// <br/> 	one changelist to another.
		/// <br/> 
		/// <br/> 	The -f (force) flag must be used with the -c or -i flag to overwrite
		/// <br/> 	any existing shelved files in a pending changelist.
		/// <br/> 
		/// <br/> 	The -r flag (used with -c or -i) enables you to replace all shelved
		/// <br/> 	files in that changelist with the files opened in your own workspace
		/// <br/> 	at that changelist number.  Previously shelved files will be deleted.
		/// <br/> 	Only the user and client workspace of the pending changelist can
		/// <br/> 	replace its shelved files.
		/// <br/> 
		/// <br/> 	The -a flag enables you to handle unchanged files similarly to some
		/// <br/> 	client submit options, namely 'submitunchanged' and 'leaveunchanged'.
		/// <br/> 	The default behavior of shelving all files corresponds to the
		/// <br/> 	'submitunchanged' option. The 'leaveunchanged' option only shelves
		/// <br/> 	changed files, and then leaves the files opened in the pending
		/// <br/> 	changelist on the client.
		/// <br/> 
		/// <br/> 	The -d flag (used with -c) deletes the shelved files in the specified
		/// <br/> 	changelist so that they can no longer be unshelved.  By default, only
		/// <br/> 	the user and client of the pending changelist can delete its shelved
		/// <br/> 	files. A user with 'admin' access can delete shelved files by including
		/// <br/> 	the -f flag to force the operation.
		/// <br/> 
		/// <br/> 	The -p flag promotes a shelved change from an edge server to a commit
		/// <br/> 	server where it can be accessed by other edge servers participating
		/// <br/> 	in the distributed configuration.  Once a shelved change has been
		/// <br/> 	promoted, all subsequent local modifications to the shelf are also
		/// <br/> 	pushed to the commit server and remain until the shelf is deleted.
		/// <br/> 	Once a shelf has been created, the combination of flags '-p -c' will
		/// <br/> 	promote the shelf without modification unless '-f' or '-r' are also
		/// <br/> 	used to update the shelved files before promotion.
		/// <br/> 
		/// <br/> 	'p4 shelve' is not supported for files with propagating attributes
		/// <br/> 	from an edge server in a distributed environment.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> ShelveFiles(Options options, params FileSpec[] files)
		{
			string cmdData = null;
			if (options.ContainsKey("-i"))
			{ 
				cmdData = options["-i"];
				options["-i"] = null;
			}
			return runFileListCmd("shelve", options, cmdData, files);
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="Files"></param>
		/// <param name="options"></param>
		/// <returns></returns>
		public IList<FileSpec> ShelveFiles(IList<FileSpec> Files, Options options)
		{
			return ShelveFiles(options, Files!=null?Files.ToArray(): null );
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="options"></param>
		/// <param name="files"></param>
		/// <returns></returns>
		/// <remarks>
		/// <br/><b>p4 help sync</b>
		/// <br/> 
		/// <br/>     sync -- Synchronize the client with its view of the depot
		/// <br/>     flush -- synonym for 'sync -k'
		/// <br/>     update -- synonym for 'sync -s'
		/// <br/> 
		/// <br/>     p4 sync [-f -L -n -N -k -q -r] [-m max] [file[revRange] ...]
		/// <br/>     p4 sync [-L -n -N -q -s] [-m max] [file[revRange] ...]
		/// <br/>     p4 sync [-L -n -N -p -q] [-m max] [file[revRange] ...]
		/// <br/>             --parallel=threads=N[,batch=N][,batchsize=N][,min=N][,minsize=N]
		/// <br/> 
		/// <br/> 	Sync updates the client workspace to reflect its current view (if
		/// <br/> 	it has changed) and the current contents of the depot (if it has
		/// <br/> 	changed). The client view maps client and depot file names and
		/// <br/> 	locations.
		/// <br/> 
		/// <br/> 	Sync adds files that are in the client view and have not been
		/// <br/> 	retrieved before.  Sync deletes previously retrieved files that
		/// <br/> 	are no longer in the client view or have been deleted from the
		/// <br/> 	depot.  Sync updates files that are still in the client view and
		/// <br/> 	have been updated in the depot.
		/// <br/> 
		/// <br/> 	By default, sync affects all files in the client workspace. If file
		/// <br/> 	arguments are given, sync limits its operation to those files.
		/// <br/> 	The file arguments can contain wildcards.
		/// <br/> 
		/// <br/> 	If the file argument includes a revision specifier, then the given
		/// <br/> 	revision is retrieved.  Normally, the head revision is retrieved.
		/// <br/> 	See 'p4 help revisions' for help specifying revisions.
		/// <br/> 
		/// <br/> 	If the file argument includes a revision range specification,
		/// <br/> 	only files selected by the revision range are updated, and the
		/// <br/> 	highest revision in the range is used.
		/// <br/> 
		/// <br/> 	Normally, sync does not overwrite workspace files that the user has
		/// <br/> 	manually made writable.  Setting the 'clobber' option in the
		/// <br/> 	client specification disables this safety check.
		/// <br/> 
		/// <br/> 	The -f flag forces resynchronization even if the client already
		/// <br/> 	has the file, and overwriting any writable files.  This flag doesn't
		/// <br/> 	affect open files.
		/// <br/> 
		/// <br/> 	The -L flag can be used with multiple file arguments that are in
		/// <br/> 	full depot syntax and include a valid revision number. When this
		/// <br/> 	flag is used the arguments are processed together by building an
		/// <br/> 	internal table similar to a label. This file list processing is
		/// <br/> 	significantly faster than having to call the internal query engine
		/// <br/> 	for each individual file argument. However, the file argument syntax
		/// <br/> 	is strict and the command will not run if an error is encountered.
		/// <br/> 
		/// <br/> 	The -n flag previews the operation without updating the workspace.
		/// <br/> 
		/// <br/> 	The -N flag also previews the operation without updating the
		/// <br/> 	workspace, but reports only a summary of the work the sync would do.
		/// <br/> 
		/// <br/> 	The -k flag updates server metadata without syncing files. It is
		/// <br/> 	intended to enable you to ensure that the server correctly reflects
		/// <br/> 	the state of files in the workspace while avoiding a large data
		/// <br/> 	transfer. Caution: an erroneous update can cause the server to
		/// <br/> 	incorrectly reflect the state of the workspace.
		/// <br/> 
		/// <br/> 	The -p flag populates the client workspace, but does not update the
		/// <br/> 	server to reflect those updates.  Any file that is already synced or
		/// <br/> 	opened will be bypassed with a warning message.  This option is very
		/// <br/> 	useful for build clients or when publishing content without the
		/// <br/> 	need to track the state of the client workspace.
		/// <br/> 
		/// <br/> 	The -q flag suppresses normal output messages. Messages regarding
		/// <br/> 	errors or exceptional conditions are not suppressed.
		/// <br/> 
		/// <br/> 	The -r flag allows open files which are mapped to new locations
		/// <br/> 	in the depot to be reopened in the new location.  By default, open
		/// <br/> 	workspace files remain associated with the depot files that they were
		/// <br/> 	originally opened as.
		/// <br/> 
		/// <br/> 	The -s flag adds a safety check before sending content to the client
		/// <br/> 	workspace.  This check uses MD5 digests to compare the content on the
		/// <br/> 	clients workspace against content that was last synced.  If the file
		/// <br/> 	has been modified outside of Perforce's control then an error message
		/// <br/> 	is displayed and the file is not overwritten.  This check adds some
		/// <br/> 	extra processing which will affect the performance of the operation.
		/// <br/> 	Clients with 'allwrite' and 'noclobber' set do this check by default.
		/// <br/> 
		/// <br/> 	The -m flag limits sync to the first 'max' number of files. This
		/// <br/> 	option is useful in conjunction with tagged output and the '-n'
		/// <br/> 	flag, to preview how many files will be synced without transferring
		/// <br/> 	all the file data.
		/// <br/> 
		/// <br/> 	The --parallel flag specifies options for parallel file transfer. If
		/// <br/> 	your administrator has enabled parallel file transfer by setting the
		/// <br/> 	net.parallel.max configurable, and if there are sufficient resources
		/// <br/> 	across the system, a sync command may execute more rapidly by
		/// <br/> 	transferring multiple files in parallel. Specify threads=N to request
		/// <br/> 	files be sent concurrently, using N independent network connections.
		/// <br/> 	The N threads grab work in batches; specify batch=N to control the
		/// <br/> 	number of files in a batch, or batchsize=N to control the number of
		/// <br/> 	bytes in a batch. A sync that is too small will not initiate parallel
		/// <br/> 	file transfers; specify min=N to control the minimum number of files
		/// <br/> 	in a parallel sync, or minsize=N to control the minimum number of
		/// <br/> 	bytes in a parallel sync. Requesting progress indicators causes the
		/// <br/> 	--parallel flag to be ignored.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> SyncFiles(Options options, params FileSpec[] files)
		{
			return runFileListCmd("sync", options, files);
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="Files"></param>
		/// <param name="options"></param>
		/// <returns></returns>
		public IList<FileSpec> SyncFiles(IList<FileSpec> Files, Options options)
		{
			return SyncFiles(options, Files.ToArray());
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="options"></param>
		/// <param name="files"></param>
		/// <returns></returns>
		/// <remarks>
		/// <br/><b>p4 help unlock</b>
		/// <br/> 
		/// <br/>     unlock -- Release a locked file, leaving it open
		/// <br/> 
		/// <br/>     p4 unlock [-c | -s changelist# | -x] [-f] [file ...]
		/// <br/>     p4 -c client unlock [-f] -r
		/// <br/> 
		/// <br/> 	'p4 unlock' releases locks on the specified files, which must be
		/// <br/> 	open in the specified pending changelist. If you omit the changelist
		/// <br/> 	number, the default changelist is assumed. If you omit the file name,
		/// <br/> 	all locked files are unlocked.
		/// <br/> 
		/// <br/> 	The -s flag unlocks files from a shelved changelist caused by an
		/// <br/> 	aborted 'submit -e' operation. The -c flag applies to opened files
		/// <br/> 	in a pending changelist locked by 'p4 lock' or by a failed submit
		/// <br/> 	of a change that is not shelved.
		/// <br/> 
		/// <br/> 	By default, files can be unlocked only by the changelist owner who
		/// <br/> 	must also be the person who has the files locked. The -f flag
		/// <br/> 	enables you to unlock files in changelists owned by other users.
		/// <br/> 	The -f flag requires 'admin' access, which is granted by 'p4
		/// <br/> 	protect'.
		/// <br/> 
		/// <br/> 	The -x option unlocks files that are opened 'exclusive' but are
		/// <br/> 	orphaned (see 'p4 opened -x'). This option only applies to a
		/// <br/> 	distributed installation where global tracking of these file types
		/// <br/> 	is necessary across servers.
		/// <br/> 
		/// <br/> 	If a push command from a remote server to this server fails, files
		/// <br/> 	can be left locked on this server, preventing other users from
		/// <br/> 	submitting changes to those files. In this case, the user who issued
		/// <br/> 	the push command can specify the -r flag with the name of the client
		/// <br/> 	that was used on that remote server to unlock the files on this
		/// <br/> 	server. An administrator can run 'unlock -f -r' as well.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> UnlockFiles(Options options, params FileSpec[] files)
		{
			return runFileListCmd("unlock", options, files);
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="Files"></param>
		/// <param name="options"></param>
		/// <returns></returns>
		public IList<FileSpec> UnlockFiles(IList<FileSpec> Files, Options options)
		{
			return UnlockFiles(options, Files.ToArray());
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="options"></param>
		/// <param name="files"></param>
		/// <returns></returns>
		/// <remarks>
		/// <br/><b>p4 help unshelve</b>
		/// <br/> 
		/// <br/>     unshelve -- Restore shelved files from a pending change into a workspace
		/// <br/> 
		/// <br/>     p4 unshelve -s changelist# [options] [file ...]
		/// <br/> 	Options: [-f -n] [-c changelist#] [-b branch|-S stream [-P parent]]
		/// <br/> 
		/// <br/> 	'p4 unshelve' retrieves shelved files from the specified pending
		/// <br/> 	changelist, opens them in a pending changelist and copies them
		/// <br/> 	to the invoking user's workspace.  Unshelving files from a pending
		/// <br/> 	changelist is restricted by the user's permissions on the files.
		/// <br/> 	A successful unshelve operation places the shelved files on the
		/// <br/> 	user's workspace with the same open action and pending integration
		/// <br/> 	history as if it had originated from that user and client.
		/// <br/> 
		/// <br/> 	Unshelving a file over an already opened file is permitted if both
		/// <br/> 	shelved file and opened file are opened for 'edit'. In a distributed
		/// <br/> 	environment, an additional requirement is that the shelve was created
		/// <br/> 	on the same edge server. After unshelving, the workspace file is
		/// <br/> 	flagged as unresolved, and 'p4 resolve' must be run to resolve the
		/// <br/> 	differences between the shelved file and the workspace file.
		/// <br/> 
		/// <br/> 	Unshelving a file opened for 'add' when the file already exists
		/// <br/> 	in the depot will result in the file being opened for edit. After
		/// <br/> 	unshelving, the workspace file is flagged as unresolved, and
		/// <br/> 	'p4 resolve' must be run to resolve the differences between the
		/// <br/> 	shelved file and the depot file at the head revision. Note that
		/// <br/> 	unshelving a file opened for 'add' over an already opened file is
		/// <br/> 	not supported.
		/// <br/> 
		/// <br/> 	The -s flag specifies the number of the pending changelist that
		/// <br/> 	contains the shelved files.
		/// <br/> 
		/// <br/> 	If a file pattern is specified, 'p4 unshelve' unshelves files that
		/// <br/> 	match the pattern.
		/// <br/> 
		/// <br/> 	The -b flag specifies a branch spec that the shelved files will be
		/// <br/> 	mapped through prior to being unshelved, allowing files to be shelved
		/// <br/> 	in one branch and unshelved in another.  As with unshelving into an
		/// <br/> 	open file, it may be necessary to run 'p4 resolve'. In a distributed
		/// <br/> 	environment, an additional requirement is that the shelve was created
		/// <br/> 	on the same edge server.
		/// <br/> 
		/// <br/> 	The -S flag uses a generated branch view to map the shelved files
		/// <br/> 	between the specified stream and its parent stream.  The -P flag
		/// <br/> 	can be used to generate the view using a parent stream other than
		/// <br/> 	the actual parent.
		/// <br/> 
		/// <br/> 	The -c flag specifies the changelist to which files are unshelved.
		/// <br/> 	By default,  'p4 unshelve' opens shelved files in the default
		/// <br/> 	changelist.
		/// <br/> 
		/// <br/> 	The -f flag forces the clobbering of any writeable but unopened files
		/// <br/> 	that are being unshelved.
		/// <br/> 
		/// <br/> 	The -n flag previews the operation without changing any files or
		/// <br/> 	metadata.
		/// <br/> 
		/// <br/> 	'p4 unshelve' is not supported for files with propagating attributes
		/// <br/> 	from an edge server in a distributed environment.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> UnshelveFiles(Options options, params FileSpec[] files)
		{
			return runFileListCmd("unshelve", options, files);
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="Files"></param>
		/// <param name="options"></param>
		/// <returns></returns>
		public IList<FileSpec> UnshelveFiles(IList<FileSpec> Files, Options options)
		{
			return UnshelveFiles(options, Files.ToArray());
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="files"></param>
		/// <returns></returns>
		/// <remarks>
		/// <br/><b>p4 help where</b>
		/// <br/> 
		/// <br/>     where -- Show how file names are mapped by the client view
		/// <br/> 
		/// <br/>     p4 where [file ...]
		/// <br/> 
		/// <br/> 	Where shows how the specified files are mapped by the client view.
		/// <br/> 	For each argument, three names are produced: the name in the depot,
		/// <br/> 	the name on the client in Perforce syntax, and the name on the client
		/// <br/> 	in local syntax.
		/// <br/> 
		/// <br/> 	If the file parameter is omitted, the mapping for all files in the
		/// <br/> 	current directory and below) is returned.
		/// <br/> 
		/// <br/> 	Note that 'p4 where' does not determine where any real files reside.
		/// <br/> 	It only displays the locations that are mapped by the client view.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<FileSpec> GetClientFileMappings(params FileSpec[] files)
		{
			return runFileListCmd("where", null, files);
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="Files"></param>
		/// <returns></returns>
		public IList<FileSpec> GetClientFileMappings(IList<FileSpec> Files)
		{
			return GetClientFileMappings(Files.ToArray());
		}

        /// <summary>
        /// 
        /// </summary>
        /// <param name="options"></param>
        /// <param name="fromFile"></param>
        /// <param name="toFiles"></param>
        /// <returns></returns>
        /// <remarks>
        /// <br/><b>p4 help copy</b>
        /// <br/> 
        /// <br/>     copy -- Copy one set of files to another
        /// <br/> 
        /// <br/>     p4 copy [options] fromFile[rev] toFile
        /// <br/>     p4 copy [options] -b branch [-r] [toFile[rev] ...]
        /// <br/>     p4 copy [options] -b branch -s fromFile[rev] [toFile ...]
        /// <br/>     p4 copy [options] -S stream [-P parent] [-F] [-r] [toFile[rev] ...]
        /// <br/> 
        /// <br/> 	options: -c changelist# -f -n -v -m max -q
        /// <br/> 
        /// <br/> 	'p4 copy' copies one set of files (the 'source') into another (the
        /// <br/> 	'target').
        /// <br/> 
        /// <br/> 	Using the client workspace as a staging area, 'p4 copy' makes the
        /// <br/> 	target identical to the source by branching, replacing, or deleting
        /// <br/> 	files.  'p4 submit' submits copied files to the depot. 'p4 revert'
        /// <br/> 	can be used to revert copied files instead of submitting them.  The
        /// <br/> 	history of copied files can be shown with 'p4 filelog' or 'p4
        /// <br/> 	integrated'.
        /// <br/> 
        /// <br/> 	Target files that are already identical to the source, or that are
        /// <br/> 	outside of the client view, are not affected by 'p4 copy'. Opened,
        /// <br/> 	non-identical target files cause 'p4 copy' to exit with a warning. 
        /// <br/> 	When 'p4 copy' creates or modifies files in the workspace, it leaves
        /// <br/> 	them read-only; 'p4 edit' can make them writable.  Files opened by
        /// <br/> 	'p4 copy' do not need to be resolved.
        /// <br/> 
        /// <br/> 	Source and target files (fromFile and toFile) can be specified on
        /// <br/> 	the 'p4 copy' command line or through a branch view. On the command
        /// <br/> 	line, fromFile is the source file set and toFile is the target file
        /// <br/> 	set.  With a branch view, one or more toFile arguments can be given
        /// <br/> 	to limit the scope of the target file set.
        /// <br/> 
        /// <br/> 	A revision specifier can be used to select the revision to copy; by
        /// <br/> 	default, the head revision is copied. The revision specifier can be
        /// <br/> 	used on fromFile, or on toFile, but not on both.  When used on toFile,
        /// <br/> 	it refers to source revisions, not to target revisions.  A range may
        /// <br/> 	not be used as a revision specifier.  For revision syntax, see 'p4
        /// <br/> 	help revisions'.
        /// <br/> 
        /// <br/> 	The -S flag makes 'p4 copy' copy from a stream to its parent.
        /// <br/> 	Use -r with -S to reverse the copy direction.  Note that to submit
        /// <br/> 	copied stream files, the current client must be switched to the
        /// <br/> 	target stream, or to a virtual child stream of the target stream.
        /// <br/> 	The -S flag causes 'p4 copy' to use a generated branch view that
        /// <br/> 	maps the stream to its parent.  -P can be used to generate the
        /// <br/> 	branch view using a parent stream other than the stream's actual
        /// <br/> 	parent.  The -S flag also makes 'p4 copy' respect a stream's flow.
        /// <br/> 
        /// <br/> 	The -F flag can be used with -S to force copying against a stream's
        /// <br/> 	expected flow. It can also force -S to generate a branch view based
        /// <br/> 	on a virtual stream; the mapping itself refers to the underlying
        /// <br/> 	real stream.
        /// <br/> 
        /// <br/> 	The -b flag makes 'p4 copy' use a user-defined branch view.  (See
        /// <br/> 	'p4 help branch'.) The source is the left side of the branch view
        /// <br/> 	and the target is the right side. With -r, the direction is reversed.
        /// <br/> 
        /// <br/> 	The -s flag can be used with -b to cause fromFile to be treated as
        /// <br/> 	the source, and both sides of the user-defined branch view to be
        /// <br/> 	treated as the target, per the branch view mapping.  Optional toFile
        /// <br/> 	arguments may be given to further restrict the scope of the target
        /// <br/> 	file set. -r is ignored when -s is used.
        /// <br/> 
        /// <br/> 	The -c changelist# flag opens files in the designated (numbered)
        /// <br/> 	pending changelist instead of the default changelist.
        /// <br/> 
        /// <br/> 	The -f flag forces the creation of extra revisions in order to
        /// <br/> 	explicitly record that files have been copied.  Deleted source files
        /// <br/> 	will be copied if they do not exist in the target, and files that are
        /// <br/> 	already identical will be copied if they are not connected by existing
        /// <br/> 	integration records.
        /// <br/> 
        /// <br/> 	The -n flag displays a preview of the copy, without actually doing
        /// <br/> 	anything.
        /// <br/> 
        /// <br/> 	The -m flag limits the actions to the first 'max' number of files.
        /// <br/> 
        /// <br/> 	The -q flag suppresses normal output messages. Messages regarding
        /// <br/> 	errors or exceptional conditions are displayed.
        /// <br/> 
        /// <br/> 	The -v flag causes a 'virtual' copy that does not modify client
        /// <br/> 	workspace files.  After submitting a virtual integration, 'p4 sync'
        /// <br/> 	can be used to update the workspace.
        /// <br/> 
        /// <br/> 	'p4 copy' is not supported for files with propagating attributes
        /// <br/> 	from an edge server in a distributed environment.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        public IList<FileSpec> CopyFiles(Options options, FileSpec fromFile, params FileSpec[] toFiles)
		{
			if ((options != null) && (options.ContainsKey("-s")) && (fromFile == null))
			{
				throw new ArgumentNullException("fromFile", 
					"From file cannot be null when the -s  flag is specified");
			}
			IList<FileSpec> Files = null;
			if ((toFiles != null) && (toFiles.Length > 0))
			{
				Files = new List<FileSpec>(toFiles);
			}
			else
			{
				if (fromFile != null)
				{
					Files = new List<FileSpec>();
				}
			}
			if ((Files != null) && (fromFile != null))
			{
				Files.Insert(0, fromFile);
			}
			return runFileListCmd("copy", options, Files.ToArray());
		}
		public IList<FileSpec> CopyFiles(FileSpec fromFile, IList<FileSpec> toFiles, Options options)
		{
			return CopyFiles(options, fromFile, toFiles.ToArray());
		}
		public IList<FileSpec> CopyFiles(Options options)
		{
			return runFileListCmd("copy", options);
		}

        /// <remarks>
        /// <br/><b>p4 help merge</b>
        /// <br/> 
        /// <br/>     merge -- Merge one set of files into another 
        /// <br/> 
        /// <br/>     p4 merge [options] [-F] [--from stream] [toFile][revRange]
        /// <br/>     p4 merge [options] fromFile[revRange] toFile
        /// <br/> 
        /// <br/> 	options: -c changelist# -m max -n -Ob -q
        /// <br/> 
        /// <br/> 	'p4 merge' merges changes from one set of files (the 'source') into 
        /// <br/> 	another (the 'target'). It is a simplified form of the 'p4 integrate'
        /// <br/> 	command, similar to 'p4 integrate -Rbd -Or'.
        /// <br/> 
        /// <br/> 	Using the client workspace as a staging area, 'p4 merge' schedules all
        /// <br/> 	affected target files to be resolved per changes in the source.
        /// <br/> 	Target files outside of	the current client view are not affected.
        /// <br/> 	Source files need not be within the client view.
        /// <br/> 
        /// <br/> 	'p4 resolve' must be used to resolve all changes.  'p4 submit' commits
        /// <br/> 	merged files to	the depot.  Unresolved files may not be submitted.
        /// <br/> 	Merged files can be shelved with 'p4 shelve' and abandoned with
        /// <br/> 	'p4 revert'.  The commands 'p4 integrated' and 'p4 filelog' display
        /// <br/> 	merge history.
        /// <br/> 
        /// <br/> 	When 'p4 merge' schedules a workspace file to be resolved, it leaves
        /// <br/> 	it read-only. 'p4 resolve' can operate on a read-only file;  for 
        /// <br/> 	other pre-submit changes, 'p4 edit' must be used to make the file 
        /// <br/> 	writable.
        /// <br/> 
        /// <br/> 	By default, 'p4 merge' merges changes into the current stream from its
        /// <br/> 	parent, or from another stream specified by the --from flag.  The
        /// <br/> 	source and target can also be specified on the command line as a
        /// <br/> 	pair of file paths.  More complex merge mappings can be specified
        /// <br/> 	via branchspecs as with 'p4 integrate' (see 'p4 help integrate').
        /// <br/> 
        /// <br/> 	Each file in the target is mapped to a file in the source. Mapping 
        /// <br/> 	adjusts automatically for files that have been moved or renamed, as
        /// <br/> 	long as 'p4 move' was used to move/rename files.  The scope of source
        /// <br/> 	and target file sets must include both old-named and new-named files
        /// <br/> 	for mappings to be adjusted.  Moved source files may schedule moves 
        /// <br/> 	to be resolved in target files. 
        /// <br/> 
        /// <br/> 	revRange is a revision or a revision range that limits the span of
        /// <br/> 	source history to be probed for unintegrated revisions.  For details
        /// <br/> 	about revision specifiers, see 'p4 help revisions'.
        /// <br/> 
        /// <br/> 	The -F flag can be used to force merging against a stream's expected
        /// <br/> 	flow. It can also force the generation of a branch view based on a
        /// <br/> 	virtual stream; the mapping itself refers to the underlying real
        /// <br/> 	stream.
        /// <br/> 
        /// <br/> 	The -Ob flag causes the base revision (if any) to be displayed along
        /// <br/> 	with each scheduled resolve.
        /// <br/> 
        /// <br/> 	The -q flag suppresses normal output messages. Messages regarding
        /// <br/> 	errors or exceptional conditions are displayed.
        /// <br/> 
        /// <br/> 	Merging is not supported for files with propagating attributes
        /// <br/> 	from an edge server in a distributed environment. Even if the
        /// <br/> 	merge command succeeds, the required subsequent resolve command
        /// <br/> 	will fail.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        public IList<FileSpec> MergeFiles(Options options, FileSpec fromFile, params FileSpec[] toFiles)
		{
			if ((options != null) && (options.ContainsKey("-s")) && (fromFile == null))
			{
				throw new ArgumentNullException("fromFile",
					"From file cannot be null when the -s  flag is specified");
			}
			IList<FileSpec> Files = null;
			if ((toFiles != null) && (toFiles.Length > 0))
			{
				Files = new List<FileSpec>(toFiles);
			}
			else
			{
				if (fromFile != null)
				{
					Files = new List<FileSpec>();
				}
			}
			if ((Files != null) && (fromFile != null))
			{
				Files.Insert(0, fromFile);
			}
			return runFileListCmd("merge", options, Files.ToArray());
		}
		public IList<FileSpec> MergeFiles(FileSpec fromFile, IList<FileSpec> toFiles, Options options)
		{
			return MergeFiles(options, fromFile, toFiles.ToArray());
		}
		public IList<FileSpec> MergeFiles(Options options)
		{
			return runFileListCmd("merge", options);
		}

		#endregion
	}
}
