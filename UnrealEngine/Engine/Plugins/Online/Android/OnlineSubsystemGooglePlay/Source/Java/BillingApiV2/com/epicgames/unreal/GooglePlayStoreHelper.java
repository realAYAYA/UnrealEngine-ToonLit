// Copyright Epic Games, Inc. All Rights Reserved.

package com.epicgames.unreal;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.ConsumeParams;
import com.android.billingclient.api.ConsumeResponseListener;
import com.android.billingclient.api.ProductDetails;
import com.android.billingclient.api.ProductDetailsResponseListener;
import com.android.billingclient.api.Purchase;
import com.android.billingclient.api.PurchasesResponseListener;
import com.android.billingclient.api.PurchasesUpdatedListener;
import com.android.billingclient.api.QueryProductDetailsParams;
import com.android.billingclient.api.QueryPurchasesParams;
import com.android.vending.billing.util.Base64; //WMM should use different base64 here.

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

public class GooglePlayStoreHelper implements StoreHelper, PurchasesUpdatedListener
{
	// Our IAB helper interface provided by google.
	private final BillingClient Client;

	// Flag that determines whether the store is ready to use.
	private boolean bIsIapSetup = false;

	// Output device for log messages.
	private final Logger Log;

	// Cache access to the games activity.
	private final GameActivity MainActivity;

	// Custom error value. Should match with EGooglePlayBillingResponseCode::CustomLogicError
	private final int CustomLogicErrorResponse = -127;

	public GooglePlayStoreHelper(GameActivity InGameActivity, Logger InLog)
	{
		Log = InLog;
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::GooglePlayStoreHelper");

		MainActivity = InGameActivity;

		Client = BillingClient.newBuilder(MainActivity)
			.setListener(this)
			.enablePendingPurchases()
			.build();

		Client.startConnection(new BillingClientStateListener() {
			@Override
			public void onBillingSetupFinished(@NonNull BillingResult billingResult) {
				if (billingResult.getResponseCode() == BillingClient.BillingResponseCode.OK) 
				{
					bIsIapSetup = true;
					Log.debug("[GooglePlayStoreHelper] - In-app billing supported for " + MainActivity.getPackageName());
				}
				else
				{
					Log.debug("[GooglePlayStoreHelper] - In-app billing NOT supported for " + MainActivity.getPackageName() + " error " + TranslateBillingResult(billingResult));
				}
			}

			@Override
			public void onBillingServiceDisconnected() {
				// Try to restart the connection on the next request to
				// Google Play by calling the startConnection() method.
				bIsIapSetup = false;
			}
		});
	}

	///////////////////////////////////////////////////////
	// The StoreHelper interfaces implementation for Google Play Store.

