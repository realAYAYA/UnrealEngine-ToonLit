// Copyright Epic Games, Inc. All Rights Reserved.

package com.epicgames.unreal;

import android.util.Pair;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.billingclient.api.BillingClient;
import com.android.billingclient.api.BillingClientStateListener;
import com.android.billingclient.api.BillingFlowParams;
import com.android.billingclient.api.BillingResult;
import com.android.billingclient.api.AcknowledgePurchaseParams;
import com.android.billingclient.api.AcknowledgePurchaseResponseListener;
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
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.stream.Collectors;

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

	public GooglePlayStoreHelper(GameActivity InGameActivity, Logger InLog)
	{
		Log = InLog;
		Log.debug("[GooglePlayStoreHelper] - GooglePlayStoreHelper");

		MainActivity = InGameActivity;

		Client = BillingClient.newBuilder(MainActivity)
			.setListener(this)
			.enablePendingPurchases()
			.build();

		Client.startConnection(new BillingClientStateListener() {
			@Override
			public void onBillingSetupFinished(@NonNull BillingResult billingResult)
			{
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
		Log.debug("[GooglePlayStoreHelper] - IsAllowedToMakePurchases. Billing client connected: " + bIsIapSetup);
		return bIsIapSetup;
	}

	/**
	 * Gets a set of store product ids for all the subscriptions provided in knownProducts.
	 */
	private Set<String> GetSubscriptionStoreProductIds(List<ProductDetails> KnownProducts)
	{
		Set<String> SubscriptionIds = new HashSet<String>();
		for(ProductDetails Details: KnownProducts)
		{
			if(Details.getProductType().equals(BillingClient.ProductType.SUBS))
			{
				SubscriptionIds.add(Details.getProductId());
			}
		}
		return SubscriptionIds;
	}

	/**
	 * Query product details for the provided product ids
	 */
	@Override
	public boolean QueryInAppPurchases(String[] InProductIDs)
	{
		Log.debug("[GooglePlayStoreHelper] - QueryInAppPurchases");
		Log.debug("[GooglePlayStoreHelper] - NumProducts: " + InProductIDs.length);
		Log.debug("[GooglePlayStoreHelper] - Products: " + Arrays.toString(InProductIDs));

		if (InProductIDs.length > 0)
		{
			QueryProductDetailsParams QueryProducts = CreateQueryProductDetailsParams(InProductIDs, BillingClient.ProductType.INAPP);
			QueryProductDetailsParams QuerySubs = CreateQueryProductDetailsParams(InProductIDs, BillingClient.ProductType.SUBS);

			ProductDetailsResponseListener AggregatedListener = new ProductDetailsResponseListener() {
				// This method is marked synchronized because although Google Billing Library documentation says "All methods annotated with AnyThread can be 
				// called from any thread and all the asynchronous callbacks will be returned on the same thread" those callbacks may be called on different 
				// threads if invoked from JNI.
				@Override
				public synchronized void onProductDetailsResponse(@NonNull BillingResult Result, @NonNull List<ProductDetails> ProductDetailsList)
				{
					if(AwaitingResponseCount <= 0)
					{
						return;
					}
					int ResponseCode = Result.getResponseCode();
					if(ResponseCode == BillingClient.BillingResponseCode.OK)
					{
						AllProductDetails.addAll(ProductDetailsList);
						AwaitingResponseCount -= 1;
						Log.debug("[GooglePlayStoreHelper] - QueryInAppPurchases - Received new " + ProductDetailsList.size() + " products. Waiting for additional responses: " + (AwaitingResponseCount > 0));
						if (AwaitingResponseCount == 0)
						{
							Log.debug("[GooglePlayStoreHelper] - QueryInAppPurchases - ProductDetails:" + AllProductDetails.toString());
							Log.debug("[GooglePlayStoreHelper] - QueryInAppPurchases " + AllProductDetails.size() + " items - Success!");

							ListOfProductDetailsAsArrays DetailArrays = new ListOfProductDetailsAsArrays(AllProductDetails);
							NativeQueryComplete(BillingClient.BillingResponseCode.OK, DetailArrays.ProductIds, DetailArrays.Titles, DetailArrays.Descriptions, DetailArrays.Prices, DetailArrays.PricesRaw, DetailArrays.CurrencyCodes);
						}
					}
					else
					{
						AwaitingResponseCount = 0;
						Log.debug("[GooglePlayStoreHelper] - QueryInAppPurchases - Failed with: " + TranslateBillingResult(Result));
						NativeQueryComplete(ResponseCode, null, null, null, null, null, null);
					}

					if (AwaitingResponseCount == 0)
					{
						Log.debug("[GooglePlayStoreHelper] - NativeQueryComplete done!");
					}
				}

				// We will be querying for in app products and subscriptions 
				int AwaitingResponseCount = 2;
				List<ProductDetails> AllProductDetails = new ArrayList<>();
			};

			Client.queryProductDetailsAsync(QueryProducts, AggregatedListener);
			Client.queryProductDetailsAsync(QuerySubs, AggregatedListener);
		}
		else
		{
			Log.debug("[GooglePlayStoreHelper] - no products given to query");
			return false;
		}

		return true;
	}

	/**
	 * Start the purchase flow for a particular product id
	 */
	@Override
	public boolean BeginPurchase(final String[] ProductIds, final String ObfuscatedAccountId)
	{
		if (ProductIds.length == 0)
		{
			Log.debug("[GooglePlayStoreHelper] - BeginPurchase - Failed! No product ids provided");
			return false;

		}

		String CommonProductType = EncodedProductId.GetCommonProductType(ProductIds);
		if (CommonProductType == null)
		{
			Log.debug("[GooglePlayStoreHelper] - BeginPurchase - Failed! All products should have the same product type");
			return false;
		}

		if (CommonProductType == BillingClient.ProductType.SUBS && ProductIds.length > 1)
		{
			// This condition was added because the error produced by BillingClient.launchBillingFlow was not informative
			Log.debug("[GooglePlayStoreHelper] - BeginPurchase - Failed! Purchasing multiple subscriptions at once is not supported");
			return false;
		}

		Log.debug("[GooglePlayStoreHelper] - BeginPurchase - querying product information " + String.join(",", ProductIds));
		QueryProductDetailsParams Params = CreateQueryProductDetailsParams(ProductIds, CommonProductType);
		Client.queryProductDetailsAsync(Params, new ProductDetailsResponseListener() {
			@Override
			public void onProductDetailsResponse(@NonNull BillingResult Result, @NonNull List<ProductDetails> ProductDetailsList)
			{
				if(Result.getResponseCode() == BillingClient.BillingResponseCode.OK)
				{
					Log.debug("[GooglePlayStoreHelper] - BeginPurchase - CreateBillingFlowParams with " + ProductDetailsList.stream().map(ProductDetails::getName).collect(Collectors.joining(",")));
					final BillingFlowParams Params = CreateBillingFlowParams(ProductIds, ProductDetailsList, ObfuscatedAccountId);
					if(Params == null)
					{
						Log.debug("[GooglePlayStoreHelper] - BeginPurchase - Failed! Matching products may not exist in the store");
						NativePurchaseComplete(BillingClient.BillingResponseCode.DEVELOPER_ERROR, ProductIds, Purchase.PurchaseState.UNSPECIFIED_STATE,"", "", "");
						return;
					}
					// BillingClient.launchBillingFlow is annotated @UiThread, so dispatch the call to the Activity UI thread
					MainActivity.runOnUiThread(new Runnable()
					{
						@Override
						public void run() {
							Log.debug("[GooglePlayStoreHelper] - BeginPurchase - launchBillingFlow " + ProductDetailsList.stream().map(ProductDetails::getName).collect(Collectors.joining(",")));
							BillingResult Result = Client.launchBillingFlow(MainActivity, Params);
							int ResponseCode = Result.getResponseCode();
							if (ResponseCode != BillingClient.BillingResponseCode.OK)
							{
								Log.debug("[GooglePlayStoreHelper] - BeginPurchase - Failed! " + TranslateBillingResult(Result));
								NativePurchaseComplete(ResponseCode, ProductIds, Purchase.PurchaseState.UNSPECIFIED_STATE, "", "", "");
							}
						}
					});
				}
				else
				{
					Log.debug("[GooglePlayStoreHelper] - BeginPurchase - Failed! " + TranslateBillingResult(Result));
					NativePurchaseComplete(Result.getResponseCode(), ProductIds, Purchase.PurchaseState.UNSPECIFIED_STATE,"", "", "");
				}
			}
		});
		return true;
	}


	/**
	 * Queries for all non consumed non expired purchases
	 */
	@Override
	public boolean QueryExistingPurchases()
	{
		Log.debug("[GooglePlayStoreHelper] - QueryExistingPurchases");

		QueryPurchasesParams QueryInApp =  QueryPurchasesParams.newBuilder()
			.setProductType(BillingClient.ProductType.INAPP)
			.build();
		QueryPurchasesParams QuerySubs = QueryPurchasesParams.newBuilder()
			.setProductType(BillingClient.ProductType.SUBS)
			.build();

		PurchasesResponseListener AggregatedListener = new PurchasesResponseListener() {
			// This method is marked synchronized because although Google Billing Library documentation says "All methods annotated with AnyThread can be 
			// called from any thread and all the asynchronous callbacks will be returned on the same thread" those callbacks may be called on different 
			// threads if invoked from JNI 
			@Override
			public synchronized void onQueryPurchasesResponse(@NonNull BillingResult Result, @NonNull List<Purchase> OwnedItems)
			{
				if (AwaitingResponseCount <= 0)
					return;

				int ResponseCode = Result.getResponseCode();
				if (ResponseCode == BillingClient.BillingResponseCode.OK)
				{
					AllProducts.addAll(OwnedItems);
					AwaitingResponseCount -= 1;
					Log.debug("[GooglePlayStoreHelper] - QueryExistingPurchases - Received " + OwnedItems.size() + " products. Waiting for additional responses: " + (AwaitingResponseCount != 0));
					if (AwaitingResponseCount == 0)
					{
						Log.debug("[GooglePlayStoreHelper] - QueryExistingPurchases - Success! User has previously purchased " + AllProducts.size() + " inapp products");

						if (AllProducts.isEmpty())
						{
							NativeQueryExistingPurchasesComplete(ResponseCode, new String[0][0], new int[0], new String[0], new String[0], new String[0]);
						}
						else
						{
							// Request product details so we can fix notified product ids in case they refer to a subscription
							String[] StoreProductIds = AllProducts.stream().flatMap(Purchase -> Purchase.getProducts().stream()).toArray(String[]::new);
							QueryProductDetailsParams QuerySubs = CreateQueryProductDetailsParams(StoreProductIds, BillingClient.ProductType.SUBS);
							Client.queryProductDetailsAsync(QuerySubs, new ProductDetailsResponseListener() {
								@Override
								public void onProductDetailsResponse(@NonNull BillingResult Result, @NonNull List<ProductDetails> ProductDetailsList)
								{
									int ResponseCode = Result.getResponseCode();
									if (ResponseCode == BillingClient.BillingResponseCode.OK)
									{
										Set<String> SubscriptionProductIds = GetSubscriptionStoreProductIds(ProductDetailsList);
										ListOfPurchasesAsArrays PurchasesArrays = new ListOfPurchasesAsArrays(AllProducts, SubscriptionProductIds);
										NativeQueryExistingPurchasesComplete(ResponseCode, PurchasesArrays.OwnedProducts, PurchasesArrays.PurchaseStates, PurchasesArrays.ProductTokens, PurchasesArrays.Receipts, PurchasesArrays.Signatures);
									}
									else
									{
										Log.debug("[GooglePlayStoreHelper] - QueryExistingPurchases - Failed to get product information while collecting existing purchases" + TranslateBillingResult(Result));
										NativeQueryExistingPurchasesComplete(ResponseCode, null, null, null, null, null);
									}
								}
							});
						}
					}
				}
				else
				{
					AwaitingResponseCount = 0;
					Log.debug("[GooglePlayStoreHelper] - QueryExistingPurchases - Failed to collect existing purchases" + TranslateBillingResult(Result));
					NativeQueryExistingPurchasesComplete(ResponseCode, null, null, null, null, null);
				}
			}
			// We will be querying for in app products and subscriptions 
			int AwaitingResponseCount = 2;
			List<Purchase> AllProducts = new ArrayList<>();
		};

		Client.queryPurchasesAsync(QueryInApp, AggregatedListener);
		Client.queryPurchasesAsync(QuerySubs, AggregatedListener);
		return true;
	}

	/**
	 * Locally acknowledges the purchase referred by the provided PurchaseToken
	 */
	@Override
	public void AcknowledgePurchase(final String PurchaseToken)
	{
		Log.debug("[GooglePlayStoreHelper] - Beginning AcknowledgePurchase: " + PurchaseToken);

		AcknowledgePurchaseParams Params = AcknowledgePurchaseParams.newBuilder()
			.setPurchaseToken(PurchaseToken)
			.build();
		Client.acknowledgePurchase(Params, new AcknowledgePurchaseResponseListener() {
			@Override
			public void onAcknowledgePurchaseResponse(@NonNull BillingResult Result)
			{
				Log.debug("[GooglePlayStoreHelper] - AcknowledgePurchase response: " + TranslateBillingResult(Result));
				NativeAcknowledgeComplete(Result.getResponseCode(), PurchaseToken);
			}
		});
	}

	/**
	 * Locally consumes the purchase referred by the provided PurchaseToken
	 */
	@Override
	public void ConsumePurchase(String PurchaseToken)
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
	public void onPurchasesUpdated(@NonNull BillingResult Result, @Nullable final List<Purchase> UpdatedPurchases)
	{
		int responseCode = Result.getResponseCode();
		Log.debug("[GooglePlayStoreHelper] - onPurchasesUpdated - Processing purchase result. Response Code: " + TranslateBillingResult(Result));
		if (responseCode == BillingClient.BillingResponseCode.OK && UpdatedPurchases != null)
		{
			// Request product details so we can fix notified product ids in case they refer to a subscription
			String[] StoreProductIds = UpdatedPurchases.stream().flatMap(Purchase -> Purchase.getProducts().stream()).toArray(String[]::new);
			QueryProductDetailsParams QuerySubs = CreateQueryProductDetailsParams(StoreProductIds, BillingClient.ProductType.SUBS);
			Client.queryProductDetailsAsync(QuerySubs, new ProductDetailsResponseListener() {
				@Override
				public void onProductDetailsResponse(@NonNull BillingResult Result, @NonNull List<ProductDetails> ProductDetailsList)
				{
					if (responseCode == BillingClient.BillingResponseCode.OK)
					{
						Set<String> SubscriptionProductIds = GetSubscriptionStoreProductIds(ProductDetailsList);

						for (Purchase UpdatedPurchase : UpdatedPurchases)
						{
							String[] ProductIds = EncodedProductId.StoreProductIdsToProductIds(UpdatedPurchase.getProducts(), SubscriptionProductIds);
							Log.debug("[GooglePlayStoreHelper] - onPurchasesUpdated - Processing purchase of product " + String.join(",",ProductIds));
							Log.debug("[GooglePlayStoreHelper] - Purchase data: " + UpdatedPurchase.toString());
							Log.debug("[GooglePlayStoreHelper] - Data signature: " + UpdatedPurchase.getSignature());

							String Receipt = Base64.encode(UpdatedPurchase.getOriginalJson().getBytes());
							NativePurchaseComplete(BillingClient.BillingResponseCode.OK, ProductIds, UpdatedPurchase.getPurchaseState(), UpdatedPurchase.getPurchaseToken(), Receipt, UpdatedPurchase.getSignature());
						}

					}
					else
					{
						Log.debug("[GooglePlayStoreHelper] - onPurchasesUpdated - Purchase failed while retrieving product information. Response code: " + TranslateBillingResult(Result));
						NativePurchaseComplete(responseCode, null, Purchase.PurchaseState.UNSPECIFIED_STATE, "", "", "");
					}
				}
			});
		}
		else
		{
			Log.debug("[GooglePlayStoreHelper] - onPurchasesUpdated - Purchase failed. Response code: " + TranslateBillingResult(Result));
			NativePurchaseComplete(responseCode, null, Purchase.PurchaseState.UNSPECIFIED_STATE, "", "", "");
		}
	}

	///////////////////////////////////////////////////////
	// Some helpers

	/**
	 * Helper to convert Google Billing library received data to be passed to native calls
	 */
	class ListOfProductDetailsAsArrays
	{
		public String[] ProductIds;
		public String[] Titles;
		public String[] Descriptions;
		public String[] Prices;
		public long[] PricesRaw;
		public String[] CurrencyCodes;

		private int ComputeTotalEntryCount(List<ProductDetails> InProductDetailsList)
		{
			int TotalCount = 0;
			for (ProductDetails ThisProduct : InProductDetailsList)
			{
				switch(ThisProduct.getProductType())
				{
					case BillingClient.ProductType.INAPP:
						TotalCount ++;
						break;
					case BillingClient.ProductType.SUBS:
						for (ProductDetails.SubscriptionOfferDetails SubsOfferDetails: ThisProduct.getSubscriptionOfferDetails())
						{
							TotalCount += SubsOfferDetails.getPricingPhases().getPricingPhaseList().size();
							if (SubsOfferDetails.getOfferId() != null)
							{
								// En extra entry will be added for offers without including price index on it's name
								TotalCount += 1;
							}
						}
						break;
				}
			}
			return TotalCount;
		}

		/**
		 * Transforms store data into data consumable by the engine
		 * For in app products it will create an entry with product id <store product id>
		 * For subscription it will create an entry for each base plan with a product id <subscription prefix>:<store product id>:<base plan id>
		 * For each offer in a subscription plan it will create an entry <subscription prefix>:<store product id>:<base plan id>:<offer id> with the 
		 * same price data as the base plan. Then for each step in the offer it will create entries with id 
		 * <subscription prefix>:<store product id>:<base plan id>:<offer id>:<price phase index> with increasing values for "price phase index" 
		 */
		public ListOfProductDetailsAsArrays(List<ProductDetails> InProductDetailsList)
		{
			int TotalEntryCount = ComputeTotalEntryCount(InProductDetailsList);
			ProductIds = new String[TotalEntryCount];
			Titles = new String[TotalEntryCount];
			Descriptions = new String[TotalEntryCount];
			Prices = new String[TotalEntryCount];
			PricesRaw = new long[TotalEntryCount];
			CurrencyCodes = new String[TotalEntryCount];

			int Index = 0;
			for (ProductDetails ThisProduct : InProductDetailsList)
			{
				Log.debug("[GooglePlayStoreHelper] - QueryInAppPurchases - Parsing details for: " + ThisProduct.getProductId());
				Log.debug("[GooglePlayStoreHelper] - title: " + ThisProduct.getTitle());
				Log.debug("[GooglePlayStoreHelper] - description: " + ThisProduct.getDescription());
				Titles[Index] = ThisProduct.getTitle();
				Descriptions[Index] = ThisProduct.getDescription();

				switch(ThisProduct.getProductType())
				{
					case BillingClient.ProductType.INAPP:
						ProductDetails.OneTimePurchaseOfferDetails PriceDetails = ThisProduct.getOneTimePurchaseOfferDetails();
						Log.debug("[GooglePlayStoreHelper] - price: " + PriceDetails.getFormattedPrice());
						Log.debug("[GooglePlayStoreHelper] - price_amount_micros: " + PriceDetails.getPriceAmountMicros());
						Log.debug("[GooglePlayStoreHelper] - price_currency_code: " + PriceDetails.getPriceCurrencyCode());

						ProductIds[Index] = ThisProduct.getProductId();
						Prices[Index] = PriceDetails.getFormattedPrice();
						PricesRaw[Index] = PriceDetails.getPriceAmountMicros();
						CurrencyCodes[Index] = PriceDetails.getPriceCurrencyCode();

						Index ++;
						break;
					case BillingClient.ProductType.SUBS:
						for (ProductDetails.SubscriptionOfferDetails SubsOfferDetails: ThisProduct.getSubscriptionOfferDetails())
						{
							int PricingPhaseIndex = 0;
							for( ProductDetails.PricingPhase PricingPhase: SubsOfferDetails.getPricingPhases().getPricingPhaseList())
							{
								Log.debug("[GooglePlayStoreHelper] - price: " + PricingPhase.getFormattedPrice());
								Log.debug("[GooglePlayStoreHelper] - price_amount_micros: " + PricingPhase.getPriceAmountMicros());
								Log.debug("[GooglePlayStoreHelper] - price_currency_code: " + PricingPhase.getPriceCurrencyCode());

								boolean bIsOffer = SubsOfferDetails.getOfferId() != null;
								if (bIsOffer)
								{
									ProductIds[Index] = EncodedProductId.EncodeSubscriptionOfferPricePoint(ThisProduct.getProductId(), SubsOfferDetails.getBasePlanId(), SubsOfferDetails.getOfferId(), PricingPhaseIndex);
								}
								else
								{
									ProductIds[Index] = EncodedProductId.EncodeSubscriptionBasePlan(ThisProduct.getProductId(), SubsOfferDetails.getBasePlanId());
								}
								Prices[Index] = PricingPhase.getFormattedPrice();
								PricesRaw[Index] = PricingPhase.getPriceAmountMicros();
								CurrencyCodes[Index] = PricingPhase.getPriceCurrencyCode();

								Index++;
								PricingPhaseIndex++;

								if (bIsOffer && PricingPhaseIndex == SubsOfferDetails.getPricingPhases().getPricingPhaseList().size())
								{
									// Add extra product information using last price point but not including price index on it's name
									ProductIds[Index] = EncodedProductId.EncodeSubscriptionOffer(ThisProduct.getProductId(), SubsOfferDetails.getBasePlanId(), SubsOfferDetails.getOfferId());
									Prices[Index] = PricingPhase.getFormattedPrice();
									PricesRaw[Index] = PricingPhase.getPriceAmountMicros();
									CurrencyCodes[Index] = PricingPhase.getPriceCurrencyCode();
									Index++;
								}
							}
						}
						break;
				}
			}
		}
	}

	/**
	 * Helper to convert Google Billing library received data to be passed to native calls
	 */
	static class ListOfPurchasesAsArrays
	{
		public String[][] OwnedProducts;
		public int[] PurchaseStates;
		public String[] ProductTokens;
		public String[] Signatures;
		public String[] Receipts;

		public ListOfPurchasesAsArrays(List<Purchase> InOwnedItems, Set<String> SubscriptionProductId)
		{
			OwnedProducts = new String[InOwnedItems.size()][];
			PurchaseStates = new int[InOwnedItems.size()];
			ProductTokens = new String[InOwnedItems.size()];
			Signatures = new String[InOwnedItems.size()];
			Receipts = new String[InOwnedItems.size()];

			for (int index = 0; index < InOwnedItems.size(); index ++)
			{
				Purchase OwnedPurchase = InOwnedItems.get(index);
				OwnedProducts[index] = OwnedPurchase.getProducts().stream().map(ProductId -> EncodedProductId.StoreProductIdToProductId(ProductId, SubscriptionProductId)).toArray(String[]::new);
				PurchaseStates[index] = OwnedPurchase.getPurchaseState();
				ProductTokens[index] = OwnedPurchase.getPurchaseToken();
				Signatures[index] = OwnedPurchase.getSignature();
				Receipts[index] = Base64.encode(OwnedPurchase.getOriginalJson().getBytes());
			}
		}
	}


	/*
		Helper class to Encode/Decode subscription identity data on ProductIds
		To properly select a subscription for purchase or get it's price information ProductIds passed to/received from the engine 
		may have a prefix and encoded identifiers to include all that information as a single ProductId
		On the GooglePlay Store each product is identified by a single id but subscriptions may define several base plans, which define renewal periods and base pricing.
		Each base plan may define multiple offers which include special pricings (like free trials and special pricings after that)
		Those subscription id are in the form
		<subscription prefix><store product id>[:<base plan id>[:<offer id>[:<offer price index>]]]
		To query for product information just and identifier like <subscription prefix><store product id> is enough to retrieve all base plans and offers
		In order to start a purchase the <base plan id> part is mandatory. To purchase a given offer in a base plan the <offer id> is also mandatory
	 */
	static class EncodedProductId
	{
		private final static String SUBSCRIPTION_PREFIX = "s-";

		static public String EncodeSubscriptionOfferPricePoint(String StoreProductId, String BasePlanId, String OfferId, int PricingPhaseIndex)
		{
			return String.format("%s%s:%s:%s:%d", SUBSCRIPTION_PREFIX, StoreProductId, BasePlanId, OfferId, PricingPhaseIndex);
		}
		static public String EncodeSubscriptionBasePlan(String StoreProductId, String BasePlanId)
		{
			return String.format("%s%s:%s", SUBSCRIPTION_PREFIX, StoreProductId, BasePlanId);
		}

		static public String EncodeSubscriptionOffer(String StoreProductId, String BasePlanId, String OfferId)
		{
			return String.format("%s%s:%s:%s", SUBSCRIPTION_PREFIX, StoreProductId, BasePlanId, OfferId);
		}

		static public boolean IsSubscription(String InProductId)
		{
			return InProductId.startsWith(SUBSCRIPTION_PREFIX);
		}

		/**
		 * returns the product type that matches all product ids. In product ids refer to mixed product types it returns null
		 */
		static public String GetCommonProductType(String[] InProducts)
		{
			if (InProducts.length == 0)
			{
				return null;
			}

			boolean bIsFirstProductSubscription = IsSubscription(InProducts[0]);

			for (int i=1; i < InProducts.length; ++i)
			{
				if (bIsFirstProductSubscription != IsSubscription(InProducts[i]))
				{
					return null;
				}
			}
			return bIsFirstProductSubscription? BillingClient.ProductType.SUBS : BillingClient.ProductType.INAPP;
		}

		/**
		 * Fixes store product id to reflect whether it refers to a subscription or not
		 */
		public static String StoreProductIdToProductId(String StoreProductId, Set<String> SubscriptionStoreProductIds)
		{
			return SubscriptionStoreProductIds.contains(StoreProductId)? SUBSCRIPTION_PREFIX + StoreProductId : StoreProductId;
		}

		/**
		 * Fixes a list of store product id to reflect whether they refer to a subscription or not
		 */
		public static String[] StoreProductIdsToProductIds(List<String> StoreProductIds, Set<String> SubscriptionStoreProductIds)
		{
			return StoreProductIds.stream().map(StoreProductId -> StoreProductIdToProductId(StoreProductId, SubscriptionStoreProductIds)).toArray(String[]::new );
		}

		/**
		 * Decodes the product identity data from a ProductId 
		 */
		EncodedProductId(String CompositeProductId)
		{
			bIsSubscription = IsSubscription(CompositeProductId);
			if (bIsSubscription)
			{
				String[] Parts = CompositeProductId.substring(SUBSCRIPTION_PREFIX.length()).split(":");
				if (Parts.length > 4)
				{
					// Unknown format. Get it as a full product id
					StoreProductId = CompositeProductId;
				}
				else
				{
					// The PricingPhaseIndex is not needed to start a purchase or query for products, so no need to store it
					if (Parts.length >= 1)
					{
						StoreProductId = Parts[0];
					}
					if (Parts.length >= 2)
					{
						BasePlanId = Parts[1];
					}
					if (Parts.length >= 3)
					{
						OfferId = Parts[2];
					}
				}
			}
			else
			{
				StoreProductId = CompositeProductId;
			}
		}

		/**
		 * Creates BillingFlowParams.ProductDetailsParams by looking for store objects retrieved from the store that match the identity data contained on this instance 
		 * To create a BillingFlowParams.ProductDetailsParams for a subscription we need to use the ProductDetails instance for its store product id and the offer token 
		 * from the nested SubscriptionOfferDetails list
		 * To create a BillingFlowParams.ProductDetailsParams for a inapp product we only need the ProductDetails matching its store product id 
		 * Returns null if we cannot create the proper instance
		 */
		public BillingFlowParams.ProductDetailsParams CreateProductDetailsParams(List<ProductDetails> InProductDetails)
		{
			if (bIsSubscription)
			{
				// If a ProductDetails matches the StoreProductId and also contains an offer matching BasePlanId and OfferId then build ProductDetailsParams from it and the offer token
				return InProductDetails.stream()
					.filter(Details -> Details.getProductId().equals(StoreProductId) && Details.getProductType().equals(BillingClient.ProductType.SUBS))
					.flatMap(Details -> Details.getSubscriptionOfferDetails().stream().filter(Offer -> HasMatchingOffer(Offer)).map(Offer -> new Pair<>(Details, Offer.getOfferToken())))
					.findFirst()
					.map(OfferData -> BillingFlowParams.ProductDetailsParams.newBuilder().setProductDetails(OfferData.first).setOfferToken(OfferData.second).build())
					.orElse(null);
			}
			else
			{
				// If a ProductDetails matches the StoreProductId then build ProductDetailsParams from it
				return InProductDetails.stream()
					.filter(Details -> Details.getProductId().equals(StoreProductId) && Details.getProductType().equals(BillingClient.ProductType.INAPP))
					.findFirst()
					.map(Details -> BillingFlowParams.ProductDetailsParams.newBuilder().setProductDetails(Details).build())
					.orElse(null);
			}
		}

		/**
		 * Check if the offer data matches the identity data contained on this instance
		 */
		boolean HasMatchingOffer(ProductDetails.SubscriptionOfferDetails OfferDetails)
		{
			if (!OfferDetails.getBasePlanId().equals(BasePlanId))
			{
				return false;
			}

			if (OfferId == null || OfferId.isEmpty())
			{
				return OfferDetails.getOfferId() == null;
			}
			else
			{
				return OfferId.equals(OfferDetails.getOfferId());
			}
		}

		boolean bIsSubscription;
		String StoreProductId;
		String BasePlanId;
		String OfferId;
	}

	/**
	 * Creates a BillingFlowParams from product ids with encoded identity data
	 */
	private static BillingFlowParams CreateBillingFlowParams(String[] RequestedProductIds, List<ProductDetails> InProductDetails, String InObfuscatedAccountId)
	{
		List<BillingFlowParams.ProductDetailsParams> ProductDetailsParamList = Arrays.stream(RequestedProductIds)
			.map(ProductId -> new EncodedProductId(ProductId).CreateProductDetailsParams(InProductDetails))
			.filter(Objects::nonNull)
			.collect(Collectors.toList());

		if (ProductDetailsParamList.isEmpty())
		{
			return null;
		}
		BillingFlowParams.Builder Params = BillingFlowParams.newBuilder()
			.setProductDetailsParamsList(ProductDetailsParamList);
		if (InObfuscatedAccountId != null)
		{
			Params = Params.setObfuscatedAccountId(InObfuscatedAccountId);
		}
		return Params.build();
	}

	/**
	 * Creates a QueryProductDetailsParams from product ids with encoded identity data. Only StoreProductId is needed to query information for all base plans 
	 * and offers in a subscription prodduct
	 */
	private QueryProductDetailsParams CreateQueryProductDetailsParams(String[] InProductIds, String ProductType)
	{
		ArrayList<QueryProductDetailsParams.Product> ProductList = new ArrayList<>();
		for (String ProductId : InProductIds)
		{
			EncodedProductId Info = new EncodedProductId(ProductId);
			QueryProductDetailsParams.Product Product = QueryProductDetailsParams.Product.newBuilder()
				.setProductId(Info.StoreProductId)
				.setProductType(ProductType)
				.build();
			ProductList.add(Product);
		}

		return QueryProductDetailsParams.newBuilder()
			.setProductList(ProductList)
			.build();
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

	/**
	 * Get an informative text with as much information about the BillingResult as possible.
	 */
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
		Log.debug("[GooglePlayStoreHelper] - onDestroy");

		if (Client != null)
		{
			Client.endConnection();
		}
	}

	// Callback that notify the C++ implementation that a task has completed
	public native void NativeQueryComplete(int ResponseCode, String[] ProductIds, String[] Titles, String[] Descriptions, String[] Prices, long[] PricesRaw, String[] CurrencyCodes);
	public native void NativePurchaseComplete(int ResponseCode, String[] ProductIds, int PurchaseState, String ProductToken, String ReceiptData, String Signature);
	public native void NativeQueryExistingPurchasesComplete(int ResponseCode, String[][] ProductIds, int[] PurchaseState, String[] ProductTokens, String[] ReceiptsData, String[] Signatures);
	public native void NativeConsumeComplete(int ResponseCode, String PurchaseToken);
	public native void NativeAcknowledgeComplete(int ResponseCode, String PurchaseToken);
}
