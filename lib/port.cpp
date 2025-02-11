#include <port.h>

unsigned int Port_Info::init(const unsigned int base, const unsigned int offset, const unsigned int r)
{
    range = (r > 0 ? r : range);
    start_port = base + offset * range;
    port = start_port;
    return start_port;
}

unsigned int Port_Info::inc()
{
    port_count = (port_count + 1) % range;
    port = start_port + port_count;
    return port;
}
