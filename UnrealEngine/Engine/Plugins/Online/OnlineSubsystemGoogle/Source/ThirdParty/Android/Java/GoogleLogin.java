package com.epicgames.unreal;

import android.util.Log;

import android.accounts.Account;
import android.app.Activity;
import android.content.Intent;
import android.content.IntentSender;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.content.res.Resources;
import android.os.Bundle;
import androidx.annotation.NonNull;
import android.util.Base64;

import com.google.android.gms.common.api.Result;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;

import com.google.android.gms.auth.api.signin.GoogleSignIn;
import com.google.android.gms.auth.api.signin.GoogleSignInAccount;
import com.google.android.gms.auth.api.signin.GoogleSignInClient;
import com.google.android.gms.auth.api.signin.GoogleSignInOptions;
import com.google.android.gms.auth.api.signin.GoogleSignInStatusCodes;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.tasks.OnCompleteListener;
import com.google.android.gms.tasks.Task;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

public class GoogleLogin
{
	/** Responses supported by this class */
	public static final int GOOGLE_RESPONSE_OK = 0;
	public static final int GOOGLE_RESPONSE_CANCELED = 1;
	public static final int GOOGLE_RESPONSE_ERROR = 2;
	public static final int GOOGLE_RESPONSE_DEVELOPER_ERROR = 3;

	/** Debug output tag */
	private static final String TAG = "GOOGLE";

	// Output device for log messages.
	private Logger GoogleLog;
	private Logger ActivityLog;

	/** Is this a shipping build */
	boolean bShippingBuild = false;
	/** Has init been called succesfully */
	public boolean bInitialized = false;
	/** Has onStart() been called */
	public boolean bStarted = false;

	/**
	 * Activity needed here to send the signal back when user successfully logged in.
	 */
	private GameActivity activity;

	/** Name of game package */
	private String packageName;
	/** Android key from Google API dashboard */
	private String clientId;
	/** Backend server key from Google API dashboard */
	private String serverClientId;
	/** Unique request id when using sign in activity */
	private static final int REQUEST_SIGN_IN = 9001;
	/** Google API client needed for actual sign in */
	private GoogleSignInClient mGoogleSignInClient;

	public GoogleLogin(GameActivity activity, final Logger InLog, String inPackageName, String BuildConfiguration) 
	{
		this.activity = activity;

		GoogleLog = new Logger("UE", TAG);
		ActivityLog = InLog;

		packageName = inPackageName;
		bShippingBuild = BuildConfiguration.equals("Shipping");
	} 

	public boolean init(String inClientId, String inServerClientId)
	{
		if (bShippingBuild)
		{
			GoogleLog.SuppressLogs();
		}

		boolean bClientIdValid = (inClientId != null && !inClientId.isEmpty());
		boolean bServerIdValid = (inServerClientId != null && !inServerClientId.isEmpty());
		if (bClientIdValid && bServerIdValid)
		{
			GoogleLog.debug("init");

			boolean bIsAvailable = isGooglePlayServicesAvailable();
			GoogleLog.debug("Is Google Play Services Available:" + bIsAvailable);
			if (bIsAvailable)
			{
				GoogleLog.debug("packageName: " + packageName);
				clientId = inClientId;
				GoogleLog.debug("GoogleSignIn clientId:" + clientId);
				serverClientId = inServerClientId;
				GoogleLog.debug("GoogleSignIn serverClientId:" + serverClientId);

				// Configure sign-in to request the user's ID, email address, and basic
				// profile. ID and basic profile are included in DEFAULT_SIGN_IN.
				GoogleSignInOptions gso = new GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_SIGN_IN)
						.requestIdToken(serverClientId)
						.requestProfile()
						//.requestServerAuthCode(serverClientId)
						.requestEmail()
						.build();

				// Build a GoogleSignInClient with the options specified by gso.
				mGoogleSignInClient = GoogleSignIn.getClient(activity, gso);
				
				bInitialized = true;
				PrintKeyHash(packageName);
			}
		}
		else
		{
			GoogleLog.debug("clientId: " + inClientId + " or serverClientId: " + inServerClientId + " is invalid");
		}

