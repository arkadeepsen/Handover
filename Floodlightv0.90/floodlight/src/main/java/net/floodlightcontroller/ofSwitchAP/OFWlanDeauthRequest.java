package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanDeauthRequest extends OFWlanData {
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanDeauthRequest();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanDeauthRequest.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_DEAUTH_REQ = 3;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanDeauthRequest() {
        super(WLAN_DEAUTH_REQ);
    }

	public OFWlanDeauthRequest(byte[] staMacAddress, byte[] apMacAddress) {
		super(WLAN_DEAUTH_REQ, staMacAddress, apMacAddress);
	}

}
