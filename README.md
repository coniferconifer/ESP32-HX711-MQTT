## Weight scale IoT by ESP32
![Fig.1 shematics by Fritzing](https://github.com/coniferconifer/ESP32-HX711-MQTT/blob/master/fritzing.png)
![Fig.2 TANITA weight scale reformed by ESP32-HX711-MTT](https://github.com/coniferconifer/ESP32-HX711-MQTT/blob/master/tanita.jpg)
### Legacy weight scale is reformed into WiFi connected scale by ESP32
- Load cells and push button of the scale are re-used.
- ESP32 sends weight to MQTT server at port 2883. 
- Node-RED on raspberry pi splits incoming MQTT at 2883 to Thingsboard at port 1883
  and "slack" node to inform to my mobile phone.
![Fig.3 Node-RED](https://github.com/coniferconifer/ESP32-HX711-MQTT/blob/master/node-RED.png)

## remaining issues
- sometimes WiFi reconnection fails after wake up from deep sleep
  
## License: Apache License v2

## References

- [AVIA Semiconductor HX711 data sheet]https://www.mouser.com/ds/2/813/hx711_english-1022875.pdf
- [https://github.com/bogde/HX711] https://github.com/bogde/HX711
- [#161 Measuring weight using an ESP32, a strain gauge, and a HX711]https://www.youtube.com/watch?v=iywsJB-T-mU
- [#194 IKEA Saltviken Scale Hack: Create a 150kg Scale Connected to Google Firebase (ESP32, ESP8266)]https://www.youtube.com/watch?v=14YmiEycup4

### PubSubClient
- [https://github.com/knolleary/pubsubclient](https://github.com/knolleary/pubsubclient)

### MQTT server with visiualization tools 
- [https://thingsboard.io/] (https://thingsboard.io/)

