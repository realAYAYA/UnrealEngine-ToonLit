using Perforce.P4;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using System;
using System.Collections.Generic;
using System.Diagnostics;

namespace p4api.net.unit.test
{
	
	/// <summary>
	///This is a test class for RepositoryTest and is intended
	///to contain RepositoryTest Unit Tests
	///</summary>
	public partial class RepositoryTest
	{
		/// <summary>
		///A test for CreateUser
		///</summary>
		[TestMethod()]
		public void CreateUserTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			string targetUser = "thenewguy";

		    Process p4d = null;

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
                try
                {
				    p4d = Utilities.DeployP4TestServer(TestDir, 7, unicode);
				    Server server = new Server(new ServerAddress(uri));
			
					Repository rep = new Repository(server);

					using (Connection con = rep.Connection)
					{
						con.UserName = user;
						con.Client = new Client();
						con.Client.Name = ws_client;

						bool connected = con.Connect(null);
						Assert.IsTrue(connected);

						Assert.AreEqual(con.Status, ConnectionStatus.Connected);

						User u = new User();
						u.Id = targetUser;
						u.FullName = "The New Guy";
						u.Password = "ChangeMe!";
						u.EmailAddress = "newguy@p4test.com";

						con.UserName = targetUser;
						connected = con.Connect(null);
						Assert.IsTrue(connected);

						User newGuy = rep.CreateUser(u);

						Assert.IsNotNull(newGuy);
						Assert.AreEqual(targetUser, newGuy.Id);
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}

        /// <summary>
        ///A test for CreateUser with incomplete required fields
        ///</summary>
        [TestMethod()]
        public void CreateIncompleteUserTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            string targetUser = "thenewguy";

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, 7, unicode);
                    Server server = new Server(new ServerAddress(uri));
               
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        User u = new User();
                        u.Id = targetUser;                    
                        u.Password = "ChangeMe!";
                        u.EmailAddress = "newguy@p4test.com";

                        con.UserName = targetUser;
                        connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        try
                        {
                            User newGuy = rep.CreateUser(u);
                        }
                        catch (P4Exception e)
                        {
                            Assert.AreEqual(822153261, e.ErrorCode, "Error in user specification.\nMissing required field 'FullName'.\n");
                        }
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }


