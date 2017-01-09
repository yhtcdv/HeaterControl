


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

#define TEMP_SET_ANALOGUE A0 //varies between 2.4 and 2.6V
#define MAX_SETPOINT_TEMP 35
#define MIN_SETPOINT_TEMP 5

#define BACK_LED 13
#define TFT_CS 10
#define TFT_DC 12
#define TFT_RST 11

#define TEMP_INLET_PIN A2
#define TEMP_OUTLET_PIN A1


// Declare function prototypes
void printWifiStatus();
void alarmMatch();
int print2digits(int number);
float calc_temp(int adc_val);
void create_graph_yaxis(int x_pos, int y_pos, int height, int max_val, uint16_t color);
void update_graph(int x_pos, int y_pos, int val, uint16_t color);

//////////////////// Web server stuff ///////////////////
IPAddress ip(192, 168, 1, 1); //default IP address (a new onew will be assigned by DHCP)
char ssid[] = "TP-LINK_BR";      //  your network SSID (name)
char pass[] = "b0ysr00m";   // your network password
//char ssid[] = "TP-LINK_C3A2";      //  your network SSID (name)
//char pass[] = "38091704";   // your network password
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

//////////////////// Control Stuff /////////////////
int set_temp = 20;

//////////////////// Display Stuff //////////////////////
const int temp_value_pos_x = 15 * 12;
const int temp_value_pos_y = 24;

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

/* Change these values to set the current initial time */
const byte seconds = 0;
const byte minutes = 0;
const byte hours = 16;

/* Change these values to set the current initial date */
const byte day = 1;
const byte month = 1;
const byte year = 17;


int last_seconds = 0;
int last_minutes = 0;
int last_hours = 16;


int last_day = 1;
int last_month = 1;
int last_year = 17;



void setup() {

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
  tft.println("Set temp     = ");
  tft.println("Inlet temp   = ");
  tft.println("Outlet temp  = ");
  tft.println("Fuel Used(l) = ");
  tft.println("Heater State = ");

  // Display the Y axis fo the graph
  create_graph_yaxis(0, tft.height(), graph_height, 50, ILI9340_BLUE);
  create_graph_yaxis(tft.width() - 20, tft.height(), graph_height, 140, ILI9340_RED);


  tft.setCursor(0, tft.height() - graph_height - 28);
  tft.setTextSize(1);
  tft.setTextColor(ILI9340_YELLOW);
  tft.println("Set Temp");
  tft.setTextColor(ILI9340_BLUE);
  tft.println("Inlet Temp");


  tft.setTextColor(ILI9340_RED);
  tft.setCursor(tft.width() - 70, tft.height() - graph_height - 20);
  tft.println("Outlet Temp");

  tft.setTextColor(ILI9340_WHITE);
  tft.setTextSize(2);

  /////////// Set up the pump sense to be an interrupt ///////////
  pinMode(PUMP_STROKE, INPUT_PULLUP);  // See http://arduino.cc/en/Tutorial/DigitalPins
  enableInterrupt(PUMP_STROKE, interruptFunction, RISING);

  /////////// Initialise the RTC ///////////
  rtc.begin(); // initialize RTC 24H format
  rtc.setEpoch(1451606400); // Jan 1, 2016

  rtc.setAlarmTime(16, 0, 100);
  //rtc.enableAlarm(rtc.MATCH_HHMMSS);

  //rtc.attachInterrupt(alarmMatch);

  /////////// Set up the temperature setpoint analogue pin //////////////////
  analogWriteResolution(8);
  analogWrite(TEMP_SET_ANALOGUE, map(set_temp, MIN_SETPOINT_TEMP, MAX_SETPOINT_TEMP, 160, 190));

}

