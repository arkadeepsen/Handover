package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanAddSTA extends OFWlanSTAMessage {
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanAddSTA();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanAddSTA.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_ADD_STA = 13;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanAddSTA() {
        super(WLAN_ADD_STA);
    }

	public OFWlanAddSTA(byte[] staMacAddress) {
		super(WLAN_ADD_STA, staMacAddress);
	}

}
