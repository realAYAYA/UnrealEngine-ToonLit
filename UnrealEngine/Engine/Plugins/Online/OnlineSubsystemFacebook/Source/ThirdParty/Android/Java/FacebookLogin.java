package com.epicgames.unreal;

//import android.os.Bundle;
import android.util.Log;

//import com.facebook.*;
import com.facebook.FacebookSdk;
import com.facebook.AccessToken;
import com.facebook.AccessTokenTracker;
import com.facebook.Profile;
import com.facebook.ProfileTracker;
import com.facebook.LoggingBehavior;
import com.facebook.CallbackManager;
import com.facebook.GraphRequest;
import com.facebook.GraphResponse;
import com.facebook.FacebookCallback;
import com.facebook.FacebookException;
import com.facebook.login.LoginManager;
import com.facebook.login.LoginResult;
import com.facebook.FacebookAuthorizationException;

import org.json.JSONArray;
import org.json.JSONObject;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
//import java.util.Set;

/**
 * https://developers.facebook.com/docs/facebook-login/android/
 */
public class FacebookLogin 
{
	/** Responses supported by this class */
    public static final int FACEBOOK_RESPONSE_OK = 0;
    public static final int FACEBOOK_RESPONSE_CANCELED = 1;
	public static final int FACEBOOK_RESPONSE_PENDING = 2;
	public static final int FACEBOOK_RESPONSE_ERROR = 3;
	
	// Output device for log messages.
	private Logger FBLog;
	private Logger ActivityLog;

    /**
     * Activity needed for various Facebook context calls
     */
    private GameActivity activity;

    /**
     * CallbackManager manages the callbacks into the FacebookSdk 
     * from an Activity's onActivityResult() method.
     * https://developers.facebook.com/docs/reference/android/current/interface/CallbackManager/
     */
    private CallbackManager callbackManager;

	/**
     * Tracks access token changes (login/logout/etc)
	 * https://developers.facebook.com/docs/reference/android/current/class/AccessTokenTracker/
     */
    private AccessTokenTracker tokenTracker;

	/** 
	 * Tracks changes to the profile 
	 * https://developers.facebook.com/docs/reference/android/current/class/ProfileTracker/
	 */
    private ProfileTracker profileTracker;

    /**
     * CallbackManager is exposed here to so that onActivityResult() can be called from GameActivity
     * when required.
     */
    public CallbackManager getCallbackManager() { return callbackManager; }

	private abstract class NativeLoginCallback implements FacebookCallback<LoginResult>
    {
    	abstract void invoke(int responseCode, String accessToken, String[] grantedPermissions, String[] declinedPermissions);

		@Override
		public void onSuccess(LoginResult loginResult) 
		{
			FBLog.debug("[JAVA] NativeLoginCallback.onSuccess " + loginResult);
			AccessToken Token = AccessToken.getCurrentAccessToken();
			printTokenDetails(Token);
			invoke(FACEBOOK_RESPONSE_OK, Token.getToken(), Token.getPermissions().toArray(new String[0]), Token.getDeclinedPermissions().toArray(new String[0]));
		}

		@Override
		public void onCancel() 
		{
			FBLog.debug("[JAVA] NativeLoginCallback.onCancel");
			AccessToken Token = AccessToken.getCurrentAccessToken();
			printTokenDetails(Token);
			invoke(FACEBOOK_RESPONSE_CANCELED, "", null, null);
		}

		@Override
		public void onError(FacebookException exception) 
		{
			FBLog.debug("[JAVA] NativeLoginCallback.onError " + exception);
			// ERR_NAME_NOT_RESOLVED - not connected to internet
			invoke(FACEBOOK_RESPONSE_ERROR, "", null, null);
		}
    }
	/** Constructor */
    public FacebookLogin(GameActivity activity, final Logger InLog) 
	{
        this.activity = activity;

		FBLog = new Logger("UE", "FB");
		ActivityLog = InLog;
    } 

