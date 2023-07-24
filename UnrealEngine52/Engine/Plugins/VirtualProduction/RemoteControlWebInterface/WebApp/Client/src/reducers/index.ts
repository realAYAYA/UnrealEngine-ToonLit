import { combineReducers } from 'redux';
import api, { ApiState } from './api';


export interface ReduxState {
  api: ApiState,
}

export default combineReducers({
  api,
});


export { _api } from './api';
