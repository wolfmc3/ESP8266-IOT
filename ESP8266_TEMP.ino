
#include "ESP8266.h"
#include <SoftwareSerial.h>
#include <avr/io.h>
#include <avr/wdt.h>
#include <EEPROM.h>
#define CTRL1 10
#define CTRL2 1
#define CTRL3 2

String SSID       = "";
String PASSWORD   = "";

String feedbackUrl = "";
int feedbackPin = 0;

SoftwareSerial ESP(9,10); // RX,TX collegati a ESP8266
ESP8266 wifi(ESP);

void setup(void)
{
    pinMode(13,OUTPUT);
    pinMode(6,INPUT);
    ESP.begin(9600);
    Serial.begin(9600);
    Serial.println();
    Serial.println();
    Serial.print(F("Watchdog reset:"));
    Serial.println(WDTO_8S);
    readSettings();
    if (feedbackPin) {
      pinMode(feedbackPin,INPUT);
    }
    Serial.print(F("ESP8266 Version:"));
    Serial.println(wifi.getVersion().c_str());
    if (digitalRead(6) == HIGH) { //soft ap enabled
      if (SSID != "") {
        logOK(F("Station + softap: "),wifi.setOprToStationSoftAP());
        wifi.setAutoConnect(1);
      } else {
        logOK(F("Softap:"),wifi.setOprToSoftAP());
      }
    } else {
      logOK(F("station:"),wifi.setOprToStation());
    }
    logOK(F("Autoconnect: "),wifi.setAutoConnect(1));

    /*Serial.println("AP List");
    Serial.println(wifi.getAPList());*/
    if (SSID != "") {
      if (logOK(F("Join AP:"),wifi.joinAP(SSID, PASSWORD))) {
          digitalWrite(13, HIGH);
      } 
    }
    
    Serial.println(F("IP: "));
    Serial.println(wifi.getLocalIP().c_str());    
    
    logOK(F("MUX:"),wifi.enableMUX());

        
    logOK(F("HTTP Server:"),wifi.startTCPServer(80));
    logOK(F("Set timeout:"),wifi.setTCPServerTimeout(5));
    
    Serial.print(F("setup end\r\n"));
    mem("Setup");
    wdt_enable(WDTO_8S); 
}

bool logOK(const __FlashStringHelper* title, bool res) {
  Serial.print(title);
  if (res) {
    Serial.println(F("\tOK"));
  } else {
    Serial.println(F("\tNG"));
  }
  return res;
}

int readSettings() {
  byte ctrl1 = EEPROM.read(0);
  byte ctrl2 = EEPROM.read(1);
  byte ctrl3 = EEPROM.read(2);
  if (ctrl1 == CTRL1 && ctrl2==CTRL2 && ctrl3==CTRL3) {
    Serial.println(F("EEPROM: OK"));
    byte s = 0;
    SSID = PASSWORD = "";
    for (int i = 3;i < 512;i++) {
      byte c = EEPROM.read(i);
      if (c == 0) { //EOL
        s++;
        Serial.print(F("|"));
        if (s > 3) break;
        continue;
      }
      Serial.print(s);
      switch (s){
        case 0:
          SSID += (char) c;
          break;
        case 1:
          PASSWORD += (char) c;
          break;
        case 2: 
          feedbackPin = c;
          s++;
          break;
        case 3: 
          feedbackUrl += (char) c;
          break;
      }
    }
    Serial.println(F("OK"));
  } else {
    clearSettings();
  }
}

void clearSettings() {
  Serial.println(F("Reset EEPROM"));
  delay(10000);
  for (int i = 0; i < 512; i++) {
    Serial.print(F("*"));
    EEPROM.write(i, 0);
  }
  Serial.println();
  EEPROM.write(0, CTRL1);
  EEPROM.write(1, CTRL2);
  EEPROM.write(2, CTRL3);
  SSID = "";
  PASSWORD = "";
  Serial.print(F(" OK"));
}

