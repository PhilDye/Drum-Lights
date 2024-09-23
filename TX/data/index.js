var gateway = `ws://${window.location.hostname}/ws`;
var websocket;

function initWebSocket() {
    console.log('Trying to open a WebSocket connection...');
    websocket = new WebSocket(gateway);
    websocket.onopen  = onOpen;
    websocket.onclose = onClose;
    websocket.onmessage = onMessage;
}

function onOpen(event) {
    console.log('Connection opened');
    document.getElementById("overlay").style.display = "none";
}

function onClose(event) {
    console.log('Connection closed');
    document.getElementById("overlay").style.display = "block";
    setTimeout(initWebSocket, 1000);
}

function onMessage(event) {
    console.log('Received a message from ' + event.origin);

    let msg = JSON.parse(event.data);
    console.log(msg);

    var active = $(`*[data-mode=${msg.mode}]`);

    $('.button').not(active).removeClass('buttonactive'); // remove buttonactive from other buttons
    $(active).addClass('buttonactive');
}



$(document).ready(function(){

    initWebSocket();

    $('.mode').click(function() {
        var mode = $(this).data("mode");

        if (mode == '199') {
            if (!window.confirm("Activate blue strobes?")) {
                return;
            }
        }

        // send to the websocket
        let msg = {
            mode: mode
        };
        console.log('Sending to websocket... ');
        console.log(msg);
        websocket.send(JSON.stringify(msg));
    });

  });