package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFWlanUpdateSTAAdd extends OFWlanUpdateSTA{
	
	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFWlanUpdateSTAAdd();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFWlanUpdateSTAAdd.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_UPDATE_STA_ADD = 17;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    public OFWlanUpdateSTAAdd() {
        super(WLAN_UPDATE_STA_ADD);
    }

	public OFWlanUpdateSTAAdd(byte[] staMacAddress, short auth_alg, short aid, short capability,
			short listen_interval, byte[] supported_rates, int supported_rates_len,
			byte qosinfo) {
		super(WLAN_UPDATE_STA_ADD, staMacAddress, auth_alg, aid, capability, listen_interval,
				supported_rates, supported_rates_len, qosinfo);
	}

}
