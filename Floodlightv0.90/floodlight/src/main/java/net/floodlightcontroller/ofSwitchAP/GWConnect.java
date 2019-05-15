package net.floodlightcontroller.ofSwitchAP;

import org.openflow.protocol.Instantiable;
import org.openflow.protocol.vendor.OFVendorData;

public class GWConnect extends OFWlanHeader {

	protected static Instantiable<OFVendorData> instantiable =
            new Instantiable<OFVendorData>() {
                public OFVendorData instantiate() {
                    return new GWConnect();
                }
            };

    /**
     * @return a subclass of Instantiable<OFVendorData> that instantiates
     *         an instance of GWConnect.
     */
    public static Instantiable<OFVendorData> getInstantiable() {
        return instantiable;
    }

    public static final int GW_CONNECT = 100;

    /**
     * Construct a Probe data with an unspecified role value.
     */
    
	public GWConnect() {
		super(GW_CONNECT);
	}
}