        /// <summary>
        ///A test for CreateUser using an invalid character in the name
        ///</summary>
        [TestMethod()]
        public void CreateUserWithInvalidNameTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            string targetUser = "-amy";

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, 7, unicode);
                    Server server = new Server(new ServerAddress(uri));
               
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        User u = new User();
                        u.Id = targetUser;
                        u.FullName = "The New Guy";
                        u.Password = "ChangeMe!";
                        u.EmailAddress = "newguy@p4test.com";

                        con.UserName = targetUser;
                        connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        try
                        {
                            User newGuy = rep.CreateUser(u);
                        }
                        catch (P4Exception e)
                        {
                            Assert.AreEqual(822155268, e.ErrorCode, "Invalid user (P4USER) or client (P4CLIENT) name.\nInitial dash character not allowed in '%targetUser'.\n");
                        }
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }


        /// <summary>
        ///A test for CreateUser using an invalid field
        ///</summary>
        [TestMethod()]
        public void CreateUserWithInvalidFieldTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            string targetUser = "thenewguy";
            string invalidReviewPath = "test";

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, 7, unicode);
                    Server server = new Server(new ServerAddress(uri));
               
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        User u = new User();
                        u.Id = targetUser;
                        u.FullName = "The New Guy";
                        u.Password = "ChangeMe!";
                        u.EmailAddress = "newguy@p4test.com";

                        IList < String > reviewList = new List<String>();
                        reviewList.Add(invalidReviewPath);
                        u.Reviews = reviewList; 

                        con.UserName = targetUser;
                        connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        try
                        {
                            User newGuy = rep.CreateUser(u);
                        }
                        catch (P4Exception e)
                        {
                            Assert.AreEqual(839063594, e.ErrorCode, "Error in user specification.\nPath '%invalidReviewPath' is not under '//...'.\n");
                        }
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }


        /// <summary>
        ///A test for UpdateUser: attempt to change name of a user as non-super user
        ///</summary>
        [TestMethod()]
        public void UpdateUserNameTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "Alex";
            string pass = string.Empty;
            string ws_client = "Alex_space";

            string newTargetUserName = "Alice2";

            Process p4d = null;
            
            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    string targetUser = "Alice";
                    if (i > 0)
                    {
                        targetUser = "alice";
                    }
                    p4d = Utilities.DeployP4TestServer(TestDir, 7, unicode);
                    Server server = new Server(new ServerAddress(uri));
                
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);                     
                        User u = rep.GetUser(targetUser);                      
                        Assert.IsNotNull(u);

                        u.FullName = newTargetUserName;
      
                        try
                        {
                            Options uFlags = new Options(UserCmdFlags.Force);
                            User updatedUser = rep.UpdateUser(u, uFlags);
                        }
                        catch (P4Exception e)
                        {
                            Assert.AreEqual(805705769, e.ErrorCode, "You don't have permission for this operation.\n");
                        }
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }


		/// <summary>
		///A test for DeleteUser
		///</summary>
		[TestMethod()]
		public void DeleteUserTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

			string targetUser = "deleteme";

		    Process p4d = null;

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
                try
                {
				    p4d = Utilities.DeployP4TestServer(TestDir, 7, unicode);
				    Server server = new Server(new ServerAddress(uri));
				
					Repository rep = new Repository(server);

					using (Connection con = rep.Connection)
					{
						con.UserName = user;
						con.Client = new Client();
						con.Client.Name = ws_client;

						bool connected = con.Connect(null);
						Assert.IsTrue(connected);

						Assert.AreEqual(con.Status, ConnectionStatus.Connected);

						User u = new User();
						u.Id = targetUser;

						Options uFlags = new Options(UserCmdFlags.Force);
						rep.DeleteUser(u, uFlags);

						IList<User> u2 = rep.GetUsers(new Options(UsersCmdFlags.None, -1), targetUser);

						Assert.IsNull(u2);
					}
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}

        /// <summary>
        ///A test for deleting a non-existing user
        ///</summary>
        [TestMethod()]
        public void DeleteNonExistentUserTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            string targetUser = "deleteme2";

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, 7, unicode);
                    Server server = new Server(new ServerAddress(uri));
               
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        User u = new User();
                        u.Id = targetUser;

                        Options uFlags = new Options(UserCmdFlags.Force);

                        try
                        {
                            rep.DeleteUser(u, uFlags);
                        }
                        catch (P4Exception e)
                        {
                            Assert.AreEqual(822221158, e.ErrorCode, ("User %targetUser does not exist\n"));  
                        }
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }


        /// <summary>
        ///A test for deleting other user without -f flag
        ///</summary>
        [TestMethod()]
        public void DeleteOtherUserWithoutFlagTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "Alex";
            string pass = string.Empty;
            string ws_client = "alex_space";

            string targetUser = "admin";

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    p4d = Utilities.DeployP4TestServer(TestDir, 7, unicode);
                    Server server = new Server(new ServerAddress(uri));
               
                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        User u = new User();
                        u.Id = targetUser;
                
                        try
                        {
                            rep.DeleteUser(u, null);
                        }
                        catch (P4Exception e)
                        {
                            Assert.AreEqual(822483030, e.ErrorCode, "Not user '%targetUser'; use -f to force delete.\n");
                        }
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

        /// <summary>
        ///A test for deleting a user with open files
        ///</summary>
        /*   [TestMethod()]
           public void DeleteUserWithOpenFilesTest()
           {
               bool unicode = false;

               string uri = "localhost:6666";
               string user = "admin";
               string pass = string.Empty;
               string ws_client = "admin_space";

               string targetUser = "Alice";
                 
                Process p4d = null;

               for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
               {
                   try
                   {
                        p4d = Utilities.DeployP4TestServer(TestDir, 7, unicode);
                        Server server = new Server(new ServerAddress(uri));
                   
                       Repository rep = new Repository(server);

                       using (Connection con = rep.Connection)
                       {
                           con.UserName = user;
                           con.Client = new Client();
                           con.Client.Name = ws_client;

                           bool connected = con.Connect(null);
                           Assert.IsTrue(connected);

                           Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                           User u = new User();
                           u.Id = targetUser;

                           Options uFlags = new Options(UserCmdFlags.Force);

                           try
                           {
                               rep.DeleteUser(u, uFlags);
                           }
                           catch (P4Exception e)
                           {
                               // TODO: This is a placeholder. Test criteria will need to be updated once job070607 is 
                               // fixed to handle the message returned by the server
                              Assert.IsTrue(e.Message.Contains("can't be deleted"));
                           }
                       }
                   }
                   finally
                   {
                       Utilities.RemoveTestServer(p4d, TestDir);
                   }
                   unicode = !unicode;
               }
           }
           */

        /// <summary>
		///A test for GetUser
		///</summary>
		[TestMethod()]
        public void GetUserTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            string targetUser = "Alex";
            string targetBadUser = "AlexanderTheNonExistent";

            Process p4d = null;

            for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
            {
                try
                {
                    if (unicode)
                        targetUser = "Алексей";

                    p4d = Utilities.DeployP4TestServer(TestDir, 6, unicode);
                    Server server = new Server(new ServerAddress(uri));

                    Repository rep = new Repository(server);

                    using (Connection con = rep.Connection)
                    {
                        con.UserName = user;
                        con.Client = new Client();
                        con.Client.Name = ws_client;

                        bool connected = con.Connect(null);
                        Assert.IsTrue(connected);

                        Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                        User u = rep.GetUser(targetUser, null);

                        Assert.IsNotNull(u);
                        Assert.AreEqual(targetUser, u.Id);

                        // test reviews for job093785 user
                        // Алексей has 2 review lines, the bug
                        // would skip the 2nd one
                        if (unicode)
                        {
                            Assert.AreEqual(u.Reviews.Count, 2);
                            Assert.AreEqual(u.Reviews[0], "//depot/MyCode/...");
                            Assert.AreEqual(u.Reviews[1], "//depot/TestData/...");
                        }

                        u = rep.GetUser(targetBadUser, null);

                        Assert.IsNull(u);
                    }
                }
                finally
                {
                    Utilities.RemoveTestServer(p4d, TestDir);
                }
                unicode = !unicode;
            }
        }

        /// <summary>
        ///A test for GetUsers
        ///</summary>
        [TestMethod()]
		public void GetUsersTest()
		{
			bool unicode = false;

			string uri = "localhost:6666";
			string user = "admin";
			string pass = string.Empty;
			string ws_client = "admin_space";

		    Process p4d = null;

			for (int i = 0; i < 2; i++) // run once for ascii, once for unicode
			{
                try
                {
				    p4d = Utilities.DeployP4TestServer(TestDir, 6, unicode);
				    Server server = new Server(new ServerAddress(uri));
				
					Repository rep = new Repository(server);

					using (Connection con = rep.Connection)
					{
						con.UserName = user;
						con.Client = new Client();
						con.Client.Name = ws_client;

						bool connected = con.Connect(null);
						Assert.IsTrue(connected);

						Assert.AreEqual(con.Status, ConnectionStatus.Connected);

						IList<User> u = rep.GetUsers(new Options(UsersCmdFlags.IncludeAll, 2));

						Assert.IsNotNull(u);
						Assert.AreEqual(2, u.Count);

						u = rep.GetUsers(new Options(UsersCmdFlags.IncludeAll, -1), "admin", "Alice");

						Assert.IsNotNull(u);
						Assert.AreEqual(2, u.Count);

						u = rep.GetUsers(new Options(UsersCmdFlags.IncludeAll, 3), "A*");

						Assert.IsNotNull(u);
						if (unicode)
							Assert.AreEqual(2, u.Count); // no user 'Alex' on unicode server
						else
							Assert.AreEqual(3, u.Count);

						//DateTime update = new DateTime(2011, 5, 24, 8, 48, 30);
						//if (unicode)
						//{
						//    update = new DateTime(2011, 5, 24, 9, 15, 12);
						//}
						//DateTime access = DateTime.Now;
						//access = access.Subtract(TimeSpan.FromSeconds(access.Second));
						//Assert.AreEqual(u[0].Updated,update);
						//Assert.AreEqual(u[0].Accessed.ToLongDateString(), access.ToLongDateString());
                    }
				}
				finally
				{
					Utilities.RemoveTestServer(p4d, TestDir);
				}
				unicode = !unicode;
			}
		}
        /// <summary>
        ///A test for setting and reading back Authmethod
        ///</summary>
        [TestMethod()]
        public void UpdateAuthMethodTest()
        {
            bool unicode = false;

            string uri = "localhost:6666";
            string user = "admin";
            string pass = string.Empty;
            string ws_client = "admin_space";

            Process p4d = null;

            try
            {
                p4d = Utilities.DeployP4TestServer(TestDir, 7, unicode);
                Server server = new Server(new ServerAddress(uri));

                Repository rep = new Repository(server);

                using (Connection con = rep.Connection)
                {
                    con.UserName = user;
                    con.Client = new Client();
                    con.Client.Name = ws_client;

                    bool connected = con.Connect(null);
                    Assert.IsTrue(connected);
                    Assert.AreEqual(con.Status, ConnectionStatus.Connected);

                    User u = new User
                    {
                        AuthMethod = "ldap",
                        Id = "bobski",
                        EmailAddress = "bobski@bobski.bo",
                        FullName = "bo bo booo"
                    };


                    Options uFlags = new Options(UserCmdFlags.Force);
                    rep.CreateUser(u, uFlags);

                    User userReadback = rep.GetUser("bobski");
                    Assert.AreEqual(userReadback.AuthMethod, u.AuthMethod);
                }
            }
            finally
            {
                Utilities.RemoveTestServer(p4d, TestDir);
            }
        }
    }
}
