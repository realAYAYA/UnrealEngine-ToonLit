//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.unreal;

import java.util.ArrayList;
import java.util.List;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.PermissionInfo;
import android.provider.Settings;
import android.view.View;
import android.view.WindowManager;
import android.net.Uri;

import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;

public class SplashActivity extends Activity
{
	private static final int PERMISSION_REQUEST_CODE = 1105;
	private static final int REQUEST_PERMISSION_SETTING = 1;

	private String packageName;
	private PackageManager pm;
	private String[] permissionsRequiredAtStart = {};

	private Intent GameActivityIntent;
	private boolean WaitForPermission = false;

	public static Logger Log = new Logger("UE", "SplashActivity");
	
	@SuppressLint("ObsoleteSdkInt")
	@Override
	protected void onCreate(Bundle savedInstanceState)
	{
		super.onCreate(savedInstanceState);
		packageName = getPackageName();
		pm = getPackageManager();

		boolean ShouldHideUI = false;
		boolean UseDisplayCutout = false;
		boolean IsShipping = false;
		boolean UseExternalFilesDir = false;
		try {
			ApplicationInfo ai = pm.getApplicationInfo(packageName, PackageManager.GET_META_DATA);
			Bundle bundle = ai.metaData;

			if(bundle.containsKey("com.epicgames.unreal.GameActivity.bShouldHideUI"))
			{
				ShouldHideUI = bundle.getBoolean("com.epicgames.unreal.GameActivity.bShouldHideUI");
			}
			if(bundle.containsKey("com.epicgames.unreal.GameActivity.bUseDisplayCutout"))
			{
				UseDisplayCutout = bundle.getBoolean("com.epicgames.unreal.GameActivity.bUseDisplayCutout");
			}
			if (bundle.containsKey("com.epicgames.unreal.GameActivity.BuildConfiguration"))
			{
				String Configuration = bundle.getString("com.epicgames.unreal.GameActivity.BuildConfiguration");
				IsShipping = Configuration.equals("Shipping");
			}
			if (bundle.containsKey("com.epicgames.unreal.GameActivity.bUseExternalFilesDir"))
            {
                UseExternalFilesDir = bundle.getBoolean("com.epicgames.unreal.GameActivity.bUseExternalFilesDir");
            }
			if (bundle.containsKey("com.epicgames.unreal.GameActivity.StartupPermissions"))
			{
				permissionsRequiredAtStart = filterRequiredPermissions(bundle.getString("com.epicgames.unreal.GameActivity.StartupPermissions"));
			}
		}
		catch (NameNotFoundException | NullPointerException e)
		{
			Log.error("Error when accessing application metadata", e);
		}

		if (ShouldHideUI)
		{ 
			View decorView = getWindow().getDecorView(); 
			// only do this on KitKat and above
			if(android.os.Build.VERSION.SDK_INT >= 19) {
				decorView.setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE
											| View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION
											| View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
											| View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
											| View.SYSTEM_UI_FLAG_FULLSCREEN
											| View.SYSTEM_UI_FLAG_IMMERSIVE);  // NOT sticky (will be set later in MainActivity)
			}
		}
		
