#ifndef PORT_H
#define PORT_H

#define PORT_RANGE 3

// stores info about the host port for easy arithmetic
// The port must change in between runs of Syzkaller in
// case the previous port was not freed yet.
class Port_Info
{
public:
    unsigned int port;
    unsigned int port_count;
    unsigned int start_port;
    unsigned int range;

    Port_Info()
        : port(0), port_count(0), start_port(0), range(PORT_RANGE)
    { return; }
    
    Port_Info(unsigned int p, unsigned int pc, unsigned int sp, unsigned int r)
        : port(p), port_count(pc), start_port(sp), range(r)
    { return; }

    unsigned int init(const unsigned int, const unsigned int = 0, const unsigned int = PORT_RANGE);
    unsigned int inc();
};

#endif