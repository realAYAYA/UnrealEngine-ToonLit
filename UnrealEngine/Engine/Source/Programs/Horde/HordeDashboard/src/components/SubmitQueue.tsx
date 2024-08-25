// Copyright Epic Games, Inc. All Rights Reserved.

import { CollapseAllVisibility, ConstrainMode, DetailsHeader, DetailsList, DetailsListLayoutMode, DetailsRow, Text, FocusZone, FocusZoneDirection, IColumn, IDetailsListProps, ScrollablePane, ScrollbarVisibility, SelectionMode, Stack, Sticky, StickyPositionType, Modal, IconButton, PrimaryButton, DefaultButton, TextField } from '@fluentui/react';
import React from 'react';
import { useParams } from 'react-router-dom';
import { getHordeStyling } from '../styles/Styles';

export const SubmitQueue: React.FC = () => {

   const { streamId } = useParams<{ streamId: string }>();   
   const { detailClasses } = getHordeStyling();

	const buildColumns = (): IColumn[] => {

		const fixedWidths: Record<string, number | undefined> = {
			"Change": 100,
			"Description": 740,
			"Author": 300,
			"Time": 120
		};

		const cnames = ["Change", "Description", "Author", "Time"];

		return cnames.map(c => {

			const column = { key: c, name: c === "Status" ? "" : c, fieldName: c.replace(" ", "").toLowerCase(), minWidth: fixedWidths[c], maxWidth: fixedWidths[c], isPadded: false, isResizable: false, isCollapsible: false, isMultiline: true } as IColumn;

			column.styles = (props: any): any => {
				props.cellStyleProps = { ...props.cellStyleProps };
				props.cellStyleProps.cellLeftPadding = 4;
				props.cellStyleProps.cellRightPadding = 0;
			};

			return column;
		});
	};

	const onRenderDetailsHeader: IDetailsListProps['onRenderDetailsHeader'] = (props) => {
		if (props) {
			props.selectionMode = SelectionMode.none;
			props.collapseAllVisibility = CollapseAllVisibility.hidden;
			return <Sticky stickyPosition={StickyPositionType.Header} isScrollSynced={true}>
				<DetailsHeader className={detailClasses.detailsHeader} {...props} />
			</Sticky>;
		}
		return null;
	};

	const onRenderRow: IDetailsListProps['onRenderRow'] = (props) => {
		if (props) { return <DetailsRow styles={{ root: { width: "100%", paddingTop: 8, paddingBottom: 8, backgroundColor: "FF0000" } }} {...props} />; }
		return null;
	};

	function onRenderItemColumn(item: any, index?: number, column?: IColumn) {

		return <div />;
	}

	return (
		<Stack tokens={{ padding: 4 }} className={detailClasses.detailsRow}>
			<FocusZone direction={FocusZoneDirection.vertical}>
				<div className={detailClasses.container} style={{ width: 1370, height: 'calc(100vh - 280px)', position: 'relative' }} data-is-scrollable={true}>

					{<ScrollablePane scrollbarVisibility={ScrollbarVisibility.always}>
						<DetailsList
							indentWidth={0}
							styles={{ root: { maxWidth: 1344, overflowX: "hidden" } }}
							compact={false}
							selectionMode={SelectionMode.none}
							items={[]}
							groups={[]}
							columns={buildColumns()}
							layoutMode={DetailsListLayoutMode.fixedColumns}
							constrainMode={ConstrainMode.unconstrained}
							onRenderRow={onRenderRow}
							onRenderDetailsHeader={onRenderDetailsHeader}
							onRenderItemColumn={onRenderItemColumn}
							onShouldVirtualize={() => { return true; }}
						/>

					</ScrollablePane>
					}
				</div>
			</FocusZone>
		</Stack>
	);

};

export const SubmitQueueChangeDialog: React.FC<{ show: boolean; streamId: string; onClose: () => void }> = ({ show, streamId, onClose }) => {
	
	const headerText = "Add Change";

	const close = () => {
		onClose();
	};

	const onAddChange = async () => {

	};

	const height = 200;

	return <Modal isOpen={show} styles={{ main: { padding: 8, width: 540, height: height, minHeight: height } }}>
		<Stack horizontal styles={{ root: { padding: 8 } }}>
			<Stack.Item grow={2}>
				<Text variant="mediumPlus">{headerText}</Text>
			</Stack.Item>
			<Stack.Item grow={0}>
				<IconButton
					iconProps={{ iconName: 'Cancel' }}
					ariaLabel="Close popup modal"
					onClick={() => { close(); }}
				/>
			</Stack.Item>
		</Stack>

		<Stack styles={{ root: { paddingLeft: 8, width: 500 } }}>
			{<TextField label="Shelved Change" onChange={(ev, newValue) => { ev.preventDefault(); }} />}
		</Stack>

		<Stack styles={{ root: { padding: 8 } }}>
			<Stack horizontal tokens={{ childrenGap: 16 }} styles={{ root: { paddingTop: 12, paddingLeft: 8, paddingBottom: 8 } }}>
				<PrimaryButton text="Queue" disabled={false} onClick={() => { }} />
				<DefaultButton text="Cancel" disabled={false} onClick={() => { close(); }} />
			</Stack>
		</Stack>
	</Modal>;

};