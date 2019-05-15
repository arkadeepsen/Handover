package net.floodlightcontroller.ofSwitchAP;

import org.jboss.netty.buffer.ChannelBuffer;
import org.openflow.protocol.action.OFActionVendor;

public class OFActionIPH extends OFActionVendor 
{
    public static int MINIMUM_LENGTH = 20;
	public static final int AP_EXPERIMENTER_ID = 0x00000001;
    
	protected int subtype;
	protected int tunnelEntry;
	protected int tunnelExit;
	
	public OFActionIPH() 
	{
		super();
        super.setLength((short) MINIMUM_LENGTH);
		super.setVendor(AP_EXPERIMENTER_ID);
	}

	public OFActionIPH(int tunnelEntry, int tunnelExit) {
		super();
        super.setLength((short) MINIMUM_LENGTH);
		super.setVendor(AP_EXPERIMENTER_ID);
		this.tunnelEntry = tunnelEntry;
		this.tunnelExit = tunnelExit;
	}

	public int getSubtype() {
		return subtype;
	}

	public void setSubtype(int subtype) {
		this.subtype = subtype;
	}

	public int getTunnelEntry() {
		return tunnelEntry;
	}

	public void setTunnelEntry(int tunnelEntry) {
		this.tunnelEntry = tunnelEntry;
	}

	public int getTunnelExit() {
		return tunnelExit;
	}

	public void setTunnelExit(int tunnelExit) {
		this.tunnelExit = tunnelExit;
	}

	@Override
	public int hashCode() {
		final int prime = 31;
		int result = super.hashCode();
		result = prime * result + subtype;
		result = prime * result + tunnelEntry;
		result = prime * result + tunnelExit;
		return result;
	}
	
	@Override
	public boolean equals(Object obj) {
		if (this == obj)
			return true;
		if (!super.equals(obj))
			return false;
		if (getClass() != obj.getClass())
			return false;
		OFActionIPH other = (OFActionIPH) obj;
		if (subtype != other.subtype)
			return false;
		if (tunnelEntry != other.tunnelEntry)
			return false;
		if (tunnelExit != other.tunnelExit)
			return false;
		return true;
	}

	@Override
	public void readFrom(ChannelBuffer data) {
		super.readFrom(data);
        this.subtype = data.readInt();
        this.tunnelEntry = data.readInt();
        this.tunnelExit = data.readInt();
	}

	@Override
	public void writeTo(ChannelBuffer data) {
		super.writeTo(data);
        data.writeInt(this.subtype);
        data.writeInt(this.tunnelEntry);
        data.writeInt(this.tunnelExit);
	}
	
	
}
