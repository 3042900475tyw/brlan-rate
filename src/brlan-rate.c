/*
 * brlan-rate — report realtime RX/TX rate of a network interface (default: br-lan)
 *
 * Reads the interface's 64-bit byte counters through the kernel's rtnetlink
 * interface (RTM_GETLINK + IFLA_STATS64), samples twice, and prints the
 * throughput in Mbps. No /proc, no /sys, no command parsing.
 *
 * Usage: brlan-rate [ifname] [interval_seconds]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/if_link.h>

/* Query the kernel for ifname's current rx/tx byte counters. */
static int get_stats(const char *ifname, unsigned long long *rx, unsigned long long *tx)
{
    int fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (fd < 0)
        return -1;

    /* RTM_GETLINK request: ifinfomsg + IFLA_IFNAME attribute. */
    struct {
        struct nlmsghdr nlh;
        struct ifinfomsg ifi;
        char buf[256];
    } req;
    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nlh.nlmsg_type  = RTM_GETLINK;
    req.nlh.nlmsg_flags = NLM_F_REQUEST;
    req.ifi.ifi_family  = AF_UNSPEC;

    struct rtattr *rta = (struct rtattr *)(((char *)&req) + NLMSG_ALIGN(req.nlh.nlmsg_len));
    rta->rta_type = IFLA_IFNAME;
    int nlen = strlen(ifname) + 1;
    rta->rta_len = RTA_LENGTH(nlen);
    memcpy(RTA_DATA(rta), ifname, nlen);
    req.nlh.nlmsg_len = NLMSG_ALIGN(req.nlh.nlmsg_len) + RTA_ALIGN(rta->rta_len);

    struct sockaddr_nl sa = { .nl_family = AF_NETLINK };
    if (sendto(fd, &req, req.nlh.nlmsg_len, 0, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        close(fd);
        return -1;
    }

    char reply[8192];
    ssize_t n = recv(fd, reply, sizeof(reply), 0);
    close(fd);
    if (n < 0)
        return -1;

    for (struct nlmsghdr *nlh = (struct nlmsghdr *)reply;
         NLMSG_OK(nlh, n); nlh = NLMSG_NEXT(nlh, n)) {
        if (nlh->nlmsg_type == NLMSG_ERROR) {
            /* Interface not found -> kernel replies with a negative errno (e.g. -ENODEV). */
            struct nlmsgerr *e = NLMSG_DATA(nlh);
            errno = -e->error;
            return -1;
        }
        if (nlh->nlmsg_type != RTM_NEWLINK)
            continue;

        struct ifinfomsg *ifi = NLMSG_DATA(nlh);
        int len = nlh->nlmsg_len - NLMSG_LENGTH(sizeof(*ifi));
        for (struct rtattr *a = IFLA_RTA(ifi); RTA_OK(a, len); a = RTA_NEXT(a, len)) {
            if (a->rta_type == IFLA_STATS64) {
                struct rtnl_link_stats64 *s = RTA_DATA(a);
                *rx = s->rx_bytes;
                *tx = s->tx_bytes;
                return 0;
            }
        }
    }
    return -1;
}

int main(int argc, char **argv)
{
    const char *ifname = (argc > 1) ? argv[1] : "br-lan";
    double interval     = (argc > 2) ? atof(argv[2]) : 1.0;

    unsigned long long rx0, tx0;
    if (get_stats(ifname, &rx0, &tx0) != 0) {
        fprintf(stderr, "error: cannot get stats for interface '%s': %s\n",
                ifname, strerror(errno));
        return 1;
    }

    for (;;) {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        usleep((useconds_t)(interval * 1e6));

        unsigned long long rx1, tx1;
        if (get_stats(ifname, &rx1, &tx1) != 0) {
            fprintf(stderr, "error: interface '%s' disappeared: %s\n",
                    ifname, strerror(errno));
            return 1;
        }
        clock_gettime(CLOCK_MONOTONIC, &t1);

        double dt = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
        /* bytes -> bits (*8), per second, then bits -> Mbps (/1e6). */
        printf("iface=%s rx=%.2f Mbps tx=%.2f Mbps\n",
               ifname,
               (rx1 - rx0) * 8.0 / dt / 1e6,
               (tx1 - tx0) * 8.0 / dt / 1e6);
        fflush(stdout);

        rx0 = rx1;
        tx0 = tx1;
    }
    return 0;
}
