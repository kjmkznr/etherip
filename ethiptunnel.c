#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <linux/if.h>
#include <linux/if_arp.h>
#include <linux/if_tunnel.h>

#define IPPROTO_ETHERIP 97

enum {
	ACTION_NONE = 0,
	ACTION_ADD = 1,
	ACTION_DEL = 2,
	ACTION_CHG = 3,
	ACTION_LIST = 4 };

void __init_tunnel_params(struct ip_tunnel_parm *p, char *devname, __u32 daddr, __u32 saddr, int set_saddr, int ttl)
{
	if (devname)
		strncpy(p->name, devname, IFNAMSIZ);
	else p->name[0] = 0;
	p->name[IFNAMSIZ-1] = 0;

	p->iph.version = 4;
	p->iph.protocol = IPPROTO_ETHERIP;
	p->iph.ihl = 5;
	if (daddr != 0)
		p->iph.daddr = daddr;
	if (set_saddr)
		p->iph.saddr = saddr;
	if ((ttl >= 0) && (ttl < 256))
		p->iph.ttl = ttl;
}

int __do_tunnel_action(int cmd, struct ifreq *ifr)
{
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	int err;
	
	if (fd == -1) {
		perror("socket");
		return 1;
	}

	err = ioctl(fd, cmd, ifr);
	if (err < 0)
		perror("ioctl");

	return (err < 0);
}

int do_tunnel_add(char *devname, __u32 daddr, __u32 saddr, int ttl)
{
	struct ifreq ifr;
	struct ip_tunnel_parm p;

	strncpy(ifr.ifr_name, "ethip0", IFNAMSIZ);
	ifr.ifr_ifru.ifru_data = (void *)&p;
	p.iph.ttl = 0;
	__init_tunnel_params(&p, devname, daddr, saddr, 1, ttl);

	return __do_tunnel_action(SIOCADDTUNNEL, &ifr);
}

int do_tunnel_change (char *devname, __u32 daddr, __u32 saddr, int set_saddr, int ttl)
{
	struct ifreq ifr;
	struct ip_tunnel_parm p;
	int fd;

	strncpy(ifr.ifr_name, devname, IFNAMSIZ);
	p.name[IFNAMSIZ-1] = 0;
	ifr.ifr_ifru.ifru_data = (void *)&p;
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd == -1) {
		perror("socket");
		return 1;
	}
	if (ioctl(fd, SIOCGETTUNNEL, &ifr) != 0) {
		perror("ioctl");
		close(fd);
		return 1;
	}
	close(fd);
	__init_tunnel_params(&p, devname, daddr, saddr, set_saddr, ttl);

	return __do_tunnel_action(SIOCCHGTUNNEL, &ifr);
}

int do_tunnel_del(char *devname)
{
	struct ifreq ifr;
	
	strncpy(ifr.ifr_name, devname, IFNAMSIZ);
	return __do_tunnel_action(SIOCDELTUNNEL, &ifr);
}

int do_tunnel_list()
{
	int sd; 
	FILE *fd = fopen("/proc/net/dev", "r");
	char line[2048];
	char dev[IFNAMSIZ];
	int i,j,k,found = 0;
	struct ifreq ifr;
	struct ip_tunnel_parm p;
	struct in_addr daddr_s;
	char ttl_str[8], saddr_str[16];
	char *result;
	
	sd = socket(AF_INET, SOCK_DGRAM, 0);
	if (sd == -1) {
		perror("socket");
		return 1;
	}

	if (fd == NULL) {
		perror("fopen");
		close(sd);
		return 1;
	}

	/* skip first 2 lines */
	result = fgets(line, 2048, fd);
	result = fgets(line, 2048, fd);

	while (fgets(line, 2048, fd)) {
		for (i=0;line[i] == ' ';++i);
		for (j=0;(line[i+j] != ':') && (line[i+j] != 0);++j) {
			if (j > IFNAMSIZ-1) break;
			dev[j] = line[i+j];
		}
		dev[j] = 0;
		
		strncpy(ifr.ifr_name, dev, IFNAMSIZ);
		ifr.ifr_ifru.ifru_data = (void *)&p;
		if (ioctl(sd, SIOCGETTUNNEL, &ifr) != 0) continue;
		if (p.iph.protocol != IPPROTO_ETHERIP) continue;

		for (k=0;k<IFNAMSIZ-1;++k) if (p.name[k] == 0) break;
		for (   ;k<IFNAMSIZ-1;++k) p.name[k] = ' ';
		p.name[k] = 0;

		daddr_s.s_addr = p.iph.daddr;
		
		if (p.iph.saddr != 0)
			inet_ntop(AF_INET, (void*)&p.iph.saddr, saddr_str, 16);
		else
			strcpy(saddr_str, "any            ");

		if (p.iph.ttl == 0)
			strcpy(ttl_str, "default");
		else
			snprintf(ttl_str, 7, "%d", p.iph.ttl);
		if (found == 0)
			printf("Device\t\tDestination\tSource\t\tTTL\n");
		printf("%s\t%s\t%s\t%s\n", p.name,
				inet_ntoa(daddr_s), saddr_str, ttl_str);
		++found;
	}
	if (found == 0)
		printf("No etherip devices configured\n");

	close(sd);
	fclose(fd);

	return 0;
}

