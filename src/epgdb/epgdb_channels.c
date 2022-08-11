#include <stdio.h>
#include <strings.h>
#include <memory.h>
#include <malloc.h>
#include <stdlib.h>

#include "../common.h"

#include "epgdb.h"
#include "epgdb_channels.h"

static epgdb_channel_t *channel_first = NULL;
static epgdb_channel_t *channel_last = NULL;

void epgdb_channels_reset ()
{
	channel_first = NULL;
	channel_last = NULL;
}

epgdb_channel_t *epgdb_channels_get_first () { return channel_first; }

epgdb_channel_t *epgdb_channels_add (unsigned short int nid, unsigned short int tsid, unsigned short int sid, unsigned short int type)
{
	epgdb_channel_t *tmp = channel_first;
	
	while (tmp != NULL)
	{
		if ((nid == tmp->nid) && (tsid == tmp->tsid) && (sid == tmp->sid) && (type == tmp->type)) return tmp;
		tmp = tmp->next;
	}
	
	tmp = _malloc (sizeof (epgdb_channel_t));
	tmp->sid = sid;
	tmp->nid = nid;
	tmp->tsid = tsid;
	tmp->type = type;
	tmp->title_first = NULL;
	tmp->title_last = NULL;
	
	if (channel_last == NULL)
	{
		tmp->prev = NULL;
		tmp->next = NULL;
		channel_first = tmp;
		channel_last = tmp;
	}
	else
	{
		channel_last->next = tmp;
		tmp->prev = channel_last;
		tmp->next = NULL;
		channel_last = tmp;
	}
	
	return tmp;
}
