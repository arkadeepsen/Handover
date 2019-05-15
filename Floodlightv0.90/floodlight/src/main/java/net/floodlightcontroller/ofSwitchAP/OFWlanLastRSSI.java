package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanLastRSSI extends OFWlanSTAMessage {
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanLastRSSI();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanLastRSSI.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_LAST_REC_POW = 15;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanLastRSSI() {
        super(WLAN_LAST_REC_POW);
    }

	public OFWlanLastRSSI(byte[] staMacAddress) {
		super(WLAN_LAST_REC_POW, staMacAddress);
	}

}
