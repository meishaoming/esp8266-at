#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/time.h>
#include <unistd.h>
#include "esp8266.h"
#include "socket_esp8266.h"

#include "MQTTClient.h"

#include "../common/config.h"

extern int serial_open_nonblock(const char *devpath);

static const char *portname = "/dev/cu.SLAB_USBtoUART";

static int serial_fd = -1;

static int linux_putc(unsigned char ch, int timeout)
{
    fd_set wfds;
    FD_ZERO(&wfds);
    FD_SET(serial_fd, &wfds);

    struct timeval tv = {
        .tv_sec = timeout / 1000,
        .tv_usec = (timeout % 1000) * 1000,
    };

    int count = select(serial_fd + 1, NULL, &wfds, NULL, &tv);
    if (count > 0) {
        return write(serial_fd, &ch, 1) == 1 ? 0 : -1;
    } else {
        printf("putc, select failed\n");
        return -1;
    }
}

static int linux_getc(int timeout)
{
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(serial_fd, &rfds);

    struct timeval tv = {
        .tv_sec = timeout / 1000,
        .tv_usec = (timeout % 1000) * 1000,
    };
    int count = select(serial_fd + 1, &rfds, NULL, NULL, &tv);
    if (count > 0)
    {
        unsigned char ch;
        return read(serial_fd, &ch, 1) == 1 ? ch : -1;
    }
    else
    {
        return -1;
    }
}


static atcmd_ops ops = {
    .getc_timeout = linux_getc,
    .putc_timeout = linux_putc,
};

static void init()
{
    serial_fd = serial_open_nonblock(portname);
    if (serial_fd < 0) {
        printf("open %s failed\n", portname);
        exit(-1);
    }

    esp8266_init(&ops);

    int rc = socket_init(&socket_esp8266, SSID, PASS);
    if (rc) {
        printf("socket init failed %d\n", rc);
        exit(-1);
    }
}

volatile int toStop = 0;

void usage()
{
	printf("MQTT stdout subscriber\n");
	printf("Usage: stdoutsub topicname <options>, where options are:\n");
	printf("  --host <hostname> (default is localhost)\n");
	printf("  --port <port> (default is 1883)\n");
	printf("  --qos <qos> (default is 2)\n");
	printf("  --delimiter <delim> (default is \\n)\n");
	printf("  --clientid <clientid> (default is hostname+timestamp)\n");
	printf("  --username none\n");
	printf("  --password none\n");
	printf("  --showtopics <on or off> (default is on if the topic has a wildcard, else off)\n");
	exit(-1);
}


void cfinish(int sig)
{
	signal(SIGINT, NULL);
	toStop = 1;
}

struct opts_struct
{
	char* clientid;
	int nodelimiter;
	char* delimiter;
	enum QoS qos;
	char* username;
	char* password;
	char* host;
	int port;
	int showtopics;
} opts =
{
	(char*)"stdout-subscriber", 0, (char*)"\n", QOS2, NULL, NULL, (char*)"localhost", 1883, 0
};


void getopts(int argc, char** argv)
{
	int count = 2;
	
	while (count < argc)
	{
		if (strcmp(argv[count], "--qos") == 0)
		{
			if (++count < argc)
			{
				if (strcmp(argv[count], "0") == 0)
					opts.qos = QOS0;
				else if (strcmp(argv[count], "1") == 0)
					opts.qos = QOS1;
				else if (strcmp(argv[count], "2") == 0)
					opts.qos = QOS2;
				else
					usage();
			}
			else
				usage();
		}
		else if (strcmp(argv[count], "--host") == 0)
		{
			if (++count < argc)
				opts.host = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--port") == 0)
		{
			if (++count < argc)
				opts.port = atoi(argv[count]);
			else
				usage();
		}
		else if (strcmp(argv[count], "--clientid") == 0)
		{
			if (++count < argc)
				opts.clientid = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--username") == 0)
		{
			if (++count < argc)
				opts.username = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--password") == 0)
		{
			if (++count < argc)
				opts.password = argv[count];
			else
				usage();
		}
		else if (strcmp(argv[count], "--delimiter") == 0)
		{
			if (++count < argc)
				opts.delimiter = argv[count];
			else
				opts.nodelimiter = 1;
		}
		else if (strcmp(argv[count], "--showtopics") == 0)
		{
			if (++count < argc)
			{
				if (strcmp(argv[count], "on") == 0)
					opts.showtopics = 1;
				else if (strcmp(argv[count], "off") == 0)
					opts.showtopics = 0;
				else
					usage();
			}
			else
				usage();
		}
		count++;
	}
	
}


void messageArrived(MessageData* md)
{
	MQTTMessage* message = md->message;

	if (opts.showtopics)
		printf("%.*s\t", md->topicName->lenstring.len, md->topicName->lenstring.data);

	if (opts.nodelimiter)
		printf("%.*s", (int)message->payloadlen, (char*)message->payload);
	else
		printf("%.*s%s", (int)message->payloadlen, (char*)message->payload, opts.delimiter);

	fflush(stdout);
}


int main(int argc, char** argv)
{
	int rc = 0;
	unsigned char buf[100];
	unsigned char readbuf[100];
	
	if (argc < 2)
		usage();

    init();
	
	char* topic = argv[1];

	if (strchr(topic, '#') || strchr(topic, '+'))
		opts.showtopics = 1;
	if (opts.showtopics)
		printf("topic is %s\n", topic);

	getopts(argc, argv);	

	Network n;
	MQTTClient c;

	signal(SIGINT, cfinish);
	signal(SIGTERM, cfinish);

	NetworkInit(&n);

    do {
        rc = NetworkConnect(&n, opts.host, opts.port);
        if (rc >= 0) {
            break;
        }
        sleep(1);
    } while (1);

	MQTTClientInit(&c, &n, 1000, buf, 100, readbuf, 100);

	MQTTPacket_connectData data = MQTTPacket_connectData_initializer;
	data.willFlag = 0;
	data.MQTTVersion = 3;
	data.clientID.cstring = opts.clientid;
	data.username.cstring = opts.username;
	data.password.cstring = opts.password;

	data.keepAliveInterval = 10;
	data.cleansession = 1;
	printf("Connecting to %s %d\n", opts.host, opts.port);

	rc = MQTTConnect(&c, &data);
	printf("Connected %d\n", rc);
    if (rc < 0) {
        exit(-1);
    }

    printf("Subscribing to %s\n", topic);
	rc = MQTTSubscribe(&c, topic, opts.qos, messageArrived);
	printf("Subscribed %d\n", rc);

	while (!toStop)
	{
		if (MQTTYield(&c, 1000) < 0) {
            printf("MQTTYield failed\n");
            exit(-1);
        }
	}

	printf("Stopping\n");

	MQTTDisconnect(&c);
	NetworkDisconnect(&n);

	return 0;
}
