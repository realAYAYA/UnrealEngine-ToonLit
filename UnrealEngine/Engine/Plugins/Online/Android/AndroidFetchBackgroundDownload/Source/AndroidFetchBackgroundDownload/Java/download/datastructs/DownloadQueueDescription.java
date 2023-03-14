// Copyright Epic Games, Inc. All Rights Reserved.
package com.epicgames.unreal.download.datastructs;

import android.content.Context;
import android.util.Log;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.work.Data;
import androidx.work.WorkerParameters;

import com.epicgames.unreal.Logger;

import com.epicgames.unreal.download.datastructs.DownloadDescription;
import com.epicgames.unreal.download.datastructs.DownloadWorkerParameterKeys;
import com.epicgames.unreal.download.DownloadProgressListener;

import org.json.JSONArray;
import org.json.JSONException;

import java.util.ArrayList;
import java.util.List;


//Helper class that is used to serialize any information needed for our downloads as a whole from/to WorkerParameters
public class DownloadQueueDescription
{
	//Constructor that parses the List<DownloadDescription> from our Worker InputData (WorkerParameters)
	public DownloadQueueDescription(@NonNull Data data, @NonNull Context context, @Nullable Logger Log)
	{		
		//Parse DownloadDescriptions 
		String DownloadDescriptionListFileName = GetDownloadDescriptionListFileName(data, Log);
		if (DownloadDescriptionListFileName != null)
		{
			ParseDownloadDescriptionString(DownloadDescriptionListFileName, Log);
		}
		
		MaxConcurrentDownloads = data.getInt(DownloadWorkerParameterKeys.DOWNLOAD_MAX_CONCURRENT_REQUESTS_KEY, DEFAULT_MAX_CONCURRENT_DOWNLOADS);
	}

	public static String GetDownloadDescriptionListFileName(@NonNull Data data, @Nullable Logger Log)
	{
		//Parse DownloadDescriptions 
		String DownloadDescriptionListString = data.getString(DownloadWorkerParameterKeys.DOWNLOAD_DESCRIPTION_LIST_KEY);
		if (null == DownloadDescriptionListString)
		{
			if (null != Log)
			{
				Log.error(DownloadWorkerParameterKeys.NOTIFICATION_CHANNEL_ID_KEY + " key returned null list! No downloads to process in WorkerParameters!");
			}
		}

		return DownloadDescriptionListString;
	}

	//Parses our DownloadDescriptions member based on the passed in JSONObject file
	private void ParseDownloadDescriptionString(@NonNull String DownloadDescriptionListStringFileName, @Nullable Logger Log)
	{
		DownloadDescriptions = new ArrayList<DownloadDescription>();
		
		try 
		{
			String JSONInFile = DownloadDescription.GetDownloadDescriptionJSONArrayFromFile(DownloadDescriptionListStringFileName);
			
			if (null != JSONInFile) 
			{
				JSONArray DescriptionJsonArray = new JSONArray(JSONInFile);

				for (int JsonIndex = 0; JsonIndex < DescriptionJsonArray.length(); ++JsonIndex) 
				{
					String DescriptionJsonString = DescriptionJsonArray.optString(JsonIndex);
					DownloadDescription ParsedDescription = DownloadDescription.FromJSON(DescriptionJsonString);
					DownloadDescriptions.add(ParsedDescription);
				}
			}
			else
			{
				Log.error("Failure loading JSON from file " + DownloadDescriptionListStringFileName);
			}
		}
		catch (JSONException e) 
		{
			e.printStackTrace();
			
			if (null != Log)
			{
				Log.error("Exception while parsing download descriptions: " + e.getMessage());
			}			
		}
	}

	public ArrayList<DownloadDescription> DownloadDescriptions = new ArrayList<DownloadDescription>();
	public DownloadProgressListener ProgressListener = null;
	public int DownloadGroupID = 0;
	public int MaxConcurrentDownloads = 4;
	
	private int DEFAULT_MAX_CONCURRENT_DOWNLOADS = 4;
}