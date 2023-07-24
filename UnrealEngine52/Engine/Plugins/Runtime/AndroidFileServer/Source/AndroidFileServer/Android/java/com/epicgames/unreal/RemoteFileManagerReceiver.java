// Copyright Epic Games, Inc. All Rights Reserved.

package com.epicgames.unreal;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;

import java.util.Set;

import static android.os.Environment.getExternalStorageDirectory;

public class RemoteFileManagerReceiver extends BroadcastReceiver {
    private static String TAG = "UEFS";
    private static final int SERVER_PORT = 57099;

	private boolean ValidateToken(String token)
	{
		return true;
	}

    @Override
    public void onReceive(Context context, Intent intent) {
		String action = intent.getAction();
		if (action != null && action == "com.epicgames.unreal.RemoteFileManager.intent.COMMAND") {
			String cmd = intent.getStringExtra("cmd");
			String token = intent.getStringExtra("token");
			int port = intent.getIntExtra("port", SERVER_PORT);
			if (cmd != null && cmd.equals("start") && GameActivity.AndroidFileServer_Verify(token)) {

				// make sure UnrealFileServer not already running
				boolean bUSBRunning = false;
				boolean bWiFiRunning = false;
				
				Set<Thread> threads = Thread.getAllStackTraces().keySet();
				for (Thread thread : threads) {
					String name = thread.getName();
					if (name.equals("UnrealAFS-USB"))
					{
						if (thread.isAlive())
						{
							bUSBRunning = true;
						}
					}
					else if (name.equals("UnrealAFS-WiFi"))
					{
						if (thread.isAlive())
						{
							bWiFiRunning = true;
						}
					}
				}

				if (!(bUSBRunning && bWiFiRunning)) {
                    String packageName = context.getPackageName();
                    PackageManager pm = context.getPackageManager();

					int versionCode = 0;
					try
					{
						versionCode = pm.getPackageInfo(packageName, 0).versionCode;
					}
					catch (Exception e)
					{
						// if the above failed, then, we can't use obbs
					}

					String EngineVersion = "5.0.0";
					String ProjectName = packageName;
					ProjectName = ProjectName.substring(ProjectName.lastIndexOf('.') + 1);
					boolean IsShipping = false;
					boolean UseExternalFilesDir = false;
					boolean PublicLogFiles = false;
					boolean AllowNetworkConnection = false;
					try {
						ApplicationInfo ai = pm.getApplicationInfo(packageName, PackageManager.GET_META_DATA);
						Bundle bundle = ai.metaData;

						if (bundle.containsKey("com.epicgames.unreal.GameActivity.EngineVersion"))
						{
							EngineVersion = bundle.getString("com.epicgames.unreal.GameActivity.EngineVersion");
						}
						if (bundle.containsKey("com.epicgames.unreal.GameActivity.ProjectName"))
						{
							ProjectName = bundle.getString("com.epicgames.unreal.GameActivity.ProjectName");
						}
						if (bundle.containsKey("com.epicgames.unreal.GameActivity.BuildConfiguration"))
						{
							String Configuration = bundle.getString("com.epicgames.unreal.GameActivity.BuildConfiguration");
							IsShipping = Configuration.contains("Shipping");
						}
						if (bundle.containsKey("com.epicgames.unreal.GameActivity.bUseExternalFilesDir"))
						{
							UseExternalFilesDir = bundle.getBoolean("com.epicgames.unreal.GameActivity.bUseExternalFilesDir");
						}
						if (bundle.containsKey("com.epicgames.unreal.GameActivity.bPublicLogFiles"))
						{
							PublicLogFiles = bundle.getBoolean("com.epicgames.unreal.GameActivity.bPublicLogFiles");
						}
						if (bundle.containsKey("com.epicgames.unreal.RemoteFileManager.bAllowNetworkConnection"))
						{
							AllowNetworkConnection = bundle.getBoolean("com.epicgames.unreal.RemoteFileManager.bAllowNetworkConnection");
						}
					}
					catch (PackageManager.NameNotFoundException | NullPointerException e)
					{
						Log.e(TAG,"Error when accessing application metadata", e);
					}

					String internal = context.getFilesDir().getAbsolutePath();
					String external = context.getExternalFilesDir(null).getAbsolutePath();
					String storage = android.os.Environment.getExternalStorageDirectory().getAbsolutePath();
					String obbdir = context.getObbDir().getAbsolutePath();

					if (!bUSBRunning)
					{
						RemoteFileManager fileManager = new RemoteFileManager(true, port, internal, external, storage, obbdir, packageName, versionCode, ProjectName, EngineVersion, IsShipping, PublicLogFiles);
						new Thread(fileManager, "UnrealAFS-USB").start();
					}
					if (AllowNetworkConnection && !bWiFiRunning)
					{
						RemoteFileManager fileManager = new RemoteFileManager(false, port, internal, external, storage, obbdir, packageName, versionCode, ProjectName, EngineVersion, IsShipping, PublicLogFiles);
						new Thread(fileManager, "UnrealAFS-WiFi").start();
					}
                }
            }
			else if (cmd != null && cmd.equals("stop")) {

				// send stop request to threads
				Set<Thread> threads = Thread.getAllStackTraces().keySet();
				for (Thread thread : threads) {
					String name = thread.getName();
					if (name.equals("UnrealAFS-USB") || name.equals("UnrealAFS-WiFi"))
					{
						if (thread.isAlive())
						{
							Log.d(TAG, "Sent stop request to " + context.getPackageName() + "/" + name);
							thread.interrupt();
						}
					}
				}
            }
        }
    }
}