void saveSettings() {
  Serial.println(F("Write EEPROM"));
  int m = 3;
  stringToEEPROM(m,SSID);
  stringToEEPROM(m,PASSWORD);
  EEPROM.write(m++, (byte) feedbackPin);
  stringToEEPROM(m,feedbackUrl);
  Serial.println();
  Serial.print(F("Write OK ("));
  Serial.print(m-1);
  Serial.print(F(" bytes)"));

}

void stringToEEPROM(int &m, String &text) {
  for (int i = 0; i < text.length(); i++) {
    Serial.print(F("$"));
    EEPROM.write(m++, text[i]);
  }
  EEPROM.write(m++, 0);
  
}
 
uint8_t buffer[320] = {0};
uint32_t len = 0;
uint8_t mux_id;
byte reset = 0;
unsigned long time = 0;

void loop(void)
{
    len = wifi.recv(&mux_id, buffer, sizeof(buffer), 250);
    if (len > 0) {
        
        Serial.print(F("***\nReceived from :"));
        Serial.println(mux_id);
        
        String url;
        byte iData = 0;
        for(uint32_t i = 0; i < len; i++) {
            //Serial.print((char)buffer[i]);
            if (iData > 1) break;
            if (buffer[i] == 32) {
              iData++;
            } else {
              if (iData == 1) url += (char)buffer[i];
            }

        }
        
        Serial.print(F("REQUEST "));
        Serial.println(url);
        
        uint32_t respLen = request(url, mux_id);
        Serial.print(F("Response:"));
        Serial.println(respLen);

        len = respLen;
        Serial.print(F("Send (byte):"));
        Serial.println(len);
        wdt_reset();
        logOK(F("Send response:"),wifi.send(mux_id, buffer, len));

        
        Serial.print(F("["));
        Serial.print(mux_id);
        Serial.print(F("] "));
        logOK(F("Close MUX:"), wifi.releaseTCP(mux_id));
        
        mem(F("End loop"));
    }
    if (!reset) {
      wdt_reset();
    } else {
      Serial.println(F("REBOOT SYSTEM"));
      while (1) {};
    }
    if (millis() - time > 30000) {
      time = millis();
      String res = wifi.getNowConecAp();

      if (logOK(F("WIFI"), res.indexOf("OK") != -1)) {
        if (digitalRead(13) == LOW) {
          digitalWrite(13,HIGH);
          Serial.println(F("IP: "));
          Serial.println(wifi.getLocalIP().c_str());    
        }
      } else {
        digitalWrite(13,LOW);
      }
    }
    if (feedbackPin) {
      if (digitalRead(feedbackPin) == HIGH) {
        sendFeedBack(0);
      }
    }
}//loop()

