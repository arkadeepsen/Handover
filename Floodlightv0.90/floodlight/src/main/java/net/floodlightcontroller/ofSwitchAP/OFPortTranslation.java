package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class OFPortTranslation extends OFNATMessageExt {

	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new OFPortTranslation();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of OFNATAdd.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int WLAN_PORT_TRANSLATION = 21;

    /**
     * Construct a Probe data with an unspecified role value.
     */

	public OFPortTranslation() {
		super(WLAN_PORT_TRANSLATION);
	}

	public OFPortTranslation(byte[] staMacAddress, int sourceIP, int destinationIP, short sourcePort,
			short destinationPort, short newPort) {
		super(WLAN_PORT_TRANSLATION, staMacAddress, sourceIP, destinationIP, sourcePort, destinationPort, newPort);
	}

}
