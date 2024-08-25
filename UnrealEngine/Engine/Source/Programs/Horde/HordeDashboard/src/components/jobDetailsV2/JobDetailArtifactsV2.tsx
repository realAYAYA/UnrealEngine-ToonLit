// Copyright Epic Games, Inc. All Rights Reserved.

import { DetailsHeader, DetailsList, DetailsListLayoutMode, IColumn, IDetailsHeaderStyles, IDetailsListProps, Icon, Link, PrimaryButton, ScrollablePane, ScrollbarVisibility, Selection, SelectionMode, Spinner, SpinnerSize, Stack, Sticky, StickyPositionType, Text, TextField, mergeStyleSets } from '@fluentui/react';
import { action, makeObservable, observable } from 'mobx';
import { observer } from 'mobx-react-lite';
import React, { useEffect, useState } from 'react';
import backend from '../../backend';
import { ArtifactData, GetArtifactResponse, GetArtifactZipRequest } from '../../backend/Api';
import { ISideRailLink } from '../../base/components/SideRail';
import { getHordeStyling } from '../../styles/Styles';
import { JobDataView, JobDetailsV2 } from './JobDetailsViewCommon';

const sideRail: ISideRailLink = { text: "Artifacts", url: "rail_artifacts" };

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
   jobDetails: JobDetailsV2 | null = null;
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

   setDetails(jobDetails: JobDetailsV2, stepId?: string) {
      if (jobDetails.jobData?.id !== this.jobDetails?.jobData?.id || stepId !== this.stepId) {
         this.selection.setAllSelected(false);
      }
      this.jobDetails = jobDetails;
      this.stepId = stepId;
   }

   async downloadItems(fromDownloadButton?: boolean | undefined, ctrlKeyPressed?: boolean | undefined, forceFileName?: string | undefined) {
      const request: GetArtifactZipRequest = {};
      request.jobId = this.jobDetails!.jobData!.id;
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

class ArtifactsDataView extends JobDataView {

   filterUpdated() {
      // this.updateReady();
   }

   set(stepId: string) {

      const details = this.details;

      if (!details) {
         return;
      }

      const curStepId = this.stepId;

      if (stepId === curStepId || !details.jobId) {
         return;
      }

      this.stepId = stepId;

      backend.getJobArtifacts(details.jobId, this.stepId).then((results) => {

         if (this.stepId === stepId) {
            this.artifacts = results;
            this.updateReady();
         }
      }).catch((reason) => {
         console.log(`Error getting artifacts jobId: ${details.jobId} stepId: ${this.stepId} error: ${reason}`);
      }).finally(() => {
         this.initialize((this.artifacts?.length && !details.jobData?.useArtifactsV2) ? [sideRail] : undefined)
      });

   }

   clear() {
      this.artifacts = undefined;
      this.stepId = undefined;
      super.clear();
   }

   detailsUpdated() {

      if (!this.details?.jobData) {
         return;
      }

      this.updateReady();

   }

   order = 6;
   artifacts?: GetArtifactResponse[];
   stepId?: string;
}

JobDetailsV2.registerDataView("ArtifactsDataView", (details: JobDetailsV2) => new ArtifactsDataView(details));

const artifactState = new ArtifactState();
export const JobDetailArtifactsV2: React.FC<{ jobDetails: JobDetailsV2; stepId: string }> = observer(({ jobDetails, stepId }) => {

   const dataView = jobDetails.getDataView<ArtifactsDataView>("ArtifactsDataView");

   const [state, setState] = useState<{ filter?: string }>({});
   const { hordeClasses } = getHordeStyling();

   useEffect(() => {
      return () => {
         dataView?.clear();
      };
   }, [dataView]);

   dataView.subscribe();
   dataView.set(stepId);

   if (!jobDetails.viewReady(dataView.order)) {
      return null;
   }

   if (jobDetails.jobData?.useArtifactsV2) {
      return null;
   }

   if (!dataView.artifacts?.length) {
      return null;
   }

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
         if (artifactState.selectedItemCount !== dataView.artifacts.length) {
            buttonText = `Download ${artifactState.selectedItemCount} File${artifactState.selectedItemCount > 1 ? 's' : ''}`;
         }
      }
   }

   let artifacts: ArtifactData[] = dataView.artifacts;
   if (stepId) {
      artifacts = artifacts.filter(artifact => artifact.stepId === stepId);
   }

   let height = Math.min(36 * artifacts.length + 60, 500);

   if (state.filter) {
      const f = state.filter.toLowerCase();
      artifacts = artifacts.filter(a => a.name?.toLowerCase().indexOf(f) !== -1);
   }

   return (<Stack id={sideRail.url} styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
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