void usage(const char* prog)
{
	printf("EtherIP Tunnel Setup Tool 0.1\n");
	printf("Usage: %s [-a|-r|-c|-l] [-d dest] [-n devname] [-s <source>] [-t ttl]\n", prog);
	printf("-a           add a new device\n");
	printf("-r           remove a device\n");
	printf("-c           change the destination address of a device\n");
	printf("-l           list devices (other arguments are ignored)\n");
	printf("-d <addr>    specify tunnel destination (use with -a)\n");
	printf("-s <addr>    specify tunnel source address\n");
	printf("-n <devname> specify the name of the tunnel device\n");
	printf("-t <ttl>     specify ttl for tunnel packets (0 means system default)\n");
}

int action_error()
{
	printf("Only one action can be specified\n");
	return 1;
}

int main(int argc, char **argv)
{
	int opt;
	char *devname = NULL;
	int action = ACTION_NONE;
	struct hostent *host;
	__u32 daddr = 0;
	__u32 saddr = 0;
	int set_saddr = 0;
	int ttl = -1;

	do {
		opt = getopt(argc, argv, "hard:n:lct:s:");
		switch (opt) {
			case 'h':
				usage(argv[0]);
				return 0;
			case 'a':
				if (action != ACTION_NONE)
					return action_error();

				action = ACTION_ADD;
				break;
			case 'r':
				if (action != ACTION_NONE)
					return action_error();

				action = ACTION_DEL;
				break;
			case 'c':
				if (action != ACTION_NONE)
					return action_error();

				action = ACTION_CHG;
				break;
			case 'l':
				if (action != ACTION_NONE)
					return action_error();

				action = ACTION_LIST;
				break;
			case 'd':
				host = gethostbyname(optarg);
				if (host == NULL) {
					printf("Destination host not "
							"found: %s\n", optarg);
					return 1;
				}
				daddr = ((struct in_addr*)host->h_addr)->s_addr;
				break;
			case 's':
				host = gethostbyname(optarg);
				if (host == NULL) {
					printf("Source host not "
							"found: %s\n", optarg);
					return 1;
				}
				set_saddr = 1;
				saddr = ((struct in_addr*)host->h_addr)->s_addr;
				break;
			case 'n':
				devname = optarg;
				break;
			case 't':
				ttl = atoi(optarg);
				if ((ttl < 0) || (ttl > 255)) {
					printf("TTL value must be between 0 and 255\n");
					usage(argv[0]);
					return 1;
				}
			case -1:
				break;
			default:
				usage(argv[0]);
				return 1;
		}
	} while (opt != -1);

	if (action == ACTION_ADD) {
		if (daddr == 0) {
			printf("A valid tunnel destination must be specified\n");
			return 1;
		} else return do_tunnel_add(devname, daddr, saddr, ttl);
	}
	
	if (action == ACTION_DEL) {
		if (devname == 0) {
			printf("Please specify the devicename to delete\n");
			return 1;
		} else return do_tunnel_del(devname);
	}
	
	if (action == ACTION_CHG) {
		if (devname == 0) {
			printf("A valid devicename must be specified\n");
			return 1;
		} else return do_tunnel_change(devname, daddr, saddr, set_saddr, ttl);
	}

	if (action == ACTION_LIST)
		return do_tunnel_list();
		
	printf("Please specify an action\n");
	return 1;
}
