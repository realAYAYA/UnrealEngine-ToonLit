// Copyright Epic Games, Inc. All Rights Reserved.

#include "USDTransactor.h"

#include "UnrealUSDWrapper.h"
#include "USDErrorUtils.h"
#include "USDLayerUtils.h"
#include "USDListener.h"
#include "USDLog.h"
#include "USDPrimConversion.h"
#include "USDStageActor.h"
#include "USDTypesConversion.h"
#include "USDValueConversion.h"

#include "UsdWrappers/SdfChangeBlock.h"
#include "UsdWrappers/SdfLayer.h"
#include "UsdWrappers/SdfPath.h"
#include "UsdWrappers/UsdAttribute.h"
#include "UsdWrappers/UsdPrim.h"
#include "UsdWrappers/UsdStage.h"

#if WITH_EDITOR
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Editor/TransBuffer.h"
#include "Misc/ITransaction.h"
#endif // WITH_EDITOR

#define LOCTEXT_NAMESPACE "USDTransactor"

const FName UE::UsdTransactor::ConcertSyncEnableTag = TEXT( "EnableConcertSync" );

namespace UsdUtils
{
	// Objects adapted from USDListener, since we use slightly different data here
	struct FTransactorAttributeChange
	{
		FString PropertyName;
		FString Field;
		FString AttributeTypeName;				// Full SdfValueTypeName of the attribute (e.g. normal3f, bool, texCoord3d, float2) so that we can undo/redo attribute creation
		FConvertedVtValue OldValue;				// Default old value
		FConvertedVtValue NewValue;				// Default new value
		TArray<double> TimeSamples;
		TArray< FConvertedVtValue > TimeValues; // We can't fetch old/new values when time samples change, so we just have one of these and save the whole set every time
	};

	struct FTransactorObjectChange
	{
		TArray<FTransactorAttributeChange> AttributeChanges;
		FPrimChangeFlags Flags;
		FString PrimTypeName;
		TArray<FString> PrimAppliedSchemas;
		FString OldPath;
	};

	struct FTransactorRecordedEdit
	{
		FString ObjectPath;
		TArray<FTransactorObjectChange> ObjectChanges;
	};

	struct FTransactorRecordedEdits
	{
		FString EditTargetIdentifier;
		TArray< FTransactorRecordedEdit > Edits; // Kept in the order of recording
	};

	using FTransactorEditStorage = TArray< FTransactorRecordedEdits >;

	enum class EApplicationDirection
	{
		Reverse,
		Forward
	};
}

FArchive& operator<<( FArchive& Ar, UsdUtils::FTransactorAttributeChange& Change )
{
	Ar << Change.PropertyName;
	Ar << Change.Field;
	Ar << Change.AttributeTypeName;
	Ar << Change.OldValue;
	Ar << Change.NewValue;
	Ar << Change.TimeSamples;
	Ar << Change.TimeValues;
	return Ar;
}

FArchive& operator<<( FArchive& Ar, UsdUtils::FPrimChangeFlags& Flags )
{
	TArray<uint8> Bytes;
	if ( Ar.IsLoading() )
	{
		Ar << Bytes;

		check( Bytes.Num() == sizeof( Flags ) );
		FMemory::Memcpy( reinterpret_cast< uint8* >( &Flags ), Bytes.GetData(), sizeof( Flags ) );
	}
	else if ( Ar.IsSaving() )
	{
		Bytes.SetNumZeroed( sizeof( Flags ) );
		FMemory::Memcpy( Bytes.GetData(), reinterpret_cast< uint8* >( &Flags ), sizeof( Flags ) );

		Ar << Bytes;
	}

	return Ar;
}

FArchive& operator<<( FArchive& Ar, UsdUtils::FTransactorObjectChange& Change )
{
	Ar << Change.AttributeChanges;
	Ar << Change.Flags;
	Ar << Change.PrimTypeName;
	Ar << Change.PrimAppliedSchemas;
	Ar << Change.OldPath;
	return Ar;
}

FArchive& operator<<( FArchive& Ar, UsdUtils::FTransactorRecordedEdit& Edit )
{
	Ar << Edit.ObjectPath;
	Ar << Edit.ObjectChanges;
	return Ar;
}

FArchive& operator<<( FArchive& Ar, UsdUtils::FTransactorRecordedEdits& Edits )
{
	Ar << Edits.EditTargetIdentifier;
	Ar << Edits.Edits;
	return Ar;
}

