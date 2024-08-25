// Copyright Epic Games, Inc. All Rights Reserved.

import { DetailsList, DetailsListLayoutMode, IColumn, Pivot, PivotItem, PrimaryButton, SelectionMode, Stack, Text } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import React, { useEffect, useState } from "react";
import backend from "../backend";
import { GetToolSummaryResponse } from "../backend/Api";
import { PollBase } from "../backend/PollBase";
import { useWindowSize } from "../base/utilities/hooks";
import { Breadcrumbs } from "./Breadcrumbs";
import { TopNav } from "./TopNav";
import { getHordeStyling } from "../styles/Styles";

const defaultCategory = "General";

class ToolHandler extends PollBase {

   constructor(pollTime = 30000) {

      super(pollTime);

   }

   clear() {
      this.categories = new Map();
      this.loaded = false;
      super.stop();
   }

   async poll(): Promise<void> {

      try {

         this.tools = await backend.getTools();
         this.tools = this.tools.filter(t => t.showInDashboard);
         this.categories = new Map();

         this.tools.forEach(t => {
            const cat = t.category ?? defaultCategory;
            if (!this.categories.has(cat)) {
               this.categories.set(cat, []);
            }
            this.categories.get(cat)!.push(t);
         })

         this.loaded = true;
         this.setUpdated();

      } catch (err) {

      }

   }

   categories: Map<string, GetToolSummaryResponse[]> = new Map();

   loaded = false;
   tools: GetToolSummaryResponse[] = [];
}

const handler = new ToolHandler();

const ToolPanel: React.FC<{ selectedKey: string }> = observer(({ selectedKey }) => {

   useEffect(() => {

      handler.start();

      return () => {
         handler.clear();
      };

   }, []);

   const { hordeClasses, modeColors } = getHordeStyling();

   // subscribe
   if (handler.updated) { };

   const columns: IColumn[] = [
      { key: 'column_name', name: 'Name', minWidth: 240, maxWidth: 240, isResizable: false },
      { key: 'column_desc', name: 'Description', fieldName: 'description', minWidth: 580, maxWidth: 580, isResizable: false, isMultiline: true },
      { key: 'column_version', name: 'Version', fieldName: 'version', minWidth: 280, maxWidth: 280, isResizable: false, headerClassName: hordeClasses.detailsHeader },
      { key: 'column_download', name: '', minWidth: 160, maxWidth: 160, isResizable: false }
   ];

   let tools = [...handler.tools];

   tools = tools.filter(t => (t.category ?? defaultCategory) === selectedKey)

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
         return <Stack verticalAlign="center" verticalFill={true}>
            <Text style={{ color: modeColors.text }}>{item.version}</Text>
         </Stack>
      }


      if (column.key === 'column_download') {
         return <Stack horizontalAlign="center" verticalAlign="center" verticalFill={true}>
            <PrimaryButton style={{ width: 120, color: "#FFFFFF" }} text="Download" href={`/api/v1/tools/${item.id}?action=download`} />
         </Stack>
      }

      if (!column?.fieldName) {
         return null;
      }
      return <Stack verticalAlign="center" verticalFill={true}>
         <Text style={{ color: modeColors.text }}>{item[column?.fieldName]}</Text>
      </Stack>
   };

   return <Stack>
      {!tools.length && handler.loaded && <Stack style={{ paddingBottom: 12 }}>
         <Stack verticalAlign="center">
            <Stack horizontalAlign="center">
               <Text variant="mediumPlus">No Tools Found</Text>
            </Stack>
         </Stack>
      </Stack>}

      {!!tools.length && <Stack className={hordeClasses.raised} >
         <Stack styles={{ root: { paddingLeft: 12, paddingRight: 12, paddingBottom: 12, width: "100%", selectors: { ".ms-DetailsHeader": { "paddingTop": "0px" } } } }} >
            <DetailsList
               isHeaderVisible={true}
               styles={{ headerWrapper: { paddingTop: 0 } }}
               items={tools}
               columns={columns}
               selectionMode={SelectionMode.none}
               layoutMode={DetailsListLayoutMode.justified}
               compact={false}
               onRenderItemColumn={renderItem}
            />
         </Stack>
      </Stack>}
   </Stack>
});

export const ToolViewInner: React.FC = observer(() => {

   const [selectedKey, setSelectedKey] = useState<string>(defaultCategory);

   const windowSize = useWindowSize();

   // subscribe
   if (handler.updated) { };

   const { hordeClasses, modeColors } = getHordeStyling();
   const vw = Math.max(document.documentElement.clientWidth, window.innerWidth || 0);
   const centerAlign = vw / 2 - 720;
   const key = `windowsize_view_${windowSize.width}_${windowSize.height}`;

   const categories = Array.from(handler.categories.keys()).sort((a, b) => a.localeCompare(b));

   const pivotItems = categories.map(cat => {
      if (cat === defaultCategory) {
         return undefined;
      }
      return <PivotItem headerText={cat} itemKey={cat} key={cat} style={{ color: modeColors.text }} />;
   }).filter(p => !!p);

   pivotItems.unshift(<PivotItem headerText={defaultCategory} itemKey={defaultCategory} key={defaultCategory} style={{ color: modeColors.text }} />);

   return <Stack styles={{ root: { width: "100%", backgroundColor: modeColors.background } }}>
      <Stack style={{ width: "100%", backgroundColor: modeColors.background }}>
         <Stack style={{ position: "relative", width: "100%", height: 'calc(100vh - 148px)' }}>
            <div style={{ overflowX: "auto", overflowY: "visible" }}>
               <Stack horizontal style={{ paddingTop: 12, paddingBottom: 48 }}>
                  <Stack key={`${key}`} style={{ paddingLeft: centerAlign }} />
                  <Stack style={{ width: 1440 }}>
                     <Stack style={{ paddingBottom: 12 }}>
                        <Pivot className={hordeClasses.pivot}
                           overflowBehavior='menu'
                           selectedKey={selectedKey}
                           linkSize="normal"
                           linkFormat="links"
                           onLinkClick={(item) => {
                              if (item?.props.itemKey) {

                                 setSelectedKey(item.props.itemKey);
                              }
                           }}>
                           {pivotItems}
                        </Pivot>

                     </Stack>

                     <ToolPanel selectedKey={selectedKey} />
                  </Stack>
               </Stack>
            </div>
         </Stack>
      </Stack>
   </Stack>

})

export const ToolView: React.FC = () => {


   const { hordeClasses } = getHordeStyling();


   return <Stack className={hordeClasses.horde}>
      <TopNav />
      <Breadcrumbs items={[{ text: 'Tools' }]} />
      <ToolViewInner />
   </Stack>
};

