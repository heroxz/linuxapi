/*
 * This file is part of the linuxapi package.
 * Copyright (C) 2011-2013 Mark Veltzer <mark.veltzer@gmail.com>
 *
 * linuxapi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * linuxapi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with linuxapi. If not, see <http://www.gnu.org/licenses/>.
 */

#include <firstinclude.h>
#include <libnetfilter_queue/libnetfilter_queue.h>
#include <netinet/in.h>
#include <linux/types.h>
#include <linux/netfilter.h>	/* for NF_ACCEPT */
#include <arpa/inet.h>
#include <netinet/udp.h>
#include <netinet/ip.h>
#include <stdlib.h>	// for EXIT_SUCCESS
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <err_utils.h>	// for CHECK_NOT_NULL(), CHECK_NOT_NEGATIVE(), CHECK_NOT_M1()
#include <security_utils.h>	// for check_root()
#include <multiproc_utils.h>	// for my_system()
#include <signal_utils.h>	// for signal_register_handler_sigaction()

/*
 * This is an example of how to use the netfilter-queue kernel API.
 *
 * How to run this example:
 * - you have to run this via sudo(1) (after all, getting all packets
 * is quite a priveleged operation...)
 * - notice the rule setting up at the begining so that we could get
 * all the packets into user space.
 *
 * References:
 * http://gitorious.org/meshias/mainline/blobs/db36f92bfcbdb78631aaf7f03c1ed1c156f8c218/examples/nfqueue.c
 *
 * EXTRA_LINK_FLAGS=-lnetfilter_queue -lnfnetlink
 *
 * TODO:
 * - make this program shut down nicer when we do CTRL+C.
 * - cant we get the NF_IP_* constants from somewhere ?
 */

/* After promisc drops, checksum checks. */
#define NF_IP_PRE_ROUTING 0
/* If the packet is destined for this box. */
#define NF_IP_LOCAL_IN 1
/* If the packet is destined for another interface. */
#define NF_IP_FORWARD 2
/* Packets coming from a local process. */
#define NF_IP_LOCAL_OUT 3
/* Packets about to hit the wire. */
#define NF_IP_POST_ROUTING 4
#define NF_IP_NUMHOOKS 5

static uint32_t nfqueue_packet_get_id(struct nfq_data *packet) {
	uint32_t id = -1;
	struct nfqnl_msg_packet_hdr *packetHeader;
	if((packetHeader = nfq_get_msg_packet_hdr(packet)) != NULL )
		id = ntohl(packetHeader->packet_id);
	return id;
}

static uint32_t nfqueue_packet_get_hook(struct nfq_data *packet) {
	uint32_t hook = -1;
	struct nfqnl_msg_packet_hdr *packetHeader;
	if((packetHeader = nfq_get_msg_packet_hdr(packet)) != NULL )
		hook = packetHeader->hook;
	return hook;
}

static int manage_packet(struct nfq_q_handle *qh, struct nfgenmsg *nfmsg, struct nfq_data *nfa, void *data2) {
	uint32_t hook = nfqueue_packet_get_hook(nfa);
	// AODV traffic is handled elsewhere already (by the daemon)
	uint32_t id = nfqueue_packet_get_id(nfa);
	// packets coming from different hooks are handled in different ways
	switch(hook) {
	case NF_IP_LOCAL_IN:
		puts("capturing packet from INPUT iptables hook");
		return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
		break;
	case NF_IP_FORWARD:
		puts("capturing packet from FORWARD iptables hook");
		return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
		break;
	case NF_IP_LOCAL_OUT:
		puts("capturing packet from OUTPUT iptables hook");
		return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
		break;
	default:
		puts("error: capturing packet from an iptables hook we shouldn't");
		return nfq_set_verdict(qh, id, NF_ACCEPT, 0, NULL);
		break;
	}
}

static bool over=false;

static void handler(int sig, siginfo_t *si, void *unused) {
	over=true;
}

int main(int argc, char **argv, char** envp) {
	// lets check that we are root
	check_root();
	signal_register_handler_sigaction(SIGINT, handler);
	signal_register_handler_sigaction(SIGTERM, handler);
	// setup the kernel (today there is no need for the modprobes here
	// since the iptables use will automatically load the right modules...)
	// my_system("modprobe iptable_filter");
	// my_system("modprobe ip_queue");
	my_system("iptables -A OUTPUT -j QUEUE");
	struct nfq_handle * handle = (struct nfq_handle *)CHECK_NOT_NULL(nfq_open());
	CHECK_NOT_NEGATIVE(nfq_unbind_pf(handle, AF_INET));
	CHECK_NOT_NEGATIVE(nfq_bind_pf(handle, AF_INET));
	struct nfq_q_handle * queue = (struct nfq_q_handle *)CHECK_NOT_NULL(nfq_create_queue(handle, 0, &manage_packet, NULL));
	CHECK_NOT_NEGATIVE(nfq_set_mode(queue, NFQNL_COPY_PACKET, 0xffff));
	struct nfnl_handle * netlink_handle = nfq_nfnlh(handle);
	uint32_t nfqueue_fd = nfnl_fd(netlink_handle);
	// End of NF_QUEUE initializing
	while(!over) {
		char buf[4096] __attribute__ ((aligned));
		// in case we get interrupted...
		int received = recv(nfqueue_fd, buf, sizeof(buf), 0);
		if (received == -1) {
			if (errno == EINTR) {
				continue;
			} else {
				CHECK_NOT_M1(received);
			}
		}
		nfq_handle_packet(handle, buf, received);
	}
	nfq_destroy_queue(queue);
	nfq_close(handle);
	my_system("iptables -D OUTPUT -j QUEUE");
	return EXIT_SUCCESS;
}