namespace UsdUtils
{
	/**
	 * Converts the received VtValue map to an analogue using converted UE types that can be serialized with the UUsdTransactor.
	 * Needs the stage because we need to manually fetch additional prim/attribute data, in order to support undo/redoing attribute creation
	 */
	bool ConvertFieldValueMap( const FObjectChangesByPath& InChanges, const UE::FUsdStage& InStage, FTransactorRecordedEdits& InOutEdits )
	{
		InOutEdits.Edits.Reserve( InChanges.Num() + InOutEdits.Edits.Num() );

		for ( const TPair<FString, TArray<FObjectChangeNotice>>& ChangesByPrimPath : InChanges )
		{
			const FString& PrimPath = ChangesByPrimPath.Key;
			const TArray<FObjectChangeNotice>& Changes = ChangesByPrimPath.Value;

			FTransactorRecordedEdit& Edit = InOutEdits.Edits.Emplace_GetRef();
			Edit.ObjectPath = PrimPath;

			for ( const FObjectChangeNotice& Change : Changes )
			{
				FTransactorObjectChange& ConvertedChange = Edit.ObjectChanges.Emplace_GetRef();
				ConvertedChange.Flags = Change.Flags;
				ConvertedChange.OldPath = Change.OldPath;

				UE::FUsdPrim Prim = InStage.GetPrimAtPath( UE::FSdfPath{ *PrimPath } );

				if ( Prim &&
					( ConvertedChange.Flags.bDidAddInertPrim ||
					  ConvertedChange.Flags.bDidAddNonInertPrim ||
					  ConvertedChange.Flags.bDidRemoveInertPrim ||
					  ConvertedChange.Flags.bDidRemoveNonInertPrim ) )
				{
					ConvertedChange.PrimTypeName = Prim.GetTypeName().ToString();

					TArray<FName> AppliedSchemaNames = Prim.GetAppliedSchemas();
					ConvertedChange.PrimAppliedSchemas.Reset( AppliedSchemaNames.Num() );
					for ( const FName SchemaName : AppliedSchemaNames )
					{
						ConvertedChange.PrimAppliedSchemas.Add( SchemaName.ToString() );
					}

					UE_LOG( LogUsd, Log, TEXT( "Recorded the %s of prim '%s' with TypeName '%s' and PrimAppliedSchemas [%s]" ),
						(ConvertedChange.Flags.bDidAddInertPrim || ConvertedChange.Flags.bDidAddNonInertPrim) ? TEXT("addition") : TEXT("removal"),
						*PrimPath,
						*ConvertedChange.PrimTypeName,
						*FString::Join( ConvertedChange.PrimAppliedSchemas, TEXT( ", " ) )
					);
				}

				// Don't record any attribute changes if we can't find the prim anyway
				if ( !Prim )
				{
					// We expect not to find the prim if the change says it was just removed though
					if ( !ConvertedChange.Flags.bDidRemoveInertPrim && !ConvertedChange.Flags.bDidRemoveNonInertPrim )
					{
						UE_LOG( LogUsd, Warning, TEXT( "Failed to find prim at path '%s' when serializing object changes in transactor" ), *PrimPath );
					}
					continue;
				}

				for ( const FAttributeChange& AttributeChange : Change.AttributeChanges )
				{
					// We won't be able to do anything with this, so just ignore it
					if ( AttributeChange.PropertyName.IsEmpty() )
					{
						continue;
					}

					FTransactorAttributeChange& ConvertedAttributeChange = ConvertedChange.AttributeChanges.Emplace_GetRef();
					ConvertedAttributeChange.PropertyName = AttributeChange.PropertyName;
					ConvertedAttributeChange.Field = AttributeChange.Field;

					FConvertedVtValue ConvertedOldValue;
					if ( UsdToUnreal::ConvertValue( AttributeChange.OldValue, ConvertedOldValue ) )
					{
						ConvertedAttributeChange.OldValue = ConvertedOldValue;
					}

					FConvertedVtValue ConvertedNewValue;
					if ( UsdToUnreal::ConvertValue( AttributeChange.NewValue, ConvertedNewValue ) )
					{
						ConvertedAttributeChange.NewValue = ConvertedNewValue;
					}

					// We likely won't be able to fetch the attribute if it was removed, so try deducing the typename from the value just so that we have *something*
					if ( !AttributeChange.OldValue.IsEmpty() && (ConvertedChange.Flags.bDidRemoveProperty || ConvertedChange.Flags.bDidRemovePropertyWithOnlyRequiredFields) )
					{
						ConvertedAttributeChange.AttributeTypeName = UsdUtils::GetImpliedTypeName( AttributeChange.OldValue );
						UE_LOG( LogUsd, Log, TEXT( "Recording the removal of properties is not fully supported: Using underlying type '%s' for record of attribute '%s' of prim '%s', as we don't have access to the attribute's role" ),
							*ConvertedAttributeChange.AttributeTypeName,
							*ConvertedAttributeChange.PropertyName,
							*PrimPath
						);
					}

					// If the prim is the root, this is a stage info change, and PropertyName is actually a metadata key, so
					// it's not possible (or required) to know its SdfValueTypeName to e.g. undo/redo the creation of the property
					if ( !Prim.IsPseudoRoot() && ConvertedAttributeChange.PropertyName != TEXT( "kind" ) && !ConvertedAttributeChange.PropertyName.IsEmpty() &&
						 !ConvertedChange.Flags.bDidRemoveProperty && !ConvertedChange.Flags.bDidRemovePropertyWithOnlyRequiredFields )
					{
						if ( UE::FUsdAttribute Attribute = Prim.GetAttribute( *ConvertedAttributeChange.PropertyName ) )
						{
							ConvertedAttributeChange.AttributeTypeName = Attribute.GetTypeName().ToString();

							// USD doesn't tell us what changed, what type of change it was, or old/new values... so just save the entire timeSamples of the attribute so we can mirror via multi-user
							if ( ConvertedChange.Flags.bDidChangeAttributeTimeSamples )
							{
								Attribute.GetTimeSamples( ConvertedAttributeChange.TimeSamples );
								ConvertedAttributeChange.TimeValues.SetNum( ConvertedAttributeChange.TimeSamples.Num() );

								for ( int32 Index = 0; Index < ConvertedAttributeChange.TimeSamples.Num(); Index++ )
								{
									UE::FVtValue UsdValue;
									if ( Attribute.Get( UsdValue, ConvertedAttributeChange.TimeSamples[ Index ] ) )
									{
										UsdToUnreal::ConvertValue( UsdValue, ConvertedAttributeChange.TimeValues[ Index ] );
									}
								}
							}
						}
						else
						{
							UE_LOG( LogUsd, Warning, TEXT( "Failed to find attribute '%s' for prim at path '%s' when serializing object changes in transactor" ),
								*ConvertedAttributeChange.PropertyName,
								*PrimPath
							);
						}
					}
				}
			}
		}

		return true;
	}

