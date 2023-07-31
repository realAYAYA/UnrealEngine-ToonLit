// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using Microsoft.Deployment.WindowsInstaller;
using System.IO;
using System.Diagnostics;

namespace CustomAction
{
	public class CustomActions
	{
		// This custom action will install the DirectX redistributable.  The redist files are packaged in the custom action dll and
        // will be automatically extracted when this runs.  The files will also be cleaned up automatically.
		[CustomAction]
        public static ActionResult InstallDirectX(Session session)
		{

            ActionResult RetVal = ActionResult.Success;

            session.Log("Installing DirectX.");
            
            try
			{
                string FullAssemblyPath = System.Reflection.Assembly.GetAssembly(typeof(CustomActions)).Location;
                string AssemblyDir = Path.GetDirectoryName(FullAssemblyPath);
                string DirectXPath = Path.Combine(AssemblyDir, "DXSetup.exe");

                if (File.Exists(DirectXPath))
                {
                    ProcessStartInfo StartInfo = new ProcessStartInfo(DirectXPath, "/silent");
                    StartInfo.WorkingDirectory = AssemblyDir;
                    Process DXRedist = Process.Start(StartInfo);

                    // Arbitrary timeout in seconds.
                    int TimeOut = 240;


                    int WaitCount = 0;
                    while (!DXRedist.HasExited)
                    {

                        DXRedist.WaitForExit(100);
                        WaitCount += 100;

                        if (WaitCount > TimeOut * 1000)
                        {
                            break;
                        }
                    }

                    if (!DXRedist.HasExited)
                    {
                        session.Log("Exceeded DirectX install timeout.");
                        DXRedist.Kill();
                        RetVal = ActionResult.Failure;
                    }
                    else if (DXRedist.ExitCode != 0)
                    {
                        session.Log("DXSetup exited with code: " + DXRedist.ExitCode);
                        RetVal = ActionResult.Failure;
                    }
                }
                else
                {
                    session.Log("Could not find DXSetup.exe.");
                    RetVal = ActionResult.Failure;
                }
			}
			catch (System.Exception ex)
			{
                RetVal = ActionResult.Failure;
                session.Log("DirectX install failed: " + ex.Message);
			}

            if (RetVal == ActionResult.Success)
            {
                session.Log("Successfully installed DirectX.");
            }
            else
            {
                session.Log("Failed to install DirectX.");
            }

            return RetVal;
		}
	}
}
