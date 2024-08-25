// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/View/Column/SelectionViewerColumns.h"

#include "ConcertFrontendStyle.h"
#include "Replication/Editor/Model/IEditableReplicationStreamModel.h"
#include "Replication/Editor/Model/ReplicatedPropertyData.h"
#include "Replication/Editor/Model/ReplicatedObjectData.h"
#include "Replication/Editor/View/DisplayUtils.h"
#include "Replication/Editor/View/Column/ReplicationColumnsUtils.h"
#include "Replication/PropertyChainUtils.h"

#include "Internationalization/Internationalization.h"
#include "Replication/Editor/View/Column/IObjectTreeColumn.h"
#include "Textures/SlateIcon.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "ReplicationObjectColumns"

namespace UE::ConcertSharedSlate::ReplicationColumns::TopLevel
{
	const FName IconColumnId = TEXT("IconColumn");
	const FName LabelColumnId = TEXT("LabelColumn");
	const FName TypeColumnId = TEXT("TypeColumn");
	
	FObjectColumnEntry LabelColumn(TSharedRef<IReplicationStreamModel> Model, IObjectNameModel* OptionalNameModel)
	{
		class FLabelColumn_Object : public IObjectTreeColumn
		{
		public:
			
			FLabelColumn_Object(TSharedRef<IReplicationStreamModel> Model, IObjectNameModel* OptionalNameModel)
				: Model(MoveTemp(Model))
				, OptionalNameModel(OptionalNameModel)
			{}

			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(LabelColumnId)
					.DefaultLabel(LOCTEXT("LabelColumnLabel", "Label"))
					.FillSized(FConcertFrontendStyle::Get()->GetFloat("Concert.Replication.Tree.Object.LabelRowWidth"));
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				const FReplicatedObjectData& ObjectData = InArgs.RowItem.RowData;
				const FText Text = GetDisplayText(ObjectData);
					
				return SNew(SHorizontalBox)
					.ToolTipText(FText::FromString(ObjectData.GetObjectPath().ToString()))
					
					+SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					[
						SNew(SImage)
						.Image(DisplayUtils::GetObjectIcon(*Model, ObjectData.GetObjectPath()).GetOptionalIcon())
					]
					
					+SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(6.f, 0.f, 0.f, 0.f)
					[
						SNew(STextBlock)
						.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = InArgs.HighlightText](){ return *HighlightText; }))
						.Text(Text)
					];
			}
			
			virtual void PopulateSearchString(const FObjectTreeRowContext& InItem, TArray<FString>& InOutSearchStrings) const override
			{
				const FReplicatedObjectData& ObjectData = InItem.RowData;
				InOutSearchStrings.Add(DisplayUtils::GetObjectDisplayText(ObjectData.GetObjectPath(), OptionalNameModel).ToString());
			}
			
			virtual bool CanBeSorted() const override { return true; } 
			virtual bool IsLessThan(const FObjectTreeRowContext& Left, const FObjectTreeRowContext& Right) const override
			{
				return GetDisplayText(Left.RowData).ToString() < GetDisplayText(Right.RowData).ToString();
			}

		private:
			
			const TSharedRef<IReplicationStreamModel> Model;
			IObjectNameModel* const OptionalNameModel;
			
			FText GetDisplayText(const FReplicatedObjectData& ObjectData) const
			{
				const FSoftObjectPath& ObjectPath = ObjectData.GetObjectPath();
				return DisplayUtils::GetObjectDisplayText(ObjectPath, OptionalNameModel);
			}
		};

		return {
			TReplicationColumnDelegates<FObjectTreeRowContext>::FCreateColumn::CreateLambda([Model = MoveTemp(Model), OptionalNameModel]()
			{
				return MakeShared<FLabelColumn_Object>(Model, OptionalNameModel);
			}),
			LabelColumnId,
			{ static_cast<int32>(ETopLevelColumnOrder::Label) }
		};
	}
	
	FObjectColumnEntry TypeColumn(TSharedRef<IReplicationStreamModel> Model)
	{
		class FLabelColumn_Type : public IObjectTreeColumn
		{
		public:
			
			FLabelColumn_Type(TSharedRef<IReplicationStreamModel> Model)
				: Model(MoveTemp(Model))
			{}

			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(TypeColumnId)
					.DefaultLabel(LOCTEXT("TypeColumnLabel", "Type"))
					.FillWidth(1.f);
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				return SNew(SBox)
					.Padding(8, 0, 0, 0) // So the type name text is aligned with the header column text
					[
						SNew(STextBlock)
						.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = InArgs.HighlightText](){ return *HighlightText; }))
						.Text(DisplayUtils::GetObjectTypeText(*Model, InArgs.RowItem.RowData.GetObjectPath()))
					];
			}
			
			virtual void PopulateSearchString(const FObjectTreeRowContext& InItem, TArray<FString>& InOutSearchStrings) const override
			{
				InOutSearchStrings.Add(DisplayUtils::GetObjectTypeText(Model.Get(), InItem.RowData.GetObjectPath()).ToString());
			}
			
			virtual bool CanBeSorted() const override { return true; } 
			virtual bool IsLessThan(const FObjectTreeRowContext& Left, const FObjectTreeRowContext& Right) const override
			{
				return DisplayUtils::GetObjectTypeText(*Model, Left.RowData.GetObjectPath()).ToString()
					< DisplayUtils::GetObjectTypeText(*Model, Right.RowData.GetObjectPath()).ToString();
			}

		private:
			
			const TSharedRef<IReplicationStreamModel> Model;
		};

		return {
			TReplicationColumnDelegates<FObjectTreeRowContext>::FCreateColumn::CreateLambda([Model = MoveTemp(Model)]()
			{
				return MakeShared<FLabelColumn_Type>(Model);
			}),
			TypeColumnId,
			{ static_cast<int32>(ETopLevelColumnOrder::Type) }
		};
	}
}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "ReplicationPropertyColumns"

