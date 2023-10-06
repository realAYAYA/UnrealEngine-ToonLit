// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "Styling/StyleDefaults.h"
#include "Styling/ISlateStyle.h"
#include "Styling/SlateStyle.h"

struct FAssetData;
class UCurveFloat;
class UCurveLinearColor;
class UCurveVector;
struct FSlateWidgetStyle;

class FSlateGameResources : public FSlateStyleSet, public FGCObject
{
public:	

	/** Create a new Slate resource set that is scoped to the ScopeToDirectory.
	 *
	 * All paths will be formed as InBasePath + "/" + AssetPath. Lookups will be restricted to ScopeToDirectory.
	 * e.g. Resources.Initialize( "/Game/Widgets/SFortChest", "/Game/Widgets" )
	 *      Resources.GetBrush( "SFortChest/SomeBrush" );         // Will look up "/Game/Widgets" + "/" + "SFortChest/SomeBrush" = "/Game/Widgets/SFortChest/SomeBrush"
	 *      Resources.GetBrush( "SSomeOtherWidget/SomeBrush" );   // Will fail because "/Game/Widgets/SSomeOtherWidget/SomeBrush" is outside directory to which we scoped.
	 */
	static ENGINE_API TSharedRef<FSlateGameResources> New( const FName& InStyleSetName, const FString& ScopeToDirectory, const FString& InBasePath = FString() );

	/**
	 * Construct a style chunk.
	 * @param InStyleSetName The name used to identity this style set
	 */
	ENGINE_API FSlateGameResources( const FName& InStyleSetName );

	ENGINE_API virtual ~FSlateGameResources();

	/** @See New() */
	ENGINE_API void Initialize( const FString& ScopeToDirectory, const FString& InBasePath );

	/**
	 * Populate an array of FSlateBrush with resources consumed by this style chunk.
	 * @param OutResources - the array to populate.
	 */
	ENGINE_API virtual void GetResources( TArray< const FSlateBrush* >& OutResources ) const override;

	ENGINE_API virtual void SetContentRoot( const FString& InContentRootDir ) override;

	ENGINE_API virtual const FSlateBrush* GetBrush( const FName PropertyName, const ANSICHAR* Specifier = NULL, const ISlateStyle* RequestingStyle = nullptr ) const override;
	ENGINE_API virtual const FSlateBrush* GetOptionalBrush(const FName PropertyName, const ANSICHAR* Specifier = NULL, const FSlateBrush* const DefaultBrush = FStyleDefaults::GetNoBrush()) const override;

	ENGINE_API UCurveFloat* GetCurveFloat( const FName AssetName ) const;
	ENGINE_API UCurveVector* GetCurveVector( const FName AssetName ) const;
	ENGINE_API UCurveLinearColor* GetCurveLinearColor( const FName AssetName ) const;

protected:

	ENGINE_API virtual const FSlateWidgetStyle* GetWidgetStyleInternal( const FName DesiredTypeName, const FName StyleName, const FSlateWidgetStyle* DefaultStyle, bool bWarnIfNotFound) const override;

	ENGINE_API virtual void Log( ISlateStyle::EStyleMessageSeverity Severity, const FText& Message ) const override;

	ENGINE_API virtual void Log( const TSharedRef< class FTokenizedMessage >& Message ) const;

	/** Callback registered to the Asset Registry to be notified when an asset is added. */
	ENGINE_API void AddAsset(const FAssetData& InAddedAssetData);

	/** Callback registered to the Asset Registry to be notified when an asset is removed. */
	ENGINE_API void RemoveAsset(const FAssetData& InRemovedAssetData);

	ENGINE_API bool ShouldCache( const FAssetData& InAssetData );

	ENGINE_API void AddAssetToCache( UObject* InStyleObject, bool bEnsureUniqueness );

	ENGINE_API void RemoveAssetFromCache( const FAssetData& AssetData );

	ENGINE_API FName GenerateMapName( const FAssetData& AssetData );

	ENGINE_API FName GenerateMapName( UObject* StyleObject );

	/** Takes paths from the Editor's "Copy Reference" button and turns them into paths accepted by this object.
	 *
	 *   Example: 
	 *   This: "SlateBrushAsset'/Game/UI/STeamAndHeroSelection/CS_Port_Brush.CS_Port_Brush'"
	 *	 into
	 *	 This: "/Game/UI/STeamAndHeroSelection/CS_Port_Brush"
	*/
	ENGINE_API FName GetCleanName(const FName& AssetName) const;

	ENGINE_API virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;
	ENGINE_API virtual FString GetReferencerName() const override;

private:

	TMap< FName, TObjectPtr<UObject> > UIResources;
	FString BasePath;
	bool HasBeenInitialized;
};
