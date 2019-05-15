package net.floodlightcontroller.ofSwitchAP;

import org.jboss.netty.buffer.ChannelBuffer;


public class OFWlanData extends OFWlanHeader {
	
	protected byte[] staMacAddress;
	protected byte[] apMacAddress;
	
	public OFWlanData() {
		super();
	}

	public OFWlanData(int subtype) {
		super(subtype);
	}

	public OFWlanData(int subtype, byte[] staMacAddress, byte[] apMacAddress) {
		super(subtype);
		this.staMacAddress = staMacAddress;
		this.apMacAddress = apMacAddress;
	}

	public byte[] getStaMacAddress() {
		return staMacAddress;
	}

	public void setStaMacAddress(byte[] staMacAddress) {
		this.staMacAddress = staMacAddress;
	}

	public byte[] getApMacAddress() {
		return apMacAddress;
	}

	public void setApMacAddress(byte[] apMacAddress) {
		this.apMacAddress = apMacAddress;
	}

	@Override
	public int getLength() {
		return super.getLength() + 12;
	}

	@Override
	public void readFrom(ChannelBuffer data, int length) {
		super.readFrom(data, length);
		this.staMacAddress = new byte[6];
		data.readBytes(staMacAddress);
		this.apMacAddress = new byte[6];
		data.readBytes(apMacAddress);
	}

	@Override
	public void writeTo(ChannelBuffer data) {
		super.writeTo(data);
		data.writeBytes(staMacAddress);
		data.writeBytes(apMacAddress);
	}

}
