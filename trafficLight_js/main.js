import { initializeApp } from 'firebase/app';
import { getDatabase, ref, get, onValue } from 'firebase/database';

// Võetakse URL-ist foori ID, mis tähendab, et tuleb minna aadressile "{baseaddress}/{id}"
const board_id = location.pathname.split('/')[1];
const api_url = 'https://orca-app-tlr83.ondigitalocean.app/get_start_time/';

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

// Vaikimisi tsyklipikkus_length muutujasse.
let cycle_length = 20 * 1000;

const light_ids = ['crl', 'cyl', 'cgl'];

const cycle_timer = 'cycle';
let start_time;
let next_start_time;

let fetched_next_start = false;

let pedestrian_button_state = 0;

let main_light_offsets = [];
let is_yellow_mode = false;

let yellow_cycle_interval;
let regular_cycle_interval;

//
const calculateOffsets = () => {
    const red_length = cycle_length * 0.5;
    const yellow_length = cycle_length * 0.05;
    const green_length = cycle_length * 0.25;
    const green_blink_length = cycle_length * 0.033;

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

// Abifunktsioon, mis aktiveerib vastavalt sisseantud foori IDle ja värvile vastava
// foori õige värviga.
const activateLight = (color, light_id) => {
    const light = document.getElementById(light_id);
    light.style.fill = color;
}

// Arvutab uuesti foori tulede põlemise kestvused, määrab, et uut aega ei ole päritud
// ning paneb start_time muutujasse eelnevalt setitud next_start_time väärtuse.
const resetCycle = async () => {
    calculateOffsets();
    fetched_next_start = false;
    start_time = next_start_time;
}

// Vastavalt foori IDle küsib serverilt tema uue algusaja, et ta ühildusk teiste fooridega ning tagastab selle.
const fetchInitialStartTime = async () => {
    const light_id  = location.pathname.split('/')[1];
    const response = await fetch(api_url + Number(light_id));
    const data = await response.json();
    console.log('initial start: ', data.start);

    return data.start;
}

// Vastavalt foori IDle küsib serverilt uue algusaja, see pannakse next_start_time muutujasse
// ning määratakse et uus aeg on päritud.
const fetchStartTime = () => {
    fetch(api_url + Number(board_id)).then((response) => {
        response.json().then((data) => {
            next_start_time = data.start;
            console.log('next_start: ', next_start_time);
        }).catch((err) => {
            console.error('Error while parsing json: ', err);
        });
    }).catch((err) => {
        console.error('Error while fetching startTime: ', err);
    });

    fetched_next_start = true;
}

// Kontrollib, kas algusest möödunud aeg on väiksem väärtus kui funktsiooni sisseantud indeksi kohal main_light_offsets massiivis
// oleva elemendi offset.
const isOffsetLessAtnIndexFromLast = (elapsed_time, n) => {
    return elapsed_time < main_light_offsets[main_light_offsets.length - n].offset;
}

// Tegeleb foori kollase vilkuva tule režiimiga.
const yellowCycle = () => {
    const elapsed_time = Date.now() - start_time;
    if (elapsed_time < 500) {
        activateLight(YELLOW, 'cyl');
    } else if (elapsed_time < 1000) {
        activateLight(BLACK, 'cyl');
    } else {
        start_time += 2000;
    }
}

// Põhitsükkel, kus toimub foori tegevuse kontrollimine.
const regularCycle = () => {
    const elapsed_time = Date.now() - start_time;
    // Käiakse kõik main_light_offsets massiivis olevad elemendid läbi
    for (const light_offset of main_light_offsets) {
        // Kui vastava elemendi seisund on 0 ja algusest möödunud aeg on suurem kui selle elemendi offset
        if (light_offset.state == 0 && elapsed_time > light_offset.offset) {

            // Pannakse käsitletava elemendi seisund üheks, võetakse tema värv ning fooritule ID ning 
            // pannakse nende järgi vastav tuli põlema.
            for (let i = 0; i < light_offset.light.length; ++i) {
                light_offset.state = 1;
                const color = light_offset.color[i];
                const light_id = light_offset.light[i];
                activateLight(color, light_id);
                
            }

            // Kontrollib, kas on jalakäijate ülekäigu nuppu vajutatud ning kas elemendil on olemas jalakäijate jaoks loodud atribuut.
            // Kui on, võetakse vastav jalakäija foori ID ja värv ning aktiveeritakse vastavad tuled.
            if (pedestrian_button_state && light_offset.pedestrian_light) {
                const pedestrian_light_ids = light_offset.pedestrian_light.light;
                const pedestrian_light_colors = light_offset.pedestrian_light.color;

                for (let j = 0; j < pedestrian_light_ids.length; ++j) {
                    const pedestrian_color = pedestrian_light_colors[j];
                    const pedestrian_light_id = pedestrian_light_ids[j];
                    
                    activateLight(pedestrian_color, pedestrian_light_id);
                }
            } else {
                // Kui ei ole, pannakse lihtsalt jalakäijafoor punaseks.
                activateLight(RED, 'prl');
                activateLight(BLACK, 'pgl');
            }
        }
    }
    if (isOffsetLessAtnIndexFromLast(elapsed_time, 2) && pedestrian_button_state) pedestrian_button_state = 0;
    if (elapsed_time > main_light_offsets[0].offset) resetCycle();
    if (elapsed_time > 200 && !fetched_next_start) fetchStartTime();
}

// Päritakse algusaeg, mis salvestatakse start_time muutujasse, arvutatakse foor tulede põlemise kestvused
// ning käivitatakse tavaline foori tsükkel
const startRegularCycle = async () => {
    console.log('regular_cycle');
    start_time = await fetchInitialStartTime();
    calculateOffsets();
    regular_cycle_interval = setInterval(regularCycle, 100);
}

// Pannakse start_time muutujasse praegune aeg ning käivitatakse kollase vilkuva tule tsükkel.
const startYellowCycle = () => {
    console.log('yellow_cycle');
    start_time = Date.now();
    yellow_cycle_interval = setInterval(yellowCycle, 100);
}

// Käiakse kõik tuled läbi ning pannakse kõik kustu.
const resetLights = () => {
    light_ids.forEach((light_id) => {
        activateLight(BLACK, light_id);
    })
}

// Skännitakse baasi automatic vastava foori ID-ga välja
// Kui seal väärtus muutub, kontrollitakse, kas vastus on 0 või 1 ehk kas töötab automaatselt või
// on kollase tule vilkuvas režiimis.
onValue(ref(db, `traffic_lights/automatic/${board_id}`), (snapshot) => {
    console.log('onValue called');
    const is_yellow = Boolean(snapshot.val());
    console.log("yellow:" + is_yellow);

    if (is_yellow) {
        clearInterval(regular_cycle_interval);
        resetLights();
        startYellowCycle();
    } else {
        clearInterval(yellow_cycle_interval);
        resetLights();
        startRegularCycle();
    }
})

// Skännitakse baasi cycleLength välja
// Kui seal väärtus muutub, kirjutatakse vastav väärtus cycle_length muutujasse.
onValue(ref(db, 'traffic_lights/cycleLength'), (snapshot) => {
    const db_cycle_length = snapshot.val();
    console.log(db_cycle_length);
    cycle_length = db_cycle_length;
})