		GoogleLog.debug("init complete: " + bInitialized);
		return bInitialized;
	}

	public void onStart()
	{
		GoogleLog.debug("onStart");
		bStarted = true;
	}

	public void onStop()
	{
		GoogleLog.debug("onStop");
	}

	public void onDestroy()
	{
		GoogleLog.debug("onDestroy");
	}

	public int login(String[] ScopeFields)
	{
		GoogleLog.debug("login:" + ScopeFields.toString());

		int resultCode = GOOGLE_RESPONSE_ERROR;

		Intent signInIntent = mGoogleSignInClient.getSignInIntent();
		if (signInIntent != null)
		{
			GoogleLog.debug("login start activity:");
			activity.startActivityForResult(signInIntent, REQUEST_SIGN_IN);
			resultCode = GOOGLE_RESPONSE_OK;
		} 
		else 
		{
			GoogleLog.debug("getSignInIntent failure:");
			nativeLoginComplete(GOOGLE_RESPONSE_ERROR, "");
		}

		return resultCode;
	}

	public int logout()
	{
		GoogleLog.debug("logout");

		mGoogleSignInClient.signOut()
			.addOnCompleteListener(activity, new OnCompleteListener<Void>()
			{
				@Override
				public void onComplete(@NonNull Task<Void> task) 
				{
					boolean bWasSuccessful = task.isSuccessful();
					GoogleLog.debug("onSignOut Complete success:" + bWasSuccessful);
					nativeLogoutComplete(bWasSuccessful ? GOOGLE_RESPONSE_OK : GOOGLE_RESPONSE_ERROR);
				}
			});

		return GOOGLE_RESPONSE_OK;
	}

	public void onActivityResult(int requestCode, int resultCode, Intent data) 
	{
		GoogleLog.debug("onActivityResult: " + requestCode + " result: " + resultCode);
		// Result returned from launching the Intent from GoogleSignInApi.getSignInIntent(...);
		if (requestCode == REQUEST_SIGN_IN) 
		{
			GoogleLog.debug("onActivityResult REQUEST_SIGN_IN");
			GoogleLog.debug("data: " + ((data != null) ? data.toString() : "null"));

			if (resultCode == Activity.RESULT_OK)
			{
				GoogleLog.debug("signing in");
			}

			Task<GoogleSignInAccount> completedTask = GoogleSignIn.getSignedInAccountFromIntent(data);
			try
			{
				// Try to access the account result
				GoogleSignInAccount account = completedTask.getResult(ApiException.class);
				
				// Signed in successfully
				GoogleLog.debug("Sign in success");
				PrintUserAccountInfo(account);
				nativeLoginComplete(GOOGLE_RESPONSE_OK, getLoginJsonStr(account));
			}
			catch (ApiException e)
			{
				// The ApiException status code indicates the detailed failure reason.
				// Please refer to the GoogleSignInStatusCodes class reference for more information.
				GoogleLog.debug("Sign in failure:" + GoogleSignInStatusCodes.getStatusCodeString(e.getStatusCode()));
				if (e.getStatusCode() == GoogleSignInStatusCodes.DEVELOPER_ERROR)
				{
					nativeLoginComplete(GOOGLE_RESPONSE_DEVELOPER_ERROR, "");
				}
				else
				{
					nativeLoginComplete(GOOGLE_RESPONSE_ERROR, "");
				}
			}

			GoogleLog.debug("onActivityResult end");
		}
	}

	private String getLoginJsonStr(GoogleSignInAccount acct)
	{
		if (acct != null)
		{
			return "{\"user_data\":" + getUserJsonStr(acct) + "," +
					"\"auth_data\":" + getAuthTokenJsonStr(acct) + "}";
		}

		return "";
	}

	private String getUserJsonStr(GoogleSignInAccount acct)
	{
		if (acct != null)
		{
			return "{\"sub\":\""+ acct.getId() + "\"," +
					"\"given_name\":\"" + acct.getGivenName()  + "\"," +
					"\"family_name\":\"" + acct.getFamilyName() + "\"," +
					"\"name\":\"" + acct.getDisplayName() + "\"," +
					"\"picture\":\"" + acct.getPhotoUrl() + "\"" + "}";
		}
		return "";
	}

	private String getAuthTokenJsonStr(GoogleSignInAccount acct)
	{
		if (acct != null)
		{
			return "{\"access_token\":\"androidInternal\"," +
					"\"refresh_token\":\"androidInternal\"," +
					"\"id_token\":\""+ acct.getIdToken() + "\"}";
		}
		return "";
	}

	public void PrintUserAccountInfo(GoogleSignInAccount acct)
	{
		GoogleLog.debug("PrintUserAccountInfo");
		if (acct != null)
		{
			GoogleLog.debug("User Details:");
			GoogleLog.debug("    DisplayName:" + acct.getDisplayName());
			GoogleLog.debug("    Id:" + acct.getId());
			GoogleLog.debug("    Email:" + acct.getEmail());
			GoogleLog.debug("    Account:" + acct.getAccount().toString());
			GoogleLog.debug("    Scopes:" + acct.getGrantedScopes());
			GoogleLog.debug("    IdToken:" + acct.getIdToken());
			GoogleLog.debug("    ServerAuthCode:" + acct.getServerAuthCode());
		}
		else
		{
			GoogleLog.debug("Account is null");
		}
	}

	private boolean isGooglePlayServicesAvailable() 
	{
		GoogleApiAvailability apiAvail = GoogleApiAvailability.getInstance();
		int status = apiAvail.isGooglePlayServicesAvailable(activity);
		GoogleLog.debug("isGooglePlayServicesAvailable statusCode: " + status);
		if (status == ConnectionResult.SUCCESS) 
		{
			return true;
		} 
		else 
		{
			return false;
		}
	}

	public void PrintKeyHash(String packageName) 
	{
		try 
		{
			PackageInfo info = activity.getPackageManager().getPackageInfo(
					packageName,
					PackageManager.GET_SIGNATURES);
			for (Signature signature : info.signatures) 
			{
				MessageDigest md = MessageDigest.getInstance("SHA");
				md.update(signature.toByteArray());
				GoogleLog.debug(Base64.encodeToString(md.digest(), Base64.DEFAULT));
			}
		} 
		catch (PackageManager.NameNotFoundException e) 
		{
			GoogleLog.debug("NameNotFoundException:" + e);
		} 
		catch (NoSuchAlgorithmException e) 
		{
			GoogleLog.debug("NoSuchAlgorithmException:" + e);
		}
	}

	// Callback that notify the C++ implementation that a task has completed
	public native void nativeLoginComplete(int responseCode, String javaData);
	public native void nativeLogoutComplete(int responseCode);
}
