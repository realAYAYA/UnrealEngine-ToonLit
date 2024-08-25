package com.epicgames.unreal;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.util.Base64;

import com.google.android.gms.auth.api.signin.GoogleSignIn;
import com.google.android.gms.auth.api.signin.GoogleSignInAccount;
import com.google.android.gms.auth.api.signin.GoogleSignInClient;
import com.google.android.gms.auth.api.signin.GoogleSignInOptions;
import com.google.android.gms.auth.api.signin.GoogleSignInStatusCodes;
import com.google.android.gms.common.api.ApiException;
import com.google.android.gms.common.api.Scope;
import com.google.android.gms.common.ConnectionResult;
import com.google.android.gms.common.GoogleApiAvailability;
import com.google.android.gms.tasks.Task;

import java.security.MessageDigest;

public class GoogleLogin
{
	// Responses supported by this class
	public static final int GOOGLE_RESPONSE_OK = 0;
	public static final int GOOGLE_RESPONSE_CANCELED = 1;
	public static final int GOOGLE_RESPONSE_ERROR = 2;
	public static final int GOOGLE_RESPONSE_DEVELOPER_ERROR = 3;

	// Debug output tag
	private static final String TAG = "GOOGLE";

	// Output device for log messages.
	private final Logger GoogleLog;

	// Should request Id Token?
	boolean bRequestIdToken;

	// Should request server auth code?
	boolean bRequestServerAuthCode;

	// Oauth 2.0 client id for use in server to server communication(Obtained from Google Cloud console)
	private String ServerClientId;

	// Owning activity
	Activity Activity;

	public GoogleLogin(Activity InActivity)
	{
		this.Activity = InActivity;
		GoogleLog = new Logger("UE", TAG);
	}

	public boolean Init(String InServerClientId, boolean bInRequestIdToken, boolean bInRequestServerAuthCode)
	{
		boolean bInitialized = false;

		boolean bIsAvailable = GoogleApiAvailability.getInstance().isGooglePlayServicesAvailable(Activity) == ConnectionResult.SUCCESS;
		GoogleLog.debug("Is Google Play Services Available:" + bIsAvailable);
		if (bIsAvailable)
		{
			if (InServerClientId == null && (bInRequestIdToken || bInRequestServerAuthCode))
			{
				GoogleLog.warn("Expected non empty ServerClientId to request id token and/or server auth code. Id token and/or server auth code won't be requested");
			}
			else
			{
				ServerClientId = InServerClientId;
				bRequestIdToken = bInRequestIdToken;
				bRequestServerAuthCode = bInRequestServerAuthCode;
			}
			bInitialized = true;
		}
		return bInitialized;
	}

	public void Login(String[] ScopeFields)
	{
		// Configure sign-in to request the user's ID, email address, and basic
		// profile. ID and basic profile are included in DEFAULT_SIGN_IN.
		GoogleSignInOptions.Builder Builder = new GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_SIGN_IN)
			.requestEmail()
			.requestProfile();

		if(bRequestServerAuthCode)
		{
			Builder.requestServerAuthCode(ServerClientId);
		}

		if(bRequestIdToken)
		{
			Builder.requestIdToken(ServerClientId);
		}

		for (String RequiredScope: ScopeFields)
		{
			Builder.requestScopes(new Scope(RequiredScope));
		}

		GoogleSignInClient GoogleSignInClient = GoogleSignIn.getClient(Activity, Builder.build());

