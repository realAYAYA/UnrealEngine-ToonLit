// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include <functional>

#include "CoreMinimal.h"
#include "Misc/TVariant.h"

#include "Insights/Common/SimpleRtti.h"

class FSpawnTabArgs;
class SDockTab;
class SWidget;

namespace UE
{
namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

enum class EFilterDataType : uint32
{
	Int64,
	Double,
	String,
	StringInt64Pair, // Displayed as a string but translates to a Int64 key.
};

////////////////////////////////////////////////////////////////////////////////////////////////////

enum class EFilterOperator : uint8
{
	Eq = 0, // Equals
	NotEq = 1, // Not Equals
	Lt = 2, // Less Than
	Lte = 3, // Less than or equal to
	Gt = 4, // Greater than
	Gte = 5, // Greater than or equal to
	Contains = 6,
	NotContains = 7,
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class IFilterOperator
{
public:
	virtual EFilterOperator GetKey() const = 0;
	virtual FString GetName() const = 0;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
class FFilterOperator : public IFilterOperator
{
public:
	typedef TFunction<bool(T, T)> FOperatorFunc;

public:
	FFilterOperator(EFilterOperator InKey, FString InName, FOperatorFunc InFunc)
		: Key(InKey)
		, Name(InName)
		, Func(InFunc)
	{
	}

	virtual ~FFilterOperator()
	{
	}

	virtual EFilterOperator GetKey() const override { return Key; }
	virtual FString GetName() const override { return Name; };

	bool Apply(T InValueA, T InValueB) const { return Func(InValueA, InValueB); }

private:
	EFilterOperator Key;
	FString Name;
	FOperatorFunc Func;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

enum class EFilterGroupOperator
{
	And = 0,
	Or = 1,
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FFilterGroupOperator
{
public:
	FFilterGroupOperator(EFilterGroupOperator InType, FText InName, FText InDesc)
		: Type(InType)
		, Name(InName)
		, Desc(InDesc)
	{
	}

	EFilterGroupOperator GetType() const { return Type; }
	const FText& GetName() const { return Name; }
	const FText& GetDesc() const { return Desc; }

private:
	EFilterGroupOperator Type;
	FText Name;
	FText Desc;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class IFilterValueConverter
{
public:
	virtual bool Convert(const FString& Input, int64& Output, FText& OutError) const { unimplemented(); return false; }
	virtual bool Convert(const FString& Input, double& Output, FText& OutError) const { unimplemented(); return false; }
	virtual FText GetTooltipText() const { return FText(); }
	virtual FText GetHintText() const { return FText(); }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

typedef TSharedPtr<const TArray<TSharedPtr<IFilterOperator>>> SupportedOperatorsArrayPtr;

class FFilter
{
	INSIGHTS_DECLARE_RTTI_BASE(FFilter)

public:
	FFilter(int32 InKey, FText InName, FText InDesc, EFilterDataType InDataType, TSharedPtr<IFilterValueConverter> InConverter, SupportedOperatorsArrayPtr InSupportedOperators)
		: Key(InKey)
		, Name(InName)
		, Desc(InDesc)
		, DataType(InDataType)
		, Converter(InConverter)
		, SupportedOperators(InSupportedOperators)
	{
	}

	virtual ~FFilter()
	{
	}

	int32 GetKey() const { return Key; }
	const FText& GetName() const { return Name; }
	const FText& GetDesc() const { return Desc; }
	EFilterDataType GetDataType() const { return DataType; }
	const TSharedPtr<IFilterValueConverter>& GetConverter() const { return Converter; }
	SupportedOperatorsArrayPtr GetSupportedOperators() const { return SupportedOperators; }

private:
	int32 Key;
	FText Name;
	FText Desc;
	EFilterDataType DataType;
	TSharedPtr<IFilterValueConverter> Converter;
	SupportedOperatorsArrayPtr SupportedOperators;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FFilterWithSuggestions : public FFilter
{
	INSIGHTS_DECLARE_RTTI(FFilterWithSuggestions, FFilter)

public:
	typedef TFunction<void(const FString& /*Text*/, TArray<FString>& OutSuggestions)> FGetSuggestionsCallback;

public:
	FFilterWithSuggestions(int32 InKey, FText InName, FText InDesc, EFilterDataType InDataType, TSharedPtr<IFilterValueConverter> InConverter, SupportedOperatorsArrayPtr InSupportedOperators)
		: FFilter(InKey, InName, InDesc, InDataType, InConverter, InSupportedOperators)
	{
	}

	virtual ~FFilterWithSuggestions()
	{
	}

	const FGetSuggestionsCallback& GetCallback() const { return Callback; }
	void SetCallback(FGetSuggestionsCallback InCallback) { Callback = InCallback; }
	void GetSuggestions(const FString& Text, TArray<FString>& OutSuggestions) { Callback(Text, OutSuggestions); }

private:
	FGetSuggestionsCallback Callback;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FFilterStorage
{
public:
	FFilterStorage();
	~FFilterStorage();

	const TArray<TSharedPtr<FFilterGroupOperator>>& GetFilterGroupOperators() const { return FilterGroupOperators; }

	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> GetDoubleOperators() const { return DoubleOperators; }
	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> GetIntegerOperators() const { return IntegerOperators; }
	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> GetStringOperators() const { return StringOperators; }

private:
	TArray<TSharedPtr<FFilterGroupOperator>> FilterGroupOperators;

	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> DoubleOperators;
	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> IntegerOperators;
	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> StringOperators;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FFilterService
{
public:
	FFilterService();
	~FFilterService();

	static void Initialize();
	static void Shutdown();
	static TSharedPtr<FFilterService> Get() { return Instance; }

	void RegisterTabSpawner();
	void UnregisterTabSpawner();
	TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& Args);

	const TArray<TSharedPtr<FFilterGroupOperator>>& GetFilterGroupOperators() const { return FilterStorage.GetFilterGroupOperators(); }

	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> GetDoubleOperators() const { return FilterStorage.GetDoubleOperators(); }
	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> GetIntegerOperators() const { return FilterStorage.GetIntegerOperators(); }
	TSharedPtr<TArray<TSharedPtr<IFilterOperator>>> GetStringOperators() const { return FilterStorage.GetStringOperators(); }

	TSharedPtr<SWidget> CreateFilterConfiguratorWidget(TSharedPtr<class FFilterConfigurator> FilterConfiguratorViewModel);

private:
	static const FName FilterConfiguratorTabId;

	static TSharedPtr<FFilterService> Instance;
	FFilterStorage FilterStorage;

	TSharedPtr<class SAdvancedFilter> PendingWidget;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

class FFilterContext
{
public:
	typedef TVariant<double, int64, FString> ContextData;

public:
	template<typename T>
	void AddFilterData(int32 Key, const T& InData)
	{
		ContextData VariantData;
		VariantData.Set<T>(InData);
		DataMap.Add(Key, VariantData);
	}

	template<typename T>
	void SetFilterData(int32 Key, const T& InData)
	{
		DataMap[Key].Set<T>(InData);
	}

	template<typename T>
	void GetFilterData(int32 Key, T& OutData) const
	{
		const ContextData* Data = DataMap.Find(Key);
		check(Data);

		check(Data->IsType<T>());
		OutData = Data->Get<T>();
	}

	bool HasFilterData(int32 Key) const
	{
		return DataMap.Contains(Key);
	}

	bool GetReturnValueForUnsetFilters() const { return bReturnValueForUnsetFilters; }
	void SetReturnValueForUnsetFilters(bool InValue) { bReturnValueForUnsetFilters = InValue; }

private:
	TMap<int32, ContextData> DataMap;
	bool bReturnValueForUnsetFilters = true;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
} // namespace UE
