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
 * Name		: Group.cs
 *
 * Author	: dbb
 *
 * Description	: Class used to abstract a form specification in Perforce.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;


namespace Perforce.P4
{
	/// <summary>
	/// A user group on a Perforce Server, represented by a P4 Group spec.
	/// </summary>
		/// <remarks>
		/// <br/><b>p4 help group</b>
		/// <br/> 
		/// <br/>     group -- Change members of user group
		/// <br/> 
		/// <br/>     p4 group [-a|-A] name
		/// <br/>     p4 group -d [-a] name
		/// <br/>     p4 group -o name
		/// <br/>     p4 group -i [-a|-A]
		/// <br/> 
		/// <br/> 	Create a group or modify the membership of an existing group.
		/// <br/> 	A group can contain users and other groups. The group specification
		/// <br/> 	is put into a temporary file and the editor (configured by the
		/// <br/> 	environment variable $P4EDITOR) is invoked.
		/// <br/> 
		/// <br/> 	A group exists when it has any users or other groups in it, and
		/// <br/> 	ceases to exist if all users and groups in it are removed.
		/// <br/> 
		/// <br/> 	Each group has MaxResults, MaxScanRows, and MaxLockTime fields,
		/// <br/> 	which limit the resources committed to operations performed by
		/// <br/> 	members of the group.  For these fields, 'unlimited' or 'unset'
		/// <br/> 	means no limit for that	group.  An individual user's limit is the
		/// <br/> 	highest of any group with a limit to which he belongs, unlimited if
		/// <br/> 	any of his groups has 'unlimited' for that field, or unlimited
		/// <br/> 	if he belongs to no group with a limit.  See 'p4 help maxresults'
		/// <br/> 	for more information on MaxResults, MaxScanRows and MaxLockTime.
		/// <br/> 
		/// <br/> 	Each group also has a Timeout field, which specifies how long (in
		/// <br/> 	seconds)  a 'p4 login' ticket remains valid.  A value of 'unset' or
		/// <br/> 	'unlimited' is equivalent to no timeout. An individual's timeout is
		/// <br/> 	the highest of any group with a limit to which he belongs, unlimited
		/// <br/> 	if any of his groups has 'unlimited' for the timeout value, or
		/// <br/> 	unlimited if he belongs to no group with a limit. See 'p4 help login'
		/// <br/> 	for more information.
		/// <br/> 
		/// <br/> 	Each group has a PasswordTimeout field, which determines how long a
		/// <br/> 	password remains valid for members of the group.
		/// <br/> 
		/// <br/> 	A group may be synchronized with an LDAP group by setting the three
		/// <br/> 	fields: LdapConfig, LdapSearchQuery and LdapUserAttribute. This takes
		/// <br/> 	the LDAP configuration (see 'p4 ldap') specified by LdapConfig and uses
		/// <br/> 	it to execute the query stored by LdapSearchQuery. The LDAP attribute
		/// <br/> 	specified by LdapUserAttribute is taken to be user's username and is
		/// <br/> 	added to the group's user list. At least one group owner must be set if
		/// <br/> 	these LDAP fields are used. If the LDAP server requires login for
		/// <br/> 	read-only queries, then the LDAP configuration must contain valid bind
		/// <br/> 	credentials in the LDAP spec's SearchBindDN and SearchPasswd fields
		/// <br/> 
		/// <br/> 	The -d flag deletes a group.
		/// <br/> 
		/// <br/> 	The -o flag writes the group specification to standard output. The
		/// <br/> 	user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a group specification from standard input. The
		/// <br/> 	user's editor is not invoked.  The new group specification replaces
		/// <br/> 	the previous one.
		/// <br/> 
		/// <br/> 	The -a flag enables a user without 'super' access to modify the group
		/// <br/> 	if that user is an 'owner' of that group. Group owners	are specified
		/// <br/> 	in the 'Owners' field of the group spec.
		/// <br/> 
		/// <br/> 	The -A flag enables a user with 'admin' access to add a new group.
		/// <br/> 	Existing groups may not be modified when this flag is used.
		/// <br/> 
		/// <br/> 	All commands that require access granted by 'p4 protect' consider a
		/// <br/> 	user's groups when calculating access levels.
		/// <br/> 
		/// <br/> 	'p4 group' requires 'super' access granted by 'p4 protect' unless
		/// <br/> 	invoked with the '-a' or '-A' flag by a qualified user.
		/// <br/> 
		/// <br/> 
		/// </remarks>
	/// <summary>
	/// A group specification in a Perforce repository. 
	/// </summary>
	public class Group
	{
		private FormBase _baseForm;

