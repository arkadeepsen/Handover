package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFNATAdd extends OFNATMessageExt {

	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFNATAdd();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFNATAdd.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_NAT_ADD = 20;

    /**
     * Construct a Probe data with an unspecified role value.
     */

	public OFNATAdd() {
		super(WLAN_NAT_ADD);
	}

	public OFNATAdd(byte[] staMacAddress, int sourceIP, int destinationIP, short sourcePort,
			short destinationPort, short newPort) {
		super(WLAN_NAT_ADD, staMacAddress, sourceIP, destinationIP, sourcePort, destinationPort, newPort);
	}

}