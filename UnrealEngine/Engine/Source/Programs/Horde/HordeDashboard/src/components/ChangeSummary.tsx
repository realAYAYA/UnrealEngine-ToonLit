import { observer } from 'mobx-react-lite';
import React from 'react';

export const ChangeSummary: React.FC<{ streamId: string, change: number }> = observer(({ streamId, change }) => {

   return null;

   /*
   const [state, setState] = useState<{ data?: ChangeSummaryData, loading?: boolean }>({});

   if (!state.data) {

      if (!state.loading) {

         backend.getChangeSummaries(streamId, change, change, 1).then(data => {
            setState({ data: data[0], loading: false });
         })

      }

      return <Stack>
         <Stack style={{ paddingLeft: 0 }}>
            <div style={{ whiteSpace: "pre" }}>
               <Stack horizontal tokens={{ childrenGap: 12 }}>
                  <Stack>
                     <Text>{`CL ${change} - Loading`}</Text>
                  </Stack>
                  <Stack>
                     <Spinner />
                  </Stack>
               </Stack>
            </div>
         </Stack></Stack>

   }


   return <Stack>
      <Stack style={{ paddingLeft: 0 }}>
         <div style={{ whiteSpace: "pre" }}>
            <Stack>
               <Stack>
                  <Text>{`CL ${change} - Author: ${state.data.author}`}</Text>
               </Stack>
               <Stack style={{ paddingLeft: 8, paddingTop: 4 }}>
                  {`${state.data.description}`}
               </Stack>
            </Stack>
         </div>
      </Stack></Stack>
      */
})