    /**
     * FacebookSDK is auto initialized (FacebookSdk.sdkInitialize() is deprecated)
	 * Hook into the various SDK features
     * https://developers.facebook.com/docs/reference/android/current/interface/FacebookCallback/
     */
    public boolean init(String BuildConfiguration, boolean bEnableAppEvents, boolean bEnableAdId) 
	{
		boolean bShippingBuild = BuildConfiguration.equals("Shipping");

		if (bShippingBuild)
		{
			FBLog.SuppressLogs();
		}

		FBLog.debug("Facebook::Init()");
		boolean bIsFacebookEnabled = FacebookSdk.isInitialized();
		if (bIsFacebookEnabled)
		{
			// Enable various SDK debug verbosity
			if (!bShippingBuild)
			{
				FacebookSdk.addLoggingBehavior(LoggingBehavior.REQUESTS);
				FacebookSdk.addLoggingBehavior(LoggingBehavior.DEVELOPER_ERRORS);
				//FacebookSdk.addLoggingBehavior(LoggingBehavior.APP_EVENTS);
				//FacebookSdk.addLoggingBehavior(LoggingBehavior.CACHE);
				FacebookSdk.addLoggingBehavior(LoggingBehavior.GRAPH_API_DEBUG_WARNING);
				FacebookSdk.addLoggingBehavior(LoggingBehavior.GRAPH_API_DEBUG_INFO);
				//Set<LoggingBehavior> Behaviors = FacebookSdk.getLoggingBehaviors();

				FacebookSdk.setIsDebugEnabled(true);
			}
			boolean bIsDebugEnabled = FacebookSdk.isDebugEnabled();

			String APIVersion = FacebookSdk.getGraphApiVersion();
			String SDKVersion = FacebookSdk.getSdkVersion();
			String AppId = FacebookSdk.getApplicationId();
			FBLog.debug("Facebook SDK v" + SDKVersion);
			FBLog.debug(" API: " + APIVersion + " AppId: " + AppId + " Enabled: " + bIsFacebookEnabled + " Debug: " + bIsDebugEnabled);

			FacebookSdk.setAutoLogAppEventsEnabled(bEnableAppEvents);
			FacebookSdk.setAdvertiserIDCollectionEnabled(bEnableAdId);

			callbackManager = CallbackManager.Factory.create();

			profileTracker = new ProfileTracker() 
			{
				@Override
				protected void onCurrentProfileChanged(Profile oldProfile, Profile currentProfile) 
				{
					FBLog.debug("[JAVA] CurrentProfileChange Old: " + oldProfile + " New:" + currentProfile);
					Profile profile = Profile.getCurrentProfile();
					printProfileDetails(profile);
				}
			};

			tokenTracker = new AccessTokenTracker() 
			{
				@Override
				protected void onCurrentAccessTokenChanged(AccessToken oldAccessToken,
														   AccessToken currentAccessToken) 
				{
					FBLog.debug("[JAVA] AccessTokenChange Old: " + oldAccessToken + " New:" + currentAccessToken);
					printTokenDetails(currentAccessToken);
				}
			};

			if (isAccessTokenValid())
			{
				AccessToken Token = AccessToken.getCurrentAccessToken();
				printTokenDetails(Token);
			}

			FBLog.debug(" AT:" + tokenTracker.isTracking() + " PT:" + profileTracker.isTracking());
		}
		else
		{
			FBLog.debug("Facebook failed init, check your client id DefaultEngine.ini [OnlineSubsystemFacebook.OnlineIdentityFacebook]");
		}

		return bIsFacebookEnabled;
    }

    /**
     * Called when GameActivity resumes. Ensures tokenTracker tracks token changes
     */
    public void activate() 
	{
        FBLog.debug("[JAVA] Facebook activate");
        profileTracker.startTracking();
        tokenTracker.startTracking();

        FBLog.debug("AT:" + tokenTracker.isTracking() + " PT:" + profileTracker.isTracking());
    }

    /**
     * Called when GameActivity is paused. Ensures tokenTracker stops tracking
     */
    public void deactivate() 
	{
        FBLog.debug("[JAVA] Facebook deactivate");
        tokenTracker.stopTracking();
        profileTracker.stopTracking();

        FBLog.debug("AT:" + tokenTracker.isTracking() + " PT:" + profileTracker.isTracking());
    }

    public void login(String[] ScopeFields) 
	{
		FBLog.debug("[JAVA] Facebook login");

		boolean bNeedsLogin = !areAllPermissionsGranted(ScopeFields); 
		if (bNeedsLogin) 
		{
			FBLog.debug("Login required");
			LoginManager loginManager = LoginManager.getInstance();
			loginManager.registerCallback(callbackManager, new NativeLoginCallback()
			{
				@Override
				public void invoke(int responseCode, String accessToken, String[] grantedPermissions, String[] declinedPermissions)
				{
					nativeLoginComplete(responseCode, accessToken, grantedPermissions, declinedPermissions);
				} 
			});
			loginManager.logInWithReadPermissions(
					this.activity,
					Arrays.asList(ScopeFields));
		}
		else
		{
			AccessToken Token = AccessToken.getCurrentAccessToken();
			printTokenDetails(Token);
			nativeLoginComplete(FACEBOOK_RESPONSE_OK, Token.getToken(), Token.getPermissions().toArray(new String[0]), Token.getDeclinedPermissions().toArray(new String[0]));
		}
    }

    public void logout()
	{
		FBLog.debug("[JAVA] Facebook logout");
		if (isAccessTokenValid()) {
			LoginManager.getInstance().logOut();
			nativeLogoutComplete(FACEBOOK_RESPONSE_OK);
		}
		else
		{
			FBLog.debug("No logout required");
			nativeLogoutComplete(FACEBOOK_RESPONSE_OK);
		}
    }