		GoogleLog.debug("Attempting silent sign in");
		Task<GoogleSignInAccount> Task = GoogleSignInClient.silentSignIn();
		if (Task.isSuccessful())
		{
			GoogleSignInAccount Account = Task.getResult();
			SignInSucceeded(Account);
		}
		else
		{
			Task.addOnCompleteListener(CompletedTask -> {
				try
				{
					GoogleSignInAccount Account = CompletedTask.getResult(ApiException.class);
					SignInSucceeded(Account);
				}
				catch (ApiException ApiException)
				{
					if (ApiException.getStatusCode() == GoogleSignInStatusCodes.SIGN_IN_REQUIRED)
					{
						Intent SignInIntent = GoogleSignInClient.getSignInIntent();
						GoogleLog.debug("Start login intent");
						Activity.startActivityForResult(SignInIntent, GameActivity.REQUEST_CODE_OSSGOOGLE_LOGIN);
					}
					else
					{
						SignInFailed(ApiException);
					}
				}
			});
		}
	}

	public void Logout()
	{
		GoogleLog.debug("Logout started");

		GoogleSignInOptions SignInOptions = new GoogleSignInOptions.Builder(GoogleSignInOptions.DEFAULT_SIGN_IN)
			.build();

		GoogleSignInClient GoogleSignInClient = GoogleSignIn.getClient(Activity, SignInOptions);

		GoogleSignInClient.signOut()
			.addOnCompleteListener(Task -> {
				boolean bWasSuccessful = Task.isSuccessful();
				GoogleLog.debug("Logout complete success: " + bWasSuccessful);
				nativeLogoutComplete(bWasSuccessful ? GOOGLE_RESPONSE_OK : GOOGLE_RESPONSE_ERROR);
			});
	}

	public void onActivityResult(int ResultCode, Intent Data)
	{
		// Result returned from launching the Intent from GoogleSignInApi.getSignInIntent(...);
		GoogleLog.debug("onActivityResult: Result " + ResultCode + " Data: " + ((Data != null) ? Data.toString() : "null"));

		Task<GoogleSignInAccount> CompletedTask = GoogleSignIn.getSignedInAccountFromIntent(Data);
		try
		{
			GoogleSignInAccount Account = CompletedTask.getResult(ApiException.class);
			SignInSucceeded(Account);
		}
		catch (ApiException apiException)
		{
			SignInFailed(apiException);
		}

		GoogleLog.debug("onActivityResult end");
	}

	private void SignInSucceeded(GoogleSignInAccount Account)
	{
		GoogleLog.debug("Sign in succeeded");
		PrintUserAccountInfo(Account);

		String PhotoUrl = null;
		if (Account.getPhotoUrl() != null)
		{
			PhotoUrl = Account.getPhotoUrl().toString();
		}
		nativeLoginSuccess(Account.getId(), Account.getGivenName(), Account.getFamilyName(), Account.getDisplayName(), PhotoUrl, Account.getIdToken(), Account.getServerAuthCode());
	}

	private void SignInFailed(ApiException ApiException)
	{
		int ErrorCode = ApiException.getStatusCode();

		GoogleLog.debug("Sign in failed: ErrorCode: " + GoogleSignInStatusCodes.getStatusCodeString(ErrorCode) + "ErrorMessage: " + ApiException.getMessage());

		switch (ErrorCode )
		{
			case GoogleSignInStatusCodes.DEVELOPER_ERROR:
			{
				LogDeveloperError();
				nativeLoginFailed(GOOGLE_RESPONSE_DEVELOPER_ERROR);
				break;
			}
			case GoogleSignInStatusCodes.CANCELED:
				nativeLoginFailed(GOOGLE_RESPONSE_CANCELED);
				break;
			default:
			{
				nativeLoginFailed(GOOGLE_RESPONSE_ERROR);
				break;
			}
		}
	}

	private void PrintUserAccountInfo(GoogleSignInAccount Acct)
	{
		if (Acct != null)
		{
			GoogleLog.debug("User Details:");
			GoogleLog.debug("    DisplayName:" + Acct.getDisplayName());
			GoogleLog.debug("    Id:" + Acct.getId());
			GoogleLog.debug("    Email:" + Acct.getEmail());
			GoogleLog.debug("    Scopes:" + Acct.getGrantedScopes());
			GoogleLog.debug("    IdToken:" + Acct.getIdToken());
			GoogleLog.debug("    ServerAuthCode:" + Acct.getServerAuthCode());
		}
		else
		{
			GoogleLog.debug("Account is null");
		}
	}

	private void LogDeveloperError()
	{
		GoogleLog.error("Sign in failed with DEVELOPER_ERROR status code. Is the apk signed with the certificate expected by the Oauth 2.0 client id associated to your app in Google Cloud Console?");
		String PackageName = Activity.getPackageName();
		GoogleLog.error("PackageName: " + PackageName);
		try
		{
			PackageInfo Info = Activity.getPackageManager().getPackageInfo(PackageName, PackageManager.GET_SIGNATURES);
			for (Signature Signature : Info.signatures)
			{
				MessageDigest Digest = MessageDigest.getInstance("SHA");
				Digest.update(Signature.toByteArray());
				GoogleLog.error("Signing certificate signature: " + Base64.encodeToString(Digest.digest(), Base64.DEFAULT));
			}
		}
		catch (Exception Ex)
		{
			GoogleLog.error("Could not get signature:" + Ex);
		}
	}

	private native void nativeLoginSuccess(String InUserId, String InGivenName, String InFamilyName, String InDisplayName, String InPhotoUrl, String InIdToken, String InServerAuthCode);
	private native void nativeLoginFailed(int ResponseCode);
	private native void nativeLogoutComplete(int ResponseCode);
}
