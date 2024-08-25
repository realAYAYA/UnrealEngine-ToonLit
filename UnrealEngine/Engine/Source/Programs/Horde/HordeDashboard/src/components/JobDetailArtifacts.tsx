// Copyright Epic Games, Inc. All Rights Reserved.

import { Stack, Text, IColumn, mergeStyleSets, Icon, DetailsList, Selection, SelectionMode, DetailsListLayoutMode, ScrollablePane, ScrollbarVisibility, StickyPositionType, IDetailsListProps, IDetailsHeaderStyles, Sticky, DetailsHeader, PrimaryButton, SpinnerSize, Spinner, Link, TextField } from '@fluentui/react';
import React, { useState } from 'react';
import { ArtifactData, GetArtifactZipRequest } from '../backend/Api';
import { JobDetails } from '../backend/JobDetails';
import { observer } from 'mobx-react-lite';
import { observable, action, makeObservable } from 'mobx';
import backend from '../backend';
import { getHordeStyling } from '../styles/Styles';

const classNames = mergeStyleSets({
   fileIconHeaderIcon: {
      padding: 0,
      fontSize: '24px',
   },
   fileIconCell: {
      textAlign: 'center',
      selectors: {
         '&:before': {
            content: '.',
            display: 'inline-block',
            verticalAlign: 'middle',
            height: '100%',
            width: '0px',
            visibility: 'hidden',
         },
      },
   },
   fileIconImg: {
      verticalAlign: 'middle',
      maxHeight: '24px',
      maxWidth: '24px',
      fontSize: '24px'
   },
});

class ArtifactState {

   constructor() {
      makeObservable(this);
   }

   @observable selectedItemCount = 0;
   @observable isDownloading = false;
   jobDetails: JobDetails | null = null;
   stepId?: string | undefined = undefined;

   selection: Selection = new Selection({ onSelectionChanged: () => { this.setSelectedItemCount(this.selection.getSelectedCount()); } });

   @action
   private setSelectedItemCount(count: number) {
      this.selectedItemCount = count;
   }

   @action
   setIsDownloading(isDownloading: boolean) {
      this.isDownloading = isDownloading;
   }

   setDetails(jobDetails: JobDetails, stepId?: string) {
      if (jobDetails!.id! !== this.jobDetails?.id || stepId !== this.stepId) {
         this.selection.setAllSelected(false);
      }
      this.jobDetails = jobDetails;
      this.stepId = stepId;
   }

   async downloadItems(fromDownloadButton?: boolean | undefined, ctrlKeyPressed?: boolean | undefined, forceFileName?: string | undefined) {
      const request: GetArtifactZipRequest = {};
      request.jobId = this.jobDetails!.id!;
      request.stepId = this.stepId;
      if (this.selectedItemCount !== 0) {
         const items: ArtifactData[] = [];
         this.selection.getSelection().forEach(function (item) {
            items.push(item as ArtifactData);
         });

         request.artifactIds = items.map(item => item.id);
         if (request.artifactIds.length === 1) {
            request.fileName = items[0].name;
         }
      }
      request.isForcedDownload = fromDownloadButton;
      request.isOpenNewTab = ctrlKeyPressed;
      if (!request.fileName && forceFileName) {
         request.fileName = forceFileName;
      }
      this.setIsDownloading(true);
      backend.downloadJobArtifacts(request).then(response => {

      }).finally(() => {
         this.setIsDownloading(false);
      });
   }

   columns: IColumn[] = [
      {
         key: 'type',
         name: 'File Type',
         className: classNames.fileIconCell,
         iconClassName: classNames.fileIconHeaderIcon,
         iconName: 'Page',
         isIconOnly: true,
         fieldName: 'type',
         minWidth: 24,
         maxWidth: 24,
         onRender: (item: ArtifactData) => {
            let iconName = "Page";
            switch (item.mimeType) {
               case "text/plain":
                  iconName = "TextDocument";
                  break;
               case "text/xml":
                  iconName = "FileCode";
                  break;
               case "application/octet-stream":
                  iconName = "ProcessMetaTask";
                  break;
               case "application/json":
                  iconName = "Code";
                  break;
            }
            return <Icon iconName={iconName} className={classNames.fileIconImg} />;
         }
      },
      {
         key: 'name',
         name: 'Name',
         fieldName: 'name',
         minWidth: 210,
         maxWidth: 350,
         isRowHeader: true,
         data: 'string',
         isPadded: true,
         onRender: (item: ArtifactData) => {
            return (<Stack styles={{ root: { height: '100%' } }} horizontal>
               <Stack.Item align={"center"}>
                  <Link href={`${backend.serverUrl}/api/v1/artifacts/${item.id}/download?filename=${encodeURI(item.name)}&code=${item.code}`} target="_blank">
                     {item.name}
                  </Link>
               </Stack.Item>
            </Stack>);
         }
      }
   ];

}

