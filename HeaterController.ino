


/*
  WiFi Web Server LED Blink

  A simple web server that lets you blink an LED via the web.
  This sketch will print the IP address of your WiFi Shield (once connected)
  to the Serial monitor. From there, you can open that address in a web browser
  to turn on and off the LED on pin 9.

  If the IP address of your shield is yourAddress:
  http://yourAddress/H turns the LED on
  http://yourAddress/L turns it off

  This example is written for a network using WPA encryption. For
  WEP or WPA, change the Wifi.begin() call accordingly.

  Circuit:
   WiFi shield attached
   LED attached to pin 9

  created 25 Nov 2012
  by Tom Igoe
*/
#include <SPI.h>
#include <WiFi101.h>
#include <RTCZero.h>

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_ILI9340.h"
#include <EnableInterrupt.h>


#define HEATER_ENABLE 9
#define PUMP_STROKE 6

#define BACK_LED 13
#define TFT_CS 10
#define TFT_DC 12
#define TFT_RST 11

#define TEMP_INLET_PIN A0
#define TEMP_OUTLET_PIN A1


// Declare function prototypes
void printWifiStatus();
void alarmMatch();
void print2digits(int number);
float calc_temp(int adc_val);
void create_graph_yaxis(int x_pos, int y_pos, int height, int max_val, uint16_t color);
void update_graph(int x_pos, int y_pos, int val, uint16_t color);

//////////////////// Web server stuff ///////////////////
IPAddress ip(192, 168, 1, 1); //default IP address (a new onew will be assigned by DHCP)
char ssid[] = "TP-LINK_C3A2";      //  your network SSID (name)
char pass[] = "38091704";   // your network password
//char ssid[] = "PLUSNET-FC7T";      //  your network SSID (name)
//char pass[] = "4c3d9edcd3";   // your network password
int keyIndex = 0;                 // your network key Index number (needed only for WEP)
int status = WL_IDLE_STATUS;
WiFiServer server(80);

/*Create a tft object */
Adafruit_ILI9340 tft = Adafruit_ILI9340(TFT_CS, TFT_DC, TFT_RST);
/* Create an rtc object */
RTCZero rtc;

//////////////////// Sensor stuff ///////////////////
const float adc_factor = ((3.3 / 1024.0));
const float temp_offset = 1.8528;
const float temp_factor = -0.01179;
int max_temp = 40;
float temp_inlet = 0;
float temp_outlet = 0;
String heater_state = "idle";
unsigned long pump_strokes = 0;
const float pump_volume = 0.000025;

void interruptFunction() {
  pump_strokes++;
}

//////////////////// Display Stuff //////////////////////
const int temp_value_pos_x = 15 * 12;
const int temp_value_pos_y = 20 + 16;

const int time_pos_x = 0;
const int time_pos_y = 12;

const int ctrl_type_pos_x = 30 * 6;
const int ctrl_type_pos_y = 0;

const int graph_height = 100;
int graph_width = tft.width();
const int graph_x_pos = 0;
int graph_y_pos = tft.height();

String control_state = "local";

int graph_cursor = graph_x_pos + 25;

//////////////////// RTC Stuff //////////////////////

int last_seconds = 0;

/* Change these values to set the current initial time */
const byte seconds = 0;
const byte minutes = 0;
const byte hours = 16;

/* Change these values to set the current initial date */
const byte day = 1;
const byte month = 1;
const byte year = 17;