	/**
	 * Determine whether the store is ready for purchases.
	 */
	@Override
	public boolean IsAllowedToMakePurchases()
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::IsAllowedToMakePurchases");
		return bIsIapSetup;
	}

	/**
	 * Query product details for the provided product ids
	 */
	@Override
	public boolean QueryInAppPurchases(String[] InProductIDs)
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases");
		Log.debug("[GooglePlayStoreHelper] - NumProducts: " + InProductIDs.length);
		Log.debug("[GooglePlayStoreHelper] - Products: " + Arrays.toString(InProductIDs));

		if (InProductIDs.length > 0)
		{
			QueryProductDetailsParams Params = CreateQueryProductDetailsParamsForProducts(InProductIDs);
			
			Client.queryProductDetailsAsync(Params, new ProductDetailsResponseListener() {
				@Override
				public void onProductDetailsResponse(@NonNull BillingResult Result, @NonNull List<ProductDetails> ProductDetailsList)
				{
					int ResponseCode = Result.getResponseCode();

					if(ResponseCode == BillingClient.BillingResponseCode.OK)
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - ProductDetails:" + ProductDetailsList.toString());
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases " + ProductDetailsList.size() + " items - Success!");

						ListOfProductDetailsAsArrays DetailArrays = new ListOfProductDetailsAsArrays(ProductDetailsList);
						NativeQueryComplete(BillingClient.BillingResponseCode.OK, DetailArrays.ProductIds, DetailArrays.Titles, DetailArrays.Descriptions, DetailArrays.Prices, DetailArrays.PricesRaw, DetailArrays.CurrencyCodes);
					}
					else
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Failed with: " + TranslateBillingResult(Result));
						NativeQueryComplete(ResponseCode, null, null, null, null, null, null);
					}

					Log.debug("[GooglePlayStoreHelper] - NativeQueryComplete done!");
				}
			});
		}
		else
		{
			Log.debug("[GooglePlayStoreHelper] - no products given to query");
			NativeQueryComplete(CustomLogicErrorResponse, null, null, null, null, null, null);
			return false;
		}

		return true;
	}

	/**
	 * Start the purchase flow for a particular product id
	 */
	@Override
	public boolean BeginPurchase(final String ProductId, final String ObfuscatedAccountId)
	{
		QueryProductDetailsParams Params = CreateQueryProductDetailsParamsForProduct(ProductId);

		Client.queryProductDetailsAsync(Params, new ProductDetailsResponseListener() {
			@Override
			public void onProductDetailsResponse(@NonNull BillingResult Result, @NonNull List<ProductDetails> ProductDetailsList)
			{
				if(Result.getResponseCode() == BillingClient.BillingResponseCode.OK)
				{
					final ProductDetails DetailsForProductId = FindProductDetailsInList(ProductId, ProductDetailsList);
					if(DetailsForProductId != null)
					{
						// BillingClient.launchBillingFlow is annotated @UiThread, so dispatch the call to the Activity UI thread
						MainActivity.runOnUiThread(new Runnable()
						{
							@Override
							public void run() {
								Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Launching billing flow " + ProductId);
								BillingFlowParams Params = CreateBillingFlowParams(DetailsForProductId, ObfuscatedAccountId);
								BillingResult Result = Client.launchBillingFlow(MainActivity, Params);
								int ResponseCode = Result.getResponseCode();
								if (ResponseCode != BillingClient.BillingResponseCode.OK) 
								{
									Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Failed! " + TranslateBillingResult(Result));
									NativePurchaseComplete(ResponseCode, ProductId, Purchase.PurchaseState.UNSPECIFIED_STATE, "", "", "");
								}
							}
						});
					}
					else
					{
						Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Failed! Could not find ProductDetails for " + ProductId);
						NativePurchaseComplete(CustomLogicErrorResponse, ProductId, Purchase.PurchaseState.UNSPECIFIED_STATE,"", "", "");
					}
				}
				else
				{
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::BeginPurchase - Failed! " + TranslateBillingResult(Result));
					NativePurchaseComplete(Result.getResponseCode(), ProductId, Purchase.PurchaseState.UNSPECIFIED_STATE,"", "", "");
				}
			}
		});
		return true;
	}

	@Override
	public boolean QueryExistingPurchases()
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryExistingPurchases");

		QueryPurchasesParams Params =  QueryPurchasesParams.newBuilder()
			.setProductType(BillingClient.ProductType.INAPP)
			.build();
		
		Client.queryPurchasesAsync(Params, new PurchasesResponseListener() {
			@Override
			public void onQueryPurchasesResponse(@NonNull BillingResult Result, @NonNull List<Purchase> OwnedItems) 
			{
				int ResponseCode = Result.getResponseCode();
				if (ResponseCode == BillingClient.BillingResponseCode.OK) 
				{
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryExistingPurchases - User has previously purchased " + OwnedItems.size() + " inapp products");
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryExistingPurchases - Success!");

					ListOfPurchasesAsArrays PurchasesArrays = new ListOfPurchasesAsArrays(OwnedItems);
					NativeQueryExistingPurchasesComplete(ResponseCode, PurchasesArrays.OwnedProducts, PurchasesArrays.PurchaseStates, PurchasesArrays.ProductTokens, PurchasesArrays.Receipts, PurchasesArrays.Signatures);
				} 
				else 
				{
					Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryExistingPurchases - Failed to collect existing purchases" + TranslateBillingResult(Result));
					NativeQueryExistingPurchasesComplete(ResponseCode, null, null, null, null, null);
				}
			}
		});
		return true;
	}

	@Override
	public void ConsumePurchase(final String PurchaseToken)
	{
		Log.debug("[GooglePlayStoreHelper] - Beginning ConsumePurchase: " + PurchaseToken);

		ConsumeParams Params = ConsumeParams.newBuilder()
			.setPurchaseToken(PurchaseToken)
			.build();
		Client.consumeAsync(Params, new ConsumeResponseListener() {
			@Override
			public void onConsumeResponse(@NonNull BillingResult Result, @NonNull String PurchaseToken) 
			{
				Log.debug("[GooglePlayStoreHelper] - ConsumePurchase response: " + TranslateBillingResult(Result));
				NativeConsumeComplete(Result.getResponseCode(), PurchaseToken);
			}
		});
	}

	///////////////////////////////////////////////////////
	// The Google's PurchasesUpdatedListener interface implementation.

	@Override
	public void onPurchasesUpdated(@NonNull BillingResult Result, @Nullable List<Purchase> UpdatedPurchases){
		int responseCode = Result.getResponseCode();
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onPurchasesUpdated - Processing purchase result. Response Code: " + TranslateBillingResult(Result));
		if (responseCode == BillingClient.BillingResponseCode.OK && UpdatedPurchases != null) 
		{
			for (final Purchase UpdatedPurchase : UpdatedPurchases) 
			{
				final String ProductId = UpdatedPurchase.getProducts().get(0);
				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onPurchasesUpdated - Processing purchase of product " + ProductId);
				Log.debug("[GooglePlayStoreHelper] - Purchase data: " + UpdatedPurchase.toString());
				Log.debug("[GooglePlayStoreHelper] - Data signature: " + UpdatedPurchase.getSignature());

				String Receipt = Base64.encode(UpdatedPurchase.getOriginalJson().getBytes());
				NativePurchaseComplete(BillingClient.BillingResponseCode.OK, ProductId, UpdatedPurchase.getPurchaseState(), UpdatedPurchase.getPurchaseToken(), Receipt, UpdatedPurchase.getSignature());
			}
		}
		else
		{
			Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onPurchasesUpdated - Purchase failed. Response code: " + TranslateBillingResult(Result));
 			NativePurchaseComplete(responseCode, "", Purchase.PurchaseState.UNSPECIFIED_STATE, "", "", "");
		}
	}

	///////////////////////////////////////////////////////
	// Some helpers

	class ListOfProductDetailsAsArrays
	{
		public String[] ProductIds;
		public String[] Titles;
		public String[] Descriptions;
		public String[] Prices;
		public float[] PricesRaw;
		public String[] CurrencyCodes;

		public ListOfProductDetailsAsArrays(List<ProductDetails> InProductDetailsList)
		{
			ProductIds = new String[InProductDetailsList.size()];
			Titles = new String[InProductDetailsList.size()];
			Descriptions = new String[InProductDetailsList.size()];
			Prices = new String[InProductDetailsList.size()];
			PricesRaw = new float[InProductDetailsList.size()];
			CurrencyCodes = new String[InProductDetailsList.size()];

			for (int index = 0; index < InProductDetailsList.size(); index++) 
			{
				ProductDetails ThisProduct = InProductDetailsList.get(index);
				ProductDetails.OneTimePurchaseOfferDetails PriceDetails = ThisProduct.getOneTimePurchaseOfferDetails();

				Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::QueryInAppPurchases - Parsing details for: " + ThisProduct.getProductId());
				Log.debug("[GooglePlayStoreHelper] - title: " + ThisProduct.getTitle());
				Log.debug("[GooglePlayStoreHelper] - description: " + ThisProduct.getDescription());
				Log.debug("[GooglePlayStoreHelper] - price: " + PriceDetails.getFormattedPrice());
				Log.debug("[GooglePlayStoreHelper] - price_amount_micros: " + PriceDetails.getPriceAmountMicros());
				Log.debug("[GooglePlayStoreHelper] - price_currency_code: " + PriceDetails.getPriceCurrencyCode());

				ProductIds[index] = ThisProduct.getProductId();
				Titles[index] = ThisProduct.getTitle();
				Descriptions[index] = ThisProduct.getDescription();
				Prices[index] = PriceDetails.getFormattedPrice();
				PricesRaw[index] = (float)(PriceDetails.getPriceAmountMicros() / 1000000.0);
				CurrencyCodes[index] = PriceDetails.getPriceCurrencyCode();
			}
		}
	}

	class ListOfPurchasesAsArrays 
	{
		public String[] OwnedProducts;
		public int[] PurchaseStates;
		public String[] ProductTokens;
		public String[] Signatures;
		public String[] Receipts;

		public ListOfPurchasesAsArrays(List<Purchase> InOwnedItems) 
		{
			OwnedProducts = new String[InOwnedItems.size()];
			PurchaseStates = new int[InOwnedItems.size()];
			ProductTokens = new String[InOwnedItems.size()];
			Signatures = new String[InOwnedItems.size()];
			Receipts = new String[InOwnedItems.size()];

			for (int index = 0; index < InOwnedItems.size(); index ++) 
			{
				Purchase OwnedPurchase = InOwnedItems.get(index);
				OwnedProducts[index] = OwnedPurchase.getProducts().get(0);
				PurchaseStates[index] = OwnedPurchase.getPurchaseState();
				ProductTokens[index] = OwnedPurchase.getPurchaseToken();
				Signatures[index] = OwnedPurchase.getSignature();
				Receipts[index] = Base64.encode(OwnedPurchase.getOriginalJson().getBytes());
			}
		}
	}
	
	private static BillingFlowParams CreateBillingFlowParams(ProductDetails InProductDetails, String InObfuscatedAccountId)
	{
		ArrayList<BillingFlowParams.ProductDetailsParams> ProductDetailsParamList = new ArrayList<>(1);
		ProductDetailsParamList.add(BillingFlowParams.ProductDetailsParams.newBuilder()
			.setProductDetails(InProductDetails)
			.build());
		
		BillingFlowParams.Builder Params = BillingFlowParams.newBuilder()
			.setProductDetailsParamsList(ProductDetailsParamList);
		if (InObfuscatedAccountId != null)
		{
			Params = Params.setObfuscatedAccountId(InObfuscatedAccountId);
		}
		return Params.build();
	}

	private static QueryProductDetailsParams CreateQueryProductDetailsParamsForProduct(String InProductId)
	{
		ArrayList<QueryProductDetailsParams.Product> ProductList = new ArrayList<>(1);
		ProductList.add(QueryProductDetailsParams.Product.newBuilder()
			.setProductId(InProductId)
			.setProductType(BillingClient.ProductType.INAPP)
			.build());
		return QueryProductDetailsParams.newBuilder()
			.setProductList(ProductList)
			.build();
	}

	private static QueryProductDetailsParams CreateQueryProductDetailsParamsForProducts(String[] InProductIds)
	{
		ArrayList<QueryProductDetailsParams.Product> ProductList = new ArrayList<>();
		for (String ProductId : InProductIds)
		{
			QueryProductDetailsParams.Product Product = QueryProductDetailsParams.Product.newBuilder()
				.setProductId(ProductId)
				.setProductType(BillingClient.ProductType.INAPP)
				.build();
			ProductList.add(Product);
		}

		return QueryProductDetailsParams.newBuilder()
			.setProductList(ProductList)
			.build();
	}

	private static ProductDetails FindProductDetailsInList(String ProductId, List<ProductDetails> InProductDetailsList)
	{
		for(ProductDetails Details : InProductDetailsList) 
		{
			if (Details.getProductId().equals(ProductId)) 
			{
				return Details;
			}
		}
		return null;
	}
	
	/**
	 * Get a text translation of the Response Codes returned by google play.
	 */

	private static String TranslateBillingResponseCode(int ResponseCode)
	{
		switch(ResponseCode)
		{
			case BillingClient.BillingResponseCode.OK:
				return "BillingResponseCode.OK";
			case BillingClient.BillingResponseCode.USER_CANCELED:
				return "BillingResponseCode.USER_CANCELED";
			case BillingClient.BillingResponseCode.SERVICE_UNAVAILABLE:
				return "BillingResponseCode.SERVICE_UNAVAILABLE";
			case BillingClient.BillingResponseCode.SERVICE_TIMEOUT:
				return "BillingResponseCode.SERVICE_TIMEOUT";
			case BillingClient.BillingResponseCode.SERVICE_DISCONNECTED:
				return "BillingResponseCode.SERVICE_DISCONNECTED";
			case BillingClient.BillingResponseCode.BILLING_UNAVAILABLE:
				return "BillingResponseCode.BILLING_UNAVAILABLE";
			case BillingClient.BillingResponseCode.FEATURE_NOT_SUPPORTED:
				return "BillingResponseCode.FEATURE_NOT_SUPPORTED";
			case BillingClient.BillingResponseCode.ITEM_UNAVAILABLE:
				return "BillingResponseCode.ITEM_UNAVAILABLE";
			case BillingClient.BillingResponseCode.DEVELOPER_ERROR:
				return "BillingResponseCode.DEVELOPER_ERROR";
			case BillingClient.BillingResponseCode.ERROR:
				return "BillingResponseCode.ERROR";
			case BillingClient.BillingResponseCode.ITEM_ALREADY_OWNED:
				return "BillingResponseCode.ITEM_ALREADY_OWNED";
			case BillingClient.BillingResponseCode.ITEM_NOT_OWNED:
				return "BillingResponseCode.ITEM_NOT_OWNED";
			default:
				return "Unknown Response Code: " + ResponseCode;
		}
	}

	private static String TranslateBillingResult(@NonNull BillingResult Result)
	{
		int ResponseCode = Result.getResponseCode();
		switch(ResponseCode)
		{
			case BillingClient.BillingResponseCode.OK:
			case BillingClient.BillingResponseCode.USER_CANCELED:
				return TranslateBillingResponseCode(ResponseCode);
			default:
				return TranslateBillingResponseCode(ResponseCode) + " DebugMessage: " + Result.getDebugMessage();
		}
	}

	///////////////////////////////////////////////////////
	// Game Activity/Context driven methods we need to listen for.

	/**
	 * On Destroy disconnect from service
	 */
	public void onDestroy()
	{
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper::onDestroy");

		if (Client != null)
		{
			Client.endConnection();
		}
	}

	// Callback that notify the C++ implementation that a task has completed
	public native void NativeQueryComplete(int ResponseCode, String[] ProductIDs, String[] Titles, String[] Descriptions, String[] Prices, float[] PricesRaw, String[] CurrencyCodes);
	public native void NativePurchaseComplete(int ResponseCode, String ProductId, int PurchaseState, String ProductToken, String ReceiptData, String Signature);
	public native void NativeQueryExistingPurchasesComplete(int ResponseCode, String[] ProductIds, int[] PurchaseState, String[] ProductTokens, String[] ReceiptsData, String[] Signatures);
	public native void NativeConsumeComplete(int ResponseCode, String PurchaseToken);
}
