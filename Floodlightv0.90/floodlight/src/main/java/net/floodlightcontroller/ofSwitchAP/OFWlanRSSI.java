package net.floodlightcontroller.ofSwitchAP;

import org.jboss.netty.buffer.ChannelBuffer;

public class OFWlanRSSI extends OFWlanHeader {
	
	protected byte[] staMacAddress;
	protected byte[] apMacAddress;
	protected byte rssi;
	
	public OFWlanRSSI() {
		super();
	}

	public OFWlanRSSI(int subtype) {
		super(subtype);
	}

	public OFWlanRSSI(int subtype, byte[] staMacAddress, byte[] apMacAddress, byte rssi) {
		super(subtype);
		this.staMacAddress = staMacAddress;
		this.apMacAddress = apMacAddress;
		this.rssi = rssi;
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

	public byte getRssi() {
		return rssi;
	}

	public void setRssi(byte rssi) {
		this.rssi = rssi;
	}

	@Override
	public int getLength() {
		return super.getLength() + 13;
	}

	@Override
	public void readFrom(ChannelBuffer data, int length) {
		super.readFrom(data, length);
		this.staMacAddress = new byte[6];
		data.readBytes(staMacAddress);
		this.apMacAddress = new byte[6];
		data.readBytes(apMacAddress);
		rssi = data.readByte();
	}

	@Override
	public void writeTo(ChannelBuffer data) {
		super.writeTo(data);
		data.writeBytes(staMacAddress);
		data.writeBytes(apMacAddress);
		data.writeByte(rssi);
	}


}
