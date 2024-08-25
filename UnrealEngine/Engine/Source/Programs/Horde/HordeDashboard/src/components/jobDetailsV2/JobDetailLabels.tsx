import { ConstrainMode, DefaultButton, DetailsList, DetailsListLayoutMode, IColumn, SelectionMode, Stack, Text } from "@fluentui/react";
import { observer } from "mobx-react-lite";
import { useNavigate, useLocation } from "react-router-dom";
import { LabelOutcome, LabelState } from "../../backend/Api";
import { JobLabel } from "../../backend/JobDetails";
import { getLabelColor } from "../../styles/colors";
import { useQuery } from "../JobDetailCommon";
import { JobDataView, JobDetailsV2 } from "./JobDetailsViewCommon";
import { getHordeStyling } from "../../styles/Styles";
import dashboard from "../../backend/Dashboard";


export const LabelsPanelV2: React.FC<{ jobDetails: JobDetailsV2, dataView: JobDataView }> = observer(({ jobDetails, dataView }) => {

   const query = useQuery();
   const navigate = useNavigate();
   const location = useLocation();

   const { hordeClasses, modeColors } = getHordeStyling();

   dataView.subscribe();

   const labelIdx = query.get("label") ? parseInt(query.get("label")!) : undefined;
   const qlabel = jobDetails.labelByIndex(labelIdx);
   if (typeof (labelIdx) === `number` && !qlabel) {            
      return null;
   }
   
   const jobFilter = jobDetails.filter;

   // subscribe   
   if (jobFilter.inputChanged) { }

   const batchFilter = query.get("batch");
   if (batchFilter)
   {
      return null;   
   }

   let labels = jobDetails.labels.filter(label => label.stateResponse.state !== LabelState.Unspecified);

   if (qlabel) {
      labels = labels.filter(l => l.category === qlabel?.category && l.name === qlabel?.name);
   }

   const stateFilter = jobFilter.filterStates;
   labels = labels.filter(label => {

      if (stateFilter.indexOf("All") !== -1) {
         return true;
      }

      let include = false;

      if (stateFilter.indexOf("Failure") !== -1) {
         if (label.stateResponse.outcome === LabelOutcome.Failure) {
            include = true;
         }
      }

      if (stateFilter.indexOf("Warnings") !== -1) {
         if (label.stateResponse.outcome === LabelOutcome.Warnings) {
            include = true;
         }
      }

      if (stateFilter.indexOf("Completed") !== -1) {
         if (label.stateResponse.state === LabelState.Complete) {
            include = true;
         }
      }

      if (stateFilter.indexOf("Running") !== -1) {
         if (label.stateResponse.state === LabelState.Running) {
            include = true;
         }
      }

      return include;

   });   

   if (!labels) {
      return null;
   }

   type LabelItem = {
      category: string;
      labels: JobLabel[];
   };

   const categories: Set<string> = new Set();
   labels.forEach(label => { if (label.name) { categories.add(label.category!); } });


   let items = Array.from(categories.values()).map(c => {
      return {
         category: c,
         labels: labels.filter(label => label.name && (label.category === c)).sort((a, b) => {
            return a.name! < b.name! ? -1 : 1;
         })
      } as LabelItem;
   }).filter(item => item.labels?.length).sort((a, b) => {
      return a.category < b.category ? -1 : 1;
   });

   const filter = jobDetails.filter;

   if (filter.search) {
      items = items.filter(i => {
         i.labels = i.labels.filter(label => label.name.toLowerCase().indexOf(filter.search!.toLowerCase()) !== -1);
         return i.labels.length !== 0;
      });
   }

   if (filter.currentInput) {
      items = items.filter(i => {
         i.labels = i.labels.filter(label => label.name.toLowerCase().indexOf(filter.currentInput!.toLowerCase()) !== -1);
         return i.labels.length !== 0;
      });
   }   

   if (!items.length) {
      return null;
   }

   const buildColumns = (): IColumn[] => {

      const widths: Record<string, number> = {
         "Name": 120,
         "Labels": 1024
      };

      const cnames = ["Name", "Labels"];

      return cnames.map(c => {
         return { key: c, name: c === "Status" ? "" : c, fieldName: c.replace(" ", "").toLowerCase(), minWidth: widths[c] ?? 100, maxWidth: widths[c] ?? 100, isResizable: false, isMultiline: true } as IColumn;
      });

   };

   function onRenderLabelColumn(item: LabelItem, index?: number, column?: IColumn) {

      switch (column!.key) {

         case 'Name':
            return <Stack verticalAlign="center" verticalFill={true}> <Text style={{ fontFamily: "Horde Open Sans SemiBold" }}>{item.category}</Text> </Stack>;

         case 'Labels':
            return <Stack wrap horizontal tokens={{ childrenGap: 4 }} styles={{ root: { paddingTop: 2 } }}>
               {item.labels.map(label => {
                  const color = getLabelColor(label.stateResponse.state, label.stateResponse.outcome);
                  let textColor: string | undefined = undefined;

                  let filtered = false;
                  if (qlabel) {
                     if (qlabel.category !== label.category || qlabel.name !== label.name) {
                        filtered = true;
                     }
                  }
                  return <Stack key={`labels_${label.category}_${label.name}`} className={hordeClasses.badge}>
                     <DefaultButton
                        onClick={() => {
                           if (qlabel?.category === label.category && qlabel?.name === label.name) {
                              navigate(location.pathname)
                           } else {

                              const idx = jobDetails.labelIndex(label.name, label.category);
                              if (idx >= 0) {
                                 navigate(location.pathname + `?label=${idx}`)
                              } else {
                                 navigate(location.pathname, {replace: true})
                              }                              
                           }

                        }}
                        key={label.name} style={{ backgroundColor: color.primaryColor, color: textColor, filter: filtered ? "brightness(0.70)" : undefined }}
                        text={label.name!}>
                        {!!color.secondaryColor && <div style={{
                           borderLeft: "10px solid transparent",
                           borderRight: `10px solid ${color.secondaryColor}`,
                           borderBottom: "10px solid transparent",
                           height: 0,
                           width: 0,
                           position: "absolute",
                           right: 0,
                           top: 0,
                           zIndex: 1,
                           filter: filtered ? "brightness(0.70)" : undefined
                        }} />}
                     </DefaultButton>
                  </Stack>
               })}</Stack>;

         default:
            return <span>???</span>;
      }
   }


   return <Stack styles={{ root: { paddingTop: 18, paddingRight: 12 } }}>
      <Stack style={{ paddingLeft: 12 }}>
         <Stack styles={{root:{selectors: {'.ms-DetailsRow': {backgroundColor: dashboard.darktheme ? modeColors.content : undefined}}}}}>
            <DetailsList
               isHeaderVisible={false}
               indentWidth={0}
               compact={true}
               selectionMode={SelectionMode.none}
               items={items}
               columns={buildColumns()}
               layoutMode={DetailsListLayoutMode.fixedColumns}
               constrainMode={ConstrainMode.unconstrained}
               onRenderItemColumn={onRenderLabelColumn}
               onShouldVirtualize={() => { return false; }}
            />
         </Stack>
      </Stack>
   </Stack>;

});
