package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanReassociationRequest extends OFWlanData {
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanReassociationRequest();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanReassociationRequest.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    /**
     * The data type value for a role reply
     */
    public static final int WLAN_REASSOC_REQ = 7;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanReassociationRequest() {
        super(WLAN_REASSOC_REQ);
    }

	public OFWlanReassociationRequest(byte[] staMacAddress, byte[] apMacAddress) {
		super(WLAN_REASSOC_REQ, staMacAddress, apMacAddress);
	}


}
