// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "SQLitePreparedStatement.h"

/**
 * Modes used when opening a database.
 */
enum class ESQLiteDatabaseOpenMode : uint8
{
	/** Open the database in read-only mode. Fails if the database doesn't exist. */
	ReadOnly,

	/** Open the database in read-write mode if possible, or read-only mode if the database is write protected. Fails if the database doesn't exist. */
	ReadWrite,

	/** Open the database in read-write mode if possible, or read-only mode if the database is write protected. Attempts to create the database if it doesn't exist. */
	ReadWriteCreate,
};

/**
 * Wrapper around an SQLite database.
 * @see sqlite3.
 */
class SQLITECORE_API FSQLiteDatabase
{
public:
	/** Construction/Destruction */
	FSQLiteDatabase();
	~FSQLiteDatabase();

	/** Non-copyable */
	FSQLiteDatabase(const FSQLiteDatabase&) = delete;
	FSQLiteDatabase& operator=(const FSQLiteDatabase&) = delete;

	/** Movable */
	FSQLiteDatabase(FSQLiteDatabase&& Other);
	FSQLiteDatabase& operator=(FSQLiteDatabase&& Other);

	/**
	 * Is this a valid SQLite database? (ie, has been successfully opened).
	 */
	bool IsValid() const;

	/**
	 * Open (or create) an SQLite database file.
	 */
	bool Open(const TCHAR* InFilename, const ESQLiteDatabaseOpenMode InOpenMode = ESQLiteDatabaseOpenMode::ReadWriteCreate);

	/**
	 * Close an open SQLite database file.
	 */
	bool Close();

	/**
	 * Get the filename of the currently open database, or an empty string.
	 * @note The returned filename will be an absolute pathname.
	 */
	FString GetFilename() const;

	/**
	 * Get the application ID set in the database header.
	 * @note A list of assigned application IDs can be seen by consulting the magic.txt file in the SQLite source repository.
	 * @return true if the get was a success.
	 */
	bool GetApplicationId(int32& OutApplicationId) const;

	/**
	 * Set the application ID in the database header.
	 * @note A list of assigned application IDs can be seen by consulting the magic.txt file in the SQLite source repository.
	 * @return true if the set was a success.
	 */
	bool SetApplicationId(const int32 InApplicationId);

	/**
	 * Get the user version set in the database header.
	 * @return true if the get was a success.
	 */
	bool GetUserVersion(int32& OutUserVersion) const;

	/**
	 * Set the user version in the database header.
	 * @return true if the set was a success.
	 */
	bool SetUserVersion(const int32 InUserVersion);

	/**
	 * Execute a statement that requires no result state.
	 * @note For statements that require a result, or that you wish to reuse repeatedly (including using bindings), you should consider using FSQLitePreparedStatement.
	 * @return true if the execution was a success.
	 */
	bool Execute(const TCHAR* InStatement);

	/**
	 * Execute a statement and enumerate the result state.
	 * @note For statements that require a result, or that you wish to reuse repeatedly (including using bindings), you should consider using FSQLitePreparedStatement.
	 * @return The number of rows enumerated (which may be less than the number of rows returned if ESQLitePreparedStatementExecuteRowResult::Stop is returned during enumeration), or INDEX_NONE if an error occurred (including returning ESQLitePreparedStatementExecuteRowResult::Error during enumeration).
	 */
	int64 Execute(const TCHAR* InStatement, TFunctionRef<ESQLitePreparedStatementExecuteRowResult(const FSQLitePreparedStatement&)> InCallback);

	/**
	 * Prepare a statement for manual processing.
	 * @note This is the same as using the FSQLitePreparedStatement constructor, but won't assert if the current database is invalid (not open).
	 * @return A prepared statement object (check IsValid on the result).
	 */
	FSQLitePreparedStatement PrepareStatement(const TCHAR* InStatement, const ESQLitePreparedStatementFlags InFlags = ESQLitePreparedStatementFlags::None);

	/**
	 * Prepare a statement defined by SQLITE_PREPARED_STATEMENT for manual processing.
	 * @note This is the same as using the T constructor, but won't assert if the current database is invalid (not open).
	 * @return A prepared statement object (check IsValid on the result).
	 */
	template <typename T>
	T PrepareStatement(const ESQLitePreparedStatementFlags InFlags = ESQLitePreparedStatementFlags::None)
	{
		return Database
			? T(*this, InFlags)
			: T();
	}

	/**
	 * Get the last error reported by this database.
	 */
	FString GetLastError() const;

	/**
	 * Get the rowid of the last successful INSERT statement on any table in this database.
	 * @see sqlite3_last_insert_rowid
	 */
	int64 GetLastInsertRowId() const;

	/** Performs a quick check on the integrity of the database, returns true if everything is ok. */
	bool PerformQuickIntegrityCheck() const;

private:
	friend class FSQLitePreparedStatement;

	/** Internal SQLite database handle */
	struct sqlite3* Database;
};
