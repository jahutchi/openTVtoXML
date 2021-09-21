#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <malloc.h>
#include <limits.h>
#include <linux/dvb/dmx.h>

#include "common.h"

#include "core/log.h"
#include "dvb/dvb.h"

#include "opentv/opentv.h"
#include "opentv/huffman.h"
#include "providers/providers.h"

#include "epgdb/epgdb.h"
#include "epgdb/epgdb_channels.h"
#include "epgdb/epgdb_titles.h"

buffer_t buffer[65536];
unsigned short buffer_index;
unsigned int buffer_size;
unsigned int buffer_size_last;
bool huffman_debug_titles = false;
bool huffman_debug_summaries = false;

char *db_root = DEFAULT_DB_ROOT;
char demuxer[256];
char homedir[256];
FILE *outfile;

static volatile bool stop = false;
static volatile bool exec = false;
static volatile bool quit = false;
static volatile bool timeout_enable = true;
int timeout = 0;

bool iactive = false;

bool sdt_callback (int size, unsigned char* data)
{
	if ((data[0] == 0x42) || (data[0] == 0x46))
		opentv_read_channels_sdt (data, size);
	else return !stop;

	if (iactive) log_add ("1/6 - Reading.. %d services", opentv_channels_name_count ());
	return !stop;
}

bool bat_callback (int size, unsigned char* data)
{
	if (data[0] == 0x4a) opentv_read_channels_bat (data, size, db_root);
		if (iactive) log_add ("2/6 - Reading.. %d channels", opentv_channels_count ());
	return !stop;
}

static void format_size (char *string, int size)
{
	if (size > (1024*1024))
	{
		int sz = size / (1024*1024);
		int dc = (size % (1024*1024)) / (1024*10);
		if (dc > 0)
		{
			if (dc < 10)
				sprintf (string, "%d.0%d MB", sz, dc);
			else if (dc < 100)
				sprintf (string, "%d.%d MB", sz, dc);
			else
				sprintf (string, "%d.99 MB", sz);
		}
		else
			sprintf (string, "%d MB", sz);
	}
	else if (size > 1024)
		sprintf (string, "%d KB", (size / 1024));
	else
		sprintf (string, "%d bytes", size);
}

char *replace_str(char *str, char *orig, char *rep)
{
  static char buffer[4096];
  char *p;

  if(!(p = strstr(str, orig)))  // Is 'orig' even in 'str'?
    return str;

  strncpy(buffer, str, p-str); // Copy characters from 'str' start to 'orig' st$
  buffer[p-str] = '\0';

  sprintf(buffer+(p-str), "%s%s", rep, p+strlen(orig));

  return buffer;
}

bool opentv_titles_callback (int size, unsigned char* data)
{
	char fsize[256];
	if ((data[0] != 0xa0) && (data[0] != 0xa1) && (data[0] != 0xa2) && (data[0] != 0xa3)) return !stop;
	buffer[buffer_index].size = size;
	buffer[buffer_index].data = _malloc (size);
	memcpy(buffer[buffer_index].data, data, size);
	buffer_index++;
	buffer_size += size;
	if (buffer_size_last + 100000 < buffer_size)
	{
		format_size (fsize, buffer_size);
		if (iactive) log_add ("3/6 - Reading titles.. %s", fsize);
		buffer_size_last = buffer_size;
	}
	return !stop;
}

bool opentv_summaries_callback (int size, unsigned char* data)
{
	char fsize[256];
	buffer[buffer_index].size = size;
	buffer[buffer_index].data = _malloc (size);
	memcpy(buffer[buffer_index].data, data, size);
	buffer_index++;
	buffer_size += size;
	if (buffer_size_last + 100000 < buffer_size)
	{
		format_size (fsize, buffer_size);
		if (iactive) log_add ("5/6 - Reading summaries.. %s", fsize);
		buffer_size_last = buffer_size;
	}
	return !stop;
}

