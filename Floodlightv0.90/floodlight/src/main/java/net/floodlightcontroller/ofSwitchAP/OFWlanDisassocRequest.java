package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanDisassocRequest extends OFWlanData {
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanDisassocRequest();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanDisassocRequest.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_DISASSOC_REQ = 10;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanDisassocRequest() {
        super(WLAN_DISASSOC_REQ);
    }

	public OFWlanDisassocRequest(byte[] staMacAddress, byte[] apMacAddress) {
		super(WLAN_DISASSOC_REQ, staMacAddress, apMacAddress);
	}

}
