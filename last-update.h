#include <Arduino.h>

#ifndef last_update_client_h
#define last_update_client_h

class LastUpdateClient {
  public:
    void setHostUrl(String hostUrl);
    void setDeviceName(String deviceName);
    String getLastUpdate();

  private:
    String _hostUrl;
    String _deviceName;
};

#endif /* last_update_client_h */