namespace UE::ConcertSharedSlate::ReplicationColumns::Property
{
	const FName ReplicatesColumnId = TEXT("ReplicatedColumn");
	const FName LabelColumnId = TEXT("LabelColumn");
	const FName TypeColumnId = TEXT("TypeColumn");
	
	FPropertyColumnEntry LabelColumn()
	{
		class FLabelColumn_Property : public IPropertyTreeColumn
		{
		public:

			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(LabelColumnId)
					.DefaultLabel(LOCTEXT("LabelColumnLabel", "Label"))
					.FillSized(FConcertFrontendStyle::Get()->GetFloat("Concert.Replication.Tree.Property.LabelRowWidth"));
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				const FReplicatedPropertyData& PropertyData = InArgs.RowItem.RowData;
				return SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
					.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = InArgs.HighlightText](){ return *HighlightText; }))
					.Text(DisplayUtils::GetPropertyDisplayText(PropertyData.GetProperty(), ResolveOrLoadClass(PropertyData)));
			}
			
			virtual void PopulateSearchString(const FPropertyTreeRowContext& InItem, TArray<FString>& InOutSearchStrings) const override
			{
				const FReplicatedPropertyData& PropertyData = InItem.RowData;
				FString DisplayString = DisplayUtils::GetPropertyDisplayString(InItem.RowData.GetProperty(), ResolveOrLoadClass(PropertyData));
				InOutSearchStrings.Emplace(MoveTemp(DisplayString));
			}
			
			virtual bool CanBeSorted() const override { return true; } 
			virtual bool IsLessThan(const FPropertyTreeRowContext& Left, const FPropertyTreeRowContext& Right) const override
			{
				return DisplayUtils::GetPropertyDisplayString(Left.RowData.GetProperty(), ResolveOrLoadClass(Left.RowData))
					< DisplayUtils::GetPropertyDisplayString(Right.RowData.GetProperty(), ResolveOrLoadClass(Right.RowData));
			}

		private:

			static UClass* ResolveOrLoadClass(const FReplicatedPropertyData& PropertyData)
			{
#if WITH_EDITOR
				// On editor there may be Blueprints that need loading...
				return PropertyData.GetOwningClass().TryLoadClass<UObject>();
#else
				// ... but on the server everything should be native C++ and hence already loaded
				return PropertyData.GetOwningClass().ResolveClass();
#endif
			}
		};
		
		return {
			TReplicationColumnDelegates<FPropertyTreeRowContext>::FCreateColumn::CreateLambda([]()
			{
				return MakeShared<FLabelColumn_Property>();
			}),
			LabelColumnId,
			{ static_cast<int32>(EReplicationPropertyColumnOrder::Label) }
		};
	}
	
	FPropertyColumnEntry TypeColumn()
	{
		class FTypeColumn_Property : public IPropertyTreeColumn
		{
		public:

			virtual SHeaderRow::FColumn::FArguments CreateHeaderRowArgs() const override
			{
				return SHeaderRow::Column(TypeColumnId)
					.DefaultLabel(LOCTEXT("TypeColumnLabel", "Type"))
					.FillWidth(1.f);
			}
			
			virtual TSharedRef<SWidget> GenerateColumnWidget(const FBuildArgs& InArgs) override
			{
				const FReplicatedPropertyData& PropertyData = InArgs.RowItem.RowData;
				return SNew(SBox)
					.Padding(8, 0, 0, 0) // So the type name text is aligned with the header column text
					[
						SNew(STextBlock)
						.Font(FAppStyle::GetFontStyle("PropertyWindow.NormalFont"))
						.HighlightText(TAttribute<FText>::CreateLambda([HighlightText = InArgs.HighlightText](){ return *HighlightText; }))
						.Text(GetDisplayText(InArgs.RowItem.RowData))
					];
			}
			
			virtual void PopulateSearchString(const FPropertyTreeRowContext& InItem, TArray<FString>& InOutSearchStrings) const override
			{
				InOutSearchStrings.Add(GetDisplayText(InItem.RowData).ToString());
			}
			
			virtual bool CanBeSorted() const override { return true; } 
			virtual bool IsLessThan(const FPropertyTreeRowContext& Left, const FPropertyTreeRowContext& Right) const override
			{
				return GetDisplayText(Left.RowData).ToString() < GetDisplayText(Right.RowData).ToString();
			}

		private:
			
			static FText GetDisplayText(const FReplicatedPropertyData& Args)
			{
				UClass* Class = Args.GetOwningClass().TryLoadClass<UObject>();
				const FProperty* Property = Class ? ConcertSyncCore::PropertyChain::ResolveProperty(*Class, Args.GetProperty()) : nullptr;
				return Property ? FText::FromString(Property->GetCPPType()) : LOCTEXT("Unknown", "Unknown");	
			}
		};
		
		return {
			TReplicationColumnDelegates<FPropertyTreeRowContext>::FCreateColumn::CreateLambda([]()
			{
				return MakeShared<FTypeColumn_Property>();
			}),
			TypeColumnId,
			{ static_cast<int32>(EReplicationPropertyColumnOrder::Type) }
		};
	}
}

#undef LOCTEXT_NAMESPACE