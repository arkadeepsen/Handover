package net.floodlightcontroller.ofSwitchAP;

public class OFActionPushIPH extends OFActionIPH 
{
	public static final int OFPAT_PUSH_IPH = 0x00000000;

	public OFActionPushIPH() {
		super();
		super.subtype = OFPAT_PUSH_IPH;
	}

	public OFActionPushIPH(int tunnelEntry, int tunnelExit) {
		super(tunnelEntry, tunnelExit);
		super.subtype = OFPAT_PUSH_IPH;
	}

}
