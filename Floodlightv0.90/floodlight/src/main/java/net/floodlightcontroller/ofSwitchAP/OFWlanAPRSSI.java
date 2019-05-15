package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanAPRSSI extends OFWlanRSSI {
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanAPRSSI();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanAPRSSI.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_AP_REC_POW = 16;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanAPRSSI() {
        super(WLAN_AP_REC_POW);
    }

	public OFWlanAPRSSI(byte[] staMacAddress, byte[] apMacAddress, byte rssi) {
		super(WLAN_AP_REC_POW, staMacAddress, apMacAddress, rssi);
	}

}
