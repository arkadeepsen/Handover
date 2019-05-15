package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanRemoveSTA extends OFWlanSTAMessage {
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanRemoveSTA();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanRemoveSTA.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_REMOVE_STA = 18;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanRemoveSTA() {
        super(WLAN_REMOVE_STA);
    }

	public OFWlanRemoveSTA(byte[] staMacAddress) {
		super(WLAN_REMOVE_STA, staMacAddress);
	}

}
