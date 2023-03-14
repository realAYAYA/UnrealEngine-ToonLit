// Copyright Epic Games, Inc. All Rights Reserved.

import { DefaultButton, IContextualMenuProps, mergeStyleSets, Pivot, PivotItem, PrimaryButton, Stack, Text, TextField } from '@fluentui/react';
import { observer } from 'mobx-react-lite';
import React, { useEffect, useState } from 'react';
import { useHistory, useLocation, useParams } from 'react-router-dom';
import { useBackend } from '../backend';
import { JobFilterSimple } from '../base/utilities/filter';
import { useWindowSize } from '../base/utilities/hooks';
import { modeColors, hordeClasses } from '../styles/Styles';
import { BreadcrumbItem, Breadcrumbs } from './Breadcrumbs';
import { useQuery } from './JobDetailCommon';
import { JobSearchSimpleModal } from './JobSearchSimple';
import { JobView } from './JobView';
import { JobViewAll } from './JobViewAll';
import { JobViewIncremental } from './JobViewIncremental';
import { NewBuild } from './NewBuild';
import { StreamSummary } from './StreamSummary';
import { TopNav } from './TopNav';

export const SummaryPage: React.FC = () => {

   return (<Stack tokens={{ padding: 16 }} ><Text variant="mediumPlus">{`Summary Page`}</Text></Stack>);
};

export const GridView: React.FC = () => {

   return (<Stack tokens={{ padding: 16 }} ><Text variant="mediumPlus">{`Grid View`}</Text></Stack>);
};


export const customClasses = mergeStyleSets({
   filterBar: {
      selectors: {
         '.ms-Button': {
            paddingLeft: 16,
            paddingRight: 16,
            minWidth: 80,
            height: 32,
            borderRadius: 2
         },
         '.ms-Button-label': {
            fontFamily: "Horde Open Sans SemiBold !important"
         }
      }
   },
   raised: {
      backgroundColor: "#ffffff",
      boxShadow: "0 0 0 0 rgba(0,0,0,0.132), 0 0 0.9px 0 rgba(0,0,0,0.108)",
      padding: "25px 12px 25px 30px",
      margin: 0
   }

});

