import { initializeApp } from 'firebase/app';
import { getDatabase, ref, get, update } from 'firebase/database';

const firebaseConfig = {
    apiKey: "AIzaSyBt0s4jGLD1k3_p8SfGS4yhoil0BKmZgKE",
    authDomain: "budget-kahoot.firebaseapp.com",
    databaseURL: "https://budget-kahoot-default-rtdb.europe-west1.firebasedatabase.app",
    projectId: "budget-kahoot",
    storageBucket: "budget-kahoot.appspot.com",
    messagingSenderId: "332478696005",
    appId: "1:332478696005:web:2b6adcec8ceeb90b4c312c"
};

const firebaseApp = initializeApp(firebaseConfig);
const db = getDatabase(firebaseApp);

const sendStateData = () => {
    const lightId = document.getElementById("lightId").value;
    const state = document.getElementById("lightState").value;
    if (state == "" || lightId == "") {
        return;
    }

    const updates = {};
    updates['/traffic_lights/automatic/' + lightId] = Number(state);

    update(ref(db), updates);
}

const sendCycleData = () => {
    const cycleLength = document.getElementById("cycleLength").value;
    if (cycleLength == "") {
        return;
    }

    const updates = {};
    updates['/traffic_lights/cycleLength'] = Number(cycleLength);

    update(ref(db), updates);
}

const sendWaveData = () => {
    const waveToggle = document.getElementById("waveToggle").value;
    const waveLength = document.getElementById("waveLength").value;

    if (waveToggle == "" || waveLength == "") {
        return;
    }

    const updates = {};
    updates['traffic_lights/waveToggle'] = Number(waveToggle);
    updates['traffic_lights/waveLength'] = Number(waveLength);

    update(ref(db), updates);
}

document.getElementById('sendStateDataButton')
    .addEventListener('click', sendStateData, false);

document.getElementById('sendCycleDataButton')
    .addEventListener('click', sendCycleData, false);

document.getElementById('sendWaveDataButton')
    .addEventListener('click', sendWaveData, false);