	bool ApplyPrimChange( const UE::FSdfPath& PrimPath, const FTransactorObjectChange& PrimChange, UE::FUsdStage& Stage, EApplicationDirection Direction )
	{
		const bool bAdd = PrimChange.Flags.bDidAddInertPrim || PrimChange.Flags.bDidAddNonInertPrim;
		const bool bRemove = PrimChange.Flags.bDidRemoveInertPrim || PrimChange.Flags.bDidRemoveNonInertPrim;
		const bool bRename = PrimChange.Flags.bDidRename;

		if ( ( bAdd && Direction == EApplicationDirection::Forward ) || ( bRemove && Direction == EApplicationDirection::Reverse ) )
		{
			UE_LOG( LogUsd, Log, TEXT( "Creating prim '%s' with typename '%s'" ),
				*PrimPath.GetString(),
				*PrimChange.PrimTypeName
			);

			UE::FUsdPrim Prim = Stage.DefinePrim( PrimPath, *PrimChange.PrimTypeName );
			if ( !Prim )
			{
				return false;
			}

			return true;
		}
		else if ( ( bAdd && Direction == EApplicationDirection::Reverse ) || ( bRemove && Direction == EApplicationDirection::Forward ) )
		{
			UE_LOG( LogUsd, Log, TEXT( "Removing prim '%s' with typename '%s'" ),
				*PrimPath.GetString(),
				*PrimChange.PrimTypeName
			);

			UsdUtils::RemoveAllLocalPrimSpecs( Stage.GetPrimAtPath( PrimPath ), Stage.GetEditTarget() );
			return true;
		}
		else if ( bRename )
		{
			FString NewName;
			FString CurrentPath;

			if ( Direction == EApplicationDirection::Forward )
			{
				// It hasn't been renamed yet, so it's still at the old path
				CurrentPath = PrimChange.OldPath;
				NewName = PrimPath.GetElementString();
			}
			else
			{
				CurrentPath = PrimPath.GetString();
				NewName = UE::FSdfPath( *PrimChange.OldPath ).GetElementString();
			}

			// When redoing, we'll be using the old path, and USD sends it with all the variant selections in there
			// UsdUtils::RenamePrim can figure out the variant selections on its own, but we need to strip them here
			// to be able to GetPrimAtPath with this path
			UE::FSdfPath UsdCurrentPath = UE::FSdfPath( *CurrentPath ).StripAllVariantSelections();

			if ( UE::FUsdPrim Prim = Stage.GetPrimAtPath( UsdCurrentPath ) )
			{
				UE_LOG( LogUsd, Log, TEXT( "Renaming prim '%s' to '%s'" ),
					*Prim.GetPrimPath().GetString(),
					*NewName
				);

				const bool bDidRename = UsdUtils::RenamePrim( Prim, *NewName );
				if ( bDidRename )
				{
					return true;
				}
			}
			else if ( UE::FUsdPrim TargetPrim = Stage.GetPrimAtPath( UsdCurrentPath.ReplaceName( *NewName ) ) )
			{
				// We couldn't find a prim at the old path but found one at the new path, so just assume its the prim that we
				// wanted to rename anyway, as USD wouldn't have let us rename a prim onto an existing path in the first place.
				// This is useful because sometimes we may get multiple rename edits for the same prim in the same notice, like when we have
				// multiple specs per prim on the same layer.
				return true;
			}

			UE_LOG( LogUsd, Warning, TEXT( "Failed to rename prim at path '%s' to name '%s'" ),
				*CurrentPath,
				*NewName
			);
		}

		return false;
	}