void setup() {

  // Set up the pump sense to be an interrupt
  pinMode(PUMP_STROKE, INPUT_PULLUP);  // See http://arduino.cc/en/Tutorial/DigitalPins
  enableInterrupt(PUMP_STROKE, interruptFunction, RISING);

  /////////// Initialise the RTC ///////////
  rtc.begin(); // initialize RTC 24H format

  rtc.setTime(hours, minutes, seconds);
  rtc.setDate(day, month, year);

  rtc.setAlarmTime(16, 0, 100);
  rtc.enableAlarm(rtc.MATCH_HHMMSS);

  rtc.attachInterrupt(alarmMatch);

  /////////// Initialise the TFT ///////////
  tft.begin();
  tft.setRotation(3);
  tft.fillScreen(ILI9340_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9340_WHITE);  tft.setTextSize(2);
  WiFi.setPins(8, 7, 4, 2);
  //Serial.begin(9600);      // initialize serial communication
  pinMode(HEATER_ENABLE, OUTPUT);      // set the LED pin mode
  pinMode(BACK_LED, OUTPUT);      // set the LED pin mode
  digitalWrite(BACK_LED, HIGH);               // GET /H turns the LED onBACK_LED
  tft.setCursor(0, 0);
  tft.setTextColor(ILI9340_WHITE);
  tft.setTextSize(1);

  graph_width = tft.width();
  graph_y_pos = tft.height();



  // check for the presence of the shield:
  if (WiFi.status() == WL_NO_SHIELD) {
    tft.setCursor(0, 0);
    tft.println("No Communications from Wifi module!");
  }

  // attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) {
    tft.println("Attempting to connect to: ");
    tft.println(ssid);                   // print the network name (SSID);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  server.begin();                           // start the web server on port 80
  printWifiStatus();                        // you're connected now, so print out the status
  tft.setCursor(0, 0);
  tft.setTextSize(1);
  tft.print("IP Address : ");
  tft.println(ip);                   // print the network name (SSID);
  tft.setCursor(0, temp_value_pos_y);
  tft.setTextSize(2);
  tft.println("Inlet temp   = ");
  tft.println("Outlet temp  = ");
  tft.println("Fuel Used(l) = ");
  tft.println("Heater State = ");

  // Display the Y axis fo the graph
  create_graph_yaxis(0, tft.height(), graph_height, 50, ILI9340_BLUE);
  create_graph_yaxis(tft.width() - 20, tft.height(), graph_height, 140, ILI9340_RED);

  tft.setTextColor(ILI9340_BLUE);
  tft.setCursor(0, tft.height() - graph_height - 20);
  tft.setTextSize(1);
  tft.println("Inlet Temp");


  tft.setTextColor(ILI9340_RED);
  tft.setCursor(tft.width() - 70, tft.height() - graph_height - 20);
  tft.println("Outlet Temp");

  tft.setTextColor(ILI9340_WHITE);
  tft.setTextSize(2);

}