void loop() {


  WiFiClient client = server.available();   // listen for incoming clients

  //Update the display every second
  if (rtc.getSeconds() != last_seconds)
  {
    //Update the temperature sensor inputs
    temp_inlet = calc_temp(analogRead(TEMP_INLET_PIN));
    temp_outlet = calc_temp(analogRead(TEMP_OUTLET_PIN));
    //Draw the graph
    update_graph(graph_x_pos, graph_y_pos, temp_outlet, 140, ILI9340_RED);
    update_graph(graph_x_pos, graph_y_pos, temp_inlet, 50, ILI9340_BLUE);
    update_graph(graph_x_pos, graph_y_pos, set_temp, 50, ILI9340_YELLOW);

    if (graph_cursor ==  graph_width - 40)
    {
      graph_cursor = 25;
    }
    else
    {
      graph_cursor ++;
    }

    // Clear the last graph values for a few samples
    if (graph_cursor <  graph_width - 25)
    {
      tft.fillRect(graph_cursor, tft.height() - graph_height, 20, graph_height, ILI9340_BLACK);
    }

    //Update the temperature and other parameters
    tft.fillRect(temp_value_pos_x, temp_value_pos_y,  tft.height() - temp_value_pos_y, 80, ILI9340_BLACK);
    tft.setCursor(temp_value_pos_x, temp_value_pos_y);
    tft.println(set_temp);
    tft.setCursor(temp_value_pos_x, temp_value_pos_y + 16);
    tft.println(temp_inlet);
    tft.setCursor(temp_value_pos_x, temp_value_pos_y + 32);
    tft.println(temp_outlet);
    tft.setCursor(temp_value_pos_x, temp_value_pos_y + 48);
    tft.println(pump_strokes * pump_volume, 4);
    //tft.print("("); tft.print(pump_strokes); tft.print(")");
    tft.setCursor(temp_value_pos_x, temp_value_pos_y + 64);
    tft.println(heater_state);

    /////////////////// Print the time and date //////////////
    tft.setTextSize(1);
    tft.fillRect(time_pos_x, time_pos_y, 320, 8, ILI9340_BLACK);
    tft.setTextColor(ILI9340_WHITE);
    tft.setCursor(time_pos_x, time_pos_y);
    // Print date...
    last_day =  print2digits(rtc.getDay());
    tft.print("/");
    last_month =  print2digits(rtc.getMonth());
    tft.print("/");
    last_year =  print2digits(rtc.getYear());
    tft.print(" ");

    // ...and time
    last_hours = print2digits(rtc.getHours());
    tft.print(":");
    last_minutes =  print2digits(rtc.getMinutes());
    tft.print(":");
    last_seconds =  print2digits(rtc.getSeconds());
    tft.setTextSize(2);
  }


  if (client) {                             // if you get a client,
    tft.setCursor(180, 0);
    tft.setTextSize(1);
    tft.fillRect(ctrl_type_pos_x, ctrl_type_pos_y, 320 - ctrl_type_pos_x, 8, ILI9340_BLACK);
    tft.print("Web Control");           // print a message out the serial port
    tft.setTextSize(2);
    bool clinet_resp = false;
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        //tft.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // if the byte is a newline character

          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            clinet_resp = true;
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            client.println("<HTML>");
            client.println("<HEAD>");
            //client.println("<meta http-equiv= \"refresh\" content= \"2\" >");
            client.println("<TITLE>Heater Control</TITLE>");

            client.println("<script type = \"text/javascript\">");
            //            client.println("function create_time_url(){");
            //            client.println("var client_date = new Date();");
            //            client.println("var client_year = Date().getYear();");
            //            client.println("var client_month = Date().getMonth();");
            //            client.println("var client_day = Date().getDay();");
            //            client.println("var client_hour = Date().getHour();");
            //            client.println("var client_min = Date().getMinutes();");
            //            client.println("var url = window.location.href;");
            //            client.println("url = url + client_date;");
            //            //client.println("url = url + \"\time\\\";");
            //            client.println("return url;}");

            client.println("var baseurl = window.location.href;");
            client.println("baseurl = baseurl.substring(0 , baseurl.indexOf('?'))");

            client.println("</script >");
            client.println("</HEAD>");



            client.println("<H1>Boys Room Heater Control</H1>");
            //            if (heater_state != "running")
            //            {
            client.print("<button onClick=\"window.location=baseurl + '?HeaterEn'\" style='FONT-SIZE: 12px; HEIGHT: 25px; FONT-FAMILY: Arial; WIDTH: 100px;'>");
            client.print("Heater Enable");
            client.println("</button>");
            //            }
            //            else
            //            {
            client.print("<button onClick=\"window.location=baseurl + '?HeaterDis'\" style='FONT-SIZE: 12px; HEIGHT: 25px; FONT-FAMILY: Arial; WIDTH: 100px;'>");
            client.print("Heater Disable");
            client.println("</button>");
            //            }

            client.println("<br>");
            client.println("<br>");
            client.print("<button onClick=\"window.location=baseurl + '?time' + (new Date().valueOf());return false\"; style='FONT-SIZE: 12px; HEIGHT: 25px; FONT-FAMILY: Arial; WIDTH: 100px;'>");
            client.print("Set Time");
            client.println("</button>");
            client.println("<br>");

            client.println("<br>");
            client.print("<button onClick=\"window.location=baseurl + '?temp' + 10 ;return false\"; style='FONT-SIZE: 12px; HEIGHT: 25px; FONT-FAMILY: Arial; WIDTH: 100px;'>");
            client.print("Set Temp");
            client.println("</button>");

            client.println("<input type=\"text\" name=\"settemp\"><br>");
            client.println("<br>");

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
            client.print(" (");
            client.print(set_temp);
            client.print(")");
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

          }
          else
          { // if you got a newline, then clear currentLine:
            currentLine = "";
            tft.setTextSize(1);
            tft.setTextColor(ILI9340_WHITE);
            tft.setCursor(time_pos_x + 160, time_pos_y);
            tft.println("end!");
            tft.setTextSize(2);
          }
        }
        else if (c != '\r')
        { // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }
        else
        {
          if (clinet_resp)
          {
            clinet_resp = false;
            if (currentLine.endsWith("HeaterEn"))
            {
              digitalWrite(HEATER_ENABLE, HIGH);               // GET /HeaterEn turns the heater on
              heater_state = "running";
            }
            else if (currentLine.endsWith("HeaterDis"))
            {
              digitalWrite(HEATER_ENABLE, LOW);               // GET /HeaterDis turns the heater off
              heater_state = "idle";
            }
            else if (currentLine.indexOf("time") > 0)
            {
              String epochString = currentLine.substring(currentLine.indexOf("time") + 4, currentLine.length());
              int epoch = epochString.toInt();
              rtc.setEpoch(epoch);
            }
            else if (currentLine.indexOf("temp") > 0)
            {
              String tempString = currentLine.substring(currentLine.indexOf("temp") + 4, currentLine.indexOf(" "));
              set_temp = tempString.toInt();
            }
          }
        }
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


int print2digits(int number)
{
  if (number < 10) {
    tft.print("0"); // print a 0 before if the number is < than 10
  }
  tft.print(number);
  return number;
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

  if (val > max_val)
  {
    val = max_val;
  }
  if (val < 0)
  {
    val = 0;
  }

  //Rescale the data to fit the graph
  val = map(val, 0, max_val, 0, 100);
  //Draw the line
  tft.drawFastVLine(graph_cursor, tft.height() - val, 3, color);
}