		#region properties
		/// <summary>
		/// The name of the group.
		/// </summary>
		public string Id { get; internal set; }
		/// <summary>
		/// Limits the rows (unless 'unlimited' or 'unset') any one
		/// operation can return to the client.
		/// </summary>
		public int MaxResults { get; set; }
		/// <summary>
		/// Limits the rows (unless 'unlimited' or 'unset') any one
		/// operation can scan from any one database table.
		/// </summary>
		public int MaxScanRows { get; set; }
		/// <summary>
		/// Limits the time (in milliseconds, unless 'unlimited' or
		/// 'unset') any one operation can lock any database table when
		/// scanning data.
		/// </summary>
		public int MaxLockTime { get; set; }
		/// <summary>
		/// The maximum number of files that a member of a group 
		/// can open using a single command. 
		/// </summary>
		public int MaxOpenFiles { get; set; }
		/// <summary>
		/// A time (in seconds, unless 'unlimited' or 'unset')
		/// which determines how long a 'p4 login'
		/// session ticket remains valid (default is 12 hours).
		/// </summary>
		public int TimeOut { get; set; }
		/// <summary>
		/// A time (in seconds, unless 'unlimited' or 'unset')
		/// which determines how long a 'p4 password'
		/// password remains valid (default is unset).
		/// </summary>
		public int PasswordTimeout { get; set; }
		/// <summary>
		/// Users allowed to change this group without requiring super
		/// access permission.
		/// </summary>
		public IList<string> OwnerNames { get; set; }
		/// <summary>
		/// The users in the group.
		/// </summary>
		public IList<string> UserNames { get; set; }
		/// <summary>
		/// Other groups automatically included in this group.
		/// </summary>
		public IList<string> SubGroups { get; set; }

		public FormSpec Spec { get; set; }

		#endregion

		#region creators

		internal Group()
		{
		}
		/// <summary>
		/// Create a group supplying the name.
		/// </summary>
		/// <param name="id"></param>
		public Group(string id)
		{
			Id = id;
			MaxResults = -1;
			MaxScanRows = -1;
			MaxLockTime = -1;
			MaxOpenFiles = -1;
			TimeOut = -1;
			PasswordTimeout = -1;
			OwnerNames = null;
			UserNames = null;
			SubGroups = null;
		}
		/// <summary>
		/// Create a group providing all of the properties
		/// </summary>
		/// <param name="id"></param>
		/// <param name="maxResults"></param>
		/// <param name="maxScanRows"></param>
		/// <param name="maxLockTime"></param>
		/// <param name="maxOpenFiles"></param>
		/// <param name="timeOut"></param>
		/// <param name="passwordTimeout"></param>
		/// <param name="ownerNames"></param>
		/// <param name="userNames"></param>
		/// <param name="subGroups"></param>
		/// <param name="spec"></param>
		public Group(	string id,
						int maxResults,
						int maxScanRows,
						int maxLockTime,
						int maxOpenFiles,
						int timeOut,
						int passwordTimeout,
						IList<string> ownerNames,
						IList<string> userNames,
						IList<string> subGroups,
						FormSpec spec)
		{
			Id = id;
			MaxResults = maxResults;
			MaxScanRows = maxScanRows;
			MaxLockTime = maxLockTime;
			MaxOpenFiles = maxOpenFiles;
			TimeOut = timeOut;
			PasswordTimeout = passwordTimeout;
			OwnerNames = ownerNames;
			UserNames = userNames;
			SubGroups = subGroups;
			Spec = spec;
		}

		/// <summary>
		/// Create a group providing all of the properties
		/// </summary>
		/// <param name="id"></param>
		/// <param name="maxResults"></param>
		/// <param name="maxScanRows"></param>
		/// <param name="maxLockTime"></param>		
		/// <param name="timeOut"></param>
		/// <param name="passwordTimeout"></param>
		/// <param name="ownerNames"></param>
		/// <param name="userNames"></param>
		/// <param name="subGroups"></param>
		/// <param name="spec"></param>
		[ObsoleteAttribute("Use Group(string id, int maxResults, int maxScanRows, int maxLockTime, int maxOpenFiles, int timeOut, int passwordTimeout, IList<string> ownerNames, IList<string> userNames, IList<string> subGroups, FormSpec spec)", false)]
		public Group(string id,
						int maxResults,
						int maxScanRows,
						int maxLockTime,						
						int timeOut,
						int passwordTimeout,
						IList<string> ownerNames,
						IList<string> userNames,
						IList<string> subGroups,
						FormSpec spec)
		{
			Id = id;
			MaxResults = maxResults;
			MaxScanRows = maxScanRows;
			MaxLockTime = maxLockTime;
			TimeOut = timeOut;
			PasswordTimeout = passwordTimeout;
			OwnerNames = ownerNames;
			UserNames = userNames;
			SubGroups = subGroups;
			Spec = spec;
		}
		#endregion

