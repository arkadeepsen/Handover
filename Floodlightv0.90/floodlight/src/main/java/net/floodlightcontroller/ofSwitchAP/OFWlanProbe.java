package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanProbe extends OFWlanData {
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanProbe();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanProbe.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_PROBE = 12;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanProbe() {
        super(WLAN_PROBE);
    }

	public OFWlanProbe(byte[] staMacAddress, byte[] apMacAddress) {
		super(WLAN_PROBE, staMacAddress, apMacAddress);
	}

}
