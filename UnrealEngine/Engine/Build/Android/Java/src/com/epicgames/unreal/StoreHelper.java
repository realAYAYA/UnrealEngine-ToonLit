//This file needs to be here so the "ant" build step doesnt fail when looking for a /src folder.

package com.epicgames.unreal;

import android.content.Intent;
import android.os.Bundle;

public interface StoreHelper
{
	public boolean QueryInAppPurchases(String[] ProductIDs);
	public boolean BeginPurchase(String[] ProductIDs, String ObfuscatedAccountId);
	public boolean IsAllowedToMakePurchases();
	public void AcknowledgePurchase(String purchaseToken);
	public void ConsumePurchase(String purchaseToken);
	public boolean QueryExistingPurchases();
	public void	onDestroy();
}