	bool ApplyAttributeTimeSamples( const FTransactorAttributeChange& AttributeChange, UE::FUsdPrim& Prim )
	{
		if ( !Prim ||
			AttributeChange.TimeSamples.Num() == 0 ||
			AttributeChange.TimeSamples.Num() != AttributeChange.TimeValues.Num() ||
			AttributeChange.Field != TEXT( "timeSamples" ) )
		{
			return false;
		}

		// Try Getting first because we shouldn't trust our AttributeTypeName to always just CreateAttribute, as it may be just deduced from a value and be different
		UE::FUsdAttribute Attribute = Prim.GetAttribute( *AttributeChange.PropertyName );
		if ( !Attribute )
		{
			Attribute = Prim.CreateAttribute( *AttributeChange.PropertyName, *AttributeChange.AttributeTypeName );
		}
		if ( !Attribute )
		{
			return false;
		}

		// Clear all timesamples because we may have more timesamples than we receive, and we want our old ones to be removed
		// This corresponds to the token SdfFieldKeys->TimeSamples, and is extracted from UsdAttribute::Clear
		Attribute.ClearMetadata( TEXT( "timeSamples" ) );

		UE_LOG( LogUsd, Log, TEXT( "Applying '%d' timeSamples for attribute '%s' of prim '%s'" ),
			AttributeChange.TimeSamples.Num(),
			*AttributeChange.PropertyName,
			*Prim.GetPrimPath().GetString()
		);

		bool bSuccess = true;
		for ( int32 Index = 0; Index < AttributeChange.TimeSamples.Num(); Index++ )
		{
			UE::FVtValue Value;
			if ( UnrealToUsd::ConvertValue( AttributeChange.TimeValues[ Index ], Value ) )
			{
				if ( !Attribute.Set( Value, AttributeChange.TimeSamples[ Index ] ) )
				{
					UE_LOG(LogUsd, Warning, TEXT("Failed to apply value '%s' at timesample '%f' for attribute '%s' of prim '%s'"),
						*UsdUtils::Stringify( Value ),
						AttributeChange.TimeSamples[Index],
						*AttributeChange.PropertyName,
						*Prim.GetPrimPath().GetString()
					);
					bSuccess = false;
				}
			}
			else
			{
				UE_LOG( LogUsd, Warning, TEXT( "Failed to convert value for timesample '%f' for attribute '%s' of prim '%s'" ),
					AttributeChange.TimeSamples[ Index ],
					*AttributeChange.PropertyName,
					*Prim.GetPrimPath().GetString()
				);
				bSuccess = false;
			}
		}

		return bSuccess;
	}

	bool ApplyAttributeChange( const FString& PropertyName, const FString& Field, const FString& AttributeTypeName, bool bRemoveProperty, const FConvertedVtValue& Value, UE::FUsdPrim& Prim, TOptional<double> Time = {} )
	{
		if ( !Prim )
		{
			return false;
		}

		bool bCreated = false;

		UE::FUsdAttribute Attribute;
		if ( PropertyName != TEXT( "kind" ) ) // Kind is prim metadata, not an attribute
		{
			if ( bRemoveProperty )
			{
				Attribute = Prim.GetAttribute( *PropertyName );
				if ( !Attribute )
				{
					return true;
				}
			}
			else
			{
				bool bHadAttr = Prim.HasAttribute( *PropertyName );
				Attribute = Prim.CreateAttribute( *PropertyName, *AttributeTypeName );
				if ( !Attribute )
				{
					// We expect to fail to create an attribute if we have no typename here (e.g. undo remove property)
					if ( AttributeTypeName.IsEmpty() )
					{
						UE_LOG( LogUsd, Warning, TEXT( "Failed to create attribute '%s' with typename '%s' for prim '%s'" ),
							*PropertyName,
							*AttributeTypeName,
							*Prim.GetPrimPath().GetString()
						);
					}

					return false;
				}

				bCreated = !bHadAttr;
			}
		}

		UE::FVtValue WrapperValue;
		if ( !UnrealToUsd::ConvertValue( Value, WrapperValue ) )
		{
			UE_LOG( LogUsd, Warning, TEXT( "Failed to convert VtValue back to USD when applying it to attribute '%s' of prim '%s'" ),
				*PropertyName,
				*Prim.GetPrimPath().GetString()
			);
			return false;
		}

		UE_LOG( LogUsd, Log, TEXT( "%s property '%s' (typename '%s'), field '%s' of prim '%s' with value '%s' at time '%s'" ),
			bCreated ? TEXT("Creating") : WrapperValue.IsEmpty() ? bRemoveProperty ? TEXT("Removing") : TEXT( "Clearing" ) : TEXT( "Setting" ),
			*PropertyName,
			*AttributeTypeName,
			*Field,
			*Prim.GetPrimPath().GetString(),
			WrapperValue.IsEmpty() ? TEXT("<empty>") : *UsdUtils::Stringify( WrapperValue ),
			Time.IsSet() ? *LexToString( Time.GetValue() ) : TEXT( "<unset>" )
		);

		if ( PropertyName == TEXT( "kind" ) )
		{
#if USE_USD_SDK
			if ( Value.Entries.Num() == 1 && Value.Entries[ 0 ].Num() == 1 )
			{
				if ( const FString* KindString = Value.Entries[ 0 ][ 0 ].TryGet<FString>() )
				{
					if ( !IUsdPrim::SetKind( Prim, UnrealToUsd::ConvertToken( **KindString ).Get() ) )
					{
						UE_LOG( LogUsd, Warning, TEXT( "Failed to set Kind '%s' for prim '%s'" ), **KindString, *Prim.GetPrimPath().GetString() );
					}
				}
			}
			else
			{
				if ( !IUsdPrim::ClearKind( Prim ) )
				{
					UE_LOG( LogUsd, Warning, TEXT( "Failed to clear Kind for prim '%s'" ), *Prim.GetPrimPath().GetString() );
				}
			}
#endif // USE_USD_SDK
		}
		else if ( Field == TEXT( "default" ) )
		{
			if ( WrapperValue.IsEmpty() )
			{
				if ( bRemoveProperty )
				{
					Prim.RemoveProperty( *PropertyName );
				}
				if ( Time.IsSet() )
				{
					Attribute.ClearAtTime( Time.GetValue() );
				}
				else
				{
					Attribute.Clear();
				}
			}
			else
			{
				Attribute.Set( WrapperValue, Time );
			}
		}
		else // variability, colorSpace, etc.
		{
			if ( WrapperValue.IsEmpty() )
			{
				Attribute.Clear();
			}
			else
			{
				Attribute.SetMetadata( *Field, WrapperValue );
			}
		}

		return true;
	}

