// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "Widgets/SCompoundWidget.h"

namespace UE::ConcertSharedSlate
{
	/**
	 * Widget which views a replication stream.
	 * @see ReplicationWidgetFactories.h
	 */
	class CONCERTSHAREDSLATE_API IReplicationStreamViewer : public SCompoundWidget
	{
	public:

		/** Call after the data underlying the model was externally changed and needs to be redisplayed in the UI. */
		virtual void Refresh() = 0;

		/**
		 * Requests that column be resorted; the column is in the top object view.
		 * This is to be called in response to a column's content changing. The rows will be resorted if the given column has a sort priority assigned.
		 */
		virtual void RequestObjectColumnResort(const FName& ColumnId) = 0;
		/**
		 * Requests that column be resorted; the column is in the bottom property view.
		 * This is to be called in response to a column's content changing. The rows will be resorted if the given column has a sort priority assigned.
		 */
		virtual void RequestPropertyColumnResort(const FName& ColumnId) = 0;

		/**
		 * @return The objects for which the properties are being edited / displayed.
		 * If there is an IReplicationSubobjectView, this is IReplicationSubobjectView::GetSelectedObjects.
		 * Otherwise it is IReplicationStreamViewer::GetSelectedTopLevelObjects.
		 */
		virtual TArray<FSoftObjectPath> GetObjectsBeingPropertyEdited() const = 0;

		virtual ~IReplicationStreamViewer() = default;
	};
}