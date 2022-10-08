

const char html_head[] PROGMEM = R"rawliteral(
  <!DOCTYPE html> <html>
  <head><meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>Swan Samba LED Control</title>
  <style>html { font-family: Helvetica, sans-serif; display: inline-block; margin: 0px auto; text-align: center; background-color:#444444;}
  body{margin-top: 20px;} h1 {color: #eeeeee;margin: 10px auto 10px;font-size:110%;} h3 {color: #eeeeee;margin: 0 auto 10px auto;font-size:25px}
  p {font-size: 14px;color: #888;margin-bottom: 10px;}
  .button {display: block;width: 85px;background-color: #3498db;border: none;color: white;padding: 15px 13px;text-decoration: none;font-size: 22
  px;margin: 0px auto 15px;cursor: pointer;border-radius: 4px;}
  .button-on {background-color: #3498db;}
  .button-on:active {background-color: #2980b9;}
  .button-off {background-color: #34495e;}
  .button-off:active {background-color: #2c3e50;}
  .col {float:left; width: 33%; margin: 0px auto;}
  .row {float: left; width:100%; margin: 0px auto;}
  .row .button { float: left; margin-right: 8px;}
  </style>
  </head>)rawliteral";
  