void mem(String point) {
  extern int __heap_start, *__brkval; 
  int v; 
  int r = (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
  Serial.print(point);
  
  Serial.print(F(" mem: "));    
  Serial.print(r);
  Serial.println(F(" bytes"));    
}

const int urlCount = 14;
const String url[] = { 
 "/",
 "/temp",
 "/temp/json",
 "/conn",
 "/connset",
 "/reboot",
 "/reset",
 "/input",
 "/output",
 "/get",
 "/set",
 "/analog",
 "/feedback",
 "/feedback_set",
};

void sendFeedBack(int val) {
  wdt_reset();
  String HOST_NAME = "";
  String PROTOCOL = "";
  String PATH = "";
  uint8_t mux_send = mux_id+1;
  int PORT;
  int pos = feedbackUrl.indexOf("//");
  PROTOCOL = feedbackUrl.substring(0,pos-1);
  HOST_NAME = feedbackUrl.substring(pos+2);
  if (PROTOCOL == "HTTP" || PROTOCOL == "http") {
    PORT = 80;
  }
  pos = HOST_NAME.indexOf("/");
  PATH = HOST_NAME.substring(pos);
  HOST_NAME = HOST_NAME.substring(0,pos);
  pos = HOST_NAME.indexOf(":");
  if (pos != -1) {
    PORT = HOST_NAME.substring(pos+1).toInt();
    HOST_NAME = HOST_NAME.substring(0, pos);
  }
  if (PATH == "") {
    PATH = "/";
  }
  Serial.println(PROTOCOL);
  Serial.println(PORT);
  Serial.println(HOST_NAME);
  Serial.println(PATH);
  if (!PORT) return;
  if (logOK(F("Open TCP:"),wifi.createTCP(mux_send, HOST_NAME, PORT))) {
      Serial.println(mux_send);
      wdt_reset();
      String request = "GET " + PATH +" HTTP/1.1\r\nConnection: close\r\n\r\n";
      uint32_t lenResp = 0;
      
      for (int i = 0; i < request.length(); i++) {
        if (lenResp > sizeof(buffer)) break;
        buffer[lenResp++] = (uint8_t) request[i];
        //Serial.print((byte)header[i]);
        //Serial.print(" ");
      }
      logOK(F("Send:"),wifi.send(mux_send,(const uint8_t*)buffer, lenResp));
      wdt_reset();
      delay(1000);
      uint32_t len = wifi.recv(mux_send, buffer, sizeof(buffer), 1000);
      if (len > 0) {
        Serial.print(F("Received:["));
        for(uint32_t i = 0; i < len; i++) {
            Serial.print((char)buffer[i]);
        }
        Serial.println(F("]"));
      }
      wdt_reset();
  
      logOK(F("Close:"),wifi.releaseTCP(mux_send));

  } 


}

uint32_t request(String &uri, uint8_t &mux_id) {
        String content = "";
        //String header = F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n");
        String header = F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n");
        
        int index = -1;
        String path = uri;
        String par = "";
        int qsPos = path.indexOf('?');
        if (qsPos > -1) {
          par = path.substring(qsPos+1);
          path = path.substring(0,qsPos);
        }
        for (int i = 0;i < urlCount;i++) {
          if (url[i] == path) {
            index = i;
            break;
          }
        }
        int idx; 
        String par1;
        String par2;

        switch (index) {
          case 0: /* GET:/ */
            //String header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
            content = F("<a href=\"/conn\">Set SSID</a><br><a href=\"/reboot\">Reboot</a><br><a href=\"/temp\" >Temp</a><br><a href=\"/temp/json\" >Temp (Json)</a><br><br><a href=\"/reset\">Reset</a><br>\r\n");
            break;
          case 1: /* GET:/temp */
            //String header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
            content = "<H1>Temp</H1><br>Temperatura:"+ readTemp() +"<br><a href=\"/\" >Return to Home</a>\r\n";
            break;
          case 2: /* GET:/temp/json */
            header = F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n");
            content = "{\"temp\": ";
            content += readTemp();
            content += "}\n";       
            break;
          case 3: /* GET:/conn */
            //String header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
            content = F("<H1>Wifi setting</H1><br><form action=\"/connset\">SSID:<input type=text name=s /><br>Password:<input type=text name=p /><br><br><input type=submit value=Connect /></form>\r\n");
            break;
          case 4: /* GET:/set */
            idx = par.indexOf('&'); 
            par1 = urlDecode(par.substring(2,idx));
            par2 = urlDecode(par.substring(idx+3));
            //Serial.println("New SSID:" + par1);
            //Serial.println("New Password:" + par2);
            SSID = par1;
            PASSWORD = par2;
            saveSettings();
            header = F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nrefresh:20;url=/\r\n");
            content = F("<H1>Restart...</H1>\r\n");
            reset = 1;
            break;
          case 5: /* GET:/reboot */
            header = F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nrefresh:20;url=/\r\n");
            content = F("<H1>Restart...</H1>\r\n");
            if (millis() > 15000) reset = 1;
            break;
          case 6: /* GET:/reset */
            clearSettings();
            header = F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nrefresh:25;url=/\r\n");
            content = F("<H1>Restart...</H1>\r\n");
            wifi.joinAP(SSID, PASSWORD);
            delay(1000);
            reset = 1;
            break;
          case 7: /* GET:/input?[pin] */
            pinMode(par.toInt(),INPUT);
            //String header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
            content = F("OK\r\n");
            break;
          case 8: /* GET:/output?[pin] */
            pinMode(par.toInt(),OUTPUT);
            //String header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
            content = F("OK\r\n");
            break;
          case 9: /* GET:/get?[pin] */
            header = F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n");
            content = "{\"pin-"+par+"\": ";
            content += digitalRead(par.toInt());
            content += "}\n";       
            break;
          case 10: /* GET:/set?[pin]&[value] */
            //String header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
            idx = par.indexOf('&'); 
            par1 = urlDecode(par.substring(0,idx));
            par2 = urlDecode(par.substring(idx+1));
            analogWrite( par1.toInt(),par2.toInt());
            content = F("OK\r\n");
            break;
          case 11: /* GET:/analog?[pin] */
            header = F("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n");
            content = "{\"A"+par+"\": ";
            content += analogRead(par.toInt());
            content += "}\n";       
            break;
          case 12: /* GET:/feedback */
            //String header = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n";
            content = F("<H1>Feedback</H1><br><form action=\"/feedback_set\">PIN:<input type=text name=p /><br>URL:<input type=text name=u /><br><br><input type=submit value=Salva /></form>\r\n");
            break;
          case 13: /* GET:/feedback_set */
            idx = par.indexOf('&'); 
            par1 = urlDecode(par.substring(2,idx));
            par2 = urlDecode(par.substring(idx+3));
            //Serial.println("New SSID:" + par1);
            //Serial.println("New Password:" + par2);
            feedbackPin = par1.toInt();
            feedbackUrl = par2;
            saveSettings();
            header = F("HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nrefresh:2;url=/\r\n");
            content = F("<H1>Save...</H1>\r\n");
            break;
          default:
            header = F("HTTP/1.1 404 Not Found\r\n");
            content = F("<H1>HTTP 404 - Page not found</H1><br><a href=\"/\" >Return to Home</a>\r\n");
        }

        /*header += "Content-Length:";
        header += content.length();
        header += "\r\n\r\n";*/
        header += "\r\n\r\n";
        uint32_t lenResp = 0;
        //mem("Response end");

        for (int i = 0; i < header.length(); i++) {
          if (lenResp > sizeof(buffer)) break;
          buffer[lenResp++] = (uint8_t) header[i];
          //Serial.print((byte)header[i]);
          //Serial.print(" ");
        }
        //Serial.println();
        for (int i = 0; i < content.length(); i++) {
          if (lenResp > sizeof(buffer)) break;
          buffer[lenResp++] = (uint8_t) content[i];
          //Serial.print((byte)content[i]);
          //Serial.print(" ");
        }
        buffer[lenResp] = 0;
        header = "";
        content = "";
        //for (int i = 0; i < len-1; i++) Serial.println((char) buffer[i]);
        return lenResp;
}

String readTemp() {
  int reading = analogRead(0);  
  for (int i = 0;i < 3;i++) reading = (reading + analogRead(0))/2;
  float voltage = reading * 5.0 / 1024.0;
  float temperatureC = (voltage - 0.5) * 100 ; 
  //Serial.print(temperatureC); Serial.println(F(" degrees C"));
  return (String) temperatureC;
}
 
String urlDecode(String text) {
  String res = "";
  for (int i = 0;i < text.length();i++) {
    if (text[i] == '%') {
      char c = (HexToInt(text[++i])*16) + HexToInt(text[++i]);
      res += c;
    } else {
      res += text[i];
    }
  }
  return res;
}  

int HexToInt(char c) {
   if (c >= '0' && c <= '9') {
       return c - '0';
   } else if (c >= 'a' && c <= 'f') {
       return c - 'a' + 10;
   } else if (c >= 'A' && c <= 'F') {
       return c - 'A' + 10;
   } else {
       return -1;   // getting here is bad: it means the character was invalid
   }

}