void loop() {


  WiFiClient client = server.available();   // listen for incoming clients

  if (rtc.getSeconds() != last_seconds)
  {
    last_seconds = rtc.getSeconds();
    temp_inlet = calc_temp(analogRead(TEMP_INLET_PIN));
    temp_outlet = calc_temp(analogRead(TEMP_OUTLET_PIN));
    update_graph(graph_x_pos, graph_y_pos, temp_outlet, 140, ILI9340_RED);
    update_graph(graph_x_pos, graph_y_pos, temp_inlet, 50, ILI9340_BLUE);

    // Clear the last graph values for a few samples
    if (graph_cursor <  graph_width - 25)
    {
      tft.fillRect(graph_cursor, tft.height() - graph_height, 20, graph_height, ILI9340_BLACK);
    }

    tft.fillRect(temp_value_pos_x, temp_value_pos_y, 8 * 12, 64, ILI9340_BLACK);
    tft.setCursor(temp_value_pos_x, temp_value_pos_y);
    tft.println(temp_inlet);
    tft.setCursor(temp_value_pos_x, temp_value_pos_y + 16);
    tft.println(temp_outlet);
    tft.setCursor(temp_value_pos_x, temp_value_pos_y + 32);
    tft.print(pump_strokes * pump_volume, 4);
    //tft.print("("); tft.print(pump_strokes); tft.print(")");
    tft.setCursor(temp_value_pos_x, temp_value_pos_y + 48);
    tft.println(heater_state);
    delay(1000);

    /////////////////// Print the time and date //////////////
    tft.fillRect(time_pos_x, time_pos_y, 320, 16, ILI9340_BLACK);
    tft.setCursor(time_pos_x, time_pos_y);
    // Print date...
    print2digits(rtc.getDay());
    tft.print("/");
    print2digits(rtc.getMonth());
    tft.print("/");
    print2digits(rtc.getYear());
    tft.print(" ");

    // ...and time
    print2digits(rtc.getHours());
    tft.print(":");
    print2digits(rtc.getMinutes());
    tft.print(":");
    print2digits(rtc.getSeconds());
  }


  if (client) {                             // if you get a client,
    tft.setCursor(180, 0);
    tft.setTextSize(1);
    tft.fillRect(ctrl_type_pos_x, ctrl_type_pos_y, 320 - ctrl_type_pos_x, 8, ILI9340_BLACK);
    tft.print("Web Control");           // print a message out the serial port
    tft.setTextSize(2);
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        //tft.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            client.println("<HTML>");
            client.println("<HEAD>");
            client.println("<TITLE>Heater Control</TITLE>");

            client.println("<script type = \"text/javascript\">");
            client.println("function display_ct() {");
            client.println("var refresh = 1000; // Refresh rate in milli seconds");
            client.println("mytime = setTimeout('display_ct()', refresh)} ");
            client.println("function display_ct() {");
            client.println("var strcount");
            client.println("var x = new Date()");
            client.println("document.getElementById('ct').innerHTML = x;");
            client.println("tt = display_c(); }");
            client.println("</script >");

            client.println("</HEAD>");

            client.println("<H1>Boys Room Heater Control</H1>");
            if (heater_state != "running")
            {
              client.println("<button onClick=location.href='/H\' style='FONT-SIZE: 12px; HEIGHT: 25px; FONT-FAMILY: Arial; WIDTH: 100px;'>");
              client.println("Heater Enable");
              client.println("</button>");
            }
            else
            {
              client.println("<button onClick=location.href='/L\' style='FONT-SIZE: 12px; HEIGHT: 25px; FONT-FAMILY: Arial; WIDTH: 100px;'>");
              client.println("Heater Disable");
              client.println("</button>");
            }

            client.println("<button onClick=location.href='/time\' style='FONT-SIZE: 12px; HEIGHT: 25px; FONT-FAMILY: Arial; WIDTH: 100px;'>");
            client.println("Set Time");
            client.println("</button>");


            client.print("<br>");
            client.print("Current Date: ");
            client.println("<body onload = display_ct();>");
            client.println("<span id = 'ct' > </span>");
            client.print("<br>");

            client.print("Heater Date: ");
            client.print(rtc.getDay());
            client.print("/");
            client.print(rtc.getMonth());
            client.print("/");
            client.print(rtc.getYear());
            client.print(" ");
            client.print(rtc.getHours());
            client.print(":");
            client.print(rtc.getMinutes());
            client.print(":");
            client.print(rtc.getSeconds());



            client.print("<br>");

            client.print("Inlet temp   = ");
            client.print(temp_inlet);
            client.print("<br>");
            client.print("Outlet temp  = ");
            client.print(temp_outlet);
            client.print("<br>");
            client.print("Fuel Used(l)    = ");
            client.print(pump_strokes * pump_volume, 4);
            client.print("<br>");
            client.print("Heater State = ");
            client.print(heater_state);
            client.print("<br>");

            client.println("</BODY>");
            client.println("</HTML>");


            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;


            /* // send a standard http response header
               client.println("HTTP/1.1 200 OK");
               client.println("Content-Type: text/html");
               client.println("Connection: close");
               client.println();
               // send web page
               client.println("<!DOCTYPE html>");
               client.println("<html>");
               client.println("<head>");
               client.println("<title>Arduino Web Page</title>");
               client.println("</head>");
               client.println("<body>");
               client.println("<h1>Hello from Arduino!</h1>");
               client.println("<p>A web page from my Arduino server</p>");
               client.println("<p style=\"color:red\">Red<form><input type=\"text\"></form></p>");
               client.println("<p style=\"color:blue\">Blue<form><input type=\"text\"></form></p>");
               client.println("<p style=\"color:green\">Green<form><input type=\"text\"></form></p>");
               client.println("<button type=\"button\"onclick=\"GetColors()\">Set Color</button>");
               client.println("</body>");
               client.println("</html>");
               break; */
          }
          else
          { // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        }
        else if (c != '\r')
        { // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
      }


      // Check to see if the client request was "GET /H" or "GET /L":
      if (currentLine.endsWith("GET /H"))
      {
        digitalWrite(HEATER_ENABLE, HIGH);               // GET /H turns the LED on
        heater_state = "running";
      }
      if (currentLine.endsWith("GET /L"))
      {
        digitalWrite(HEATER_ENABLE, LOW);               // GET /L turns the LED off
        heater_state = "idle";
      }
    }
  }
  // close the connection:
  client.stop();
  //tft.println("client disonnected");
}