		// allow certain models for now to use full area around cutout
		boolean BlockDisplayCutout = android.os.Build.VERSION.SDK_INT < 30;
		if (android.os.Build.MANUFACTURER.equals("HUAWEI"))
		{
			BlockDisplayCutout = false;
		}
		else if (android.os.Build.MANUFACTURER.equals("HMD Global"))
		{
			String model = android.os.Build.MODEL;
			if (model.equals("Nokia 8.1"))
			{
				BlockDisplayCutout = false;
			}
		}
		else if (android.os.Build.MANUFACTURER.equals("samsung"))
		{
			String model = android.os.Build.MODEL;
			if (model.startsWith("SM-G970") || model.startsWith("SM-G973") || model.startsWith("SM-G975") ||
				model.startsWith("SC-03L") || model.startsWith("SCV41") || model.startsWith("SC-04L") ||
				model.startsWith("SCV42") || model.startsWith("SM-N97") || model.startsWith("SM-F700") ||
				model.startsWith("SM-G98") || model.startsWith("SCV47") || model.startsWith("SCG01") ||
				model.startsWith("SCG02") || model.startsWith("SC-51A") || model.startsWith("SC-52A") ||
				android.os.Build.VERSION.SDK_INT >= 28)
			{
				BlockDisplayCutout = false;
			}
		}
		else if (android.os.Build.MANUFACTURER.equals("Xiaomi"))
		{
			String model = android.os.Build.MODEL;
			if (model.startsWith("POCOPHONE F1"))
			{
				BlockDisplayCutout = false;
			}
		}
		else if (android.os.Build.MANUFACTURER.equals("OnePlus"))
		{
			String model = android.os.Build.MODEL;
			if (model.startsWith("KB2000") || model.startsWith("KB2001") || model.startsWith("KB2003") ||
				model.startsWith("KB2005") || model.startsWith("KB2007") || model.startsWith("LE2110") ||
				model.startsWith("LE2111") || model.startsWith("LE2113") || model.startsWith("LE2115") ||
				model.startsWith("LE2117") || model.startsWith("LE2119") || model.startsWith("LE2100") ||
				model.startsWith("LE2101") || model.startsWith("LE2120") || model.startsWith("LE2121") ||
				model.startsWith("LE2123") || model.startsWith("LE2125") || model.startsWith("LE2127") ||
				model.startsWith("IN2020") || model.startsWith("IN2021") || model.startsWith("IN2023") ||
				model.startsWith("IN2025") || model.startsWith("IN2010") || model.startsWith("IN2011") ||
				model.startsWith("IN2013") || model.startsWith("IN2015") || model.startsWith("IN2017") ||
				model.startsWith("IN2019") || model.startsWith("AC2001") || model.startsWith("AC2003") ||
				model.startsWith("BE2025") || model.startsWith("BE2026") || model.startsWith("BE2028") ||
				model.startsWith("BE2029"))
			{
				BlockDisplayCutout = false;
			}
		}
		if (BlockDisplayCutout)
		{
			UseDisplayCutout = false;
		}

		if (UseDisplayCutout)
		{
			// only do this on Android Pie and above
			if (android.os.Build.VERSION.SDK_INT >= 28)
			{
	            WindowManager.LayoutParams params = getWindow().getAttributes();
		        params.layoutInDisplayCutoutMode = WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
			    getWindow().setAttributes(params);
			}
			else
			{
				UseDisplayCutout = false;
			}
		}

		GameActivityIntent = new Intent(this, GameActivity.class);
		GameActivityIntent.putExtras(getIntent());
		GameActivityIntent.addFlags(Intent.FLAG_ACTIVITY_NO_ANIMATION);
		GameActivityIntent.putExtra("UseSplashScreen", "true");
		if (ShouldHideUI)
		{
			GameActivityIntent.putExtra("ShouldHideUI", "true");
		}
		if (UseDisplayCutout)
		{
			GameActivityIntent.putExtra("UseDisplayCutout", "true");
		}

		//pass down any extras added to this Activity's intent to the GameActivity intent (GCM data, for example)
		Intent intentFromActivity = getIntent();
		Bundle intentBundle = intentFromActivity.getExtras();
		if(intentBundle != null)
		{
			GameActivityIntent.putExtras(intentBundle);
		}
		
		// pass the action if available
		String intentAction = intentFromActivity.getAction();
		if (intentAction != null)
		{
			GameActivityIntent.setAction(intentAction);
		}

		// make a list of ungranted dangerous permissions in manifest required at start of GameActivity and request any we still need
		ArrayList<String> ungrantedPermissions = getUngrantedPermissions(this, getDangerousPermissions(pm, packageName), permissionsRequiredAtStart);
		if (ungrantedPermissions.size() > 0)
		{
			ActivityCompat.requestPermissions(this, ungrantedPermissions.toArray(new String[ungrantedPermissions.size()]), PERMISSION_REQUEST_CODE);
			WaitForPermission = true;
		}

