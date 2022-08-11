#ifndef _EPGDB_CHANNELS_H_
#define _EPGDB_CHANNELS_H_

epgdb_channel_t *epgdb_channels_get_first ();
epgdb_channel_t *epgdb_channels_add (unsigned short int nid, unsigned short int tsid, unsigned short int sid, unsigned short int type);
void epgdb_channels_reset ();

#endif // _EPGDB_CHANNELS_H_