	bool ApplyStageMetadataChange( const FString& PropertyName, const FConvertedVtValue& Value, UE::FUsdStage& Stage )
	{
		if ( !Stage || PropertyName.IsEmpty() )
		{
			return false;
		}

		UE::FVtValue WrapperValue;
		if ( !UnrealToUsd::ConvertValue( Value, WrapperValue ) )
		{
			UE_LOG( LogUsd, Warning, TEXT( "Failed to convert VtValue back to USD when applying it to stage metadata field '%s'" ),
				*PropertyName
			);
			return false;
		}

		UE_LOG( LogUsd, Log, TEXT( "Setting stage metadata '%s', with value '%s'" ),
			*PropertyName,
			*UsdUtils::Stringify( WrapperValue )
		);

		UE::FSdfLayer OldEditTarget = Stage.GetEditTarget();
		Stage.SetEditTarget( Stage.GetRootLayer() ); // Stage metadata always needs to be set at the root layer
		{
			if ( WrapperValue.IsEmpty() )
			{
				Stage.ClearMetadata( *PropertyName );
			}
			else
			{
				Stage.SetMetadata( *PropertyName, WrapperValue );
			}
		}
		Stage.SetEditTarget( OldEditTarget );

		return true;
	}

	/** Applies the field value pairs to all prims on the stage, and returns a list of prim paths for modified prims */
	TArray<FString> ApplyFieldMapToStage( const FTransactorEditStorage& EditStorage, EApplicationDirection Direction, UE::FUsdStage& Stage, double Time )
	{
		if ( !Stage )
		{
			return {};
		}

		TArray<FString> PrimsChanged;

		int32 Start = 0;
		int32 End = 0;
		int32 Incr = 0;
		if ( Direction == EApplicationDirection::Forward )
		{
			Start = 0;
			End = EditStorage.Num();
			Incr = 1;
		}
		else
		{
			Start = EditStorage.Num() - 1;
			End = -1;
			Incr = -1;
		}

		UE::FSdfLayer OldEditTarget = Stage.GetEditTarget();
		FString LastEditTargetIdentifier;

		for ( int32 EditIndex = Start; EditIndex != End; EditIndex += Incr )
		{
			const FTransactorRecordedEdits& Edits = EditStorage[ EditIndex ];

			if ( Edits.EditTargetIdentifier != LastEditTargetIdentifier )
			{
#if USE_USD_SDK
				UE::FSdfLayer EditTarget = UsdUtils::FindLayerForIdentifier( *Edits.EditTargetIdentifier, Stage );
				if ( !EditTarget )
				{
					UE_LOG(LogUsd, Warning, TEXT("Ignoring application of recorded USD stage changes as the edit target with identifier '%s' cannot be found or opened"), *Edits.EditTargetIdentifier )
					continue;
				}

				Stage.SetEditTarget( EditTarget );
				LastEditTargetIdentifier = Edits.EditTargetIdentifier;
#endif // USE_USD_SDK
			}

			for ( const FTransactorRecordedEdit& Edit : Edits.Edits )
			{
				if ( Edit.ObjectChanges.Num() == 0 )
				{
					continue;
				}

				PrimsChanged.Add( Edit.ObjectPath );

				if ( Edit.ObjectPath == TEXT("/") )
				{
					for ( const FTransactorObjectChange& PrimChange : Edit.ObjectChanges )
					{
						for ( const FTransactorAttributeChange& AttributeChange : PrimChange.AttributeChanges )
						{
							ApplyStageMetadataChange(
								AttributeChange.PropertyName,
								Direction == EApplicationDirection::Forward ? AttributeChange.NewValue : AttributeChange.OldValue,
								Stage
							);
						}
					}
				}
				else
				{
					for ( const FTransactorObjectChange& PrimChange : Edit.ObjectChanges )
					{
						UE::FSdfPath PrimPath = UE::FSdfPath{ *Edit.ObjectPath };
						if ( ApplyPrimChange( PrimPath, PrimChange, Stage, Direction ) )
						{
							// If we managed to apply a prim change, we know there aren't any other attribute changes in the same ObjectChange
							continue;
						}

						UE::FUsdPrim Prim = Stage.GetPrimAtPath( PrimPath );
						if ( !Prim )
						{
							continue;
						}

						// Whether we should remove a property instead of clearing the opinion, when asked to apply an empty value
						bool bShouldRemove = Direction == EApplicationDirection::Forward
							? PrimChange.Flags.bDidRemoveProperty || PrimChange.Flags.bDidRemovePropertyWithOnlyRequiredFields
							: PrimChange.Flags.bDidAddProperty || PrimChange.Flags.bDidAddPropertyWithOnlyRequiredFields;

						// Can't block more than this, as Defining prims (from within ApplyPrimChange) needs to trigger its notices immediately,
						// and our changes/edits may depend on previous changes/edits triggering
						UE::FSdfChangeBlock ChangeBlock;

						if ( PrimChange.Flags.bDidChangeAttributeTimeSamples )
						{
							for ( const FTransactorAttributeChange& AttributeChange : PrimChange.AttributeChanges )
							{
								ApplyAttributeTimeSamples( AttributeChange, Prim );
							}
						}
						else
						{
							for ( const FTransactorAttributeChange& AttributeChange : PrimChange.AttributeChanges )
							{
								ApplyAttributeChange(
									AttributeChange.PropertyName,
									AttributeChange.Field,
									AttributeChange.AttributeTypeName,
									bShouldRemove,
									Direction == EApplicationDirection::Forward ? AttributeChange.NewValue : AttributeChange.OldValue,
									Prim
								);
							}
						}
					}
				}
			}
		}

		Stage.SetEditTarget( OldEditTarget );

		return PrimsChanged;
	}