const StreamViewInner: React.FC = observer(() => {

   const windowSize = useWindowSize();

   const { streamId } = useParams<{ streamId: string }>();
   const history = useHistory();
   const location = useLocation();
   const query = useQuery();

   const [filter, setFilter] = useState<JobFilterSimple>({ showOthersPreflights: false });

   const [shown, setShown] = useState(query.get("newbuild") ? true : false);
   const [findJobsShown, setFindJobsShown] = useState(false);

   useEffect(() => {
      return () => {
         setFilter({ showOthersPreflights: filter.showOthersPreflights })
      }
      // eslint-disable-next-line
   }, [location])

   const { projectStore } = useBackend();




   const stream = projectStore.streamById(streamId);
   const project = stream?.project;

   if (!stream || !project) {
      console.error("Bad stream or project id in StreamView");
      history.replace("/");
      return null;
   }

   let queryTab = query.get("tab") ?? undefined;

   if (!queryTab || (queryTab.toLowerCase() !== "summary" && queryTab.toLowerCase() !== "all" && !stream.tabs.find(t => t.title === queryTab))) {

      if (!!stream.tabs.find(t => t.title.toLowerCase() === "incremental")) {
         history.replace(`/stream/${streamId}?tab=Incremental`);
      } else if (stream.tabs.length) {
         history.replace(`/stream/${streamId}?tab=${stream.tabs[0].title}`);
      } else {
         history.replace(`/stream/${streamId}?tab=all`);
      }

      return null;
   }

   const isSummary = queryTab === 'summary';
   const isAllView = queryTab === 'all';

   let crumbText = stream.name;

   if (!isSummary && !isAllView) {
      const templateFilter = query.get("template") ?? undefined;
      if (templateFilter) {
         const template = stream.templates?.find(t => t.id === templateFilter);
         if (template) {
            crumbText += ` - ${template.name}`;
         }
      }
   }

   const crumbItems: BreadcrumbItem[] = [
      {
         text: stream.project!.name,
         link: `/project/${stream.project!.id}`
      },
      {
         text: crumbText
      }
   ];

   const crumbTitle = `Horde: //${stream.project?.name}/${stream.name}`;

   const pivotItems = stream.tabs.map(tab => {
      return <PivotItem headerText={tab.title} itemKey={tab.title} key={tab.title} headerButtonProps={{ href: `/stream/${streamId}?tab=${encodeURIComponent(tab.title)}` }} />;
   });

   pivotItems.unshift(<PivotItem headerText="All" itemKey="all" key="pivot_item_all" headerButtonProps={{ href: `/stream/${streamId}?tab=all` }} />)

   pivotItems.unshift(<PivotItem headerText="Summary" itemKey="summary" key="pivot_item_summary" headerButtonProps={{ href: `/stream/${streamId}?tab=summary` }} />)


   let findJobsItems: IContextualMenuProps = { items: [] };

   if (!isSummary) {

      findJobsItems = {
         items: [
            {
               key: 'no_results',
               onRender: (item, dismissMenu) => {
                  return <Stack style={{ padding: 8, paddingTop: 16, paddingBottom: 12 }}>
                     <TextField
                        deferredValidationTime={750}
                        validateOnLoad={false}
                        defaultValue={filter.filterKeyword}
                        spellCheck={false}
                        placeholder="Filter Jobs"

                        onGetErrorMessage={(newValue) => {

                           setFilter({
                              showOthersPreflights: filter.showOthersPreflights,
                              filterKeyword: newValue
                           });
                           return undefined;


                        }}

                     />
                  </Stack>
               }
            },
            {
               key: 'show_other_preflights',
               text: 'Show preflights for all users',
               iconProps: { iconName: filter.showOthersPreflights ? 'Tick' : "" },
               onClick: () => {
                  setFilter({
                     showOthersPreflights: !filter.showOthersPreflights,
                     filterKeyword: filter.filterKeyword
                  });
               }
            }
         ]
      };
   }


   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   const newBuildTab = queryTab?.toLowerCase() === "summary" ? "all" : queryTab;
   const isSwarmTab = queryTab?.toLowerCase() === "swarm" || queryTab?.toLowerCase() === "presubmit";
   let isIncrementalTab = queryTab?.toLowerCase() === "incremental";

   // @todo: We should have a way of defining a tab's views, these are transitioning anyway, so, ok
   if (!isIncrementalTab && queryTab) {
      const tabName = queryTab.toLowerCase();
      isIncrementalTab = tabName === "primary inc" || tabName === "secondary" || tabName.indexOf("incremental") !== -1;
   }

   /*
            <Stack horizontal style={{ width: rootWidth, paddingLeft: 0, paddingTop: 12, paddingBottom: 24, paddingRight: 0 }} >
               <Stack verticalAlign="center" style={{ paddingRight: 0, height: 32 }}>
                  <JobPanelPivot jobDetails={details} />
               </Stack>
               <Stack grow />
               <Stack verticalAlign="center" style={{ paddingRight: 0, height: 32 }}>
                  <JobOperations jobDetails={details}  />
                  </Stack>
                  </Stack>
      
   */

   const windowWidth = windowSize.width;

   return (
      <Stack className={hordeClasses.horde}>
         <TopNav />
         <Breadcrumbs items={crumbItems} title={crumbTitle} />
         <NewBuild streamId={streamId!} jobKey={newBuildTab!} show={shown} onClose={(newJobId) => {
            setShown(false);
            if (newJobId) {
               history.push(`/job/${newJobId}`);
            } else {
               if (query.get("newbuild")) {
                  history.replace(`/stream/${streamId}?tab=${queryTab}`);
               }
            }
         }} />
         {findJobsShown && <JobSearchSimpleModal onClose={() => { setFindJobsShown(false) }} streamId={stream.id} />}
         <Stack horizontal>
            <div key={`windowsize_streamview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - 900, flexShrink: 0, backgroundColor: modeColors.background }} />
            <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: modeColors.background, width: "100%" } }}>
               <Stack style={{ width: 1800, paddingTop: 12, marginLeft: 4 }}>
                  <Stack style={{ maxWidth: windowWidth - 12 }} horizontal verticalAlign='center' verticalFill={true}>
                     <Stack style={{ paddingLeft: 4, paddingTop: 2 }}>
                        <Pivot className={hordeClasses.pivot}
                           selectedKey={queryTab}
                           linkSize="normal"
                           linkFormat="links"
                           onLinkClick={(item) => {
                              if (item!.props.itemKey !== queryTab) {
                                 // this still needs to be here, even though specific button href in item
                                 history.push(`/stream/${streamId}?tab=${encodeURIComponent(item!.props.itemKey!)}`);
                              }
                           }}>
                           {pivotItems}
                        </Pivot>
                     </Stack>
                     <Stack grow />
                     <Stack horizontal verticalAlign="center" horizontalAlign={"end"} tokens={{ childrenGap: 8 }}>
                        <Stack horizontal tokens={{ childrenGap: 18 }}>
                           {!isSwarmTab && <DefaultButton
                              styles={{ root: { fontFamily: "Horde Open Sans SemiBold !important", backgroundColor: modeColors.background } }}
                              text="Search"
                              split={!isSummary}
                              menuProps={!isSummary ? findJobsItems : undefined}
                              onClick={() => setFindJobsShown(true)}
                           />}
                           <PrimaryButton
                              styles={{ root: { fontFamily: "Horde Open Sans SemiBold !important" } }}
                              onClick={() => { setShown(true); }}>
                              New&nbsp;Job
                           </PrimaryButton>
                        </Stack>
                     </Stack>
                  </Stack>

               </Stack>
               <Stack style={{ width: "100%", paddingTop: 12, marginLeft: 4 }} >
                  <Stack >

                     {!isSummary && !isAllView && !isIncrementalTab && queryTab && <JobView tab={queryTab} filter={filter} />}
                     {isSummary && <StreamSummary />}
                     {isAllView && <JobViewAll filter={filter} />}
                     {isIncrementalTab && <JobViewIncremental tab={queryTab} filter={filter} />}
                  </Stack>
               </Stack>

            </Stack>
         </Stack>
      </Stack>
   );
});

export const StreamView: React.FC = () => {

   return <StreamViewInner />;

};


