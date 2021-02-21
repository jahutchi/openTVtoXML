An application to convert OpenTV EPG (used by eg Sky TV) into XMLTV format.

This application reads the EPG directly from the DVB-S tuner, however it cannot at present tune the receiver to the currect mux. It is necessary to do the tuning (and hold the receiver open) with another application such as Tvheadend.

### USAGE
```
openTVtoXML [options]
Options:
  -d db_root    output folder (default: /tmp/xmltv)
  -x demuxer    dvb demuxer (default: /dev/dvb/adapter0/demux0)
  -l homedir    home directory (default: .)
  -p provider   opentv provider (default: skyuk_28.2)
  -k nice       see "man nice"
  -c            carousel dvb polling
  -f            Un-encrypted channels only
  -n            no dvb polling
  -r            show progress
  -s            show Sky channel numbers
  -y            debug mode for huffman dictionary (summaries)
  -z            debug mode for huffman dictionary (titles)
  -h            show this help
```
Once the output file has been created it is necessary to feed the data into the Tvheadend EPG grabber, using a command such as:
```
cat /tmp/xmltv/skyuk_28.2.xml | nc -w5 -U /home/hts/.hts/tvheadend/epggrab/xmltv.sock
```
(note that not all versions of netcat (nc) have the needed '-U' option.)

### TODO:
- Add code to tune to mux.

- Add more elements to XML output (eg series link).
