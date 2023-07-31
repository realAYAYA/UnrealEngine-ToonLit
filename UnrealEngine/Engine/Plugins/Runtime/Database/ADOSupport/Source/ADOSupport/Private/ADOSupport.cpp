// Copyright Epic Games, Inc. All Rights Reserved.

#include "ADOSupport.h"
#include "Database.h"

DEFINE_LOG_CATEGORY_STATIC(LogADODataBase, Log, All);

#ifdef PVS_STUDIO
// Bug in the MSVC preprocessor (as of VS2017 15.9.4) causes ICE when preprocessing files with import directives
#define USE_ADO_INTEGRATION 0
#else
// @todo clang: #import is not supported by Clang on Windows platform, but we currently need this for ADO symbol importing.  For now we disable ADO support in Clang builds.
#define USE_ADO_INTEGRATION (!PLATFORM_COMPILER_CLANG)
#endif

#if USE_ADO_INTEGRATION
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/AllowWindowsPlatformAtomics.h"

/*-----------------------------------------------------------------------------
	ADO integration for database connectivity
-----------------------------------------------------------------------------*/

// Using import allows making use of smart pointers easily. Please post to the list if a workaround such as
// using %COMMONFILES% works to hide the localization issues and non default program file folders.
//#import "C:\Program files\Common Files\System\Ado\msado15.dll" rename("EOF", "ADOEOF")

#pragma warning(push)
#pragma warning(disable: 4471) // a forward declaration of an unscoped enumeration must have an underlying type (int assumed)
#include "msado15.tlh"
#pragma warning(pop)

/*-----------------------------------------------------------------------------
	FADODataBaseRecordSet implementation.
-----------------------------------------------------------------------------*/

/**
 * ADO implementation of database record set.
 */
class FADODataBaseRecordSet : public FDataBaseRecordSet
{
private:
	ADODB::_RecordsetPtr ADORecordSet;

protected:
	/** Moves to the first record in the set. */
	virtual void MoveToFirst()
	{
		if( !ADORecordSet->BOF || !ADORecordSet->ADOEOF )
		{
			ADORecordSet->MoveFirst();
		}
	}
	/** Moves to the next record in the set. */
	virtual void MoveToNext()
	{
		if( !ADORecordSet->ADOEOF )
		{
			ADORecordSet->MoveNext();
		}
	}
	/**
	 * Returns whether we are at the end.
	 *
	 * @return true if at the end, false otherwise
	 */
	virtual bool IsAtEnd() const
	{
		return !!ADORecordSet->ADOEOF;
	}

public:

	/** 
	 *   Returns a count of the number of records in the record set
	 */
	virtual int32 GetRecordCount() const
	{
		return ADORecordSet->RecordCount;
	}

	/**
	 * Returns a string associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual FString GetString( const TCHAR* Column ) const
	{
		FString ReturnString;

		// Retrieve specified column field value for selected row.
		_variant_t Value = ADORecordSet->GetCollect( Column );
		// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
		if( Value.vt != VT_NULL )
		{
			ReturnString = (TCHAR*)_bstr_t(Value);
		}
		// Unknown column.
		else
		{
			ReturnString = TEXT("Unknown Column");
		}

		return ReturnString;
	}

	/**
	 * Returns an integer associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual int32 GetInt( const TCHAR* Column ) const
	{
		int32 ReturnValue = 0;

		// Retrieve specified column field value for selected row.
		_variant_t Value = ADORecordSet->GetCollect( Column );
		// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
		if( Value.vt != VT_NULL )
		{
			ReturnValue = (int32)Value;
		}
		// Unknown column.
		else
		{
			UE_LOG(LogADODataBase, Log, TEXT("Failure retrieving int32 value for column [%s]"),Column);
		}

		return ReturnValue;
	}

	/**
	 * Returns a float associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual float GetFloat( const TCHAR* Column ) const
	{
		float ReturnValue = 0;

		// Retrieve specified column field value for selected row.
		_variant_t Value = ADORecordSet->GetCollect( Column );
		// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
		if( Value.vt != VT_NULL )
		{
			ReturnValue = (float)Value;
		}
		// Unknown column.
		else
		{
			UE_LOG(LogADODataBase, Log, TEXT("Failure retrieving float value for column [%s]"),Column);
		}

		return ReturnValue;
	}

	/**
	 * Returns an int64 associated with the passed in field/ column for the current row.
	 *
	 * @param	Column	Name of column to retrieve data for in current row
	 */
	virtual int64 GetBigInt( const TCHAR* Column ) const
	{
		int64 ReturnValue = 0;

		// Retrieve specified column field value for selected row.
		_variant_t Value = ADORecordSet->GetCollect( Column );
		// Check variant type for validity and cast to specified type. _variant_t has overloaded cast operators.
		if( Value.vt != VT_NULL )
		{
			ReturnValue = (int64)Value;
		}
		// Unknown column.
		else
		{
			UE_LOG(LogADODataBase, Log, TEXT("Failure retrieving BIGINT value for column [%s]"),Column);
		}

		return ReturnValue;
	}

