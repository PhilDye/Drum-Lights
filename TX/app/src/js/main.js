// Import our custom CSS
import '../scss/custom.scss'

// Import Bootstrap JS
import { Modal, Tab } from 'bootstrap'

// The catalogue of modes, shared with the iOS app; the build checks it against the firmware
import catalogue from '../modes.json'

const modesByNumber = {};

function modeLabel(entry) {
    return entry.name || entry.colours.map((c) => catalogue.colours[c].name).join('/');
}

function modeButton(entry) {
    modesByNumber[entry.mode] = entry;

    const button = document.createElement('button');
    button.className = 'btn btn-outline-primary';
    button.dataset.mode = entry.mode;
    button.textContent = modeLabel(entry);
    if (entry.confirm) {
        button.dataset.bsToggle = 'modal';
        button.dataset.bsTarget = '#nineninenineModal';
    }
    return button;
}

function spacerButton() {
    const button = document.createElement('button');
    button.className = 'btn btn-outline-primary';
    button.disabled = true;
    button.innerHTML = '&nbsp;';
    return button;
}

function buttonColumn(buttons) {
    const column = document.createElement('div');
    column.className = 'col d-grid gap-3';
    column.append(...buttons);
    return column;
}

// Two-colour modes sit beside the single colour they start with, with gaps where there is none
function twoColourButtons(effect) {
    const remaining = [...effect.twoColour];
    const buttons = effect.modes.map((single) => {
        const index = remaining.findIndex((two) => two.colours[0] === single.colours[0]);
        return index === -1 ? spacerButton() : modeButton(remaining.splice(index, 1)[0]);
    });
    while (buttons.length && buttons[buttons.length - 1].disabled) {
        buttons.pop();
    }
    return [...buttons, ...remaining.map(modeButton)];
}

function renderCatalogue() {
    const nav = document.querySelector('#nav-tab');
    const strobe = document.querySelector('#btnStrobe');
    const panes = document.querySelector('#nav-tabContent');

    catalogue.effects.forEach((effect) => {
        const tab = document.createElement('button');
        tab.className = 'nav-link';
        tab.id = `nav-${effect.id}-tab`;
        tab.type = 'button';
        tab.textContent = effect.name;
        tab.dataset.bsToggle = 'pill';
        tab.dataset.bsTarget = `#nav-${effect.id}`;
        tab.setAttribute('role', 'tab');
        tab.setAttribute('aria-controls', `nav-${effect.id}`);
        tab.setAttribute('aria-selected', 'false');
        nav.insertBefore(tab, strobe);

        const pane = document.createElement('div');
        pane.className = 'tab-pane';
        pane.id = `nav-${effect.id}`;
        pane.tabIndex = 0;
        pane.setAttribute('role', 'tabpanel');
        pane.setAttribute('aria-labelledby', tab.id);

        const singles = buttonColumn(effect.modes.map(modeButton));
        if (effect.twoColour) {
            // singles on the left, two-colour on the right
            const row = document.createElement('div');
            row.className = 'row';
            [singles, buttonColumn(twoColourButtons(effect))].forEach((column) => {
                const col = document.createElement('div');
                col.className = 'col';
                col.append(column);
                row.append(col);
            });
            pane.append(row);
        } else {
            pane.append(singles);
        }
        panes.append(pane);
    });

    // Strobe, Off and Auto keep their place in the page; only their mode numbers come from the catalogue
    document.querySelectorAll('[data-action]').forEach((button) => {
        button.dataset.mode = catalogue.actions[button.dataset.action];
    });
}

function showConfirm(entry) {
    document.querySelector('#nineninenineModalTitle').textContent = entry.confirm.title;
    document.querySelector('#nineninenineModal .modal-body p').textContent = entry.confirm.message;
    document.querySelector('#btnNineNineNine').dataset.confirmMode = entry.mode;
}

var gateway = `ws://${window.location.hostname}/ws`;
if (process.env.NODE_ENV === 'development') {
    var gateway = `ws://websocket-echo.com/`;
}

var networkModalEl = document.querySelector('#networkModal');
var networkModal = Modal.getOrCreateInstance(networkModalEl); // Returns a Bootstrap modal instance

let websocket;

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen  = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    networkModal.hide();
}

function onClose(event) {
    console.log('Connection closed');
    networkModal.show();  
    setTimeout(initWebSocket, 1000);
}

function onMessage(event) {
    console.log('Received a message from ' + event.origin);

    let msg = JSON.parse(event.data);
    console.log(msg);

    document.querySelectorAll('.btn.active').forEach((b) => {
        b.classList.remove('active');
    });

    if (msg.mode == 0) {
        // hide all tabs
        document.querySelectorAll('.nav-link.active').forEach((b) => {
            b.classList.remove('active');
        });
        document.querySelectorAll('div.tab-pane.active.show').forEach((b) => {
            b.classList.remove('active');
            b.classList.remove('show');
        });
    } else {
        var newButton = $(`.btn[data-mode=${msg.mode}]`);

        // find the tab that contains the new mode
        var labelledBy = newButton.parents('div.tab-pane').attr('aria-labelledby');
        $('#' + labelledBy).trigger("click");

        $(newButton).addClass('active');

    }

    if (msg.auto) {
        document.getElementById('btnAuto').classList.add('active');
    }
}

$(function(){

    renderCatalogue();

    initWebSocket();

    $('.btn[data-mode]').on("click", function(e) {
        e.preventDefault()

        var mode = $(this).data("mode");

        if (modesByNumber[mode] && modesByNumber[mode].confirm) {
            // this is handled by a modal
            showConfirm(modesByNumber[mode]);
            return;
        }

        // send to the websocket
        let msg = {
            mode: mode
        };
        console.log('Sending to websocket... ');
        console.log(msg);
        websocket.send(JSON.stringify(msg));
    });

    $('#btnNineNineNine').on("click", function(e) {
        e.preventDefault()

        // send to the websocket
        let msg = {
            mode: Number(this.dataset.confirmMode)
        };
        console.log('Sending to websocket... ');
        console.log(msg);
        websocket.send(JSON.stringify(msg));
    });

  });