    /**
     * Request additional permissions when needed
     * See https://developers.facebook.com/docs/facebook-login/android/permissions for more info on permissions
     */
    public void requestReadPermissions(String[] inPermissions) 
	{
        if (!areAllPermissionsGranted(inPermissions)) 
		{
			Collection<String> permissions = new ArrayList<String>(Arrays.asList(inPermissions));

			LoginManager loginManager = LoginManager.getInstance();
			loginManager.registerCallback(callbackManager, new NativeLoginCallback()
			{
				@Override
				public void invoke(int responseCode, String accessToken, String[] grantedPermissions, String[] declinedPermissions)
				{
					nativeRequestReadPermissionsComplete(responseCode, accessToken, grantedPermissions, declinedPermissions);
				} 
			});
            loginManager.getInstance().logInWithReadPermissions(this.activity, permissions);
        }
		else
		{
			AccessToken Token = AccessToken.getCurrentAccessToken();
			if (accessTokenValid(Token))
			{
				nativeRequestReadPermissionsComplete(FACEBOOK_RESPONSE_OK, getAccessToken(), Token.getPermissions().toArray(new String[0]), Token.getDeclinedPermissions().toArray(new String[0]));
			}
			else
			{
				nativeRequestReadPermissionsComplete(FACEBOOK_RESPONSE_OK, "", null, null);
			}
		}
    }

	 public void requestPublishPermissions(String[] inPermissions) 
	 {
        if (!areAllPermissionsGranted(inPermissions))
		{
            Collection<String> permissions = new ArrayList<String>(Arrays.asList(inPermissions));

			LoginManager loginManager = LoginManager.getInstance();
			loginManager.registerCallback(callbackManager, new NativeLoginCallback()
			{
				@Override
				public void invoke(int responseCode, String accessToken, String[] grantedPermissions, String[] declinedPermissions)
				{
					nativeRequestPublishPermissionsComplete(responseCode, accessToken, grantedPermissions, declinedPermissions);
				} 
			});
			loginManager.logInWithPublishPermissions(this.activity, permissions);
        }
		else
		{
			AccessToken Token = AccessToken.getCurrentAccessToken();
			if (accessTokenValid(Token))
			{
				nativeRequestPublishPermissionsComplete(FACEBOOK_RESPONSE_OK, getAccessToken(), Token.getPermissions().toArray(new String[0]), Token.getDeclinedPermissions().toArray(new String[0]));
			}
			else
			{
				nativeRequestPublishPermissionsComplete(FACEBOOK_RESPONSE_OK, "", null, null);
			}
		}
    }

	public String getAccessToken()
	{
		FBLog.debug("[JAVA] getAccessToken");
		AccessToken token = AccessToken.getCurrentAccessToken();
		if (accessTokenValid(token))
		{
			return token.getToken();
		}

		return "";
	}

    /**
     * Checks if user is logged in and access token hasn't expired.
     */
    public static boolean isAccessTokenValid() 
	{
        return accessTokenValid(AccessToken.getCurrentAccessToken());
    }

    /**
     * Checks if user has granted particular permission to the app
     * See https://developers.facebook.com/docs/facebook-login/android/permissions
     */
    public static boolean isPermissionGranted(String permission) 
	{
        return tokenHasPermission(AccessToken.getCurrentAccessToken(), permission);
    }

	public static boolean areAllPermissionsGranted(String[] inPermissions)
	{
		boolean allPermissionsGranted = false;

		AccessToken currentToken = AccessToken.getCurrentAccessToken();
		if (accessTokenValid(currentToken))
		{
			allPermissionsGranted = true;
			for (String permission : inPermissions)
			{
				if (!currentToken.getPermissions().contains(permission))
				{
					allPermissionsGranted = false;
					break;
				}
			}
		}

		return allPermissionsGranted;
	}

    /**
     * Checks if the given access token is valid
     */
    public static boolean accessTokenValid(AccessToken token) 
	{
        return token != null && !token.isExpired();
    }

    /**
     * Checks if the given access token has permission
     */
    public static boolean tokenHasPermission(AccessToken token, String permission) 
	{
        return accessTokenValid(token) && token.getPermissions().contains(permission);
    }

	public void printTokenDetails(AccessToken token)
	{
		FBLog.debug("Facebook Token Details: ");
		if (token != null)
        {
			FBLog.debug("UserId: " + token.getUserId() + " Token:" + token.getToken() + " Expires: " + token.getExpires());
			printPermissions(token);
		}
		else
		{
			FBLog.debug("No Access Token!!!");
		}
	}

    public void printPermissions(AccessToken token)
    {
        if (token != null)
        {
            FBLog.debug("Permissions: " + token.getPermissions().toString());
            FBLog.debug("Declined: " + token.getDeclinedPermissions().toString());
        }
        else
        {
            FBLog.debug("No Permissions!!!");
        }
    }

	public void printProfileDetails(Profile profile)
	{
		FBLog.debug("Facebook Profile Details: ");
		if (profile != null) 
		{
			FBLog.debug("Name: " + profile.getName() + " Link: " + profile.getLinkUri());
		}
		else 
		{
			FBLog.debug("No profile");
		}
	}
	
	// Callback that notify the C++ implementation that a task has completed
	public native void nativeLoginComplete(int responseCode, String accessToken, String[] grantedPermissions, String[] declinedPermissions);
	public native void nativeRequestReadPermissionsComplete(int responseCode, String accessToken, String[] grantedPermissions, String[] declinedPermissions);
	public native void nativeRequestPublishPermissionsComplete(int responseCode, String accessToken, String[] grantedPermissions, String[] declinedPermissions);
	public native void nativeLogoutComplete(int responseCode);
}