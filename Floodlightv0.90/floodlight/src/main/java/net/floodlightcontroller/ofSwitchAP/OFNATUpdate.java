package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFNATUpdate extends OFNATMessage {

	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFNATUpdate();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFNATUpdate.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_NAT_UPDATE = 19;

    /**
     * Construct a Probe data with an unspecified role value.
     */

	public OFNATUpdate() {
		super(WLAN_NAT_UPDATE);
	}

	public OFNATUpdate(byte[] staMacAddress, int sourceIP, int destinationIP, short sourcePort,
			short destinationPort) {
		super(WLAN_NAT_UPDATE, staMacAddress, sourceIP, destinationIP, sourcePort, destinationPort);
	}

}
