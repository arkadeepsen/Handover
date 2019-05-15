package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanAuthRequest extends OFWlanAuthentication {
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanAuthRequest();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanAuthRequest.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_AUTH_REQ = 0;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanAuthRequest() {
        super(WLAN_AUTH_REQ);
    }

	public OFWlanAuthRequest(byte[] staMacAddress, byte[] apMacAddress) {
		super(WLAN_AUTH_REQ, staMacAddress, apMacAddress);
	}

}
