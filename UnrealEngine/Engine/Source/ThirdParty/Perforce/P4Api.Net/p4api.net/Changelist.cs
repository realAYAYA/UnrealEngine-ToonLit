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
 * Name		: Changelist.cs
 *
 * Author	: dbb
 *
 * Description	: Class used to abstract a client in Perforce.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

/// <summary>
/// Specifies type of Change List
/// Can be either Public or Restricted
/// </summary>
public enum ChangeListType {
    /// <summary>
    /// No Type specified
    /// </summary>
    None, 
    /// <summary>
    /// Changelist is publically accessable
    /// </summary>
    Public, 
    /// <summary>
    /// ChangeList is restricted, only accessable to owners and users with list permission for at least one file
    /// </summary>
    Restricted
};

namespace Perforce.P4
{
	/// <summary>
	/// Shelved file information from a Describe -S command
	/// </summary>
	public class ShelvedFile
	{
        /// <summary>
        /// Property to access the Server Depot Path 
        /// </summary>
		public DepotPath Path { get; set; }
        /// <summary>
        /// Property to access the Revision
        /// </summary>
		public int Revision { get; set; }
        /// <summary>
        /// Property to access FileAction
        /// </summary>
		public FileAction Action { get; set; }
        /// <summary>
        /// Property to access FileType
        /// </summary>
		public FileType Type { get; set; }
        /// <summary>
        /// Property to access the File Size
        /// </summary>
		public long Size { get; set; }
        /// <summary>
        /// Property to access the Digest
        /// </summary>
		public string Digest { get; set; }
	}
	/// <summary>
	/// A changelist specification in a Perforce repository. 
	/// </summary>
	public class Changelist
	{
		private bool _initialized = false;

