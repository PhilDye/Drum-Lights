// Import our custom CSS
import '../scss/custom.scss'

// Import Bootstrap JS
import { Modal, Tab } from 'bootstrap'

var gateway = `ws://${window.location.hostname}/ws`;
if (process.env.NODE_ENV === 'development') {
    var gateway = `ws://websocket-echo.com/`;
}

var networkModalEl = document.querySelector('#networkModal')
var networkModal = Modal.getOrCreateInstance(networkModalEl) // Returns a Bootstrap modal instance

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
}

$(function(){

    initWebSocket();

    $('.btn[data-mode]').on("click", function(e) {
        e.preventDefault()

        var mode = $(this).data("mode");

        if (mode == 199) {
            // this is handled by a modal
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
            mode: 199
        };
        console.log('Sending to websocket... ');
        console.log(msg);
        websocket.send(JSON.stringify(msg));
    });

  });