		#region initializers

		/// <summary>
		/// Read the fields from the tagged output of a group command
		/// </summary>
		/// <param name="objectInfo">Tagged output from the 'user' command</param>
		public void FromGroupCmdTaggedOutput(TaggedObject objectInfo)
		{
			_baseForm = new FormBase();

			_baseForm.SetValues(objectInfo);

			if (objectInfo.ContainsKey("Group"))
				Id = objectInfo["Group"];

			if (objectInfo.ContainsKey("MaxResults"))
			{
				int v = -1;
				int.TryParse(objectInfo["MaxResults"], out v);
				MaxResults = v;
			}

			if (objectInfo.ContainsKey("MaxScanRows"))
			{
				int v = -1;
				int.TryParse(objectInfo["MaxScanRows"], out v);
				MaxScanRows = v;
			}

			if (objectInfo.ContainsKey("MaxLockTime"))
			{
				int v = -1;
				int.TryParse(objectInfo["MaxLockTime"], out v);
				MaxLockTime = v;
			}

			if (objectInfo.ContainsKey("MaxOpenFiles"))
			{
				int v = -1;
				int.TryParse(objectInfo["MaxOpenFiles"], out v);
				MaxOpenFiles = v;
			}

			if (objectInfo.ContainsKey("Timeout"))
			{
				int v = -1;
				int.TryParse(objectInfo["Timeout"], out v);
				TimeOut = v;
			}

			if (objectInfo.ContainsKey("PasswordTimeout"))
			{
				int v = -1;
				int.TryParse(objectInfo["PasswordTimeout"], out v);
				PasswordTimeout = v;
			}
			String key = "Users0";
			if (objectInfo.ContainsKey(key))
			{
				int idx = 1;
				UserNames = new StringList();
				while (objectInfo.ContainsKey(key))
				{
					UserNames.Add(objectInfo[key]);
					key = String.Format("Users{0}", idx++);
				}
			}
			key = "Owners0";
			if (objectInfo.ContainsKey(key))
			{
				int idx = 1;
				OwnerNames = new StringList();
				while (objectInfo.ContainsKey(key))
				{
					OwnerNames.Add(objectInfo[key]);
					key = String.Format("Owners{0}", idx++);
				}
			}
			key = "Subgroups0";
			if (objectInfo.ContainsKey(key))
			{
				int idx = 1;
				SubGroups = new StringList();
				while (objectInfo.ContainsKey(key))
				{
					SubGroups.Add(objectInfo[key]);
					key = String.Format("Subgroups{0}", idx++);
				}
			}
		}