		if (!WaitForPermission)
		{
			startActivity(GameActivityIntent);
			finish();
			overridePendingTransition(0, 0);
		}
	}

	private int getResourceId(String VariableName, String ResourceName, String PackageName)
	{
		try {
			return getResources().getIdentifier(VariableName, ResourceName, PackageName);
		}
		catch (Exception e) {
			e.printStackTrace();
			return -1;
		} 
	}

	private String getResourceStringOrDefault(String PackageName, String ResourceName, String DefaultString)
	{
		int resourceId = getResourceId(ResourceName, "string", PackageName);
		return (resourceId < 1) ? DefaultString : getString(resourceId);
	}

	private String[] filterRequiredPermissions(String permissions)
	{
		String manufacturer = android.os.Build.MANUFACTURER;
		ArrayList<String> keptPermissions = new ArrayList<>();
		String[] requiredPermissions = permissions.split(",");
		for (String required : requiredPermissions)
		{
			required = required.replaceAll("\\s", "");
			if (required.length() > 0)
			{
				int conditionalIndex = required.indexOf("|");
				if (conditionalIndex > 1)
				{
					String conditions[] = required.substring(1, conditionalIndex-1).split(",");
					for (String condition : conditions)
					{
						String make = condition;

						// check for Android version requirements first; can be <, <=, ==, !=, >=, >
						try
						{
							int compareIndex = condition.indexOf("<=");
							if (compareIndex > 0)
							{
								make = condition.substring(0, compareIndex);
								int version = Integer.parseInt(condition.substring(compareIndex+1));
								if (android.os.Build.VERSION.SDK_INT > version)
								{
									continue;
								}
							}
							else
							{
								compareIndex = condition.indexOf("<");
								if (compareIndex > 0)
								{
									make = condition.substring(0, compareIndex);
									int version = Integer.parseInt(condition.substring(compareIndex+1));
									if (android.os.Build.VERSION.SDK_INT >= version)
									{
										continue;
									}
								}
								else
								{
									compareIndex = condition.indexOf(">=");
									if (compareIndex > 0)
									{
										make = condition.substring(0, compareIndex);
										int version = Integer.parseInt(condition.substring(compareIndex+1));
										if (android.os.Build.VERSION.SDK_INT < version)
										{
											continue;
										}
									}
									else
									{
										compareIndex = condition.indexOf(">");
										if (compareIndex > 0)
										{
											make = condition.substring(0, compareIndex);
											int version = Integer.parseInt(condition.substring(compareIndex+1));
											if (android.os.Build.VERSION.SDK_INT <= version)
											{
												continue;
											}
										}
										else
										{
											compareIndex = condition.indexOf("==");
											if (compareIndex > 0)
											{
												make = condition.substring(0, compareIndex);
												int version = Integer.parseInt(condition.substring(compareIndex+1));
												if (android.os.Build.VERSION.SDK_INT != version)
												{
													continue;
												}
											}
											else
											{
												compareIndex = condition.indexOf("!=");
												if (compareIndex > 0)
												{
													make = condition.substring(0, compareIndex);
													int version = Integer.parseInt(condition.substring(compareIndex+1));
													if (android.os.Build.VERSION.SDK_INT == version)
													{
														continue;
													}
												}
											}
										}
									}
								}
							}
						}
						catch (Exception e)
						{
							Log.error("Error parsing required permissions: " + condition);
							continue;
						}
						
						if (make.equals("ALL") || make.equals(manufacturer))
						{
							keptPermissions.add(required.substring(conditionalIndex + 1));
							break;
						}
					}
				}
				else
				{
					keptPermissions.add(required);
				}
			}
		}
		return keptPermissions.toArray(new String[keptPermissions.size()]);
	}

	public ArrayList<String> getUngrantedPermissions(Context context, ArrayList<String> dangerousPermissions, String[] requiredPermissions)
	{
		ArrayList<String> ungrantedPermissions = new ArrayList<>();
		if (dangerousPermissions.size() > 0)
		{
			for (String required : requiredPermissions)
			{
				if (dangerousPermissions.contains(required))
				{
					if (ContextCompat.checkSelfPermission(context, required) != PackageManager.PERMISSION_GRANTED)
					{
						ungrantedPermissions.add(required);
					}
				}
			}
		}
		return ungrantedPermissions;
	}

	public ArrayList<String> getDangerousPermissions(PackageManager pm, String packageName)
	{
		int targetSdkVersion = 0;
		ArrayList<String> dangerousPermissions = new ArrayList<>();
		try 
		{
			PackageInfo packageInfo = pm.getPackageInfo(packageName, 0);
			targetSdkVersion = packageInfo.applicationInfo.targetSdkVersion;

			// 23 is the API level (Marshmallow) where runtime permission handling is available
			if (android.os.Build.VERSION.SDK_INT >= 23 && targetSdkVersion >= 23)
			{
				packageInfo = pm.getPackageInfo(packageName, PackageManager.GET_PERMISSIONS);
				if (packageInfo != null)
				{
					if (packageInfo.requestedPermissions != null && packageInfo.requestedPermissions.length > 0)
					{
						if (android.os.Build.VERSION.SDK_INT >= 28)
						{
							for (String permission : packageInfo.requestedPermissions)
							{
								try
								{
									PermissionInfo permissionInfo = pm.getPermissionInfo(permission, 0);
									if (permissionInfo.getProtection() == PermissionInfo.PROTECTION_DANGEROUS)
									{
										dangerousPermissions.add(permission);
									}
								}
								catch (PackageManager.NameNotFoundException e)
								{
								}
							}
						}
						else
						{
							for (String permission : packageInfo.requestedPermissions)
							{
								try
								{
									PermissionInfo permissionInfo = pm.getPermissionInfo(permission, 0);
									if ((permissionInfo.protectionLevel & PermissionInfo.PROTECTION_MASK_BASE) == PermissionInfo.PROTECTION_DANGEROUS)
									{
										dangerousPermissions.add(permission);
									}
								}
								catch (PackageManager.NameNotFoundException e)
								{
								}
							}
						}
					}
				}
			}
		}
		catch (PackageManager.NameNotFoundException e) 
		{
		}

		// if asking for WRITE_EXTERNAL_STORAGE, don't also need ask for READ_EXTERNAL_STORAGE
		if (dangerousPermissions.contains("android.permission.WRITE_EXTERNAL_STORAGE"))
		{
			dangerousPermissions.remove("android.permission.READ_EXTERNAL_STORAGE");
		}

		return dangerousPermissions;
	}

	public String getRationale(String permission)
	{
		return getResourceStringOrDefault(packageName, "PERM_Info_" + permission, "This permission is required to start the game:\n" + permission);
	}

	public void showDialog(String title, String message, boolean bShowSettings)
	{
		final String dialogTitle = title;
		final String dialogMessage = message;
		final boolean dialogSettings = bShowSettings;

		runOnUiThread(new Runnable()
		{
			@Override
			public void run()
			{
				AlertDialog.Builder dialog = new AlertDialog.Builder(SplashActivity.this);
				dialog.setCancelable(false);
				dialog.setTitle(dialogTitle);
				dialog.setMessage(dialogMessage);
				dialog.setNegativeButton(getResourceStringOrDefault(packageName, "PERM_Quit", "Quit"), new DialogInterface.OnClickListener() {
					@Override
					public void onClick(DialogInterface dialog, int id) {
						dialog.dismiss();
						System.exit(0);
					}
				});
				if (!dialogSettings)
				{
					dialog.setPositiveButton(getResourceStringOrDefault(packageName, "PERM_OK", "OK"), new DialogInterface.OnClickListener() {
						@Override
						public void onClick(DialogInterface dialog, int id) {
							dialog.dismiss();
							ArrayList<String> ungrantedPermissions = getUngrantedPermissions(SplashActivity.this, getDangerousPermissions(pm, packageName), permissionsRequiredAtStart);
							if (ungrantedPermissions.size() > 0)
							{
								ActivityCompat.requestPermissions(SplashActivity.this, ungrantedPermissions.toArray(new String[ungrantedPermissions.size()]), PERMISSION_REQUEST_CODE);
							}
							else
							{
								// should not get here, but launch GameActivity since have all permissions
								startActivity(GameActivityIntent);
								finish();
								overridePendingTransition(0, 0);
							}
						}
					});
				}
				else
				{
					dialog.setPositiveButton(getResourceStringOrDefault(packageName, "PERM_Settings", "Settings"), new DialogInterface.OnClickListener() {
						@Override
						public void onClick(DialogInterface dialog, int id) {
							dialog.dismiss();
							Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
							Uri uri = Uri.fromParts("package", packageName, null);
							intent.setData(uri);
							startActivityForResult(intent, REQUEST_PERMISSION_SETTING);
							System.exit(0);
						}
					});
				}

				dialog.create().show();
			}
		});
	}

	@Override
	public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults)
	{
		if (requestCode==PERMISSION_REQUEST_CODE && permissions.length>0) 
		{
			for (int index=0; index < grantResults.length; index++)
			{
				String permission = permissions[index];
				if (grantResults[index] == PackageManager.PERMISSION_DENIED)
				{
					boolean showRationale = ActivityCompat.shouldShowRequestPermissionRationale(SplashActivity.this, permission);
					if (showRationale)
					{
						showDialog(getResourceStringOrDefault(packageName, "PERM_Caption_PermRequired", "Permissions Required"), getRationale(permission), false);
					}
					else
					{
						showDialog(getResourceStringOrDefault(packageName, "PERM_Caption_PermRequired", "Permissions Required"), getResourceStringOrDefault(packageName, "PERM_Info_ApproveSettings", "You must approve this permission in App Settings:") + "\n\n" +
									getResourceStringOrDefault(packageName, "PERM_SettingsName_" + permission, permission), true);
					}
					return;
				}
			}

			// all permissions granted, start GameActivity
			startActivity(GameActivityIntent);
			finish();
			overridePendingTransition(0, 0);
		}
	}

	@Override
	protected void onPause()
	{
		super.onPause();
		if (!WaitForPermission)
		{
			finish();
			overridePendingTransition(0, 0);
		}
	}

}