	/**
	 * Returns the set of column names for this Recordset.  This is useful for determining  
	 * what you can actually ask the record set for without having to hard code those ahead of time.  
	 */  
	virtual TArray<FDatabaseColumnInfo> GetColumnNames() const
	{  
		TArray<FDatabaseColumnInfo> Retval;  

		if( !ADORecordSet->BOF || !ADORecordSet->ADOEOF ) 
		{  
			ADORecordSet->MoveFirst();

			for( int16 i = 0; i < ADORecordSet->Fields->Count; ++i )  
			{  
				_bstr_t bstrName = ADORecordSet->Fields->Item[i]->Name;  
				_variant_t varValue = ADORecordSet->Fields->Item[i]->Value;  
				ADODB::DataTypeEnum DataType = ADORecordSet->Fields->Item[i]->Type;  

				FDatabaseColumnInfo NewInfo;  
				NewInfo.ColumnName = FString((TCHAR*)_bstr_t(bstrName));  

				// from http://www.w3schools.com/ado/prop_field_type.asp#datatypeenum  
				switch( DataType )  
				{  
				case ADODB::adInteger:  
				case ADODB::adBigInt:
					NewInfo.DataType = DBT_INT;  
					break;  
				case ADODB::adSingle:  
				case ADODB::adDouble:  
					NewInfo.DataType = DBT_FLOAT;  
					break;  

				case ADODB::adWChar:
				case ADODB::adVarWChar:
					NewInfo.DataType = DBT_STRING;
					break;

				default:  
					UE_LOG(LogADODataBase, Warning, TEXT("Unable to find a EDataBaseUE3Types (%s) from DODB::DataTypeEnum DataType: %d "), *NewInfo.ColumnName, static_cast<int32>(DataType) );
					NewInfo.DataType = DBT_UNKOWN;  
					break;  
				}  


				Retval.Add( NewInfo );  
			}  
		}  

		// here for debugging as this code is new.
		for( int32 i = 0; i < Retval.Num(); ++i )
		{  
			UE_LOG(LogADODataBase, Warning, TEXT( "ColumnName %d: Name: %s  Type: %d"), i, *Retval[i].ColumnName, static_cast<int32>(Retval[i].DataType) );
		}  

		return Retval;  
	}   


	/**
	 * Constructor, used to associate ADO record set with this class.
	 *
	 * @param InADORecordSet	ADO record set to use
	 */
	FADODataBaseRecordSet( ADODB::_RecordsetPtr InADORecordSet )
	:	ADORecordSet( InADORecordSet )
	{
	}

	/** Destructor, cleaning up ADO record set. */
	virtual ~FADODataBaseRecordSet()
	{
		if(ADORecordSet != nullptr && (ADORecordSet->State & ADODB::adStateOpen))
		{
			// We're using smart pointers so all we need to do is close and assign NULL.
			ADORecordSet->Close();
		}

		ADORecordSet = NULL;
	}
};


/*-----------------------------------------------------------------------------
	FADODataBaseConnection implementation.
-----------------------------------------------------------------------------*/