        /// <summary>
        /// Go to the server to get details about this changelist
        /// </summary>
        /// <param name="connection">connection to server</param>
		public void initialize(Connection connection)
		{
			if (connection == null)
			{
				P4Exception.Throw(ErrorSeverity.E_FAILED, "Changelist cannot be initialized");
				return;
			}
			Connection = connection;

			if (!connection.connectionEstablished())
			{
				// not connected to the server yet
				return;
			}
			if (Id < 0)
			{
				// new change list
				_initialized = true;
				return;
			}
			P4Command cmd = new P4Command(connection, "change", true, "-o", Id.ToString());

			P4CommandResult results = cmd.Run();

			if ((results.Success) && (results.TaggedOutput != null) && (results.TaggedOutput.Count > 0))
			{
				FromChangeCmdTaggedOutput(results.TaggedOutput[0],string.Empty,false);
				_initialized = true;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
		}
		internal Connection Connection { get; private set; }
		internal FormBase _baseForm;

        /// <summary>
        /// Property to access change ID
        /// </summary>
		public int Id { get; internal set; }
        /// <summary>
        /// Property to access change Description
        /// </summary>
		public string Description { get; set; }
        /// <summary>
        /// Property to access change owner
        /// </summary>
		public string OwnerName { get; set; }
        /// <summary>
        /// Property to specify change as Pending
        /// </summary>
		public bool Pending { get; private set; }
        /// <summary>
        /// Property to access last Modified time
        /// </summary>
		public DateTime ModifiedDate { get; private set; }
        /// <summary>
        /// Name of client/workspace associated with this change
        /// </summary>
		public string ClientId { get; set; }
        /// <summary>
        /// Property to access if this change has shelved files
        /// </summary>
		public bool Shelved { get; set; }
        /// <summary>
        /// Property to access List of Shelved files
        /// </summary>
		public IList<ShelvedFile> ShelvedFiles { get; set; }
        /// <summary>
        /// Property to access List of Jobs associated with this change.
        /// </summary>
		public Dictionary<string, string> Jobs { get; set; }
        /// <summary>
        /// Property to access List of Metadata for Files in this change
        /// </summary>
		public IList<FileMetaData> Files { get; set; }
        /// <summary>
        /// Property to access FormSpec for changelist
        /// </summary>
		public FormSpec Spec { get; set; }

		private StringEnum<ChangeListType> _type;

		/// <summary>
		/// Property to access Identity, 
		/// Identifier for this change.
		/// </summary>
		public string Identity { get; set; }

		/// <summary>
		/// Property to read ImportedBy,
		/// The user who fetched or pushed this 
		/// change to this server.
		/// </summary>
		public string ImportedBy { get; private set; }

		/// <summary>
		/// Property to access stream that is to be 
		/// added to this changelist.
		/// </summary>
		public string Stream { get; set; }
		/// <summary>
		/// Property to access type of the changelist
		/// </summary>
		public ChangeListType Type 
		{
			get { return _type; }
			set { _type = value; }
		}

		/// <summary>
		/// Create a new pending changelist
		/// </summary>
		public Changelist()
		{
			Pending = true;
			Id = -1;

			Type = ChangeListType.None;

			Files = new List<FileMetaData>();
		}
		/// <summary>
		/// Create a new numbered changelist
		/// </summary>
		/// <param name="id"></param>
		/// <param name="pending"></param>
		public Changelist(int id, bool pending)
		{
			Id = id;
			Pending = pending;

			Type = ChangeListType.None;

			Files = new List<FileMetaData>();
		}

        /// <summary>
        /// Fill in the fields for the changelist using the tagged output of a "change' command
        /// </summary>
        /// <param name="objectInfo">The tagged output of a "change' command</param>
        /// <param name="offset">Offset within array</param>
        /// <param name="dst_mismatch">Daylight savings time for conversions</param>
        public void FromChangeCmdTaggedOutput(TaggedObject objectInfo, string offset, bool dst_mismatch)
		{
			FromChangeCmdTaggedOutput(objectInfo, false, offset, dst_mismatch);
		}

        /// <summary>
        /// Fill in the fields for the changelist using the tagged output of a "change' command
        /// </summary>
        /// <param name="objectInfo">The tagged output of a "change' command</param>
        /// <param name="GetShelved">Access shelved files or not</param>
        /// <param name="offset">Offset within array</param>
        /// <param name="dst_mismatch">Daylight savings time for conversions</param>
        public void FromChangeCmdTaggedOutput(TaggedObject objectInfo, bool GetShelved, string offset, bool dst_mismatch)
		{
			// need to check for tags starting with upper and lower case, it the 'change' command's
			//  output the tags start with an uppercase character whereas with the 'changes' command
			//  they start with a lower case character, i.e. "Change" vs "change"

			_baseForm = new FormBase();

			_baseForm.SetValues(objectInfo);

            if (objectInfo.ContainsKey("Change"))
            {
                int v = -1;
                if (int.TryParse(objectInfo["Change"], out v))
                {
                    Id = v;
                }
            }
            else if (objectInfo.ContainsKey("change"))
            {
                int v = -1;
                if (int.TryParse(objectInfo["change"], out v))
                {
                    Id = v;
                }
            }

			if (objectInfo.ContainsKey("Date"))
			{
				DateTime v;
				DateTime.TryParse(objectInfo["Date"], out v);
				ModifiedDate = v;
			}
			else if (objectInfo.ContainsKey("time"))
			{
				long v;
				if (long.TryParse(objectInfo["time"], out v))
				{
                    DateTime UTC = FormBase.ConvertUnixTime(v);
                    DateTime GMT = new DateTime(UTC.Year, UTC.Month, UTC.Day, UTC.Hour, UTC.Minute, UTC.Second,
                        DateTimeKind.Unspecified);
                    ModifiedDate = FormBase.ConvertFromUTC(GMT, offset, dst_mismatch);
				}
			}
			if (objectInfo.ContainsKey("Client"))
				ClientId = objectInfo["Client"];
			else if (objectInfo.ContainsKey("client"))
				ClientId = objectInfo["client"];
			
			if (objectInfo.ContainsKey("User"))
				OwnerName = objectInfo["User"];
			else if (objectInfo.ContainsKey("user"))
				OwnerName = objectInfo["user"];

			if (objectInfo.ContainsKey("ImportedBy"))
				ImportedBy = objectInfo["ImportedBy"];
			else if (objectInfo.ContainsKey("importedby"))
				ImportedBy = objectInfo["importedby"];
			else if (objectInfo.ContainsKey("Importedby"))
				ImportedBy = objectInfo["Importedby"];
			else if (objectInfo.ContainsKey("importedBy"))
				ImportedBy = objectInfo["importedBy"];

			if (objectInfo.ContainsKey("Identity"))
				Identity = objectInfo["Identity"];
			else if (objectInfo.ContainsKey("changeIdentity"))
				Identity = objectInfo["changeIdentity"];

			if (objectInfo.ContainsKey("Stream"))
				Identity = objectInfo["Stream"];
			else if (objectInfo.ContainsKey("stream"))
				Identity = objectInfo["stream"];

			if (objectInfo.ContainsKey("Status"))
			{
				Pending = true;
				String v = objectInfo["Status"];
				if (v == "submitted")
					Pending = false;
			}
			else if (objectInfo.ContainsKey("status"))
			{
				Pending = true;
				String v = objectInfo["status"];
				if (v == "submitted")
					Pending = false;
			}

			if (objectInfo.ContainsKey("Description"))
				Description = objectInfo["Description"];
			else if (objectInfo.ContainsKey("desc"))
				Description = objectInfo["desc"];
			char[] array = {'\r', '\n'};
			Description = Description.TrimEnd(array);
			Description = Description.Replace("\r", "");
			Description = Description.Replace("\n", "\r\n");

			if (objectInfo.ContainsKey("Type"))
			{
				_type = objectInfo["Type"];
			}
			else if (objectInfo.ContainsKey("changeType"))
			{
				_type = objectInfo["changeType"];
			}

			if (objectInfo.ContainsKey("shelved"))
			{
				Shelved = true;
			}

            int idx = 0;
			String key = "Jobs0";
			if (objectInfo.ContainsKey(key))
			{
				idx = 1;
				Jobs = new Dictionary<string, string>();
				do
				{
					Jobs.Add(objectInfo[key], null);
					key = String.Format("Jobs{0}", idx++);
				} while (objectInfo.ContainsKey(key));
			}
			else
			{
				key = "jobs0";
				if (objectInfo.ContainsKey(key))
				{
					idx = 1;
					Jobs = new Dictionary<string, string>();
					do
					{
						Jobs.Add(objectInfo[key], null);
						key = String.Format("jobs{0}", idx++);
					} while (objectInfo.ContainsKey(key)) ;
				}
			}

			key = "Files0";
			if (objectInfo.ContainsKey(key))
			{
				idx = 1;
				Files = new List<FileMetaData>();
				do
				{
					FileMetaData file = new FileMetaData();
					file.DepotPath = new DepotPath(PathSpec.UnescapePath(objectInfo[key]));
					Files.Add(file);
					key = String.Format("Files{0}", idx++);
				} while (objectInfo.ContainsKey(key));
			}
			else
			{
				key = "files0";
				if (objectInfo.ContainsKey(key))
				{
					idx = 1;
					SimpleList<FileMetaData> files = new SimpleList<FileMetaData>();
					do
					{
						FileMetaData file = new FileMetaData();
						file.DepotPath = new DepotPath(PathSpec.UnescapePath(objectInfo[key]));
						Files.Add(file);
						key = String.Format("files{0}", idx++);
					} while (objectInfo.ContainsKey(key));
                    Files = (List<FileMetaData>) files;
				}
			}
			if (GetShelved)
			{
				key = "depotFile0";
				String actionKey = "action0";
				String typeKey = "type0";
				String revKey = "rev0";
				String sizeKey = "fileSize0";
				String digestKey = "digest0";

				if (objectInfo.ContainsKey(key))
				{
					SimpleList<ShelvedFile> shelvedFiles = new SimpleList<ShelvedFile>();
					idx = 1;
					do
					{
						ShelvedFile file = new ShelvedFile();
						file.Path = new DepotPath(PathSpec.UnescapePath(objectInfo[key]));
						StringEnum<FileAction> action = objectInfo[actionKey];
						file.Action = action;
						file.Type = new FileType(objectInfo[typeKey]);
						string revstr = objectInfo[revKey];
						if (revstr == "none")
						{
							revstr = "0";
						}
						int rev = Convert.ToInt32(revstr);
						file.Revision = rev;

						if (objectInfo.ContainsKey(sizeKey))
						{

						long size = -1;
						long.TryParse(objectInfo[sizeKey], out size);
						file.Size = size;
						}

						if (objectInfo.ContainsKey(digestKey))
						{

							file.Digest = objectInfo[digestKey];
						}

						shelvedFiles.Add(file);

						key = String.Format("depotFile{0}", idx);
						actionKey = String.Format("action{0}", idx);
						typeKey = String.Format("type{0}", idx);
						revKey = String.Format("rev{0}", idx);
						sizeKey = String.Format("fileSize{0}", idx);
						digestKey = String.Format("digest{0}", idx++);

					} while (objectInfo.ContainsKey(key));
                    ShelvedFiles = (List<ShelvedFile>) shelvedFiles;
				}
			}
			else
			{
				key = "depotFile0";
				String actionKey = "action0";
				String typeKey = "type0";
				String revKey = "rev0";
				String sizeKey = "fileSize0";
				String digestKey = "digest0";

				if (objectInfo.ContainsKey(key))
				{
					idx = 1;
					SimpleList<FileMetaData> fileList = new SimpleList<FileMetaData>();
					do
					{
						FileMetaData file = new FileMetaData();
						file.DepotPath = new DepotPath(PathSpec.UnescapePath(objectInfo[key]));
						StringEnum<FileAction> action = objectInfo[actionKey];
						file.Action = action;
						file.Type = new FileType(objectInfo[typeKey]);
						string revstr = objectInfo[revKey];
						int rev = Convert.ToInt32(revstr);
						file.HeadRev = rev;

						if (objectInfo.ContainsKey(sizeKey))
						{

							long size = -1;
							long.TryParse(objectInfo[sizeKey], out size);
							file.FileSize = size;
						}

						if (objectInfo.ContainsKey(digestKey))
						{

							file.Digest = objectInfo[digestKey];
						}

                        fileList.Add(file);

						key = String.Format("depotFile{0}", idx);
						actionKey = String.Format("action{0}", idx);
						typeKey = String.Format("type{0}", idx);
						revKey = String.Format("rev{0}", idx);
						sizeKey = String.Format("fileSize{0}", idx);
						digestKey = String.Format("digest{0}", idx++);

					} while (objectInfo.ContainsKey(key));
                    Files = (List<FileMetaData>) fileList;
				}
			}

			key = "job0";
			String statKey = "jobstat0";

			if (objectInfo.ContainsKey(key))
			{
				idx = 1;
				Jobs = new Dictionary<string, string>();
				do
				{
					string jobStatus = string.Empty;
					string jobId = objectInfo[key];
					if (objectInfo.ContainsKey(statKey))
					{ jobStatus = objectInfo[statKey]; }
					Jobs.Add(jobId, jobStatus);
					key = String.Format("job{0}", idx);
					statKey = String.Format("jobstat{0}", idx++);
				} while (objectInfo.ContainsKey(key));
			}


		}
		/// <summary>
		/// Parse the fields from a changelist specification 
		/// </summary>
		/// <param name="spec">Text of the changelist specification in server format</param>
		/// <returns>true if parse successful</returns>
		public bool Parse(String spec)
		{
			_baseForm = new FormBase();
			_baseForm.Parse(spec); // parse the values into the underlying dictionary

			if (_baseForm.ContainsKey("Change"))
			{
				int v = -1;
				int.TryParse((string)_baseForm["Change"], out v);
				Id = v;
			}
			if (_baseForm.ContainsKey("Date"))
			{
				DateTime v;
				DateTime.TryParse((string)_baseForm["Date"], out v);
				ModifiedDate = v;
			}
			if (_baseForm.ContainsKey("Client"))
				ClientId = (string)_baseForm["Client"];

			if (_baseForm.ContainsKey("User"))
				OwnerName = (string)_baseForm["User"];

			if (_baseForm.ContainsKey("Status"))
			{
				Pending = true;
				String v = (string)_baseForm["Status"];
				if (v == "submitted")
					Pending = false;
			}

			if (_baseForm.ContainsKey("Type"))
			{
				_type = (string)_baseForm["Type"];
			}

			if (_baseForm.ContainsKey("Identity"))
				Identity = (string)_baseForm["Identity"];

			if (_baseForm.ContainsKey("ImportedBy"))
				ImportedBy = (string)_baseForm["ImportedBy"];

			if (_baseForm.ContainsKey("Stream"))
				ImportedBy = (string)_baseForm["Stream"];

			if (_baseForm.ContainsKey("Description"))
			{
				if (_baseForm["Description"] is string)
				{
					Description = (string)_baseForm["Description"];
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
				else if (_baseForm["Description"] is IList<string>)
				{
                    IList<string> strList = _baseForm["Description"] as IList<string>;
					Description = string.Empty;
                    for (int idx = 0; idx < strList.Count; idx++)
					{
                        if (idx >0)
                        {
                            Description += "\r\n";
                        }
                        Description += strList[idx];
					}
				}
			}
            if (_baseForm.ContainsKey("Jobs"))
            {
                if (_baseForm["Jobs"] is IList<string>)
                {
                    IList<string> strList = _baseForm["Jobs"] as IList<string>;
                    Jobs = new Dictionary<string, string>(strList.Count);
                    foreach (string job in strList)
                    {
                        if (job.Contains('#'))
                        {
                            string[] parts = job.Split('#');
                            Jobs[parts[0].Trim()] = null;
                        }
                        else
                        {
                            Jobs[job] = null;
                        }
                    }
                }
                else if (_baseForm["Jobs"] is SimpleList<string>)
                {
                    SimpleList<string> strList = _baseForm["Jobs"] as SimpleList<string>;
                    Jobs = new Dictionary<string, string>(strList.Count);
                    SimpleListItem<string> current = strList.Head;
                    while (current != null)
                    {
                        string job = current.Item;
                        if (job.Contains('#'))
                        {
                            string[] parts = job.Split('#');
                            Jobs[parts[0].Trim()] = null;
                        }
                        else
                        {
                            Jobs[job] = null;
                        }
                        current = current.Next;
                    }
                }
            }
            if (_baseForm.ContainsKey("Files"))
            {
                if (_baseForm["Files"] is IList<string>)
                {
                    IList<string> files = (IList<string>)_baseForm["Files"];
                    Files = new List<FileMetaData>(files.Count);
                    foreach (string path in files)
                    {
                        FileMetaData file = new FileMetaData();
                        if (path.Contains('#'))
                        {
                            string[] parts = path.Split('#');
                            file.DepotPath = new DepotPath(parts[0].Trim());
                        }
                        else
                        {
                            file.DepotPath = new DepotPath(path);
                        }
                        Files.Add(file);
                    }
                }
                else if (_baseForm["Files"] is SimpleList<string>)
                {
                    SimpleList<string> files = (SimpleList<string>)_baseForm["Files"];
                    Files = new List<FileMetaData>(files.Count);
                    SimpleListItem<string> current = files.Head;
                    while (current != null)
                    {
                        FileMetaData file = new FileMetaData();
                        if (current.Item.Contains('#'))
                        {
                            string[] parts = current.Item.Split('#');
                            file.DepotPath = new DepotPath(parts[0].Trim());
                        }
                        else
                        {
                            file.DepotPath = new DepotPath(current.Item);
                        }
                        Files.Add(file);
                        current = current.Next;
                    }
                }
            }
			return true;
		}

		/// <summary>
		/// Format of a user specification used to save a user to the server
		/// </summary>
		private static String ChangelistSpecFormat =
													"Change:\t{0}\n" +
													"\n" +
													"Date:\t{1}\n" +
													"\n" +
													"Client:\t{2}\n" +
													"\n" +
													"User:\t{3}\n" +
													"\n" +
													"Status:\t{4}\n" +
													"\n" +
													"{5}" + // Type
													"{6}" + // ImporteBy
													"{7}" + // Identity
													"Description:\n" +
													"\t{8}\n" + //Description
													"\n" +
													"Jobs:\n" +
													"{9}\n" + //Jobs
													"\n" +
													"{10}" + // Stream
													"Files:\n{11}";

		/// <summary>
		/// Convert to specification in server format
		/// </summary>
		/// <returns></returns>
		override public String ToString()
		{
            String descStr = String.Empty;
		    if (Description != null)
		        descStr = FormBase.FormatMultilineField(Description.ToString());
            
            // only add the files field if it is a pending changelist
			//
			// #  Jobs:        What opened jobs are to be closed by this changelist.
			// #               You may delete jobs from this list.  (New changelists only.)
			// #  Files:       What opened files from the default changelist are to be added
			// #               to this changelist.  You may delete files from this list.
			// #               (New changelists only.)
			String jobsStr = String.Empty;
			if (Jobs != null)
			{
				foreach (string jobId in Jobs.Keys)
				{
					jobsStr += string.Format("\t{0}\r\n", jobId);
				}
			}
			
			String filesStr = String.Empty;
			if (Files != null&&Pending)
			{
				foreach (FileSpec file in Files)
				{
					filesStr += String.Format("\t{0}\r\n", file.ToEscapedString());
				}
			}

			String ChangeNumbeStr = "new";
			if (Id != -1)
				ChangeNumbeStr = Id.ToString();

			String restrictedStr = String.Empty;
			if (_type == ChangeListType.Restricted)
				restrictedStr = "Type:\trestricted\n\n";
			if (_type == ChangeListType.Public)
				restrictedStr = "Type:\tpublic\n\n";

			String importedStr = String.Empty;
			if (ImportedBy != null)
				importedStr = "ImportedBy:\t" + ImportedBy + "\n\n";

			String identityStr = String.Empty;
			if (Identity != null)
				identityStr = "Identity:\t" + Identity + "\n\n";

			String streamStr = String.Empty;
			if (Stream != null)
				identityStr = "Stream:\t" + Stream + "\n\n";

			String statusStr = Pending ? "pending" : "submitted";
			if (Id == -1)
				statusStr = "new";

			String value = String.Format(ChangelistSpecFormat, ChangeNumbeStr,
				FormBase.FormatDateTime(ModifiedDate), ClientId, OwnerName,
				statusStr,
				restrictedStr, importedStr, identityStr, descStr, jobsStr, streamStr, filesStr);
			return value;
		}

		/// <summary>
		/// Convert to a string for display
		/// </summary>
		/// <param name="includeTime">Include the time as well as the date</param>
		/// <returns></returns>
		public string ToString(bool includeTime)
		{
			String dateTime;
			if (includeTime)
				dateTime = String.Format("{0} {1}", ModifiedDate.ToShortDateString(), ModifiedDate.ToShortTimeString());
			else
				dateTime = ModifiedDate.ToShortDateString();

            String desc = String.Empty;
            if (Description != null)
                desc = FormBase.FormatMultilineField(Description.ToString());
			return String.Format("Change {0} on {1} by {2}: {3}", Id, dateTime, OwnerName, desc);
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="options"></param>
		/// <param name="jobs"></param>
		/// <returns></returns>
		/// <remarks>
		/// <br/><b>p4 help fix</b>
		/// <br/> 
		/// <br/>     fix -- Mark jobs as being fixed by the specified changelist
		/// <br/> 
		/// <br/>     p4 fix [-d] [-s status] -c changelist# jobName ...
		/// <br/> 
		/// <br/> 	'p4 fix' marks each named job as being fixed by the changelist
		/// <br/> 	number specified with -c.  The changelist can be pending or
		/// <br/> 	submitted and the jobs can be open or closed (fixed by another
		/// <br/> 	changelist).
		/// <br/> 
		/// <br/> 	If the changelist has already been submitted and the job is still
		/// <br/> 	open, then 'p4 fix' marks the job closed.  If the changelist has not
		/// <br/> 	been submitted and the job is still open, the job is closed when the
		/// <br/> 	changelist is submitted.  If the job is already closed, it remains
		/// <br/> 	closed.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified fixes.  This operation does not
		/// <br/> 	otherwise affect the specified changelist or jobs.
		/// <br/> 
		/// <br/> 	The -s flag uses the specified status instead of the default defined
		/// <br/> 	in the job specification.
		/// <br/> 
		/// <br/> 	The fix's status is reported by 'p4 fixes', and is related to the
		/// <br/> 	job's status. Certain commands set the job's status to the fix's
		/// <br/> 	status for each job associated with the change. When a job is fixed
		/// <br/> 	by a submitted change, the job's status is set to match the fix
		/// <br/> 	status.  When a job is fixed by a pending change, the job's status
		/// <br/> 	is set to match the fix status when the change is submitted. If the
		/// <br/> 	fix's status is 'same', the job's status is left unchanged.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<Fix> FixJobs(Options options, params Job[] jobs)
		{
			if (_initialized == false)
			{
				throw new ApplicationException("Changelist connection is not initialized");
			}
			IList<Fix> value = null;

			P4Command cmd = null;
			if ((jobs != null) && (jobs.Length > 0))
			{
				cmd = new P4Command(Connection, "fix", true, Job.ToStrings(jobs));
			}
			else
			{
				cmd = new P4Command(Connection, "fix", true);
			}
			if (options == null)
			{
				options = new Options();
			}
			if (options.ContainsKey("-c") == false)
			{
				options["-c"] = this.Id.ToString();
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				value = new List<Fix>();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
					Fix fix = Fix.FromFixCmdTaggedOutput(obj);
					value.Add(fix);
				}
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}

			return value;
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="jobs"></param>
		/// <param name="options"></param>
		/// <returns></returns>
		public IList<Fix> FixJobs(IList<Job> jobs, Options options)
		{
			return FixJobs(options, jobs.ToArray());
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="options">options/flags</param>
		/// <param name="jobIds">jobs to filter on</param>
		/// <returns></returns>
		/// <remarks>
		/// <br/><b>p4 help fix</b>
		/// <br/> 
		/// <br/>     fix -- Mark jobs as being fixed by the specified changelist
		/// <br/> 
		/// <br/>     p4 fix [-d] [-s status] -c changelist# jobName ...
		/// <br/> 
		/// <br/> 	'p4 fix' marks each named job as being fixed by the changelist
		/// <br/> 	number specified with -c.  The changelist can be pending or
		/// <br/> 	submitted and the jobs can be open or closed (fixed by another
		/// <br/> 	changelist).
		/// <br/> 
		/// <br/> 	If the changelist has already been submitted and the job is still
		/// <br/> 	open, then 'p4 fix' marks the job closed.  If the changelist has not
		/// <br/> 	been submitted and the job is still open, the job is closed when the
		/// <br/> 	changelist is submitted.  If the job is already closed, it remains
		/// <br/> 	closed.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified fixes.  This operation does not
		/// <br/> 	otherwise affect the specified changelist or jobs.
		/// <br/> 
		/// <br/> 	The -s flag uses the specified status instead of the default defined
		/// <br/> 	in the job specification.
		/// <br/> 
		/// <br/> 	The fix's status is reported by 'p4 fixes', and is related to the
		/// <br/> 	job's status. Certain commands set the job's status to the fix's
		/// <br/> 	status for each job associated with the change. When a job is fixed
		/// <br/> 	by a submitted change, the job's status is set to match the fix
		/// <br/> 	status.  When a job is fixed by a pending change, the job's status
		/// <br/> 	is set to match the fix status when the change is submitted. If the
		/// <br/> 	fix's status is 'same', the job's status is left unchanged.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		public IList<Fix> FixJobs(Options options, params string[] jobIds)
		{
			if (_initialized == false)
			{
				throw new ApplicationException("Changelist connection is not initialized");
			}
			IList<Fix> value = null;

			P4Command cmd = null;
			if ((jobIds != null) && (jobIds.Length > 0))
			{
				cmd = new P4Command(Connection, "fix", true, jobIds);
			}
			else
			{
				cmd = new P4Command(Connection, "fix", true);
			}
			if (options == null)
			{
				options = new Options();
			}
			if (options.ContainsKey("-c") == false)
			{
				options["-c"] = this.Id.ToString();
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				value = new List<Fix>();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
					Fix fix = Fix.FromFixCmdTaggedOutput(obj);
					value.Add(fix);
				}
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}

			return value;
		}
		/// <summary>
		/// 
		/// </summary>
		/// <param name="jobIds">jobs to filter on</param>
		/// <param name="options">options/flags</param>
		/// <returns></returns>
		public IList<Fix> FixJobs(IList<string> jobIds, Options options)
		{
			return FixJobs(options, jobIds.ToArray());
		}

        /// <summary>
        /// Submit a pending change to the server
        /// </summary>
        /// <param name="options">Submit Options</param>
        /// <returns>object describing Submit Results</returns>
		public SubmitResults Submit(Options options)
		{
			if (_initialized == false)
			{
				throw new ApplicationException("Changelist connection is not initialized");
			}
			if (options == null)
			{
				options = new Options();
			}
			if (options.ContainsKey("-e") == false&&
                options.ContainsKey("-c") == false)
			{
				options["-c"] = this.Id.ToString();
			}
			return Connection.Client.SubmitFiles(options, null);
		}
	}
}
