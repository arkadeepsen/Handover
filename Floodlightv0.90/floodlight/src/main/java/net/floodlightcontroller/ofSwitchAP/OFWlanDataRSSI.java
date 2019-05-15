package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanDataRSSI extends OFWlanRSSI {
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanDataRSSI();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanDataRSSI.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_REC_POW = 14;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanDataRSSI() {
        super(WLAN_REC_POW);
    }

	public OFWlanDataRSSI(byte[] staMacAddress, byte[] apMacAddress, byte rssi) {
		super(WLAN_REC_POW, staMacAddress, apMacAddress, rssi);
	}

}