	// Class aware of undo/redo/ConcertSync that handles serializing/applying the received FTransactorRecordedEdits data.
	// We need the awareness because we respond to undo from PreEditUndo, and respond to redo from PostEditUndo. This in turn because:
	// - In PreEditUndo we still have OldValues of the current transaction, and to undo we want to apply those OldValues to the stage;
	// - In PostEditUndo we have the NewValues of the next transaction, and to redo we want to apply those NewValues to the stage;
	// - ConcertSync always applies changes and then calls PostEditUndo, and to sync we want to apply those received NewValues to the stage.
	class FUsdTransactorImpl
	{
	public:
		FUsdTransactorImpl();
		~FUsdTransactorImpl();

		void Update( FTransactorRecordedEdits&& NewEdits );
		void Serialize( FArchive& Ar );
		void PreEditUndo( AUsdStageActor* StageActor );
		void PostEditUndo( AUsdStageActor* StageActor );

		// Returns whether the transaction buffer is currently in the middle of an Undo operation.
		// WARNING: This approach is only accurate if we're checking from within PreEditUndo/PostEditUndo/PostTransacted/Serialize (which we always do in this file)
		bool IsTransactionUndoing();

		// Returns whether the transaction buffer is currently in the middle of a Redo operation. Returns false when we're applying
		// a ConcertSync transaction, even though concert sync sort of works by applying transactions via Redo
		// WARNING: This approach is only accurate if we're checking from within PreEditUndo/PostEditUndo/PostTransacted/Serialize (which we always do in this file)
		bool IsTransactionRedoing();

		// Returns whether ConcertSync (multi-user) is currently applying a transaction received from the network
		bool IsApplyingConcertSyncTransaction() { return bApplyingConcertSync; };

	private:
		// This is called after *any* undo/redo transaction is finalized, so our LastFinalizedUndoCount is kept updated
		void HandleTransactionStateChanged( const FTransactionContext& InTransactionContext, const ETransactionStateEventType InTransactionState );
		int32 LastFinalizedUndoCount = 0;

		// We use this to detect when a ConcertSync transaction is starting, as it has a particular title
		void HandleBeforeOnRedoUndo( const FTransactionContext& TransactionContext );

		// We use this to detect when a ConcertSync transaction has ended, as it has a particular title
		void HandleOnRedo( const FTransactionContext& TransactionContext, bool bSucceeded );

	private:
		// Main data storage container
		FTransactorEditStorage Values;

		// We use these to stash our Values before they're overwritten by ConcertSync, and to restore them afterwards.
		// This is because when we receive a ConcertSync transaction the UUsdTransactor's values will be overwritten with the received data.
		// That is ok because we want to apply it to the stage, but after that we want to discard those values altogether, so that if *we*
		// undo, we won't undo the received transaction, but instead undo the last transaction that *we* made.
		FTransactorEditStorage StoredValues;

		// When ClientA undoes a change, it handles it's own undo changes from its PreEditUndo, but its final state after the undo transaction
		// is complete will have the *previous* OldValues/NewValues. This final state is what is sent over the network. ClientB that receives this
		// can't use these previous OldValues/NewValues to undo the change that ClientA just undone: It needs something else, which this
		// member provides. When ClientA starts to undo, it will stash it's *current* OldValues in here, and make sure they are visible when serialized
		// by ConcertSync. ClientB will receive these, and when available will apply those to the scene instead, undoing the same changes that ClientA undone.
		TOptional< FTransactorEditStorage > ReceivedValuesBeforeUndo;

		// During the same transaction we continuously append the received change info into the same storage. When the transaction changes, we clear it
		FGuid LastTransactionId;

		bool bApplyingConcertSync = false;
	};

	FUsdTransactorImpl::FUsdTransactorImpl()
	{
#if WITH_EDITOR
		if ( GEditor )
		{
			if ( UTransBuffer* Transactor = Cast<UTransBuffer>( GEditor->Trans ) )
			{
				Transactor->OnTransactionStateChanged().AddRaw( this, &FUsdTransactorImpl::HandleTransactionStateChanged );
				Transactor->OnBeforeRedoUndo().AddRaw( this, &FUsdTransactorImpl::HandleBeforeOnRedoUndo );
				Transactor->OnRedo().AddRaw( this, &FUsdTransactorImpl::HandleOnRedo );
			}
		}
#endif // WITH_EDITOR
	}

	FUsdTransactorImpl::~FUsdTransactorImpl()
	{
#if WITH_EDITOR
		if ( GEditor )
		{
			if ( UTransBuffer* Transactor = Cast<UTransBuffer>( GEditor->Trans ) )
			{
				Transactor->OnTransactionStateChanged().RemoveAll( this );
				Transactor->OnBeforeRedoUndo().RemoveAll( this );
				Transactor->OnRedo().RemoveAll( this );
			}
		}
#endif // WITH_EDITOR
	}