void download_opentv ()
{
	int i;
	dvb_t settings;
	char dictionary[PATH_MAX];
	char themes[PATH_MAX];

	log_add ("Started OpenTV decoder");
	log_add ("Started OpenTV events download, DVB poll %s\n", no_dvb_poll ? "disabled" : "enabled");

	snprintf (dictionary, PATH_MAX, "%s/providers/%s.dict", homedir, provider);
	snprintf (themes, PATH_MAX, "%s/providers/%s.themes", homedir, provider);

	opentv_init ();
	if (huffman_read_dictionary (dictionary) && opentv_read_themes (themes))
	{
		char size[256];

		settings.demuxer = demuxer;
		settings.buffer_size = 4 * 1024;
		settings.min_length = 11;
		settings.filter = 0x40;
		settings.mask = 0xf0;
		settings.pid = 0x11;
		settings.pids = providers_get_channels_pids();
		settings.pids_count = providers_get_channels_pids_count();

		log_add ("1/6 - Reading services...");
		dvb_read (&settings, *sdt_callback);

		char name_file[PATH_MAX];
		snprintf(name_file, PATH_MAX, "%s/%s.xml", db_root, provider);
		outfile = fopen(name_file,"w");
		fprintf(outfile,"<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n");
		fprintf(outfile, "<!DOCTYPE tv SYSTEM \"xmltv.dtd\">\n");
		fprintf(outfile,"<tv generator-info-name=\"OpenTVdecoder\"");
		fprintf(outfile," generator-info-url=\"https://github.com/dave-p/OpenTVdecoder\">\n");

		print_meminfo ();
		log_add ("1/6 - Read %d services", opentv_channels_name_count ());

		settings.filter = 0x4a;
		settings.mask = 0xff;

		log_add ("2/6 - Reading channels...");
		dvb_read (&settings, *bat_callback);

		print_meminfo ();
		log_add ("2/6 - Read %d channels", opentv_channels_count ());
		if (stop) goto opentv_stop;

		settings.min_length = 20;
		int pid;

		for (pid=0x30; pid<=0x37; pid++)
		{
			settings.min_length = 20;
			settings.filter = 0xa0;
			settings.mask = 0xfc;
			settings.pid = pid;
			settings.pids = providers_get_titles_pids ();
			settings.pids_count = providers_get_titles_pids_count ();

			buffer_index = 0;
			buffer_size = 0;
			buffer_size_last = 0;
			log_add ("3/6 - Reading titles...");
			dvb_read (&settings, *opentv_titles_callback);
			print_meminfo ();
			format_size (size, buffer_size);
			log_add ("3/6 - Read %s", size);
			if (stop) goto opentv_stop;

			log_add ("4/6 - Parsing titles...");
			buffer_size = 0;
			time_t lasttime = 0;
			for (i=0; i<buffer_index; i++)
			{
				if (!stop) opentv_read_titles (buffer[i].data, buffer[i].size, huffman_debug_titles);
				buffer_size += buffer[i].size;
				_free (buffer[i].data);
				if ((i % 100) == 0)
				{
					if (lasttime != time (NULL) || (i == buffer_index-1))
					{
						lasttime = time (NULL);
						format_size (size, buffer_size);
						if (iactive) log_add ("4/6 - Progress %d%% Parsing.. %s", (i*100)/buffer_index, size);
						print_meminfo ();
					}
				}
			}
			format_size (size, buffer_size);
			print_meminfo ();
			log_add ("4/6 - Titles parsed %s", size);

			if (stop) goto opentv_stop;

			settings.filter = 0xa8;
			settings.mask = 0xfc;
			settings.pid = pid+0x10;
			settings.pids = providers_get_summaries_pids();
			settings.pids_count = providers_get_summaries_pids_count();

			buffer_index = 0;
			buffer_size = 0;
			buffer_size_last = 0;
			log_add ("5/6 - Reading summaries...");
			dvb_read (&settings, *opentv_summaries_callback);
			print_meminfo ();
			format_size (size, buffer_size);
			print_meminfo ();
			log_add ("5/6 - Read %s", size);
			if (stop) goto opentv_stop;

			log_add ("6/6 - Parsing summaries...");
			buffer_size = 0;
			lasttime = 0;
			for (i=0; i<buffer_index; i++)
			{
				if (!stop) opentv_read_summaries (buffer[i].data, buffer[i].size, huffman_debug_summaries, db_root);
				buffer_size += buffer[i].size;
				_free (buffer[i].data);
				if ((i % 100) == 0)
				{
					if (lasttime != time (NULL) || (i == buffer_index-1))
					{
						lasttime = time (NULL);
						format_size (size, buffer_size);
						if (iactive) log_add ("6/6 - Progress %d%% Parsing.. %s", (i*100)/buffer_index, size);
						print_meminfo ();
					}
				}
			}
			format_size (size, buffer_size);
			print_meminfo ();
			log_add ("6/6 - Summaries parsed %s", size);
			opentv_print_episode_summary();

			if (!no_dvb_poll) break;
		}
opentv_stop:
		huffman_free_dictionary ();
		epgdb_clean ();
		opentv_cleanup();

		fprintf(outfile,"</tv>\n");
		fclose(outfile);
	}

	exec = false;
	log_add ("Ended OpenTV decoder");
}

