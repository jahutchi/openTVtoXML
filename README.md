An application to convert OpenTV EPG (used by eg Sky TV) into XMLTV format.

This application reads the EPG directly from the DVB-S tuner, however it cannot at present tune the receiver to the currect mux. It is necessary to do the tuning (and hold the receiver open) with another application such as Tvheadend.

### TODO:
- Add code to tune to mux.

- Add more elements to XML output (eg series link).
