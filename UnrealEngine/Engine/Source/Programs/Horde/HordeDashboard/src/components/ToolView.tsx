// Copyright Epic Games, Inc. All Rights Reserved.

import { DetailsList, DetailsListLayoutMode, IColumn, PrimaryButton, SelectionMode, Stack, Text } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import React, { useEffect } from "react";
import backend from "../backend";
import { GetToolSummaryResponse } from "../backend/Api";
import { PollBase } from "../backend/PollBase";
import { useWindowSize } from "../base/utilities/hooks";
import { detailClasses, hordeClasses, modeColors } from "../styles/Styles";
import { Breadcrumbs } from "./Breadcrumbs";
import { TopNav } from "./TopNav";

class ToolHandler extends PollBase {

   constructor(pollTime = 30000) {

      super(pollTime);

   }

   clear() {
      super.stop();
   }

   async poll(): Promise<void> {

      try {

         this.tools = await backend.getTools();         
         this.setUpdated();

      } catch (err) {

      }

   }

   tools: GetToolSummaryResponse[] = [];
}

const handler = new ToolHandler();

const ToolPanel: React.FC = observer(() => {

   useEffect(() => {

      handler.start();

      return () => {
         handler.clear();
      };

   }, []);

   // subscribe
   if (handler.updated) { };

   const columns:IColumn[] = [      
      { key: 'column_name', name: 'Name', minWidth: 240, maxWidth: 240, isResizable: false },
      { key: 'column_desc', name: 'Description', fieldName: 'description', minWidth: 580, maxWidth: 580, isResizable: false, isMultiline: true },
      { key: 'column_version', name: 'Version', fieldName: 'version', minWidth: 280, maxWidth: 280, isResizable: false, headerClassName: detailClasses.detailsHeader },      
      { key: 'column_download', name: '', minWidth: 160, maxWidth: 160, isResizable: false }
   ];

   let tools = [...handler.tools];

   tools = tools.sort((a, b) => a.name.localeCompare(b.name));

   const renderItem = (item: any, index?: number, column?: IColumn) => {

      if (!column) {
         return null;
      }

      if (column.key === 'column_name') {
         return <Stack verticalAlign="center" verticalFill={true}>
            <Text style={{ fontFamily: "Horde Open Sans SemiBold", color: modeColors.text }}>{item.name}</Text>
         </Stack>
      }

      if (column.key === 'column_version') {
         if (!item.version) {
            return null;
         }
         return <Stack horizontalAlign="center" verticalAlign="center" verticalFill={true}>
            <Text style={{ color: modeColors.text }}>{item.version}</Text>
         </Stack>
      }


      if (column.key === 'column_download') {
         return <Stack horizontalAlign="center" verticalAlign="center" verticalFill={true}>
            <PrimaryButton style={{width: 120, color: "#FFFFFF"}} text="Download" href={`/api/v1/tools/${item.id}?action=download`} />
         </Stack>
      }

      if (!column?.fieldName) {
         return null;
      }
      return <Stack verticalAlign="center" verticalFill={true}>
         <Text style={{ color: modeColors.text }}>{item[column?.fieldName]}</Text>
      </Stack>
   };

   return (<Stack>
      <Stack styles={{ root: { paddingTop: 0, paddingLeft: 12, paddingRight: 12, width: "100%" } }} >
         <Stack tokens={{ childrenGap: 12 }}>
            <div style={{ overflowY: 'auto', overflowX: 'hidden', maxHeight: "calc(100vh - 312px)" }} data-is-scrollable={true}>
               <Stack>
                  <DetailsList
                     isHeaderVisible={true}
                     items={tools}
                     columns={columns}
                     selectionMode={SelectionMode.none}
                     layoutMode={DetailsListLayoutMode.justified}
                     compact={false}
                     onRenderItemColumn={renderItem}
                  />
               </Stack>
            </div>
         </Stack>
      </Stack>
   </Stack>);
});


export const ToolView: React.FC = () => {

   const windowSize = useWindowSize();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);

   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: 'Tools' }]} />
      <Stack horizontal>
         <div key={`windowsize_streamview_${windowSize.width}_${windowSize.height}`} style={{ width: vw / 2 - (1440/2), flexShrink: 0, backgroundColor: modeColors.background }} />
         <Stack tokens={{ childrenGap: 0 }} styles={{ root: { backgroundColor: modeColors.background, width: "100%" } }}>
            <Stack style={{ maxWidth: 1440, paddingTop: 6, marginLeft: 4, height: 'calc(100vh - 8px)' }}>
               <Stack horizontal className={hordeClasses.raised}>
                  <Stack style={{ width: "100%", height: 'calc(100vh - 228px)' }} tokens={{ childrenGap: 18 }}>
                     <ToolPanel />
                  </Stack>
               </Stack>
            </Stack>
         </Stack>
      </Stack>
   </Stack>
};

