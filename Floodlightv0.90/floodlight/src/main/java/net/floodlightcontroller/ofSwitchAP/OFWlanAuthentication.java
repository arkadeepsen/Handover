package net.floodlightcontroller.ofSwitchAP;

import org.jboss.netty.buffer.ChannelBuffer;

public class OFWlanAuthentication extends OFWlanData {
	
	protected short auth_alg;

	public OFWlanAuthentication() {
		super();
	}

	public OFWlanAuthentication(int subtype) {
		super(subtype);
	}

	public OFWlanAuthentication(int subtype, byte[] staMacAddress,
			byte[] apMacAddress) {
		super(subtype, staMacAddress, apMacAddress);
	}

	public OFWlanAuthentication(int subtype, byte[] staMacAddress,
			byte[] apMacAddress, short auth_alg) {
		super(subtype, staMacAddress, apMacAddress);
		this.auth_alg = auth_alg;
	}

	public short getAuth_alg() {
		return auth_alg;
	}

	public void setAuth_alg(short auth_alg) {
		this.auth_alg = auth_alg;
	}

	@Override
	public int getLength() {
		return super.getLength() + 2;
	}

	@Override
	public void readFrom(ChannelBuffer data, int length) {
		super.readFrom(data, length);
		this.auth_alg = data.readShort();
	}

	@Override
	public void writeTo(ChannelBuffer data) {
		super.writeTo(data);
		data.writeShort(this.auth_alg);
	}
	
}
