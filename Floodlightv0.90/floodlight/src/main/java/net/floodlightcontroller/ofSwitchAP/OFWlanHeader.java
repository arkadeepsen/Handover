package net.floodlightcontroller.ofSwitchAP;

import org.jboss.netty.buffer.ChannelBuffer;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanHeader implements OFVendorData {
	
	public static final int AP_EXPERIMENTER_ID = 0x00000001;
	
	protected int subtype;
	
	@SuppressWarnings("unused")
	private static enum exp_type {
		WLAN_AUTH_REQ,
	    WLAN_AUTH_RESP,
	    WLAN_AUTH_ERROR,

	    WLAN_DEAUTH_REQ,

	    WLAN_ASSOC_REQ,
	    WLAN_ASSOC_RESP,
	    WLAN_ASSOC_ERROR,

	    WLAN_REASSOC_REQ,
	    WLAN_REASSOC_RESP,
	    WLAN_REASSOC_ERROR,

	    WLAN_DISASSOC_REQ,

	    WLAN_BSSID,

	    WLAN_PROBE,

	    WLAN_ADD_STA,

	    WLAN_REC_POW,

	    WLAN_LAST_REC_POW,

	    WLAN_AP_REC_POW,

	    WLAN_UPDATE_STA_ADD,

	    WLAN_REMOVE_STA
	}
	
	public OFWlanHeader() {
	}

	public OFWlanHeader(int subtype) {
		this.subtype = subtype;
	}

	public int getSubtype() {
		return subtype;
	}

	public void setSubtype(int subtype) {
		this.subtype = subtype;
	}

	@Override
	public int getLength() {
		return 4;
	}

	@Override
	public void readFrom(ChannelBuffer data, int length) {
		this.subtype = data.readInt();
	}

	@Override
	public void writeTo(ChannelBuffer data) {
		data.writeInt(this.subtype);
	}

}