void *download (void *args)
{
	char opentv_file[PATH_MAX];

	snprintf (opentv_file, PATH_MAX, "%s/providers/%s.conf", homedir, provider);

	if (providers_read (opentv_file))
	{
		download_opentv ();
	}
	else
	{
		log_add ("cannot read provider");
		exec = false;
	}

	return NULL;
}

int main (int argc, char **argv)
{
	int c, i;
	opterr = 0;

	strcpy (homedir, argv[0]);
	for (i = strlen (homedir)-1; i >= 0; i--)
	{
		bool ended = false;
		if (homedir[i] == '/') ended = true;
		homedir[i] = '\0';
		if (ended) break;
	}

	strcpy (demuxer, DEFAULT_DEMUXER);
	strcpy (provider, DEFAULT_OTV_PROVIDER);

	while ((c = getopt (argc, argv, "h:d:x:l:p:k:cfnrsyz")) != -1)
	{
		switch (c)
		{
			case 'd':
				db_root = optarg;
				break;
			case 'x':
				strcpy (demuxer, optarg);
				break;
			case 'l':
				strcpy (homedir, optarg);
				break;
			case 'p':
				strcpy (provider, optarg);
				break;
			case 'k':
				if (nice (atoi(optarg)))
					printf ("nice %s\n", optarg);
				break;
			case 'c':
				carousel_dvb_poll = true;
				break;
			case 'f':
				free_only = true;
				break;
			case 'n':
				no_dvb_poll = true;
				break;
			case 'r':
				iactive = true;
				break;
			case 's':
				show_lcns = true;
				break;
			case 'y':
				huffman_debug_summaries = true;
				break;
			case 'z':
				huffman_debug_titles = true;
				break;
			case '?':
				printf ("Usage:\n");
				printf ("  ./openTVtoXML [options]\n");
				printf ("Options:\n");
				printf ("  -d db_root	output folder\n");
				printf ("		default: %s\n", db_root);
				printf ("  -x demuxer	dvb demuxer\n");
				printf ("		default: %s\n", demuxer);
				printf ("  -l homedir	home directory\n");
				printf ("		default: %s\n", homedir);
				printf ("  -p provider	opentv provider\n");
				printf ("		default: %s\n", provider);
				printf ("  -k nice	see \"man nice\"\n");
				printf ("  -c		carousel dvb polling\n");
				printf ("  -f		Un-encrypted channels only\n");
				printf ("  -n		no dvb polling\n");
				printf ("  -r		show progress\n");
				printf ("  -s		show Sky channel numbers\n");
				printf ("  -y		debug mode for huffman dictionary (summaries)\n");
				printf ("  -z		debug mode for huffman dictionary (titles)\n");
				printf ("  -h		show this help\n");
				return 0;
		}
	}
	
	while (homedir[strlen (homedir) - 1] == '/') homedir[strlen (homedir) - 1] = '\0';
	while (db_root[strlen (db_root) - 1] == '/') db_root[strlen (db_root) - 1] = '\0';
	
	mkdir (db_root, S_IRWXU|S_IRWXG|S_IRWXO);

	log_new (db_root);
	log_open (db_root);
	log_banner ("OpenTVdecoder");

	char opentv_file[PATH_MAX];

	snprintf (opentv_file, PATH_MAX, "%s/providers/%s.conf", homedir, provider);
	if (providers_read (opentv_file))
	{
		log_add ("Provider %s opentv configured", provider);
		download_opentv ();
		epgdb_clean ();
		print_meminfo ();
	}
	else
		log_add ("Cannot load provider configuration (%s)", opentv_file);
	
	memory_stats ();
	log_close ();
	return 0;
}
