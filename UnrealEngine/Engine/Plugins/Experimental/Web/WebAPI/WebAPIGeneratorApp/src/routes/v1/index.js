const express = require('express');
const generatorRoutes = require('./generators.routes');

const router = express.Router();

const defaultRoutes = [
    {
        path: '/gen',
        route: generatorRoutes
    }
];

defaultRoutes.forEach(route => {
    router.use(route.path, route.route);
});

module.exports = router;