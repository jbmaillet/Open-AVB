/*
  Copyright (c) 2013 Katja Rohloff <Katja.Rohloff@uni-jena.de>
  Copyright (c) 2014, Parrot SA

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "mrpdclient.h"
#include "listener_mrp_client.h"

/* global variables */

volatile int talker = 0;
unsigned char stream_id[8];

static int msg_process(char *buf, int buflen)
{
	uint32_t id;
	int j, l;

	fprintf(stderr, "Msg: %s\n", buf);
	if (strncmp(buf, "SNE T:", 6) == 0 || strncmp(buf, "SJO T:", 6) == 0)
	{
		l = 6; /* skip "Sxx T:" */
		while ((l < buflen) && ('S' != buf[l++]));
		if (l = buflen)
			return -1;
		l++;
		for(j = 0; j < 8 ; l+=2, j++)
		{
			sscanf(&buf[l],"%02x",&id);
			stream_id[j] = (unsigned char)id;
		}
		talker = 1;
	}

	return 0;
}

/*
 * public
 */

int report_domain_status(SOCKET mrpd_sock)
{
	int rc;
	char* msgbuf;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "S+D:C=6,P=3,V=0002");
	
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, 1500);
	free(msgbuf);

	return rc;
}

int join_vlan(SOCKET mrpd_sock)
{
	int rc;
	char *msgbuf;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	msgbuf = malloc(1500);
	if (NULL == msgbuf)
		return -1;
	memset(msgbuf, 0, 1500);
	sprintf(msgbuf, "V++:I=0002");
	rc = mrpdclient_sendto(mrpd_sock, msgbuf, 1500);
	free(msgbuf);

	return rc;
}

int await_talker(SOCKET mrpd_sock)
{
	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	while (0 == talker)
		mrpdclient_recv(mrpd_sock, msg_process);

	return 0;
}

int send_ready(SOCKET mrpd_sock)
{
	char *databuf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	databuf = malloc(1500);
	if (NULL == databuf)
		return -1;
	memset(databuf, 0, 1500);
	sprintf(databuf, "S+L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=2",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);
	rc = mrpdclient_sendto(mrpd_sock, databuf, 1500);
#ifdef DEBUG
	fprintf(stdout,"Ready-Msg: %s\n", databuf);
#endif 
	free(databuf);

	return rc;
}

int send_leave(SOCKET mrpd_sock)
{
	char *databuf;
	int rc;

	if (SOCKET_ERROR == mrpd_sock)
		return -1;

	databuf = malloc(1500);
	if (NULL == databuf)
		return -1;
	memset(databuf, 0, 1500);
	sprintf(databuf, "S-L:L=%02x%02x%02x%02x%02x%02x%02x%02x, D=3",
		     stream_id[0], stream_id[1],
		     stream_id[2], stream_id[3],
		     stream_id[4], stream_id[5],
		     stream_id[6], stream_id[7]);
	rc = mrpdclient_sendto(mrpd_sock, databuf, 1500);
	free(databuf);

	return rc;
}
