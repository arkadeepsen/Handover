#ifndef CLICK_PRINTSIGNALSTRENGTH_HH
#define CLICK_PRINTSIGNALSTRENGTH_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
CLICK_DECLS

class PrintSignalStrength : public Element { 
	
	public:
		PrintSignalStrength() CLICK_COLD;
		~PrintSignalStrength() CLICK_COLD;

		const char *class_name() const	{ return "PrintSignalStrength"; }
		const char *port_count() const	{ return PORTS_1_1; }
		const char *processing() const	{ return AGNOSTIC; }

		int configure(Vector<String> &, ErrorHandler *) CLICK_COLD;

		bool can_live_reconfigure() const	{ return true; }

		Packet *simple_action(Packet *);
	private:
		EtherAddress _addr;

};

CLICK_ENDDECLS
#endif
