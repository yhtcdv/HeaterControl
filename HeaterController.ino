


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

#define TEMP_INLET_PIN A1
#define TEMP_OUTLET_PIN A2
#define TEMP_AMBIENT A3


// Declare function prototypes
void printWifiStatus();
void alarmMatch();
int print2digits(int number);
float calc_temp(int adc_val);
void create_graph_yaxis(int x_pos, int y_pos, int height, int max_val, uint16_t color);
void update_graph(int x_pos, int y_pos, int val, uint16_t color);
void draw_web_graph(int data[], int num_points, int height, int x_step, String line_colour, int val_pos, WiFiClient *client);


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
float temp_ambient = 0;
unsigned long pump_strokes = 0;
unsigned long last_pump_strokes = 0;
unsigned strokes_per_min = 0;
const float pump_volume = 0.000025;

void interruptFunction() {
  pump_strokes++;
}

//////////////////// Control Stuff /////////////////
int set_temp = 16;

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
String heater_state = "idle";
String heater_power = "Off";

int graph_cursor = graph_x_pos + 25;

byte temp_history_pointer = 0;

int ambient_temp_history[75];
int outlet_temp_history[75];


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

int time_stamp_minutes = 0;
int time_stamp_hours = 0;



void setup() {
  //Initialise the memory
  memset (outlet_temp_history, 0, sizeof(outlet_temp_history));
  memset (ambient_temp_history, 0, sizeof(ambient_temp_history));

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
  tft.println("Ambient temp = ");
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
  tft.println("Ambient Temp");
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
    //Update the DAC
    analogWrite(TEMP_SET_ANALOGUE, map(set_temp, MIN_SETPOINT_TEMP, MAX_SETPOINT_TEMP, 160, 190));
    //Update the temperature sensor inputs
    temp_inlet = calc_temp(analogRead(TEMP_INLET_PIN));
    temp_outlet = calc_temp(analogRead(TEMP_OUTLET_PIN));
    temp_ambient = calc_temp(analogRead(TEMP_AMBIENT));

    //Draw the graph
    update_graph(graph_x_pos, graph_y_pos, temp_outlet, 140, ILI9340_RED);
    update_graph(graph_x_pos, graph_y_pos, temp_inlet, 50, ILI9340_BLUE);
    update_graph(graph_x_pos, graph_y_pos, temp_ambient, 50, ILI9340_YELLOW);

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
    tft.print(temp_ambient);
    tft.print("("); tft.print(set_temp); tft.print(")");
    tft.setCursor(temp_value_pos_x, temp_value_pos_y + 16);
    tft.println(temp_inlet);
    tft.setCursor(temp_value_pos_x, temp_value_pos_y + 32);
    tft.println(temp_outlet);
    tft.setCursor(temp_value_pos_x, temp_value_pos_y + 48);
    tft.println(pump_strokes * pump_volume, 4);
   
    tft.setCursor(temp_value_pos_x, temp_value_pos_y + 64);
    tft.println(heater_state);

    // Every 10s check the pump strokes and update the temp history
    if (last_seconds == 0 || last_seconds == 10 || last_seconds == 20 || last_seconds == 30 || last_seconds == 40 || last_seconds == 50)
    {
      //update the temp history
      outlet_temp_history[temp_history_pointer] = temp_outlet;
      ambient_temp_history[temp_history_pointer] = temp_ambient;

      //Move the poiunter
      if (temp_history_pointer == sizeof(outlet_temp_history) - 1)
      {
        temp_history_pointer = 0;
        time_stamp_minutes = last_minutes;
        time_stamp_hours = last_hours;
      }
      else
      {
        temp_history_pointer++;
      }

    }

    //Every minute update the pump rate
    if (last_minutes != rtc.getMinutes())
    {
      strokes_per_min = pump_strokes - last_pump_strokes;
      last_pump_strokes = pump_strokes;
      if (strokes_per_min < 10)
      {
        heater_power = "Off";
      }
      else if (strokes_per_min >= 10 && strokes_per_min < 100)
      {
        heater_power = "Low";
      }
      else if (strokes_per_min >= 100 && strokes_per_min < 160)
      {
        heater_power = "Med";
      }
      else
      {
        heater_power = "High";
      }
    }

    /////////////////// Print the time and date //////////////
    tft.setTextSize(1);
    tft.fillRect(time_pos_x, time_pos_y, 160, 8, ILI9340_BLACK);
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
            clinet_resp = false;
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            client.println("<HTML>");

            client.println("<HEAD>");
            client.println("<TITLE>Heater Control</TITLE>");
            client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">");
            client.println("<meta http-equiv=\"refresh\" content=\"5\"; URL=baseurl>");
            client.println("</HEAD>");

            client.println("<BODY>");

            client.println("<H1>Boys Room Heater Control</H1>");

            client.print("<button onClick=\"window.location=baseurl + '?HeaterEn'\" style='FONT-SIZE: 12px; HEIGHT: 25px; FONT-FAMILY: Arial; WIDTH: 100px;'>");
            client.print("Heater Enable");
            client.println("</button>");

            client.print("<button onClick=\"window.location=baseurl + '?HeaterDis'\" style='FONT-SIZE: 12px; HEIGHT: 25px; FONT-FAMILY: Arial; WIDTH: 100px;'>");
            client.print("Heater Disable");
            client.println("</button>");

            client.println("<br>");
            client.println("<br>");
            client.print("<button onClick=\"set_time()\"; style='FONT-SIZE: 12px; HEIGHT: 25px; FONT-FAMILY: Arial; WIDTH: 100px;'>");
            client.print("Set Time");
            client.println("</button>");
            client.println("<br>");

            client.println("<br>");
            client.print("<button onClick=\"set_temp()\"; style='FONT-SIZE: 12px; HEIGHT: 25px; FONT-FAMILY: Arial; WIDTH: 100px;'>");
            client.print("Set Temp");
            client.println("</button>");

            client.println("<input type=\"number\" id=\"settemp\" value=\"16\" maxlength=\"2\" max=\"25\" min=\"5\"/>");
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
            client.println("<br>");
            client.print("Outlet temp  = ");
            client.print(temp_outlet);
            client.println("<br>");
            client.print("Ambient temp  = ");
            client.print(temp_ambient);
            client.print(" (");
            client.print(set_temp);
            client.print(")");
            client.println("<br>");
            client.print("Fuel Used(l)    = ");
            client.print(pump_strokes * pump_volume, 4);
            client.println("<br>");
            client.print("Pump Strokes Per Min = ");
            client.print(strokes_per_min);
            client.println("<br>");
            client.print("Heater State = ");
            client.print(heater_state);
            client.println("<br>");
            client.print("Heater Power = ");
            client.print(heater_power);
            client.println("<br>");

            //Create a canvas to draw the temperature history onto
            client.println(" <canvas id=\"myCanvas\" width=\"300\" height=\"150\" style=\"border:1px solid #d3d3d3;\">");
            client.println("Your browser does not support the HTML5 canvas tag.</canvas>");

            client.println("<script type = \"text/javascript\">");

            //Create a base URL to strip all added parameters by the client
            client.println("var baseurl = window.location.href;");
            client.println("baseurl = baseurl.substring(0 , baseurl.indexOf('?'));");
            
            client.println("setTimeout(function(){location.assign(baseurl)}, 5000);");

            //The best way I have found of adding the temp parameter is in the script (I may move all parameters to script based)
            //Set temperature function
            client.println("function set_temp() {window.location=baseurl + '?temp' + document.getElementById(\"settemp\").value;}");
            //Set time function
            client.println("function set_time() {window.location=baseurl + '?time' + (new Date().valueOf())}");

            //Create a canvas to draw the temperature history onto
            client.println("var c = document.getElementById(\"myCanvas\");");
            client.println("var ctx = c.getContext(\"2d\");");
            client.println("ctx.lineWidth = \"3\";");

            //Put a time stamp on the oragin of the graph
            client.println("ctx.font = \"8px Arial\";");
            client.print("ctx.fillText(\"");
            client.print(time_stamp_hours);
            client.print(":");
            client.print(time_stamp_minutes);
            client.print("\"");
            client.println(", 0, 150);");

            draw_web_graph(ambient_temp_history, temp_history_pointer, 150, 4, "blue", 12, &client);
            draw_web_graph(outlet_temp_history, temp_history_pointer, 150, 4, "red", -12, &client);

            client.println("</script >");

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
            clinet_resp = true;
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
              int time_start_index = currentLine.indexOf("time") + 4;
              String epochString = currentLine.substring(time_start_index, currentLine.length() - 3); //Ignore the mili seconds!
              unsigned long epoch = strtoul(epochString.c_str(), NULL, 10);
              rtc.setEpoch(epoch);
            }
            else if (currentLine.indexOf("temp") > 0)
            {
              String tempString = currentLine.substring(currentLine.indexOf("temp") + 4, currentLine.length());
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

void draw_web_graph(int data[], int num_points, int height, int x_step, String line_colour, int val_pos, WiFiClient *client)
{
  //Define the line colour
  client->print("ctx.strokeStyle = \"");
  client->print(line_colour);
  client->println("\";");

  //Define the starting position (X=0 Y = array[0])
  client->println("ctx.beginPath();");
  client->print("ctx.moveTo(0,");
  client->print(data[0]);
  client->println(");");

  //Loop around to create the line
  for (int graph_x_pos = 1; graph_x_pos < num_points; graph_x_pos ++)
  {
    client->print("ctx.lineTo(");
    client->print(graph_x_pos * x_step);
    client->print(", ");
    client->print(height - data[graph_x_pos]);
    client->println(");");
  }
  client->println("ctx.stroke();");  // Draw it

  //Draw the terminal line

  client->println("ctx.strokeStyle = \"black\";");  //Black end marke
  client->println("ctx.beginPath();");

  client->print("ctx.moveTo(");
  client->print((num_points - 1) * 4);
  client->print(", ");
  client->print(5 + height - data[(num_points - 1)]);
  client->println(");");

  client->print("ctx.lineTo(");
  client->print((num_points - 1) * 4);
  client->print(", ");
  client->print(height - 5 - data[(num_points - 1)]);
  client->println(");");

  client->println("ctx.stroke();");  // Draw it

  //Put a time stamp on the last point
  client->print("ctx.font = \"8px Arial\";");
  client->print("ctx.fillText(\"");
  client->print(data[(num_points - 1)]);
  client->print("\"");
  client->print(", ");
  client->print((num_points - 1) * 4);
  client->print(", ");
  client->print(height + val_pos - data[(num_points - 1)]);
  client->println(");");
}
