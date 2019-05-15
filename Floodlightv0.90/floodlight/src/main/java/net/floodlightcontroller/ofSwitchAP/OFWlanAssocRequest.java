package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanAssocRequest extends OFWlanAssociation {
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanAssocRequest();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanAssocRequest.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_ASSOC_REQ = 4;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanAssocRequest() {
        super(WLAN_ASSOC_REQ);
    }

	public OFWlanAssocRequest(byte[] staMacAddress, byte[] apMacAddress) {
		super(WLAN_ASSOC_REQ, staMacAddress, apMacAddress);
	}

}
