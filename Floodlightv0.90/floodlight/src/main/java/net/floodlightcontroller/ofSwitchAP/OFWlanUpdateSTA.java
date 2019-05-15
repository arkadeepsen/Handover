package net.floodlightcontroller.ofSwitchAP;

import org.jboss.netty.buffer.ChannelBuffer;

public class OFWlanUpdateSTA extends OFWlanHeader {
	
	protected byte[] staMacAddress;
	protected short auth_alg;
	protected short aid;
	protected short capability;
    protected short listen_interval;
	protected byte[] supported_rates;
	protected int supported_rates_len;
	protected byte qosinfo;
	
	public OFWlanUpdateSTA() {
		super();
	}
	
	public OFWlanUpdateSTA(int subtype) {
		super(subtype);
	}

	public OFWlanUpdateSTA(int subtype, byte[] staMacAddress) {
		super(subtype);
		this.staMacAddress = staMacAddress;
	}

	public OFWlanUpdateSTA(int subtype, byte[] staMacAddress,short auth_alg,
			short aid, short capability,
			short listen_interval, byte[] supported_rates, int supported_rates_len,
			byte qosinfo) {
		super(subtype);
		this.staMacAddress = staMacAddress;
		this.auth_alg = auth_alg;
		this.aid = aid;
		this.capability = capability;
		this.listen_interval = listen_interval;
		this.supported_rates = supported_rates;
		this.supported_rates_len = supported_rates_len;
		this.qosinfo = qosinfo;
	}

	public byte[] getStaMacAddress() {
		return staMacAddress;
	}

	public void setStaMacAddress(byte[] staMacAddress) {
		this.staMacAddress = staMacAddress;
	}

	public short getAuth_alg() {
		return auth_alg;
	}

	public void setAuth_alg(short auth_alg) {
		this.auth_alg = auth_alg;
	}

	public short getAid() {
		return aid;
	}

	public void setAid(short aid) {
		this.aid = aid;
	}

	public short getCapability() {
		return capability;
	}

	public void setCapability(short capability) {
		this.capability = capability;
	}

	public short getListen_interval() {
		return listen_interval;
	}

	public void setListen_interval(short listen_interval) {
		this.listen_interval = listen_interval;
	}

	public byte[] getSupported_rates() {
		return supported_rates;
	}

	public void setSupported_rates(byte[] supported_rates) {
		this.supported_rates = supported_rates;
	}

	public int getSupported_rates_len() {
		return supported_rates_len;
	}

	public void setSupported_rates_len(int supported_rates_len) {
		this.supported_rates_len = supported_rates_len;
	}

	public byte getQosinfo() {
		return qosinfo;
	}

	public void setQosinfo(byte qosinfo) {
		this.qosinfo = qosinfo;
	}

	@Override
	public int getLength() {
		return super.getLength() + 56;
	}

	@Override
	public void readFrom(ChannelBuffer data, int length) {
		super.readFrom(data, length);
		data.readBytes(this.staMacAddress);
		this.auth_alg = data.readShort();
		this.aid = data.readShort();
		this.capability =  data.readShort();
		this.listen_interval =  data.readShort();
		data.readBytes(this.supported_rates);
		this.supported_rates_len = data.readInt();
		this.qosinfo = data.readByte();
	}

	@Override
	public void writeTo(ChannelBuffer data) {
		super.writeTo(data);
		data.writeBytes(this.staMacAddress);
		data.writeShort(this.auth_alg);
		data.writeShort(this.aid);
		data.writeShort(this.capability);
		data.writeShort(this.listen_interval);
		data.writeShort((short) 0);
		data.writeBytes(this.supported_rates);
		data.writeInt(this.supported_rates_len);
		data.writeByte(this.qosinfo);
		data.writeShort((short) 0);
		data.writeByte((byte) 0);
	}

	
}