void printWifiStatus() {
  tft.setTextSize(1);
  // print the SSID of the network you're attached to:
  tft.print("SSID: ");
  tft.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  ip = WiFi.localIP();
  tft.print("IP Address: ");
  tft.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  tft.print("signal strength (RSSI):");
  tft.print(rssi);
  tft.println(" dBm");
  // print where to go in a browser:
  tft.print("Assigned IP address = ");
  tft.println(ip);
  //Wait a while then clear the screen ready for the real display
  delay(2000);
  tft.fillScreen(ILI9340_BLACK);
}

void alarmMatch()
{
  tft.fillRect(ctrl_type_pos_x, ctrl_type_pos_y, 320 - ctrl_type_pos_x, 8, ILI9340_BLACK);
  tft.setCursor(ctrl_type_pos_x, ctrl_type_pos_y);
  tft.setTextSize(1);
  tft.print("Timer Control");
  tft.setTextSize(2);
}


void print2digits(int number)
{
  if (number < 10) {
    tft.print("0"); // print a 0 before if the number is < than 10
  }
  tft.print(number);
}

float calc_temp(int adc_val)
{
  float inter_temp = adc_factor * adc_val;
  float temp = (-2.3654 * (inter_temp * inter_temp)) + (-78.154 * inter_temp) + 153.857;
  return temp;
}

void create_graph_yaxis(int x_pos, int y_pos, int height, int max_val, uint16_t color)
{

  // Display the Y axis for a graph
  tft.setTextColor(color);
  tft.setTextSize(1);
  tft.setCursor(x_pos, y_pos - 9);
  tft.print("0");
  tft.drawFastHLine(x_pos, y_pos - 1, 20, color);

  tft.setCursor(x_pos, y_pos - (height / 4) - 8);
  tft.print(max_val / 4);
  tft.drawFastHLine(x_pos, y_pos - (height / 4), 20, color);

  tft.setCursor(x_pos, y_pos - height / 2 - 8);
  tft.print(max_val / 2);
  tft.drawFastHLine(x_pos, y_pos - (height / 2), 20, color);

  tft.setCursor(x_pos, y_pos - ((height / 4) * 3) - 8);
  tft.print((max_val / 4) * 3);
  tft.drawFastHLine(x_pos, y_pos - ((height / 4) * 3), 20, color);

  tft.setCursor(x_pos, y_pos -  height - 8);
  tft.print(max_val);
  tft.drawFastHLine(x_pos, y_pos - height, 20, color);
  tft.drawFastHLine(x_pos, y_pos - height, 20, color);

  tft.setTextColor(ILI9340_WHITE);
  tft.setTextSize(2);
}

void update_graph(int x_pos, int y_pos, int val, int max_val, uint16_t color)
{

  //Rescale the data to fit the graph
  val = map(val, 0, max_val, 0, 100);
  //Draw the line
  tft.drawFastVLine(graph_cursor, tft.height() - val, 3, color);

  if (graph_cursor ==  graph_width - 25)
  {
    graph_cursor = 25;
  }
  else
  {
    graph_cursor ++;
  }

}
