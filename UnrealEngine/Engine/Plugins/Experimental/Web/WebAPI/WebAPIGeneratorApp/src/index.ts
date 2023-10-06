import { App } from './server';

const app = new App();
app.start()
    .catch(err => {
      console.log('Error:', err.message);
    });