		/// <summary>
		/// Parse the fields from a group specification 
		/// </summary>
		/// <param name="spec">Text of group user specification in server format</param>
		/// <returns></returns>
		/// <remarks>
		/// # A Perforce Group Specification.
		/// #
		/// #  Group:       The name of the group.
		/// #  MaxResults:  Limits the rows (unless 'unlimited' or 'unset') any one
		/// #               operation can return to the client.
		/// #               See 'p4 help maxresults'.
		/// #  MaxScanRows: Limits the rows (unless 'unlimited' or 'unset') any one
		/// #               operation can scan from any one database table.
		/// #               See 'p4 help maxresults'.
		/// #  MaxLockTime: Limits the time (in milliseconds, unless 'unlimited' or
		/// #               'unset') any one operation can lock any database table when
		/// #               scanning data. See 'p4 help maxresults'.
		/// #  Timeout:     A time (in seconds, unless 'unlimited' or 'unset')
		/// #               which determines how long a 'p4 login'
		/// #               session ticket remains valid (default is 12 hours).
		/// #  PasswordTimeout:
		/// #               A time (in seconds, unless 'unlimited' or 'unset')
		/// #               which determines how long a 'p4 password'
		/// #               password remains valid (default is unset).
		/// #  Subgroups:   Other groups automatically included in this group.
		/// #  Owners:      Users allowed to change this group without requiring super
		/// #               access permission.
		/// #  Users:       The users in the group.  One per line.
		/// </remarks>
		public bool Parse(String spec)
		{
			_baseForm = new FormBase();

			_baseForm.Parse(spec); // parse the values into the underlying dictionary

            if (_baseForm.ContainsKey("Group"))
                Id = _baseForm["Group"] as string;

            if (_baseForm.ContainsKey("MaxResults"))
            {
                int v = -1;
                int.TryParse(_baseForm["MaxResults"] as string, out v);
                MaxResults = v;
            }

            if (_baseForm.ContainsKey("MaxScanRows"))
            {
                int v = -1;
                int.TryParse(_baseForm["MaxScanRows"] as string, out v);
                MaxScanRows = v;
            }

            if (_baseForm.ContainsKey("MaxLockTime"))
            {
                int v = -1;
                int.TryParse(_baseForm["MaxLockTime"] as string, out v);
                MaxLockTime = v;
            }

			if (_baseForm.ContainsKey("MaxOpenFiles"))
			{
				int v = -1;
				int.TryParse(_baseForm["MaxOpenFiles"] as string, out v);
				MaxOpenFiles = v;
			}

			if (_baseForm.ContainsKey("Timeout"))
            {
                int v = -1;
                int.TryParse(_baseForm["Timeout"] as string, out v);
                TimeOut = v;
            }

            if (_baseForm.ContainsKey("PasswordTimeout"))
            {
                int v = -1;
                int.TryParse(_baseForm["PasswordTimeout"] as string, out v);
                PasswordTimeout = v;
            }
            if (_baseForm.ContainsKey("Users"))
            {
                if (_baseForm["Users"] is IList<string>)
                {
                    UserNames = _baseForm["Users"] as IList<string>;
                }
                else if (_baseForm["Users"] is SimpleList<string>)
                {
                    UserNames = (List<string>)((SimpleList<string>)_baseForm["Users"]);
                }
            }
            if (_baseForm.ContainsKey("Owners"))
            {
                if (_baseForm["Owners"] is IList<string>)
                {
                    OwnerNames = _baseForm["Owners"] as IList<string>;
                }
                else if (_baseForm["Owners"] is SimpleList<string>)
                {
                    OwnerNames = (List<string>)((SimpleList<string>)_baseForm["Owners"]);
                }
            }
            if (_baseForm.ContainsKey("Subgroups"))
            {
                if (_baseForm["Subgroups"] is IList<string>)
                {
                    SubGroups = _baseForm["Subgroups"] as IList<string>;
                }
                else if (_baseForm["Subgroups"] is SimpleList<string>)
                {
                    SubGroups = (List<string>)((SimpleList<string>)_baseForm["Subgroups"]);
                }
            }
            return true;
		}
		#endregion

		/// <summary>
		/// Format of a user specification used to save a user to the server
		/// </summary>
		private static String GroupSpecFormat =
													"Group:\t{0}\r\n" +
													"\r\n" +
													"MaxResults:\t{1}\r\n" +
													"\r\n" +
													"MaxScanRows:\t{2}\r\n" +
													"\r\n" +
													"MaxLockTime:\t{3}\r\n" +
													"\r\n" +
													"MaxOpenFiles:\t{4}\r\n" +
													"\r\n" +
													"Timeout:\t{5}\r\n" +
													"\r\n" +
													"{6}" +
													"Subgroups:\r\n{7}" +
													"\r\n" +
													"Owners:\r\n{8}" +
													"\r\n" +
													"Users:\r\n{9}";

		/// <summary>
		/// Convert to specification in server format
		/// </summary>
		/// <returns>The specification for the group</returns>
		override public String ToString()
		{
			String subgroupsView = String.Empty;
			if (SubGroups != null)
			{
				for (int idx = 0; idx < SubGroups.Count; idx++)
				{
					subgroupsView += String.Format("\t{0}\r\n", SubGroups[idx]);
				}
			}
			String ownersView = String.Empty;
			if (OwnerNames != null)
			{
				for (int idx = 0; idx < OwnerNames.Count; idx++)
				{
					ownersView += String.Format("\t{0}\r\n", OwnerNames[idx]);
				}
			}
			String usersView = String.Empty;
			if (UserNames != null)
			{
				for (int idx = 0; idx < UserNames.Count; idx++)
				{
					usersView += String.Format("\t{0}\r\n", UserNames[idx]);
				}
			}
			String value = String.Format(GroupSpecFormat, Id, 
				(MaxResults > 0) ? MaxResults.ToString() : string.Empty,
				(MaxScanRows > 0) ? MaxScanRows.ToString() : string.Empty,
				(MaxLockTime > 0) ? MaxLockTime.ToString() : string.Empty,
				(MaxOpenFiles > 0) ? MaxOpenFiles.ToString() : string.Empty,
				(TimeOut > 0) ? TimeOut.ToString() : string.Empty,
                (PasswordTimeout > 0) ? "PasswordTimeout:\t" + PasswordTimeout.ToString() + "\r\n" + "\r\n" : string.Empty,
				subgroupsView, ownersView, usersView);

			return value;
		}
	}
}
