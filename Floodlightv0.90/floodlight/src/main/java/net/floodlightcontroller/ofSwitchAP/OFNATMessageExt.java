package net.floodlightcontroller.ofSwitchAP;

import org.jboss.netty.buffer.ChannelBuffer;

public class OFNATMessageExt extends OFNATMessage {
	
	protected short newPort;

	public OFNATMessageExt() {
		super();
	}

	public OFNATMessageExt(int subtype) {
		super(subtype);
	}

	public OFNATMessageExt(int subtype, byte[] staMacAddress, int sourceIP, int destinationIP, short sourcePort,
			short destinationPort) {
		super(subtype, staMacAddress, sourceIP, destinationIP, sourcePort, destinationPort);
	}

	public OFNATMessageExt(int subtype, byte[] staMacAddress, int sourceIP, int destinationIP, short sourcePort,
			short destinationPort, short newPort) {
		super(subtype, staMacAddress, sourceIP, destinationIP, sourcePort, destinationPort);
		this.newPort = newPort;
	}

	public short getNewPort() {
		return newPort;
	}

	public void setNewPort(short newPort) {
		this.newPort = newPort;
	}

	@Override
	public int getLength() {
		return super.getLength() + 4;
	}

	@Override
	public void readFrom(ChannelBuffer data, int length) {
		super.readFrom(data, length);
		this.newPort = data.readShort();
		data.readerIndex(data.readerIndex() + 2); // pad
	}

	@Override
	public void writeTo(ChannelBuffer data) {
		super.writeTo(data);
		data.writeShort(this.newPort);
		data.writeShort((short) 0); // pad
	}


}
