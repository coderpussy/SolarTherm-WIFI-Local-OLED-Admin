#include <ESP8266HTTPClient.h>  // To send HTTP requests.

#include "last-update.h"

void LastUpdateClient::setHostUrl(String hostUrl) {
  _hostUrl = hostUrl;
}

void LastUpdateClient::setDeviceName(String deviceName) {
  _deviceName = deviceName;
}

String LastUpdateClient::getLastUpdate() {
  
  String url = _hostUrl + "/last-update.php";
  Serial.println("LastUpdateClient: Sending POST request to LocalStorage at " + url);

  String requestBody = "{ \"device\": \"" + String(_deviceName) + "\" }";
  Serial.println("LastUpdateClient: HTTP request body: " + requestBody);

  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  
  int statusCode = http.POST(requestBody);

  Serial.printf("LastUpdateClient: Received HTTP status code: %d\r\n", statusCode);
  String responseBody = http.getString();
  Serial.println("LastUpdateClient: Received HTTP response body: " + responseBody);

  http.end();

  return responseBody;
}
