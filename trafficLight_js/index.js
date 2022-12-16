const express = require('express');
const fs = require('fs');
const parser = require('body-parser');
const cors = require('cors');

const app = express();
app.use(cors());
app.use(parser.urlencoded({extended: false}));
app.use(parser.json());
const port = process.env.PORT || 3000;

app.use(express.static('public'));

app.listen(port, () => {
    console.log("Started app on: %d", port);
})