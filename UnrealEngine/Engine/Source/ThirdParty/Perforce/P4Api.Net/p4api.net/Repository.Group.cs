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
 * Name		: Repository.Group.cs
 *
 * Author	: dbb
 *
 * Description	: Group operation for the Repository.
 *
 ******************************************************************************/
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace Perforce.P4
{
	public partial class Repository
	{
		/// <summary>
		/// Create a new group in the repository.
		/// </summary>
		/// <param name="group">Group specification for the new group</param>
		/// <param name="options">The  '-i' flags are required when creating a new group</param>
		/// <returns>The Group object if new group was created, null if creation failed</returns>
		/// <remarks> The '-i' flag is added if not specified by the caller
		/// <br/> 
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
        /// <example>               
        ///     To create the group 'Mygroup' with the owner 'Bob' and a user 'Ted' 
        ///     when connected as super user:
        ///     <code>
        ///     
        ///         string targetGroup = "Mygroup";
        ///         Group group = new Group();
        ///         group.Id = targetGroup;
        ///         group.UserNames = new  List&lt;string&gt; { "Ted" };
        ///			group.OwnerNames = new List&lt;string&gt; { "Bob" };
        ///         _repository.CreateGroup(group, null);
        ///         
        ///     </code>
        ///     To create a group "everyone" when connected as a user with admin level
        ///     rights:
        ///     <code>
        ///         
        ///         string targetGroup = "everyone";
        ///         Group group = new Group();
        ///         group.Id = targetGroup;
        ///         group.UserNames = new  List&lt;string&gt; { "Ted" };
        ///			group.OwnerNames = new List&lt;string&gt; { "Bob" };
        ///         _repository.CreateGroup(group, new Options(GroupCmdFlags.AdminAdd));
        ///         
        ///     </code>
        /// </example>
        /// <seealso cref="GroupCmdFlags"/> 
		public Group CreateGroup(Group group, Options options)
		{
			if (group == null)
			{
				throw new ArgumentNullException("group");

			}
			P4Command cmd = new P4Command(this, "group", true);

			cmd.DataSet = group.ToString();

			if (options == null)
			{
				options = new Options();
			}
			options["-i"] = null;

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				return group;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}
		/// <summary>
		/// Create a new group in the repository.
		/// </summary>
		/// <param name="group">Group specification for the new group</param>
		/// <returns>The Group object if new group was created, null if creation failed</returns>
        /// <example>               
        ///     To create the group 'Mygroup' with the owner 'Bob' and a users 'Ted' and 'Alex':
        ///     <code>
        ///     
        ///         string targetGroup = "Mygroup";
        ///         Group group = new Group();
        ///         group.Id = targetGroup;
        ///         group.UserNames = new  List&lt;string&gt; { "Ted", "Alex" };
        ///			group.OwnerNames = new  List&lt;string&gt; { "Bob" };
        ///         _repository.CreateGroup(group);
        ///         
        ///     </code>
        /// </example>
		public Group CreateGroup(Group group)
		{
			return CreateGroup(group, null);
		}
		/// <summary>
		/// Update the record for a group in the repository
		/// </summary>
		/// <param name="group">Group specification for the group being updated</param>
		/// <returns>The Group object if new group was saved, null if creation failed</returns>
        /// <example>               
        ///     To add the user 'Carol' to the group 'Mygroup':
        ///     <code>
        ///     
        ///         string targetGroup = "Mygroup";
        ///         Group group = GetGroup(targetGroup, null);
        ///         group.UserNames.Add("Carol");
        ///         _repository.UpdateGroup(group);
        ///         
        ///     </code>
        /// </example>
		public Group UpdateGroup(Group group)
		{
			return CreateGroup(group, null);
		}
        /// <summary>
        /// Update the record for a group in the repository
        /// </summary>
        /// <param name="group">Group specification for the group being updated</param>
        /// <param name="options">optional flags</param>
        /// <returns>The Group object if new group was saved, null if creation failed</returns>
        /// <example>               
        ///     To add the user 'Carol' to the group 'Mygroup':
        ///     <code>
        ///     
        ///         string targetGroup = "Mygroup";
        ///         Group group = GetGroup(targetGroup, null);
        ///         group.UserNames.Add("Carol");
        ///         _repository.UpdateGroup(group);
        ///         
        ///     </code>
        /// </example>
        public Group UpdateGroup(Group group, Options options)
        {
            return CreateGroup(group, options);
        }
		/// <summary>
		/// Get the record for an existing group from the repository.
		/// </summary>
		/// <param name="group">Group name</param>
		/// <param name="options">The Owner Access flag (-a) needs to be used if a user 
        ///  without 'super' access is an 'owner' of that group.</param>
		/// <returns>The Group object if new group was found, null if creation failed</returns>
        /// <example>
        ///		To get the group 'everyone' when connected as a user with super access:
        ///		<code> 		
        ///		
        ///			string targetGroup = "everyone";
        ///			Group group = _repository.GetGroup(targetGroup, null);	
        ///			
        ///		</code>
        ///		To get the group 'Mygroup' when connected as a without super access
        ///		who is the owner of that group:
        ///		<code>
        ///		
        ///           string targetGroup = "everyone";
        ///           GroupCmdOptions opts = new GroupCmdOptions(GroupCmdFlags.OwnerAccess);
        ///           Group group = _repository.GetGroup(targetGroup, opts);
        ///           
        ///		</code>
        /// </example>
        /// <seealso cref="GroupCmdFlags"/> 
		public Group GetGroup(string group, Options options)
		{
			if (group == null)
			{
				throw new ArgumentNullException("group");

			}
			P4Command cmd = new P4Command(this, "group", true, group);

			if (options == null)
			{
				options = new Options();
			}
			options["-o"] = null;

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				Group value = new Group();
				value.FromGroupCmdTaggedOutput((results.TaggedOutput[0]));

				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}
        /// <summary>
        /// Get the record for an existing group from the repository.
        /// </summary>
        /// <param name="group">Group name</param>
        /// <returns>The Group object if new group was found, null if creation failed</returns>
        /// <example>
        ///		To get the group 'everyone':
        ///		<code> 	
        ///		
        ///			string targetGroup = "everyone";
        ///			IList&#60;Group&#62; group = _repository.getGroup(targetGroup);
        ///			
        ///		</code>
        /// </example>
		public Group GetGroup(string group)
		{
			return GetGroup(group, null);
		}
        /// <summary>
        /// Get a list of groups from the repository
        /// </summary>
        /// <param name="options">options for the groups command<see cref="GroupsCmdOptions"/></param>
        /// <param name="groups">groups</param>
        /// <returns>A list containing the matching groups</returns>
        /// <remarks>
        /// <br/><b>p4 help groups</b>
        /// <br/> 
        /// <br/>     groups -- List groups (of users)
        /// <br/> 
        /// <br/>     p4 groups [-m max] [-v] [group]
        /// <br/>     p4 groups [-m max] [-i [-v]] user | group
        /// <br/>     p4 groups [-m max] [-g | -u | -o] name
        /// <br/> 
        /// <br/> 	The first form lists all user groups defined in the server, or just
        /// <br/> 	the specified group.
        /// <br/> 
        /// <br/> 	The second form displays subgroup relationships. If a user argument is
        /// <br/> 	specified, only groups containing that user are displayed. If a group
        /// <br/> 	argument is specified, only groups containing the group are displayed.
        /// <br/> 
        /// <br/> 	The third form is useful when there are groups and users with the
        /// <br/> 	same name, or when requesting all groups owned by a certain user.
        /// <br/> 
        /// <br/> 	The -i flag also displays groups that the user or group belongs to
        /// <br/> 	indirectly by means of membership in subgroups.
        /// <br/> 
        /// <br/> 	The -m max flag limits output to the specified number of groups.
        /// <br/> 
        /// <br/> 	The -v flag displays the MaxResults, MaxScanRows, MaxLockTime, and
        /// <br/> 	Timeout values for each group that is displayed.
        /// <br/> 
        /// <br/> 	The -g flag indicates that the 'name' argument is a group.
        /// <br/> 
        /// <br/> 	The -u flag indicates that the 'name' argument is a user.
        /// <br/> 
        /// <br/> 	The -o flag indicates that the 'name' argument is an owner.
        /// <br/> 
        /// <br/> 
        /// </remarks>
        /// <example>
        ///		To get the first 10 groups:
        ///		<code> 
        ///		
        ///			Options opts = new Options(GroupsCmdFlags.None, 10);
        ///			IList&#60;Group&#62; groups = _repository.getGroups(opts);		
        ///			
        ///		</code>
        ///		To get all groups that 'Bob' belongs to, including subgroups:
        ///		<code> 
        ///			
        ///			Options opts = new Options(GroupsCmdFlags.IncludeIndirect, -1);
        ///			IList&#60;Group&#62; groups = _repository.getGroups(opts, "Bob");
        ///			
        ///		</code>
        ///		To get all the groups with the MaxResults, MaxScanRows, MaxLockTime, and
        ///		Timeout values for the specified group:
        ///		<code> 
        ///						
        ///			Options opts = new Options(GroupsCmdFlags.IncludeAllValues, -1);
        /// 		IList&#60;Group&#62; groups = _repository.getGroups(opts);
        ///			
        ///		</code>
        /// </example>
        /// <seealso cref="GroupsCmdFlags"/> 
        public IList<Group> GetGroups(Options options, params string[] groups)
		{
			P4Command cmd = null;
			if ((groups != null) && (groups.Length > 0))
			{
				cmd = new P4Command(this, "groups", true, groups);
			}
			else
			{
				cmd = new P4Command(this, "groups", true);
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				List<Group> value = new List<Group>();
				Dictionary<string, Group> map = new Dictionary<string, Group>();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
					string groupName = obj["group"];
					Group group = null;
					if (map.ContainsKey(groupName))
					{
						group = map[groupName];
					}
					else
					{
						group = new Group(groupName);

						int v = -1;
						if (obj.ContainsKey("maxResults"))
						{
							int.TryParse(obj["maxResults"], out v);
						}
						group.MaxResults = v;

						v = -1;
						if (obj.ContainsKey("maxScanRows"))
						{
							int.TryParse(obj["maxScanRows"], out v);
						}
						group.MaxScanRows = v;

						v = -1;
						if (obj.ContainsKey("maxLockTime"))
						{
							int.TryParse(obj["maxLockTime"], out v);
						}
						group.MaxLockTime = v;

						v = -1;
						if (obj.ContainsKey("timeout"))
						{
							int.TryParse(obj["timeout"], out v);
						}
						group.TimeOut = v;

						v = -1;
						if (obj.ContainsKey("passTimeout"))
						{
							int.TryParse(obj["passTimeout"], out v);
						}
						group.MaxResults = v;

						map[groupName] = group;
						value.Add(group);
					}
					string user = null;
					if (obj.ContainsKey("user"))
					{
						user = obj["user"];
					}
					else
					{
						// no user name, can't continue
					}
					if ((obj.ContainsKey("isUser")) && (obj["isUser"] == "1"))
					{
						if (group.UserNames == null)
						{
							group.UserNames = new List<string>();
						}
						group.UserNames.Add(user);
					}
					if ((obj.ContainsKey("isOwner")) && (obj["isOwner"] == "1"))
					{
						if (group.OwnerNames == null)
						{
							group.OwnerNames = new List<string>();
						}
						group.OwnerNames.Add(user);
					}
					if ((obj.ContainsKey("isSubGroup")) && (obj["isSubGroup"] == "1"))
					{
						if (group.SubGroups == null)
						{
							group.SubGroups = new List<string>();
						}
						group.SubGroups.Add(user);
					}
				}
				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}
		/// <summary>
		/// Delete a group from the repository
		/// </summary>
		/// <param name="group">The group to be deleted</param>
        /// <param name="options">The -a flag is needed to delete
        ///  a group when the user is an owner but not a superuser</param>
        /// <example>
        ///		To delete the group 'Mygroup' when the user is a superuser:
        ///		<code>
        ///		
        ///			string targetGroup = "Mygroup";
        ///			Group group = new Group();
        ///			group.Id = targetGroup;
        ///			_repository.DeleteGroup(group, null);		
        ///			
        ///		</code>
        ///		To delete the group 'Mygroup' when the user is an owner but not a superuser:
        ///		<code>
        ///		
        ///			string targetGroup = "Mygroup";
        ///			Group group = new _repository.GetGroup(targetGroup);
        ///			GroupCmdOptions opts = new GroupCmdOptions(GroupCmdFlags.OwnerAccess);
        ///			_repository.DeleteGroup(group, opts);	
        ///			
        ///		</code>
        /// </example>
        /// <seealso cref="GroupCmdFlags"/> 
		public void DeleteGroup(Group group, Options options)
		{
			if (group == null)
			{
				throw new ArgumentNullException("group");
			}
			P4Command cmd = new P4Command(this, "group", true, group.Id);

			if (options == null)
			{
				options = new Options();
			}
			options["-d"] = null;

			P4CommandResult results = cmd.Run(options);
			if (results.Success == false)
			{
				P4Exception.Throw(results.ErrorList);
			}
		}
	}
}
