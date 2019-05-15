package net.floodlightcontroller.ofSwitchAP;

import org.jboss.netty.buffer.ChannelBuffer;

public class OFNATMessage extends OFWlanHeader {
	

	protected byte[] staMacAddress;
	protected int sourceIP;
	protected int destinationIP;
	protected short sourcePort;
	protected short destinationPort;

	public OFNATMessage() {
		super();
	}

	public OFNATMessage(int subtype) {
		super(subtype);
	}

	public OFNATMessage(int subtype, byte[] staMacAddress, int sourceIP, int destinationIP, short sourcePort,
			short destinationPort) {
		super(subtype);
		this.staMacAddress = staMacAddress;
		this.sourceIP = sourceIP;
		this.destinationIP = destinationIP;
		this.sourcePort = sourcePort;
		this.destinationPort = destinationPort;
	}

	public byte[] getStaMacAddress() {
		return staMacAddress;
	}

	public void setStaMacAddress(byte[] staMacAddress) {
		this.staMacAddress = staMacAddress;
	}

	public int getSourceIP() {
		return sourceIP;
	}

	public void setSourceIP(int sourceIP) {
		this.sourceIP = sourceIP;
	}

	public int getDestinationIP() {
		return destinationIP;
	}

	public void setDestinationIP(int destinationIP) {
		this.destinationIP = destinationIP;
	}

	public short getSourcePort() {
		return sourcePort;
	}

	public void setSourcePort(short sourcePort) {
		this.sourcePort = sourcePort;
	}

	public short getDestinationPort() {
		return destinationPort;
	}

	public void setDestinationPort(short destinationPort) {
		this.destinationPort = destinationPort;
	}

	@Override
	public int getLength() {
		return super.getLength() + 20;
	}

	@Override
	public void readFrom(ChannelBuffer data, int length) {
		super.readFrom(data, length);
		this.staMacAddress = new byte[6];
		data.readBytes(staMacAddress);
		data.readerIndex(data.readerIndex() + 2); // pad
		this.sourceIP = data.readInt();
		this.destinationIP = data.readInt();
		this.sourcePort = data.readShort();
		this.destinationPort = data.readShort();
	}

	@Override
	public void writeTo(ChannelBuffer data) {
		super.writeTo(data);
		data.writeBytes(this.staMacAddress);
		data.writeShort((short) 0); // pad
		data.writeInt(this.sourceIP);
		data.writeInt(this.destinationIP);
		data.writeShort(this.sourcePort);
		data.writeShort(this.destinationPort);
	}

}
