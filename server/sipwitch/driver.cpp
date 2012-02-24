// Copyright (C) 2008-2009 David Sugar, Tycho Softworks.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#include "driver.h"

using namespace UCOMMON_NAMESPACE;
using namespace BAYONNE_NAMESPACE;

class driver : public Driver
{
public:
    driver();

    int start(void);

    void stop(void);

    void automatic(void);

};

driver::driver() : Driver("sip", "registry")
{
    autotimer = 500;
}

int driver::start(void)
{
    linked_pointer<keydata> kp = keyserver.get("registry");
    const char *cp = NULL, *iface = NULL, *transport = NULL, *agent = NULL;
    unsigned port = 5060, expires = 300, threads = 0;
    int protocol = IPPROTO_UDP;
    int family = AF_INET;
    int tlsmode = 0;
    int socktype = SOCK_DGRAM;
    int send101 = 1;
    size_t stack = 0;
    unsigned priority = 0;

    if(keys)
        cp = keys->get("port");
    if(cp)
        port = atoi(cp);

    if(keys)
        cp = keys->get("expires");
    if(cp)
        expires = atoi(cp);

    if(keys)
        cp = keys->get("stack");
    if(cp)
        stack = atoi(cp);

    if(keys)
        cp = keys->get("priority");
    if(cp)
        priority = atoi(cp);

    if(keys)
        iface = keys->get("interface");
    if(iface) {
#ifdef  AF_INET6
        if(strchr(iface, ':'))
            family = AF_INET6;
#endif
        if(eq(iface, ":::") || eq(iface, "::0") || eq(iface, "*") || iface[0] == 0)
            iface = NULL;
    }

    if(keys)
        transport = keys->get("transport");
    if(transport && eq(transport, "tcp")) {
        socktype = SOCK_STREAM;
        protocol = IPPROTO_TCP;
    }
    else if(transport && eq(transport, "tls")) {
        socktype = SOCK_STREAM;
        protocol = IPPROTO_TCP;
        tlsmode = 1;
    }

    if(keys)
        agent = keys->get("agent");
    if(!agent)
        agent = "bayonne-" VERSION "/exosip2";

    const char *addr = iface;

    eXosip_init();
#ifdef  AF_INET6
    if(family == AF_INET6) {
        eXosip_enable_ipv6(1);
        if(!iface)
            addr = "::0";
    }
#endif
    if(!addr)
        addr = "*";

    Socket::family(family);
    if(eXosip_listen_addr(protocol, iface, port, family, tlsmode))
        shell::log(shell::FAIL, "driver cannot bind %s, port=%d", addr, port);
    else
        shell::log(shell::NOTIFY, "driver binding %s, port=%d", addr, port);

    osip_trace_initialize_syslog(TRACE_LEVEL0, (char *)"bayonne");
    eXosip_set_user_agent(agent);

#ifdef  EXOSIP2_OPTION_SEND_101
    eXosip_set_option(EXOSIP_OPT_DONT_SEND_101, &send101);
#endif

    if(keys)
        cp = keys->get("threads");
    if(cp)
        threads = atoi(cp);

    if(!threads)
        ++threads;
    while(threads--) {
        thread *t = new thread(stack * 1024l);
        t->start(priority);
    }

    if(is(kp)) {
        new registry(*kp, port, expires);
        return 0;
    }

    kp = keygroup.begin();
    while(is(kp)) {
        new registry(*kp, port, expires);
        kp.next();
    }
    return 0;
}

void driver::stop(void)
{
    eXosip_quit();
    thread::shutdown();
}

void driver::automatic(void)
{
    eXosip_lock();
    eXosip_automatic_action();
    eXosip_unlock();
}

static SERVICE_MAIN(main, argc, argv)
{
    signals::service("bayonne");
    server::start(argc, argv);
}

PROGRAM_MAIN(argc, argv)
{
    new driver;
    server::start(argc, argv, &service_main);
    PROGRAM_EXIT(0);
}


