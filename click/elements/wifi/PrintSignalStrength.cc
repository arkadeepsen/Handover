#include <click/config.h>
#include "PrintSignalStrength.hh"
#include <click/args.hh>
#include <click/error.hh>
#include <click/glue.hh>
#include <clicknet/wifi.h>
#include <click/straccum.hh>
#include <click/etheraddress.hh>
#include <click/packet_anno.hh>
CLICK_DECLS

PrintSignalStrength::PrintSignalStrength()
{
}

PrintSignalStrength::~PrintSignalStrength()
{
}

int
PrintSignalStrength::configure(Vector<String> &conf, ErrorHandler *errh)
{
    if (Args(conf, this, errh)
		.read_mp("ETHER", _addr)
		.complete() < 0)
		return -1;
    return 0;
}

Packet *
PrintSignalStrength::simple_action(Packet *p)
{
	struct click_wifi *wh = (struct click_wifi *) p->data();
	struct click_wifi_extra *ceh = WIFI_EXTRA_ANNO(p);
	
	EtherAddress src;
	EtherAddress dst;
	EtherAddress bssid;
	
	StringAccum sa;
	int len;

	switch (wh->i_fc[1] & WIFI_FC1_DIR_MASK) {
	case WIFI_FC1_DIR_TODS:
		bssid = EtherAddress(wh->i_addr1);
		src = EtherAddress(wh->i_addr2);
//		dst = EtherAddress(wh->i_addr3);
		break;
	default:
		return p;
	}
	
	if(memcmp(bssid.data(), _addr.data(), 6) == 0)
	{
		len = sprintf(sa.reserve(9), "%2d", ceh->rssi);
		sa.adjust_length(len);
		
		StringAccum filename;
		filename << "signal/";
		filename << src;
		FILE *signal = fopen (filename.c_str(),"w");
		if (signal!=NULL)
		{
			fprintf(signal, "%s\n", sa.c_str());
			fclose (signal);
		}

//		sa << " BSSID " << bssid;
//		sa << " SRC " << src;
//		sa << " DST " << dst;
		
//		click_chatter("%s\n", sa.c_str());
	}
	return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(PrintSignalStrength)