const artifactState = new ArtifactState();
export const JobDetailArtifacts: React.FC<{ jobDetails: JobDetails; stepId?: string, topPadding?: number }> = observer(({ jobDetails, stepId, topPadding }) => {

   const { hordeClasses } = getHordeStyling();
   const [state, setState] = useState<{ filter?: string }>({});

   artifactState.setDetails(jobDetails, stepId);

   function getKey(item: any, index?: number): string {
      return item.key;
   }

   // main header
   const onRenderDetailsHeader: IDetailsListProps['onRenderDetailsHeader'] = (props) => {
      const customStyles: Partial<IDetailsHeaderStyles> = {};
      if (props) {
         return (
            <Sticky stickyPosition={StickyPositionType.Header} isScrollSynced={true}>
               <DetailsHeader {...props} styles={customStyles} />
            </Sticky>
         );
      }
      return null;
   };

   let buttonText = "Downloading...";
   if (!artifactState.isDownloading) {
      buttonText = "Download All";
      if (artifactState.selectedItemCount > 0) {
         if (artifactState.selectedItemCount !== jobDetails.artifacts.length) {
            buttonText = `Download ${artifactState.selectedItemCount} File${artifactState.selectedItemCount > 1 ? 's' : ''}`;
         }
      }
   }

   let artifacts: ArtifactData[] = jobDetails.artifacts;
   if (stepId) {
      artifacts = artifacts.filter(artifact => artifact.stepId === stepId);
   }

   if (state.filter) {
      const f = state.filter.toLowerCase();
      artifacts = artifacts.filter(a => a.name?.toLowerCase().indexOf(f) !== -1);
   }


   let height = Math.min(36 * artifacts.length + 60, 500);

   return (<Stack styles={{ root: { paddingTop: topPadding ?? 18, paddingRight: 12 } }}>
      <Stack className={hordeClasses.raised}>
         <Stack tokens={{ childrenGap: 12 }}>
            <Stack horizontal horizontalAlign="space-between" styles={{ root: { minHeight: 32 } }}>
               <Text variant="mediumPlus" styles={{ root: { fontFamily: "Horde Open Sans SemiBold" } }}>Artifacts</Text>
               <Stack horizontal tokens={{ childrenGap: 24 }}>
                  <Stack>
                     <TextField
                        spellCheck={false}
                        deferredValidationTime={500}
                        validateOnLoad={false}

                        styles={{
                           root: { width: 320, fontSize: 12 }, fieldGroup: {
                              borderWidth: 1
                           }
                        }}

                        placeholder="Filter"

                        onGetErrorMessage={(newValue) => {

                           const filter = newValue ? newValue : undefined;
                           if (filter !== state.filter) {
                              setState({ ...state, filter: filter });
                           }

                           return undefined;
                        }}

                     />

                  </Stack>

                  <Stack>
                     {artifactState.isDownloading && <Spinner styles={{ root: { marginRight: 10 } }} size={SpinnerSize.medium}></Spinner>}
                     <PrimaryButton styles={{ root: { fontFamily: 'Horde Open Sans Semibold !important' } }} disabled={artifactState.isDownloading} onClick={artifactState.downloadItems.bind(artifactState, true, false, undefined)}>{buttonText}</PrimaryButton>
                  </Stack>
               </Stack>
            </Stack>
            <Stack>
               <Stack.Item styles={{ root: { height: height, maxHeight: height, position: 'relative' } }}>
                  <ScrollablePane scrollbarVisibility={ScrollbarVisibility.auto}>
                     <DetailsList
                        items={artifacts}
                        compact={true}
                        columns={artifactState.columns}
                        selectionMode={SelectionMode.multiple}
                        getKey={getKey}
                        setKey="multiple"
                        layoutMode={DetailsListLayoutMode.justified}
                        onRenderDetailsHeader={onRenderDetailsHeader}
                        isHeaderVisible={true}
                        selection={artifactState.selection}
                        selectionPreservedOnEmptyClick={true}
                     />
                  </ScrollablePane>
               </Stack.Item>
            </Stack>

         </Stack>

      </Stack></Stack>);
});