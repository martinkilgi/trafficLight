import { initializeApp } from 'firebase/app';
import { getDatabase, ref, get } from 'firebase/database';

const api_url = 'https://orca-app-tlr83.ondigitalocean.app/';


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

const RED = 'red';
const YELLOW = 'yellow';
const GREEN = 'green';
const BLACK = 'black';

const cycle_length = 20 * 1000;
const red_length = cycle_length * 0.5;
const yellow_length = cycle_length * 0.05;
const green_length = cycle_length * 0.25;
const green_blink_length = cycle_length * 0.033;

const cycle_timer = 'cycle';
let start_time;
let next_start_time;

let fetched_next_start = false;

let pedestrian_button_state = 0;

let main_light_offsets = [];

const calculateOffsets = () => {
    const d_red = red_length;
    const d_yellow = d_red + yellow_length;
    const d_green = d_yellow + green_length;

    let d_green_blinks = [0, 0, 0];
    d_green_blinks[2] = d_green + green_blink_length;
    d_green_blinks[1] = d_green_blinks[2] + (green_blink_length * 2);
    d_green_blinks[0] = d_green_blinks[1] + (green_blink_length * 3);
    const d_yellow_ag = yellow_length + d_green_blinks[0];

    main_light_offsets = [
        {
            state: 0,
            offset: d_yellow_ag,
            light: ['cyl'],
            color: [BLACK],
            pedestrian_light: {
                light: ['prl'],
                color: [RED]
            }
        },
        {
            state: 0,
            offset: d_green_blinks[0],
            light: ['cgl', 'cyl'],
            color: [BLACK, YELLOW]
        },
        {
            state: 0,
            offset: d_green_blinks[0] - (green_blink_length / 2),
            light: ['cgl'],
            color: [GREEN]
        },
        {
            state: 0,
            offset: d_green_blinks[1],
            light: ['cgl'],
            color: [BLACK]
        },
        {
            state: 0,
            offset: d_green_blinks[1] - (green_blink_length / 2),
            light: ['cgl'],
            color: [GREEN]
        },
        {
            state: 0,
            offset: d_green_blinks[2],
            light: ['cgl'],
            color: [BLACK]
        },
        {
            state: 0,
            offset: d_green_blinks[2] - (green_blink_length / 2),
            light: ['cgl'],
            color: [GREEN]
        },
        {
            state: 0,
            offset: d_green,
            light: ['cgl'],
            color: [BLACK]
        },
        {
            state: 0,
            offset: d_yellow,
            light: ['cyl', 'cgl'],
            color: [BLACK, GREEN],
            pedestrian_light: {
                light: ['pgl', 'prl'],
                color: [BLACK, RED]
            }
        },
        {
            state: 0,
            offset: d_red,
            light: ['crl', 'cyl'],
            color: [BLACK, YELLOW]
        },
        {
            state: 0,
            offset: 0,
            light: ['crl', 'cyl'],
            color: [RED, BLACK],
            pedestrian_light: {
                light: ['pgl', 'prl'],
                color: [GREEN, BLACK]
            }
        },
    ];
    
    console.log('newOffsets: ', main_light_offsets);
}

const pedestrianButton = () => {
    if (!pedestrian_button_state) pedestrian_button_state = 1;
}

document.getElementById('pedestrian-button')
    .addEventListener('click', pedestrianButton, false);

const activateLight = (color, light_id) => {
    const light = document.getElementById(light_id);
    light.style.fill = color;
}


const resetCycle = async () => {
    calculateOffsets();
    fetched_next_start = false;
    start_time = next_start_time;
}

const fetchInitialStartTime = async () => {
    const light_id  = location.pathname.split('/')[1];
    const response = await fetch(api_url + Number(light_id));
    const data = await response.json();

    console.log(data);
    return data.start;
}

const fetchStartTime = () => {
    console.log('fetchingStartTime');
    const light_id  = location.pathname.split('/')[1];
    fetch(api_url + Number(light_id)).then((response) => {
        response.json().then((data) => {
            console.log('new start_time: ', data.start);
            next_start_time = data.start;
        }).catch((err) => {
            console.error('Error while parsing json: ', err);
        });
    }).catch((err) => {
        console.error('Error while fetching startTime: ', err);
    });

    fetched_next_start = true;
}

const isOffsetLessAtnIndexFromLast = (elapsed_time, n) => {
    return elapsed_time < main_light_offsets[main_light_offsets.length - n].offset;
}

const regularCycle = () => {
    const elapsed_time = Date.now() - start_time;
    for (const light_offset of main_light_offsets) {
        if (light_offset.state == 0 && elapsed_time > light_offset.offset) {

            for (let i = 0; i < light_offset.light.length; ++i) {
                light_offset.state = 1;
                const color = light_offset.color[i];
                const light_id = light_offset.light[i];
                activateLight(color, light_id);
                
            }

            if (pedestrian_button_state && light_offset.pedestrian_light) {
                const pedestrian_light_ids = light_offset.pedestrian_light.light;
                const pedestrian_light_colors = light_offset.pedestrian_light.color;

                for (let j = 0; j < pedestrian_light_ids.length; ++j) {
                    const pedestrian_color = pedestrian_light_colors[j];
                    const pedestrian_light_id = pedestrian_light_ids[j];
                    
                    activateLight(pedestrian_color, pedestrian_light_id);
                }
            } else {
                activateLight(RED, 'prl');
                activateLight(BLACK, 'pgl');
            }
        }
    }
    if (isOffsetLessAtnIndexFromLast(elapsed_time, 2) && pedestrian_button_state) pedestrian_button_state = 0;
    if (elapsed_time > main_light_offsets[7].offset && !fetched_next_start) fetchStartTime();
    if (elapsed_time > main_light_offsets[0].offset) resetCycle();
}

start_time = await fetchInitialStartTime();
calculateOffsets();
setInterval(regularCycle, 100);

