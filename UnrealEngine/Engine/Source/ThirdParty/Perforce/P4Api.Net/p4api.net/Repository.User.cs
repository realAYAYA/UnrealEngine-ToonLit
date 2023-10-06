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
 * Name		: Repository.User.cs
 *
 * Author	: dbb
 *
 * Description	: User operations for the Repository.
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
		/// Create a new user in the repository.
		/// </summary>
		/// <param name="user">User specification for the new user</param>
		/// <param name="options">The '-f' and '-i' flags are required when creating a new user</param>
		/// <returns>The User object if new user was created, null if creation failed</returns>
		/// <remarks> The '-i' flag is added if not specified by the caller
		/// <br/>
		/// <br/><b>p4 help user</b>
		/// <br/> 
		/// <br/>     user -- Create or edit a user specification
		/// <br/> 
		/// <br/>     p4 user [-f] [name]
		/// <br/>     p4 user -d [-f] name
		/// <br/>     p4 user -o [name]
		/// <br/>     p4 user -i [-f]
		/// <br/> 
		/// <br/> 	Create a new user specification or edit an existing user specification.
		/// <br/> 	The specification form is put into a temporary file and the editor
		/// <br/> 	(configured by the environment variable $P4EDITOR) is invoked.
		/// <br/> 
		/// <br/> 	Normally, a user specification is created automatically the first
		/// <br/> 	time that the user issues any command that updates the depot. The
		/// <br/> 	'p4 user' command is typically used to edit the user's subscription
		/// <br/> 	list for change review.
		/// <br/> 
		/// <br/> 	The user specification form contains the following fields:
		/// <br/> 
		/// <br/> 	User:        The user name (read-only).
		/// <br/> 
		/// <br/> 	Email:       The user's email address (Default: user@client).
		/// <br/> 
		/// <br/> 	Update:      The date the specification was last modified (read-only).
		/// <br/> 
		/// <br/> 	Access:      The date that the user last issued a client command.
		/// <br/> 
		/// <br/> 	FullName:    The user's real name.
		/// <br/> 
		/// <br/> 	JobView:     Selects jobs that are displayed when the user creates
		/// <br/> 		     a changelist. These jobs can be closed automatically
		/// <br/> 		     when the user submits the changelist. For a description
		/// <br/> 		     of jobview syntax, see 'p4 help jobview'
		/// <br/> 
		/// <br/> 	Reviews:     The subscription list for change review.  There is no
		/// <br/> 	             limit on the number of lines that this field can contain.
		/// <br/> 		     You can include the following wildcards:
		/// <br/> 
		/// <br/> 			 ...            matches any characters including /
		/// <br/> 			 *              matches any character except /
		/// <br/> 
		/// <br/> 	Password:    The user's password.  See 'p4 help passwd'.
		/// <br/> 
		/// <br/> 	Type:        Must be 'service', operator, or 'standard'. Default is
		/// <br/> 		     'standard'. Once set, the user type cannot be changed.
		/// <br/> 
		/// <br/> 	AuthMethod:  Must be 'perforce' or 'ldap'. Default is 'perforce'
		/// <br/> 		     Unless overridden by the 'auth.default.method'
		/// <br/> 		     configurable, see 'p4 help configurables'. AuthMethod
		/// <br/> 		     can only be changed when the -f flag has been provided.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified user (unless the user has files
		/// <br/> 	open).
		/// <br/> 
		/// <br/> 	The -o flag writes the user specification to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a user specification from the standard input.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the creation, update or deletion of the specified
		/// <br/> 	user, and enables you to change the Last Modified date. By default,
		/// <br/> 	users can only delete or modify their own user specifications.  The
		/// <br/> 	-f flag requires 'super' access, which is granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To create a user with the username bsmith (when connected with
        ///		Connection.UserName = "bsmith"):
        ///		<code> 
        ///			User u = new User();
        ///			u.Id = "bsmith";
        ///			u.FullName = "Brian Smith";
        ///			u.EmailAddress = "bsmith@hotmail.com";
        ///			
        ///         u = Repository.CreateUser(u, null);
        ///		</code>
        ///		To create a user with the username bsmith (when connected with
        ///		a user that has super access):
        ///		<code> 
        ///			User u = new User();
        ///			u.Id = "bsmith";
        ///			u.FullName = "Brian Smith";
        ///			u.EmailAddress = "bsmith@hotmail.com";
        ///			
        ///         UserCmdOptions opts = new UserCmdOptions(UserCmdFlags.Force);
        ///			
        ///         u = Repository.CreateUser(u, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="UserCmdFlags"/>
        public User CreateUser(User user, Options options)
		{
			if (user == null)
			{
				throw new ArgumentNullException("user");

			}
			P4Command cmd = new P4Command(this, "user", true);

			cmd.DataSet = user.ToString();

			if (options == null)
			{
				options = new Options();
			}
			options["-i"] = null;

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				return user;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

		/// <summary>
		/// Create a new user in the repository.
		/// </summary>
		/// <param name="user">User specification for the new user</param>
		/// <returns>The User object if new user was created, null if creation failed</returns>
        /// <remarks> The '-i' flag is added if not specified by the caller
        /// <br/>
		/// <br/><b>p4 help user</b>
		/// <br/> 
		/// <br/>     user -- Create or edit a user specification
		/// <br/> 
		/// <br/>     p4 user [-f] [name]
		/// <br/>     p4 user -d [-f] name
		/// <br/>     p4 user -o [name]
		/// <br/>     p4 user -i [-f]
		/// <br/> 
		/// <br/> 	Create a new user specification or edit an existing user specification.
		/// <br/> 	The specification form is put into a temporary file and the editor
		/// <br/> 	(configured by the environment variable $P4EDITOR) is invoked.
		/// <br/> 
		/// <br/> 	Normally, a user specification is created automatically the first
		/// <br/> 	time that the user issues any command that updates the depot. The
		/// <br/> 	'p4 user' command is typically used to edit the user's subscription
		/// <br/> 	list for change review.
		/// <br/> 
		/// <br/> 	The user specification form contains the following fields:
		/// <br/> 
		/// <br/> 	User:        The user name (read-only).
		/// <br/> 
		/// <br/> 	Email:       The user's email address (Default: user@client).
		/// <br/> 
		/// <br/> 	Update:      The date the specification was last modified (read-only).
		/// <br/> 
		/// <br/> 	Access:      The date that the user last issued a client command.
		/// <br/> 
		/// <br/> 	FullName:    The user's real name.
		/// <br/> 
		/// <br/> 	JobView:     Selects jobs that are displayed when the user creates
		/// <br/> 		     a changelist. These jobs can be closed automatically
		/// <br/> 		     when the user submits the changelist. For a description
		/// <br/> 		     of jobview syntax, see 'p4 help jobview'
		/// <br/> 
		/// <br/> 	Reviews:     The subscription list for change review.  There is no
		/// <br/> 	             limit on the number of lines that this field can contain.
		/// <br/> 		     You can include the following wildcards:
		/// <br/> 
		/// <br/> 			 ...            matches any characters including /
		/// <br/> 			 *              matches any character except /
		/// <br/> 
		/// <br/> 	Password:    The user's password.  See 'p4 help passwd'.
		/// <br/> 
		/// <br/> 	Type:        Must be 'service', operator, or 'standard'. Default is
		/// <br/> 		     'standard'. Once set, the user type cannot be changed.
		/// <br/> 
		/// <br/> 	AuthMethod:  Must be 'perforce' or 'ldap'. Default is 'perforce'
		/// <br/> 		     Unless overridden by the 'auth.default.method'
		/// <br/> 		     configurable, see 'p4 help configurables'. AuthMethod
		/// <br/> 		     can only be changed when the -f flag has been provided.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified user (unless the user has files
		/// <br/> 	open).
		/// <br/> 
		/// <br/> 	The -o flag writes the user specification to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a user specification from the standard input.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the creation, update or deletion of the specified
		/// <br/> 	user, and enables you to change the Last Modified date. By default,
		/// <br/> 	users can only delete or modify their own user specifications.  The
		/// <br/> 	-f flag requires 'super' access, which is granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To create a user with the username bsmith (when connected with
        ///		Connection.UserName = "bsmith"):
        ///		<code> 
        ///			User u = new User();
        ///			u.Id = "bsmith";
        ///			u.FullName = "Brian Smith";
        ///			u.EmailAddress = "bsmith@hotmail.com";
        ///			
        ///         u = Repository.CreateUser(u);
        ///		</code>
        /// </example>
        /// <seealso cref="UserCmdFlags"/>

        public User CreateUser(User user)
		{
			return CreateUser(user, null);
		}

		/// <summary>
		/// Update the record for a user in the repository
		/// </summary>
		/// <param name="user">User specification for the user being updated</param>
		/// <returns>The User object if new user was saved, null if creation failed</returns>
        /// <remarks> The '-i' flag is added if not specified by the caller
        /// <br/>
		/// <br/><b>p4 help user</b>
		/// <br/> 
		/// <br/>     user -- Create or edit a user specification
		/// <br/> 
		/// <br/>     p4 user [-f] [name]
		/// <br/>     p4 user -d [-f] name
		/// <br/>     p4 user -o [name]
		/// <br/>     p4 user -i [-f]
		/// <br/> 
		/// <br/> 	Create a new user specification or edit an existing user specification.
		/// <br/> 	The specification form is put into a temporary file and the editor
		/// <br/> 	(configured by the environment variable $P4EDITOR) is invoked.
		/// <br/> 
		/// <br/> 	Normally, a user specification is created automatically the first
		/// <br/> 	time that the user issues any command that updates the depot. The
		/// <br/> 	'p4 user' command is typically used to edit the user's subscription
		/// <br/> 	list for change review.
		/// <br/> 
		/// <br/> 	The user specification form contains the following fields:
		/// <br/> 
		/// <br/> 	User:        The user name (read-only).
		/// <br/> 
		/// <br/> 	Email:       The user's email address (Default: user@client).
		/// <br/> 
		/// <br/> 	Update:      The date the specification was last modified (read-only).
		/// <br/> 
		/// <br/> 	Access:      The date that the user last issued a client command.
		/// <br/> 
		/// <br/> 	FullName:    The user's real name.
		/// <br/> 
		/// <br/> 	JobView:     Selects jobs that are displayed when the user creates
		/// <br/> 		     a changelist. These jobs can be closed automatically
		/// <br/> 		     when the user submits the changelist. For a description
		/// <br/> 		     of jobview syntax, see 'p4 help jobview'
		/// <br/> 
		/// <br/> 	Reviews:     The subscription list for change review.  There is no
		/// <br/> 	             limit on the number of lines that this field can contain.
		/// <br/> 		     You can include the following wildcards:
		/// <br/> 
		/// <br/> 			 ...            matches any characters including /
		/// <br/> 			 *              matches any character except /
		/// <br/> 
		/// <br/> 	Password:    The user's password.  See 'p4 help passwd'.
		/// <br/> 
		/// <br/> 	Type:        Must be 'service', operator, or 'standard'. Default is
		/// <br/> 		     'standard'. Once set, the user type cannot be changed.
		/// <br/> 
		/// <br/> 	AuthMethod:  Must be 'perforce' or 'ldap'. Default is 'perforce'
		/// <br/> 		     Unless overridden by the 'auth.default.method'
		/// <br/> 		     configurable, see 'p4 help configurables'. AuthMethod
		/// <br/> 		     can only be changed when the -f flag has been provided.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified user (unless the user has files
		/// <br/> 	open).
		/// <br/> 
		/// <br/> 	The -o flag writes the user specification to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a user specification from the standard input.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the creation, update or deletion of the specified
		/// <br/> 	user, and enables you to change the Last Modified date. By default,
		/// <br/> 	users can only delete or modify their own user specifications.  The
		/// <br/> 	-f flag requires 'super' access, which is granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To update a user with the username bsmith (when connected with
        ///		Connection.UserName = "bsmith"):
        ///		<code> 
        ///			User u = Repository.GetUser("bsmith");
        ///			
        ///			// update email address
        ///			u.EmailAddress = "bsmith@hotmail.com";
        ///			
        ///         u = Repository.UpdateUser(u, null);
        ///		</code>
        /// </example>
        /// <seealso cref="UserCmdFlags"/>
        public User UpdateUser(User user)
		{
			return CreateUser(user, null);
		}

        /// <summary>
        /// Update the record for a user in the repository
        /// </summary>
        /// <param name="user">User specification for the user being updated</param>
        /// <param name="options">Options/flags</param>
        /// <returns>The User object if new user was saved, null if creation failed</returns>
        /// <remarks> The '-i' flag is added if not specified by the caller
        /// <br/>
		/// <br/><b>p4 help user</b>
		/// <br/> 
		/// <br/>     user -- Create or edit a user specification
		/// <br/> 
		/// <br/>     p4 user [-f] [name]
		/// <br/>     p4 user -d [-f] name
		/// <br/>     p4 user -o [name]
		/// <br/>     p4 user -i [-f]
		/// <br/> 
		/// <br/> 	Create a new user specification or edit an existing user specification.
		/// <br/> 	The specification form is put into a temporary file and the editor
		/// <br/> 	(configured by the environment variable $P4EDITOR) is invoked.
		/// <br/> 
		/// <br/> 	Normally, a user specification is created automatically the first
		/// <br/> 	time that the user issues any command that updates the depot. The
		/// <br/> 	'p4 user' command is typically used to edit the user's subscription
		/// <br/> 	list for change review.
		/// <br/> 
		/// <br/> 	The user specification form contains the following fields:
		/// <br/> 
		/// <br/> 	User:        The user name (read-only).
		/// <br/> 
		/// <br/> 	Email:       The user's email address (Default: user@client).
		/// <br/> 
		/// <br/> 	Update:      The date the specification was last modified (read-only).
		/// <br/> 
		/// <br/> 	Access:      The date that the user last issued a client command.
		/// <br/> 
		/// <br/> 	FullName:    The user's real name.
		/// <br/> 
		/// <br/> 	JobView:     Selects jobs that are displayed when the user creates
		/// <br/> 		     a changelist. These jobs can be closed automatically
		/// <br/> 		     when the user submits the changelist. For a description
		/// <br/> 		     of jobview syntax, see 'p4 help jobview'
		/// <br/> 
		/// <br/> 	Reviews:     The subscription list for change review.  There is no
		/// <br/> 	             limit on the number of lines that this field can contain.
		/// <br/> 		     You can include the following wildcards:
		/// <br/> 
		/// <br/> 			 ...            matches any characters including /
		/// <br/> 			 *              matches any character except /
		/// <br/> 
		/// <br/> 	Password:    The user's password.  See 'p4 help passwd'.
		/// <br/> 
		/// <br/> 	Type:        Must be 'service', operator, or 'standard'. Default is
		/// <br/> 		     'standard'. Once set, the user type cannot be changed.
		/// <br/> 
		/// <br/> 	AuthMethod:  Must be 'perforce' or 'ldap'. Default is 'perforce'
		/// <br/> 		     Unless overridden by the 'auth.default.method'
		/// <br/> 		     configurable, see 'p4 help configurables'. AuthMethod
		/// <br/> 		     can only be changed when the -f flag has been provided.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified user (unless the user has files
		/// <br/> 	open).
		/// <br/> 
		/// <br/> 	The -o flag writes the user specification to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a user specification from the standard input.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the creation, update or deletion of the specified
		/// <br/> 	user, and enables you to change the Last Modified date. By default,
		/// <br/> 	users can only delete or modify their own user specifications.  The
		/// <br/> 	-f flag requires 'super' access, which is granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To update a user with the username bsmith (when connected with
        ///		a user that has super access):
        ///		<code> 
        ///			User u = Repository.GetUser("bsmith");
        ///			
        ///			// update email address
        ///			u.EmailAddress = "bsmith@hotmail.com";
        ///			
        ///         UserCmdOptions opts = new UserCmdOptions(UserCmdFlags.Force);
        ///			
        ///         u = Repository.UpdateUser(u, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="UserCmdFlags"/>
        public User UpdateUser(User user, Options options)
        {
            return CreateUser(user, options);
        }

		/// <summary>
		/// Get the record for an existing user from the repository.
		/// </summary>
		/// <param name="user">User name</param>
		/// <param name="options">There are no valid flags to use when fetching an existing user</param>
		/// <returns>The User object if new user was found, null if creation failed</returns>
        /// <remarks> The '-i' flag is added if not specified by the caller
        /// <br/>
		/// <br/><b>p4 help user</b>
		/// <br/> 
		/// <br/>     user -- Create or edit a user specification
		/// <br/> 
		/// <br/>     p4 user [-f] [name]
		/// <br/>     p4 user -d [-f] name
		/// <br/>     p4 user -o [name]
		/// <br/>     p4 user -i [-f]
		/// <br/> 
		/// <br/> 	Create a new user specification or edit an existing user specification.
		/// <br/> 	The specification form is put into a temporary file and the editor
		/// <br/> 	(configured by the environment variable $P4EDITOR) is invoked.
		/// <br/> 
		/// <br/> 	Normally, a user specification is created automatically the first
		/// <br/> 	time that the user issues any command that updates the depot. The
		/// <br/> 	'p4 user' command is typically used to edit the user's subscription
		/// <br/> 	list for change review.
		/// <br/> 
		/// <br/> 	The user specification form contains the following fields:
		/// <br/> 
		/// <br/> 	User:        The user name (read-only).
		/// <br/> 
		/// <br/> 	Email:       The user's email address (Default: user@client).
		/// <br/> 
		/// <br/> 	Update:      The date the specification was last modified (read-only).
		/// <br/> 
		/// <br/> 	Access:      The date that the user last issued a client command.
		/// <br/> 
		/// <br/> 	FullName:    The user's real name.
		/// <br/> 
		/// <br/> 	JobView:     Selects jobs that are displayed when the user creates
		/// <br/> 		     a changelist. These jobs can be closed automatically
		/// <br/> 		     when the user submits the changelist. For a description
		/// <br/> 		     of jobview syntax, see 'p4 help jobview'
		/// <br/> 
		/// <br/> 	Reviews:     The subscription list for change review.  There is no
		/// <br/> 	             limit on the number of lines that this field can contain.
		/// <br/> 		     You can include the following wildcards:
		/// <br/> 
		/// <br/> 			 ...            matches any characters including /
		/// <br/> 			 *              matches any character except /
		/// <br/> 
		/// <br/> 	Password:    The user's password.  See 'p4 help passwd'.
		/// <br/> 
		/// <br/> 	Type:        Must be 'service', operator, or 'standard'. Default is
		/// <br/> 		     'standard'. Once set, the user type cannot be changed.
		/// <br/> 
		/// <br/> 	AuthMethod:  Must be 'perforce' or 'ldap'. Default is 'perforce'
		/// <br/> 		     Unless overridden by the 'auth.default.method'
		/// <br/> 		     configurable, see 'p4 help configurables'. AuthMethod
		/// <br/> 		     can only be changed when the -f flag has been provided.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified user (unless the user has files
		/// <br/> 	open).
		/// <br/> 
		/// <br/> 	The -o flag writes the user specification to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a user specification from the standard input.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the creation, update or deletion of the specified
		/// <br/> 	user, and enables you to change the Last Modified date. By default,
		/// <br/> 	users can only delete or modify their own user specifications.  The
		/// <br/> 	-f flag requires 'super' access, which is granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get a user's specification for a user with the username bsmith:
        ///		<code> 
        ///			User u = Repository.GetUser("bsmith", null);
        ///		</code>
        /// </example>
        /// <seealso cref="UserCmdFlags"/>
        public User GetUser(string user, Options options)
		{
			if (user == null)
			{
				throw new ArgumentNullException("user");

			}
            // first make sure the user exists, otherwise depending on the way it is
            //  configured, the server might create a new user
            P4Command cmd = new P4Command(this, "users", true, user);

            Options usersOptions = new Options();
            usersOptions["-a"] = null;

            P4CommandResult results = cmd.Run(usersOptions);

            if ((results.Success == false) || (results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
            {
                return null;
            }
            else
            {
                bool found = false;			
                foreach (TaggedObject to in results.TaggedOutput)
                {
					// EPIC: change to case insensitive 
                    if (to.ContainsKey("User") && string.Equals(to["User"], user, StringComparison.OrdinalIgnoreCase))
                    {
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    return null;
                }
            }

            cmd = new P4Command(this, "user", true, user);

			if (options == null)
			{
				options = new Options();
			}
			options["-o"] = null;

			results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				User value = new User();

                bool dst_mismatch = false;
                string offset = string.Empty;

                if (Server != null && Server.Metadata != null)
                {
                    offset = Server.Metadata.DateTimeOffset;
                    dst_mismatch = FormBase.DSTMismatch(Server.Metadata);
                }

				value.FromUserCmdTaggedOutput((results.TaggedOutput[0]),offset,dst_mismatch);

				return value;
			}
			else
			{
				P4Exception.Throw(results.ErrorList);
			}
			return null;
		}

        /// <summary>
        /// Get the record for an existing user from the repository.
        /// </summary>
        /// <param name="user">User name</param>
        /// <returns>The User object if new user was found, null if creation failed</returns>
        /// <remarks> The '-i' flag is added if not specified by the caller
        /// <br/>
		/// <br/><b>p4 help user</b>
		/// <br/> 
		/// <br/>     user -- Create or edit a user specification
		/// <br/> 
		/// <br/>     p4 user [-f] [name]
		/// <br/>     p4 user -d [-f] name
		/// <br/>     p4 user -o [name]
		/// <br/>     p4 user -i [-f]
		/// <br/> 
		/// <br/> 	Create a new user specification or edit an existing user specification.
		/// <br/> 	The specification form is put into a temporary file and the editor
		/// <br/> 	(configured by the environment variable $P4EDITOR) is invoked.
		/// <br/> 
		/// <br/> 	Normally, a user specification is created automatically the first
		/// <br/> 	time that the user issues any command that updates the depot. The
		/// <br/> 	'p4 user' command is typically used to edit the user's subscription
		/// <br/> 	list for change review.
		/// <br/> 
		/// <br/> 	The user specification form contains the following fields:
		/// <br/> 
		/// <br/> 	User:        The user name (read-only).
		/// <br/> 
		/// <br/> 	Email:       The user's email address (Default: user@client).
		/// <br/> 
		/// <br/> 	Update:      The date the specification was last modified (read-only).
		/// <br/> 
		/// <br/> 	Access:      The date that the user last issued a client command.
		/// <br/> 
		/// <br/> 	FullName:    The user's real name.
		/// <br/> 
		/// <br/> 	JobView:     Selects jobs that are displayed when the user creates
		/// <br/> 		     a changelist. These jobs can be closed automatically
		/// <br/> 		     when the user submits the changelist. For a description
		/// <br/> 		     of jobview syntax, see 'p4 help jobview'
		/// <br/> 
		/// <br/> 	Reviews:     The subscription list for change review.  There is no
		/// <br/> 	             limit on the number of lines that this field can contain.
		/// <br/> 		     You can include the following wildcards:
		/// <br/> 
		/// <br/> 			 ...            matches any characters including /
		/// <br/> 			 *              matches any character except /
		/// <br/> 
		/// <br/> 	Password:    The user's password.  See 'p4 help passwd'.
		/// <br/> 
		/// <br/> 	Type:        Must be 'service', operator, or 'standard'. Default is
		/// <br/> 		     'standard'. Once set, the user type cannot be changed.
		/// <br/> 
		/// <br/> 	AuthMethod:  Must be 'perforce' or 'ldap'. Default is 'perforce'
		/// <br/> 		     Unless overridden by the 'auth.default.method'
		/// <br/> 		     configurable, see 'p4 help configurables'. AuthMethod
		/// <br/> 		     can only be changed when the -f flag has been provided.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified user (unless the user has files
		/// <br/> 	open).
		/// <br/> 
		/// <br/> 	The -o flag writes the user specification to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a user specification from the standard input.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the creation, update or deletion of the specified
		/// <br/> 	user, and enables you to change the Last Modified date. By default,
		/// <br/> 	users can only delete or modify their own user specifications.  The
		/// <br/> 	-f flag requires 'super' access, which is granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get a user's specification for a user with the username bsmith:
        ///		<code> 
        ///			User u = Repository.GetUser("bsmith");
        ///		</code>
        /// </example>
        /// <seealso cref="UserCmdFlags"/>
        public User GetUser(string user)
		{
			return GetUser(user, null);
		}

		/// <summary>
		/// Get a list of users from the repository
		/// </summary>
		/// <param name="options">Options for the users command. See: <see cref="UsersCmdFlags"/></param>
		/// <param name="user">Optional list of users. </param>
		/// <returns>A list containing the matching users</returns>
		/// <remarks>
		/// <br/><b>p4 help users</b>
		/// <br/> 
		/// <br/>     users -- List Perforce users
		/// <br/> 
		/// <br/>     p4 users [-l -a -r -c] [-m max] [user ...]
		/// <br/> 
		/// <br/> 	Lists all Perforce users or users that match the 'user' argument.
		/// <br/> 	The report includes the last time that each user accessed the system.
		/// <br/> 
		/// <br/> 	The -m max flag limits output to the first 'max' number of users.
		/// <br/> 
		/// <br/> 	The -a flag includes service and operator users in the output.
		/// <br/> 
		/// <br/> 	The -l flag includes additional information in the output.  The -l
		/// <br/> 	flag requires 'super' access, which is granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 	The -r and -c flags are only allowed on replica servers.  When
		/// <br/> 	-r is given only users who have used a replica are reported and
		/// <br/> 	when -c is given only the user information from the central server
		/// <br/> 	is reported.  Otherwise on a replica server, the user list will
		/// <br/> 	be slightly different from the master server as the user access times
		/// <br/> 	will reflect replica usage or master usage whichever is newer.
		/// <br/> 
		/// <br/> 
		/// </remarks>
		/// <example>
		///		To get the first 10 users that start with the letter 'A':
		///		<code> 
        ///			UsersCmdOptions opts = new UsersCmdOptions(UsersCmdFlags.None, 10);
        ///			IList&#60;User&#62; users = Repository.getUsers(opts, "A*");
		///		</code>
		///		To get the users for 'Bob', 'Ted', 'Carol' and 'Alice':
		///		<code> 
        ///			UsersCmdOptions opts = new UsersCmdOptions(UsersCmdFlags.None, -1);
        ///			IList&#60;User&#62; users = Repository.getUsers(opts, "Bob", "Ted", "Carol", "Alice");
		///		</code>
		///		To get all the users (WARNING, will fetch all users from the repository):
		///		<code> 
        ///			UsersCmdOptions opts = new UsersCmdOptions(UsersCmdFlags.IncludeAll, -1);
		/// 		IList&#60;User&#62; users = Repository.getUsers(opts, null);
		///		</code>
		/// </example>
		/// <seealso cref="UsersCmdFlags"/>
		public IList<User> GetUsers(Options options, params string[] user)
		{
			P4Command cmd = null;
			if ((user != null) && (user.Length > 0))
			{
				cmd = new P4Command(this, "users", true, user);
			}
			else
			{
				cmd = new P4Command(this, "users", true);
			}

			P4CommandResult results = cmd.Run(options);
			if (results.Success)
			{
				if ((results.TaggedOutput == null) || (results.TaggedOutput.Count <= 0))
				{
					return null;
				}
				List<User> value = new List<User>();
				foreach (TaggedObject obj in results.TaggedOutput)
				{
                    bool dst_mismatch = false;
                    string offset = string.Empty;

                    if (Server != null && Server.Metadata != null)
                    {
                        offset = Server.Metadata.DateTimeOffset;
                        dst_mismatch = FormBase.DSTMismatch(Server.Metadata);

                    }
					User u = new User();
					u.FromUserCmdTaggedOutput(obj,offset,dst_mismatch);
					value.Add(u);
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
        /// Get a list of users from the repository
        /// </summary>
        /// <param name="users">user(s) to get</param>
        /// <param name="options">Options for the users command. See: <see cref="UsersCmdFlags"/></param>
        /// <returns>A list containing the matching users</returns>
        /// <remarks>
		/// <br/><b>p4 help users</b>
		/// <br/> 
		/// <br/>     users -- List Perforce users
		/// <br/> 
		/// <br/>     p4 users [-l -a -r -c] [-m max] [user ...]
		/// <br/> 
		/// <br/> 	Lists all Perforce users or users that match the 'user' argument.
		/// <br/> 	The report includes the last time that each user accessed the system.
		/// <br/> 
		/// <br/> 	The -m max flag limits output to the first 'max' number of users.
		/// <br/> 
		/// <br/> 	The -a flag includes service and operator users in the output.
		/// <br/> 
		/// <br/> 	The -l flag includes additional information in the output.  The -l
		/// <br/> 	flag requires 'super' access, which is granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 	The -r and -c flags are only allowed on replica servers.  When
		/// <br/> 	-r is given only users who have used a replica are reported and
		/// <br/> 	when -c is given only the user information from the central server
		/// <br/> 	is reported.  Otherwise on a replica server, the user list will
		/// <br/> 	be slightly different from the master server as the user access times
		/// <br/> 	will reflect replica usage or master usage whichever is newer.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To get the first 10 users that start with the letter 'A':
        ///		<code> 
        ///			UsersCmdOptions opts = new UsersCmdOptions(UsersCmdFlags.None, 10);
        ///			
        ///         IList&#60;String&#62; names = new List&#60;String&#62;();
        ///         names.Add("A*");
        /// 
        ///			IList&#60;User&#62; users = Repository.getUsers(names, opts);
        ///		</code>
        ///		To get the users for 'Bob', 'Ted', 'Carol' and 'Alice':
        ///		<code> 
        ///			UsersCmdOptions opts = new UsersCmdOptions(UsersCmdFlags.None, -1);
        ///			
        ///         IList&#60;String&#62; names = new List&#60;String&#62;();
        ///         names.Add("Bob");
        ///         names.Add("Ted");
        ///         names.Add("Carol");
        ///         names.Add("Alice");
        /// 
        ///			IList&#60;User&#62; users = Repository.getUsers(names, opts);
        ///		</code>
        ///		To get all the users (WARNING, will fetch all users from the repository):
        ///		<code> 
        ///			UsersCmdOptions opts = new UsersCmdOptions(UsersCmdFlags.IncludeAll, -1);
        /// 		IList&#60;User&#62; users = Repository.getUsers(null, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="UsersCmdFlags"/>
		public IList<User> GetUsers( IList<string> users, Options options)
		{
			if (users != null)
			{
				return GetUsers(options, users.ToArray());
			}
			return GetUsers(options, null);
		}

		/// <summary>
		/// Delete a user from the repository
		/// </summary>
		/// <param name="user">The user to be deleted</param>
		/// <param name="options">Only the '-f' flag is valid when deleting an existing user</param>
        /// <remarks>
        /// <br/>
		/// <br/><b>p4 help user</b>
		/// <br/> 
		/// <br/>     user -- Create or edit a user specification
		/// <br/> 
		/// <br/>     p4 user [-f] [name]
		/// <br/>     p4 user -d [-f] name
		/// <br/>     p4 user -o [name]
		/// <br/>     p4 user -i [-f]
		/// <br/> 
		/// <br/> 	Create a new user specification or edit an existing user specification.
		/// <br/> 	The specification form is put into a temporary file and the editor
		/// <br/> 	(configured by the environment variable $P4EDITOR) is invoked.
		/// <br/> 
		/// <br/> 	Normally, a user specification is created automatically the first
		/// <br/> 	time that the user issues any command that updates the depot. The
		/// <br/> 	'p4 user' command is typically used to edit the user's subscription
		/// <br/> 	list for change review.
		/// <br/> 
		/// <br/> 	The user specification form contains the following fields:
		/// <br/> 
		/// <br/> 	User:        The user name (read-only).
		/// <br/> 
		/// <br/> 	Email:       The user's email address (Default: user@client).
		/// <br/> 
		/// <br/> 	Update:      The date the specification was last modified (read-only).
		/// <br/> 
		/// <br/> 	Access:      The date that the user last issued a client command.
		/// <br/> 
		/// <br/> 	FullName:    The user's real name.
		/// <br/> 
		/// <br/> 	JobView:     Selects jobs that are displayed when the user creates
		/// <br/> 		     a changelist. These jobs can be closed automatically
		/// <br/> 		     when the user submits the changelist. For a description
		/// <br/> 		     of jobview syntax, see 'p4 help jobview'
		/// <br/> 
		/// <br/> 	Reviews:     The subscription list for change review.  There is no
		/// <br/> 	             limit on the number of lines that this field can contain.
		/// <br/> 		     You can include the following wildcards:
		/// <br/> 
		/// <br/> 			 ...            matches any characters including /
		/// <br/> 			 *              matches any character except /
		/// <br/> 
		/// <br/> 	Password:    The user's password.  See 'p4 help passwd'.
		/// <br/> 
		/// <br/> 	Type:        Must be 'service', operator, or 'standard'. Default is
		/// <br/> 		     'standard'. Once set, the user type cannot be changed.
		/// <br/> 
		/// <br/> 	AuthMethod:  Must be 'perforce' or 'ldap'. Default is 'perforce'
		/// <br/> 		     Unless overridden by the 'auth.default.method'
		/// <br/> 		     configurable, see 'p4 help configurables'. AuthMethod
		/// <br/> 		     can only be changed when the -f flag has been provided.
		/// <br/> 
		/// <br/> 	The -d flag deletes the specified user (unless the user has files
		/// <br/> 	open).
		/// <br/> 
		/// <br/> 	The -o flag writes the user specification to the standard output.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -i flag reads a user specification from the standard input.
		/// <br/> 	The user's editor is not invoked.
		/// <br/> 
		/// <br/> 	The -f flag forces the creation, update or deletion of the specified
		/// <br/> 	user, and enables you to change the Last Modified date. By default,
		/// <br/> 	users can only delete or modify their own user specifications.  The
		/// <br/> 	-f flag requires 'super' access, which is granted by 'p4 protect'.
		/// <br/> 
		/// <br/> 
		/// </remarks>
        /// <example>
        ///		To delete a user with the username bsmith (when connected with
        ///		Connection.UserName = "bsmith"):
        ///		<code> 
        ///		    User u = Repository.GetUser("bsmith");
        ///         Repository.DeleteUser(u, null);
        ///		</code>
        ///		To delete a user with the username bsmith (when connected with
        ///		a user that has super access):
        ///		<code> 
        ///         UserCmdOptions opts = new UserCmdOptions(UserCmdFlags.Force);
        ///         
        ///			User u = Repository.GetUser("bsmith");
        ///         Repository.DeleteUser(u, opts);
        ///		</code>
        /// </example>
        /// <seealso cref="UserCmdFlags"/>
        public void DeleteUser(User user, Options options)
		{
			if (user == null)
			{
				throw new ArgumentNullException("user");

			}
			P4Command cmd = new P4Command(this, "user", true, user.Id);

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