/**
 * Data base connection class using ADO C++ interface to communicate with SQL server.
 */
class FADODataBaseConnection : public FDataBaseConnection
{
private:
	/** ADO database connection object. */
	ADODB::_ConnectionPtr DataBaseConnection;

public:
	/** Constructor, initializing all member variables. */
	FADODataBaseConnection()
	{
		DataBaseConnection = NULL;
	}

	/** Destructor, tearing down connection. */
	virtual ~FADODataBaseConnection()
	{
		Close();
	}

	/**
	 * Opens a connection to the database.
	 *
	 * @param	ConnectionString	Connection string passed to database layer
	 * @param   RemoteConnectionIP  The IP address which the RemoteConnection should connect to
	 * @param   RemoteConnectionStringOverride  The connection string which the RemoteConnection is going to utilize
	 *
	 * @return	true if connection was successfully established, false otherwise
	 */
	virtual bool Open( const TCHAR* ConnectionString, const TCHAR* RemoteConnectionIP, const TCHAR* RemoteConnectionStringOverride )
	{
		if (!FWindowsPlatformMisc::CoInitialize())
		{
			return false;
		}

		// Create instance of DB connection object.
		HRESULT hr = DataBaseConnection.CreateInstance(__uuidof(ADODB::Connection));
		if (FAILED(hr))
		{
			FWindowsPlatformMisc::CoUninitialize();
			throw _com_error(hr);
		}

		// Open the connection. Operation is synchronous.
		DataBaseConnection->Open( ConnectionString, TEXT(""), TEXT(""), ADODB::adConnectUnspecified );

		return true;
	}

	/**
	 * Closes connection to database.
	 */
	virtual void Close()
	{
		// Close database connection if exists and free smart pointer.
		if( DataBaseConnection != nullptr && (DataBaseConnection->State & ADODB::adStateOpen))
		{
			DataBaseConnection->Close();

			FWindowsPlatformMisc::CoUninitialize();
		}

		DataBaseConnection = NULL;
	}

	/**
	 * Executes the passed in command on the database.
	 *
	 * @param CommandString		Command to execute
	 *
	 * @return true if execution was successful, false otherwise
	 */
	virtual bool Execute( const TCHAR* CommandString )
	{
		// Execute command, passing in optimization to tell DB to not return records.
		DataBaseConnection->Execute( CommandString, NULL, ADODB::adExecuteNoRecords );

		return true;
	}

	/**
	 * Executes the passed in command on the database. The caller is responsible for deleting
	 * the created RecordSet.
	 *
	 * @param CommandString		Command to execute
	 * @param RecordSet			Reference to recordset pointer that is going to hold result
	 *
	 * @return true if execution was successful, false otherwise
	 */
	virtual bool Execute( const TCHAR* CommandString, FDataBaseRecordSet*& RecordSet )
	{
		// Initialize return value.
		RecordSet = NULL;

		// Create instance of record set.
		ADODB::_RecordsetPtr ADORecordSet = NULL;
		ADORecordSet.CreateInstance(__uuidof(ADODB::Recordset) );
				
		// Execute the passed in command on the record set. The recordset returned will be in open state so you can call Get* on it directly.
		ADORecordSet->Open( CommandString, _variant_t((IDispatch *) DataBaseConnection), ADODB::adOpenStatic, ADODB::adLockReadOnly, ADODB::adCmdText );

		// Create record set from returned data.
		RecordSet = new FADODataBaseRecordSet( ADORecordSet );

		return RecordSet != NULL;
	}
};

#include "Windows/HideWindowsPlatformAtomics.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif // USE_ADO_INTEGRATION

class FADOSupport : public IADOSupport
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** IADOSupport implementation */
	virtual FDataBaseConnection* CreateInstance() const override;
};

IMPLEMENT_MODULE(FADOSupport, ADOSupport)

void FADOSupport::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}

void FADOSupport::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

FDataBaseConnection* FADOSupport::CreateInstance() const
{
#if USE_ADO_INTEGRATION
	return new FADODataBaseConnection();
#else
	return new FDataBaseConnection();
#endif
}