	void FUsdTransactorImpl::Update( FTransactorRecordedEdits&& NewEdits )
	{
		if ( !GUndo )
		{
			return;
		}

		// New transaction -> Start a new storage
		FTransactionContext Context = GUndo->GetContext();
		if ( Context.TransactionId != LastTransactionId )
		{
			LastTransactionId = Context.TransactionId;
			Values.Reset();
		}

		Values.Add( NewEdits );
	}

	void FUsdTransactorImpl::Serialize( FArchive& Ar )
	{
		Ar << Values;

		// If we have some ReceivedValuesBeforeUndo and the undo system is trying to overwrite it with its old version to apply the undo, keep our values instead!
		// We need this data to be with us whenever the ConcertSync serializes us to send it over the network during an undo, which happens shortly after this
		if ( Ar.IsTransacting() && Ar.IsLoading() && IsTransactionUndoing() && ReceivedValuesBeforeUndo.IsSet() )
		{
			TOptional<FTransactorEditStorage> Dummy = {};
			Ar << Dummy;
		}
		else
		{
			Ar << ReceivedValuesBeforeUndo;
		}
	}

	void FUsdTransactorImpl::PreEditUndo( AUsdStageActor* StageActor )
	{
		if ( !StageActor )
		{
			return;
		}

		if ( IsTransactionUndoing() )
		{
			// We can't respond to notices from the attribute that we'll set. Whatever changes setting the attribute causes in UE actors/components/assets
			// will already be accounted for by those actors/components/assets undoing/redoing by themselves via the UE transaction buffer.
			FScopedBlockNotices BlockNotices( StageActor->GetUsdListener() );

			UE::FUsdStage& Stage = StageActor->GetOrLoadUsdStage();

			TArray<FString> PrimsChanged = UsdUtils::ApplyFieldMapToStage(
				Values,
				UsdUtils::EApplicationDirection::Reverse,
				Stage,
				StageActor->GetTime()
			);

			if ( PrimsChanged.Num() > 0 )
			{
				for ( const FString& Prim : PrimsChanged )
				{
					StageActor->OnPrimChanged.Broadcast( Prim, false );
				}
			}

			// Make sure our OldValues survive the undo in case we need to send them over ConcertSync once the transaction is complete
			ReceivedValuesBeforeUndo = Values;
		}
		else
		{
			ReceivedValuesBeforeUndo.Reset();

			// ConcertSync calls PreEditUndo, then updates our data with the received data, then calls PostEditUndo
			if ( IsApplyingConcertSyncTransaction() )
			{
				// Make sure that our own Values survive when overwritten by values that we will receive from ConcertSync.
				// We'll restore this to our values once the ConcertSync action has finished applying
				StoredValues = Values;
			}
		}
	}

	void FUsdTransactorImpl::PostEditUndo( AUsdStageActor* StageActor )
	{
		const bool bIsRedoing = IsTransactionRedoing();
		const bool bIsApplyingConcertSync = IsApplyingConcertSyncTransaction();

		if ( StageActor && ( bIsRedoing || bIsApplyingConcertSync ) )
		{
			// If we're just redoing it's a bit of a waste to let the AUsdStageActor respond to notices from the fields that we'll set,
			// because any relevant changes caused to the level/assets would be redone by themselves if the actors/assets are also in the transaction
			// buffer. If we're receiving a ConcertSync transaction, however, we do want to respond to notices because transient actors/assets
			// aren't tracked by ConcertSync
			TOptional<FScopedBlockNotices> BlockNotices;
			if ( bIsRedoing )
			{
				BlockNotices.Emplace( StageActor->GetUsdListener() );
			}

			UE::FUsdStage& Stage = StageActor->GetOrLoadUsdStage();

			TArray<FString> PrimsChanged;
			if ( bIsApplyingConcertSync && ReceivedValuesBeforeUndo.IsSet() )
			{
				// If we're applying a received ConcertSync transaction that actually is an undo on the source client then we want to use it's ReceivedValuesBeforeUndo
				// to replicate the same undo that they did
				PrimsChanged = UsdUtils::ApplyFieldMapToStage(
					ReceivedValuesBeforeUndo.GetValue(),
					UsdUtils::EApplicationDirection::Reverse,
					Stage,
					StageActor->GetTime()
				);
			}
			else
			{
				// Just a common Redo operation or any other type of ConcertSync transaction, so just apply the new values
				PrimsChanged = UsdUtils::ApplyFieldMapToStage(
					Values,
					UsdUtils::EApplicationDirection::Forward,
					Stage,
					StageActor->GetTime()
				);
			}

			// If we're redoing or applying ConcertSync we don't want to end up with these values when the transaction finalizes as it could be replicated to other clients
			ReceivedValuesBeforeUndo.Reset();

			if ( PrimsChanged.Num() > 0 )
			{
				for ( const FString& Prim : PrimsChanged )
				{
					StageActor->OnPrimChanged.Broadcast( Prim, false );
				}
			}
		}

		if ( bIsApplyingConcertSync )
		{
			// If we're finishing applying a ConcertSync transaction, revert our Values to the state that they were before we received
			// the ConcertSync transaction. This is important so that if we undo now, we undo the last change that *we* made
			Values = StoredValues;
		}
	}

	bool FUsdTransactorImpl::IsTransactionUndoing()
	{
#if WITH_EDITOR
		if ( UTransBuffer* Transactor = Cast<UTransBuffer>( GEditor->Trans ) )
		{
			// We moved away from the end of the transaction buffer -> We're undoing
			if ( GIsTransacting && Transactor->UndoCount > LastFinalizedUndoCount )
			{
				return true;
			}
		}
#endif // WITH_EDITOR

		return false;
	}

