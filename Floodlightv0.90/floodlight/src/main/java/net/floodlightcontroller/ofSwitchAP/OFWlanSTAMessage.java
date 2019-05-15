package net.floodlightcontroller.ofSwitchAP;

import org.jboss.netty.buffer.ChannelBuffer;

public class OFWlanSTAMessage extends OFWlanHeader {
	
	protected byte[] staMacAddress;

	public OFWlanSTAMessage() {
		super();
	}

	public OFWlanSTAMessage(int subtype) {
		super(subtype);
	}

	public OFWlanSTAMessage(int subtype, byte[] staMacAddress) {
		super(subtype);
		this.staMacAddress = staMacAddress;
	}

	public byte[] getStaMacAddress() {
		return staMacAddress;
	}

	public void setStaMacAddress(byte[] staMacAddress) {
		this.staMacAddress = staMacAddress;
	}

	@Override
	public int getLength() {
		return super.getLength() + 6;
	}

	@Override
	public void readFrom(ChannelBuffer data, int length) {
		super.readFrom(data, length);
		this.staMacAddress = new byte[6];
		data.readBytes(staMacAddress);
	}

	@Override
	public void writeTo(ChannelBuffer data) {
		super.writeTo(data);
		data.writeBytes(staMacAddress);
	}

}