	bool FUsdTransactorImpl::IsTransactionRedoing()
	{
#if WITH_EDITOR
		if ( UTransBuffer* Transactor = Cast<UTransBuffer>( GEditor->Trans ) )
		{
			// We moved towards the end of the transaction buffer -> We're redoing
			if ( GIsTransacting && Transactor->UndoCount < LastFinalizedUndoCount )
			{
				return true;
			}
		}
#endif // WITH_EDITOR

		return false;
	}

	void FUsdTransactorImpl::HandleTransactionStateChanged( const FTransactionContext& InTransactionContext, const ETransactionStateEventType InTransactionState )
	{
#if WITH_EDITOR
		if ( InTransactionState == ETransactionStateEventType::UndoRedoFinalized || InTransactionState == ETransactionStateEventType::TransactionFinalized )
		{
			if ( UTransBuffer* Transactor = Cast<UTransBuffer>( GEditor->Trans ) )
			{
				// Recording UndoCount works because UTransBuffer::Undo preemptively updates it *before* calling any object function, like PreEditUndo/PostEditUndo, so from
				// within PreEditUndo/PostEditUndo we will always have a delta from this value to the value that is recorded after any transaction was finalized,
				// which we record right here
				LastFinalizedUndoCount = Transactor->UndoCount;
			}
		}
#endif // WITH_EDITOR
	}

	// Must be the same as the title used within ConcertClientTransactionBridgeUtil::ProcessTransactionEvent
	const FText ConcertSyncTransactionTitle = LOCTEXT( "ConcertTransactionEvent", "Concert Transaction Event" );

	void FUsdTransactorImpl::HandleBeforeOnRedoUndo( const FTransactionContext& TransactionContext )
	{
		if ( TransactionContext.Title.EqualTo( ConcertSyncTransactionTitle ) )
		{
			bApplyingConcertSync = true;
		}
	}

	void FUsdTransactorImpl::HandleOnRedo( const FTransactionContext& TransactionContext, bool bSucceeded )
	{
		if ( bApplyingConcertSync && TransactionContext.Title.EqualTo( ConcertSyncTransactionTitle ) )
		{
			bApplyingConcertSync = false;
		}
	}
}

// Boilerplate to support Pimpl in an UObject
UUsdTransactor::UUsdTransactor( FVTableHelper& Helper )
	: Super( Helper )
{
}
UUsdTransactor::UUsdTransactor() = default;
UUsdTransactor::~UUsdTransactor() = default;

void UUsdTransactor::Initialize( AUsdStageActor* InStageActor )
{
	StageActor = InStageActor;

#if USE_USD_SDK
	if ( !IsTemplate() )
	{
		Impl = MakeUnique<UsdUtils::FUsdTransactorImpl>();
	}
#endif // USE_USD_SDK
}

void UUsdTransactor::Update( const UsdUtils::FObjectChangesByPath& NewInfoChanges, const UsdUtils::FObjectChangesByPath& NewResyncChanges )
{
	// We always send notices even when we're undoing/redoing changes (so that multi-user can broadcast them).
	// Make sure that we only ever update our OldValues/NewValues when we receive *new* updates though
	if ( Impl.IsValid() && ( Impl->IsTransactionUndoing() || Impl->IsTransactionRedoing() || Impl->IsApplyingConcertSyncTransaction() ) )
	{
		return;
	}

	// In case we close a stage in the same transaction where the actor is destroyed - our UE::FUsdStage could turn invalid at any point otherwise
	// Not much else we can do as this will get to us before the StageActor's destructor/Destroyed are called
	AUsdStageActor* StageActorPtr = StageActor.Get();
	if ( !StageActorPtr || StageActorPtr->IsActorBeingDestroyed() )
	{
		return;
	}

	Modify();

	const UE::FUsdStage& Stage = StageActorPtr->GetOrLoadUsdStage();
	if ( !Stage )
	{
		return;
	}

	const UE::FSdfLayer& EditTarget = Stage.GetEditTarget();

	UsdUtils::FTransactorRecordedEdits NewEdits;
	NewEdits.EditTargetIdentifier = EditTarget.GetIdentifier();

	UsdUtils::ConvertFieldValueMap( NewInfoChanges, Stage, NewEdits );
	UsdUtils::ConvertFieldValueMap( NewResyncChanges, Stage, NewEdits );

	Impl->Update( MoveTemp(NewEdits) );
}

void UUsdTransactor::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if ( Impl.IsValid() )
	{
		Impl->Serialize( Ar );
	}
	else
	{
		// In case we somehow serialize before we receive a valid Impl, and then later do receive one
		UsdUtils::FUsdTransactorImpl Dummy;
		Dummy.Serialize( Ar );
	}
}

#if WITH_EDITOR

void UUsdTransactor::PreEditUndo()
{
	if( Impl.IsValid() )
	{
		Impl->PreEditUndo( StageActor.Get() );
	}

	Super::PreEditUndo();
}

void UUsdTransactor::PostEditUndo()
{
	if ( Impl.IsValid() )
	{
		Impl->PostEditUndo( StageActor.Get() );
	}

	Super::PostEditUndo();
}
#endif // WITH_EDITOR

#undef LOCTEXT_NAMESPACE
