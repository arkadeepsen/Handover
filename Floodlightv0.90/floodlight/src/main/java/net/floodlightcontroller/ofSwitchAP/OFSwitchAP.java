package net.floodlightcontroller.ofSwitchAP;

import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Map.Entry;
import java.util.concurrent.ConcurrentHashMap;

import org.openflow.protocol.OFFlowMod;
import org.openflow.protocol.OFMatch;
import org.openflow.protocol.OFMessage;
import org.openflow.protocol.OFPort;
import org.openflow.protocol.OFType;
import org.openflow.protocol.OFVendor;
import org.openflow.protocol.action.OFAction;
import org.openflow.protocol.action.OFActionOutput;
import org.openflow.util.HexString;

import net.floodlightcontroller.core.FloodlightContext;
import net.floodlightcontroller.core.IFloodlightProviderService;
import net.floodlightcontroller.core.IOFMessageListener;
import net.floodlightcontroller.core.IOFSwitch;
import net.floodlightcontroller.core.IOFSwitchListener;
import net.floodlightcontroller.core.module.FloodlightModuleContext;
import net.floodlightcontroller.core.module.FloodlightModuleException;
import net.floodlightcontroller.core.module.IFloodlightModule;
import net.floodlightcontroller.core.module.IFloodlightService;
import net.floodlightcontroller.packet.IPv4;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class OFSwitchAP implements IOFMessageListener, IOFSwitchListener,
		IFloodlightModule {
	
	protected IFloodlightProviderService floodlightProvider;
	protected static Logger logger;
	
	protected Map<STAAPPair, STAAPInfo> STAAPList;
	protected Map<String, APInfo> STAAPRSSIList;
	protected Map<String, ArrayList<NATMap>> natTable;
	protected IOFSwitch gwSwitch;
	protected Map<PortPair, Short> portMap;
	
    // flow-mod - for use in the cookie
    public static final int OF_SWITCH_AP_APP_ID = 99;
    // LOOK! This should probably go in some class that encapsulates
    // the app cookie management
    public static final int APP_ID_BITS = 12;
    public static final int APP_ID_SHIFT = (64 - APP_ID_BITS);
    public static final long OF_SWITCH_AP_COOKIE = (long) (OF_SWITCH_AP_APP_ID & ((1 << APP_ID_BITS) - 1)) << APP_ID_SHIFT;
    
	@Override
	public String getName() {
		return OFSwitchAP.class.getSimpleName();
	}

	@Override
	public boolean isCallbackOrderingPrereq(OFType type, String name) {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public boolean isCallbackOrderingPostreq(OFType type, String name) {
		// TODO Auto-generated method stub
		return false;
	}

	@Override
	public Collection<Class<? extends IFloodlightService>> getModuleServices() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Map<Class<? extends IFloodlightService>, IFloodlightService> getServiceImpls() {
		// TODO Auto-generated method stub
		return null;
	}

	@Override
	public Collection<Class<? extends IFloodlightService>> getModuleDependencies() {
		Collection<Class<? extends IFloodlightService>> l =
				new ArrayList<Class<? extends IFloodlightService>>();
		l.add(IFloodlightProviderService.class);
		return l;
	}

	@Override
	public void init(FloodlightModuleContext context)
			throws FloodlightModuleException {
		floodlightProvider = context.getServiceImpl(IFloodlightProviderService.class);
		logger = LoggerFactory.getLogger(OFSwitchAP.class);
		STAAPList = new ConcurrentHashMap<OFSwitchAP.STAAPPair, OFSwitchAP.STAAPInfo>();
		STAAPRSSIList = new ConcurrentHashMap<String, OFSwitchAP.APInfo>();
		natTable = new ConcurrentHashMap<String, ArrayList<NATMap>>();
		portMap = new ConcurrentHashMap<PortPair, Short>();
	}

	@Override
	public void startUp(FloodlightModuleContext context) {
		floodlightProvider.addOFMessageListener(OFType.VENDOR, this);
		// Register for switch updates
        floodlightProvider.addOFSwitchListener(this);
	}

	@Override
	public void addedSwitch(IOFSwitch sw) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void removedSwitch(IOFSwitch sw) {
		if(sw != gwSwitch)
		{
			for(Iterator<Entry<STAAPPair, STAAPInfo>> iter= STAAPList.entrySet().iterator();
					iter.hasNext();)
			{
				STAAPInfo staAP = iter.next().getValue();
				if(staAP.sw == sw)
				{
					iter.remove();
				}
			}
			for(Iterator<Entry<String, APInfo>> iter= STAAPRSSIList.entrySet().iterator();
					iter.hasNext();)
			{
				APInfo apInfo = iter.next().getValue();
				if(apInfo.sw == sw)
				{
					iter.remove();
				}
			}
		}
		else
		{
			gwSwitch = null;
		}
	}

	@Override
	public void switchPortChanged(Long switchId) {
		// TODO Auto-generated method stub

	}

	@Override
	public net.floodlightcontroller.core.IListener.Command receive(
			IOFSwitch sw, OFMessage msg, FloodlightContext cntx) {
		if(msg instanceof OFVendor)
		{
			OFVendor vendorMsg = (OFVendor)msg;
			if(vendorMsg.getVendor() == OFWlanHeader.AP_EXPERIMENTER_ID)
			{
				OFWlanHeader wlanData = (OFWlanHeader)vendorMsg.getVendorData();
				int subType = wlanData.getSubtype();
				switch (subType) {
				case OFWlanProbe.WLAN_PROBE:
					logger.debug("Received Probe Request Message");
					OFWlanProbe wlanProbe = (OFWlanProbe) wlanData;
					handleProbeReq(sw, wlanProbe);
					break;
				case OFWlanAuthRequest.WLAN_AUTH_REQ:
					logger.info("Received Authentication Request Message");
					OFWlanAuthRequest wlanAuthReq = (OFWlanAuthRequest) wlanData;
					handleAuthReq(sw, wlanAuthReq);
					break;
				case OFWlanAssocRequest.WLAN_ASSOC_REQ:
					logger.info("Received Association Request Message");
					OFWlanAssocRequest wlanAssocReq = (OFWlanAssocRequest) wlanData;
					handleAssocReq(sw, wlanAssocReq);
					break;
				case OFWlanReassociationRequest.WLAN_REASSOC_REQ:
					logger.info("Received Reassociation Request Message");
					OFWlanReassociationRequest wlanReassocReq = 
							(OFWlanReassociationRequest) wlanData;
					handleReassocReq(sw, wlanReassocReq);
					break;
				case OFWlanDisassocRequest.WLAN_DISASSOC_REQ:
					logger.info("Received Disassociation Request Message");
					OFWlanDisassocRequest wlanDisassocReq = 
							(OFWlanDisassocRequest) wlanData;
					handleDisassocReq(sw, wlanDisassocReq);
					break;
				case OFWlanDeauthRequest.WLAN_DEAUTH_REQ:
					logger.info("Received Deauthentication Request Message");
					OFWlanDeauthRequest wlanDeauthReq = (OFWlanDeauthRequest) wlanData;
					handleDeauthReq(sw, wlanDeauthReq);
					break;
				case OFWlanAddSTA.WLAN_ADD_STA:
				case OFWlanRemoveSTA.WLAN_REMOVE_STA:
				case OFWlanUpdateSTAAdd.WLAN_UPDATE_STA_ADD:
				case OFWlanLastRSSI.WLAN_LAST_REC_POW:
				case OFNATAdd.WLAN_NAT_ADD:
					break;
				case OFWlanDataRSSI.WLAN_REC_POW:
					logger.info("Received Data RSSI Message");
					OFWlanDataRSSI wlanLastRSSI = (OFWlanDataRSSI) wlanData;
					handleLastRSSI(sw, wlanLastRSSI);
					break;
				case OFWlanAPRSSI.WLAN_AP_REC_POW:
					logger.info("Received AP RSSI Message");
					OFWlanAPRSSI wlanAPRSSI = (OFWlanAPRSSI) wlanData;
					handleAPRSSI(sw, wlanAPRSSI);
					break;
				case OFNATUpdate.WLAN_NAT_UPDATE:
					logger.info("Received NAT Update Message");
					OFNATUpdate natUpdate = (OFNATUpdate) wlanData;
					handleNATUpdate(sw, natUpdate);
					break;
				case OFPortTranslation.WLAN_PORT_TRANSLATION:
					logger.info("Received Port Translation Message");
					OFPortTranslation portTranslation = (OFPortTranslation) wlanData;
					handlePortTranslation(sw, portTranslation);
					break;
				case GWConnect.GW_CONNECT:
					logger.info("Received GW Connect Message");
					gwSwitch = sw;
					break;
				default:
					logger.warn("Unhandled WLAN message; " +
							"sub type = {}", subType);
					break;
				}
				return Command.STOP;
			}
		}
		return Command.CONTINUE;
	}

	private synchronized void handleProbeReq(IOFSwitch sw, OFWlanProbe wlanProbe)
	{
		String staAddress = HexString.toHexString(wlanProbe.staMacAddress);
		String apAddress = HexString.toHexString(wlanProbe.apMacAddress);
		if(findSTAAPPair(staAddress, apAddress))
		{
	        logger.debug("STA: {} AP: {}", staAddress, apAddress);
	        logger.debug("STA probe request already received");
	        return;
		}
		
	    STAAPInfo staAPOld = findSTAAPInfo(staAddress);
	    if(staAPOld != null)
	    {
	        logger.debug("STA: {} AP: {}", staAddress, apAddress);
	        logger.debug("STA already associated");
	        return;
	    }
	    // create STA entry if needed
	    STAAPInfo staAP = lookupSenderSTAAP(staAddress, apAddress);
	    boolean first = false;
	    if (staAP == null)
	    {
	    	first = true;
	    	STAAPPair apStaPair = new STAAPPair(staAddress, apAddress);
	    	staAP = new STAAPInfo(staAddress, apAddress, null, false, sw);
	    	STAAPList.put(apStaPair, staAP);
	    }
	    if(!first && staAP.status == STAStatus.NOT_AUTHENTICATED)
	    {
	    	first = true;
	    }
	    
	    if(first)
	    {
		    staAP.status = STAStatus.SELECTED;
		    staAP.isHomeAP = true;
	    	OFWlanAddSTA addSTAData =  new OFWlanAddSTA();
	    	addSTAData.staMacAddress = wlanProbe.staMacAddress;
	    	OFVendor addSTA = (OFVendor) floodlightProvider.getOFMessageFactory()
	    			.getMessage(OFType.VENDOR);
	    	addSTA.setVendor(OFWlanHeader.AP_EXPERIMENTER_ID);
	    	addSTA.setVendorData(addSTAData);
	    	addSTA.setLengthU(OFVendor.MINIMUM_LENGTH + 
	    			addSTAData.getLength());
	    	try {
	    		sw.write(addSTA, null);
	    		logger.debug("STA: {} AP: {}", staAddress, apAddress);
	    		logger.debug("Adding STA to AP based on the probe request received");
	    	} catch (IOException e) {
	    		logger.error("Failed to write {} to switch {}", new Object[]{ addSTA, sw }, e);
	    	}
	    }
	}
	
	private synchronized void handleAuthReq(IOFSwitch sw, OFWlanAuthRequest wlanAuthReq)
	{
		String staAddress = HexString.toHexString(wlanAuthReq.staMacAddress);
		String apAddress = HexString.toHexString(wlanAuthReq.apMacAddress);
		STAAPInfo staAP = lookupSenderSTAAP(staAddress, apAddress);
		if (staAP == null)
	    {
			logger.info("STA, AP pair does not exist");
	    	return;
	    }
		staAP.status = STAStatus.AUTHENTICATED;
		staAP.auth_alg = wlanAuthReq.auth_alg;
		logger.info("STA: {} AP: {}", staAddress, apAddress);
        logger.info("Changing state of STA to AUTHENTICATED. Auth algo = {}", 
        		staAP.auth_alg);
	}
	
	private synchronized void handleAssocReq(IOFSwitch sw, 
			OFWlanAssocRequest wlanAssocReq)
	{
		String staAddress = HexString.toHexString(wlanAssocReq.staMacAddress);
		String apAddress = HexString.toHexString(wlanAssocReq.apMacAddress);
		STAAPInfo staAP = lookupSenderSTAAP(staAddress, apAddress);
		if (staAP == null)
	    {
			logger.info("STA, AP pair does not exist");
	    	return;
	    }
		staAP.status = STAStatus.ASSOCIATED;
		staAP.aid = wlanAssocReq.aid;
		staAP.capability = wlanAssocReq.capability;
		staAP.listen_interval = wlanAssocReq.listen_interval;
		staAP.supported_rates_len = wlanAssocReq.supported_rates_len;
		staAP.supported_rates = wlanAssocReq.supported_rates;
		logger.info("STA: {} AP: {}", staAddress, apAddress);
        logger.info("Changing state of STA to ASSOCIATED");
	}
	
	private synchronized void handleReassocReq(IOFSwitch sw, 
			OFWlanReassociationRequest wlanReassocReq)
	{
		String staAddress = HexString.toHexString(wlanReassocReq.staMacAddress);
		String apAddress = HexString.toHexString(wlanReassocReq.apMacAddress);
		STAAPInfo staAP = lookupSenderSTAAP(staAddress, apAddress);
		if (staAP == null)
	    {
			logger.info("STA, AP pair does not exist");
	    	return;
	    }
		staAP.status = STAStatus.ASSOCIATED;
		logger.info("STA: {} AP: {}", staAddress, apAddress);
        logger.info("Changing state of STA to ASSOCIATED");
	}
	
	private synchronized void handleDisassocReq(IOFSwitch sw, 
			OFWlanDisassocRequest wlanDisassocReq)
	{
		String staAddress = HexString.toHexString(wlanDisassocReq.staMacAddress);
		String apAddress = HexString.toHexString(wlanDisassocReq.apMacAddress);
		STAAPInfo staAP = lookupSenderSTAAP(staAddress, apAddress);
		if (staAP == null)
	    {
			logger.info("STA, AP pair does not exist");
	    	return;
	    }
		staAP.status = STAStatus.AUTHENTICATED;
		logger.info("STA: {} AP: {}", staAddress, apAddress);
        logger.info("Changing state of STA to AUTHENTICATED");
        
        STAAPRSSIList.remove(staAddress);
        
//        STAAPInfo staAPHome = findHomeAP(staAddress);
//        
//        //Delete tunnel from Home AP to current AP
//        if(staAP != staAPHome)
//        {
//        	OFMatch match = new OFMatch();
//        	match.setDataLayerDestination(staAddress);
//        	match.setWildcards(OFMatch.OFPFW_ALL & ~OFMatch.OFPFW_DL_DST);
//
//    		logger.debug("STA: {} AP: {}", staAddress, staAPHome.apMACAddress);
//    		logger.debug("Delete tunnel from Home AP to current AP");
//    		
//        	sendFlowModMessage(false, OFFlowMod.OFPFC_DELETE, match, 
//        			OFPort.OFPP_NONE, null, staAPHome.sw);
//        }
	}
	
	private synchronized void handleDeauthReq(IOFSwitch sw, 
			OFWlanDeauthRequest wlanDeauthReq)
	{
		String staAddress = HexString.toHexString(wlanDeauthReq.staMacAddress);
		String apAddress = HexString.toHexString(wlanDeauthReq.apMacAddress);
		STAAPInfo staAP = lookupSenderSTAAP(staAddress, apAddress);
		if (staAP == null)
	    {
			logger.info("STA, AP pair does not exist");
	    	return;
	    }
		staAP.status = STAStatus.NOT_AUTHENTICATED;
		logger.info("STA: {} AP: {}", staAddress, apAddress);
        logger.info("Changing state of STA to NOT AUTHENTICATED");
	}
	
	private synchronized void handleLastRSSI(IOFSwitch sw, 
			OFWlanDataRSSI wlanLastRSSI) 
	{
		String staAddress = HexString.toHexString(wlanLastRSSI.staMacAddress);
		String apAddress = HexString.toHexString(wlanLastRSSI.apMacAddress);
		byte rssi = wlanLastRSSI.rssi;
		logger.info("STA: {} AP: {}", staAddress, apAddress);
        logger.info("RSSI: {}", wlanLastRSSI.rssi);
        
        APInfo ap = STAAPRSSIList.get(staAddress);
        
        if(ap == null)
        {
        	ap = new APInfo(apAddress, rssi, false, sw);
        	STAAPRSSIList.put(staAddress, ap);
        }
        
        if(ap.rssi < rssi)
        {
        	ap.apMACAddress = apAddress;
        	ap.rssi = rssi;
        	ap.sw = sw;
        }
        
        if(!ap.isSent)
        {
        	STAAPInfo staAP = findSTAAPInfo(staAddress);
        	if(staAP != null)
        	{
    	    	OFWlanLastRSSI apRSSIData =  new OFWlanLastRSSI();
    	    	apRSSIData.staMacAddress = wlanLastRSSI.staMacAddress;
    	    	OFVendor apRSSI = (OFVendor) floodlightProvider.getOFMessageFactory()
    	    			.getMessage(OFType.VENDOR);
    	    	apRSSI.setVendor(OFWlanHeader.AP_EXPERIMENTER_ID);
    	    	apRSSI.setVendorData(apRSSIData);
    	    	apRSSI.setLengthU(OFVendor.MINIMUM_LENGTH + 
    	    			apRSSIData.getLength());
    	    	try {
    	    		staAP.sw.write(apRSSI, null);
    	    		logger.info("STA: {} AP: {}", staAP.staMacAddress, staAP.apMACAddress);
    	    		logger.info("Requsting received signal strength of STA from AP");
    	            ap.isSent = true;
    	    	} catch (IOException e) {
    	    		logger.error("Failed to write {} to switch {}", new Object[]{ apRSSI, staAP.sw }, e);
    	    	}
        	}
        }
	}
	
	private synchronized void handleAPRSSI(IOFSwitch sw, 
			OFWlanAPRSSI wlanAPRSSI) 
	{
		String staAddress = HexString.toHexString(wlanAPRSSI.staMacAddress);
		String apAddress = HexString.toHexString(wlanAPRSSI.apMacAddress);
		byte rssi = wlanAPRSSI.rssi;
		logger.info("STA: {} AP: {}", staAddress, apAddress);
        logger.info("AP RSSI: {}", rssi);
        
        APInfo ap = STAAPRSSIList.get(staAddress);
        if(ap == null)
        {
        	logger.info("No alternate AP entry available");
        	return;
        }
        ap.isSent = false;
        
        STAAPInfo staAP = lookupSenderSTAAP(staAddress, apAddress);
        
        if(staAP != null && ap.rssi > rssi)
        {
        	logger.info("New AP has better RSSI");
            
            STAAPInfo staAPNew = lookupSenderSTAAP(staAddress, ap.apMACAddress);
            if (staAPNew == null)
    	    {
    	    	STAAPPair apStaPair = new STAAPPair(staAddress, ap.apMACAddress);
    	    	staAPNew = new STAAPInfo(staAddress, ap.apMACAddress, null, false, ap.sw);
    	    	STAAPList.put(apStaPair, staAPNew);
    	    }
	    	staAPNew.auth_alg = staAP.auth_alg;
	    	staAPNew.aid = staAP.aid;
	    	staAPNew.capability = staAP.capability;
	    	staAPNew.listen_interval = staAP.listen_interval;
	    	staAPNew.supported_rates_len = staAP.supported_rates_len;
	    	staAPNew.supported_rates = staAP.supported_rates;
            ap.apMACAddress = apAddress;
            ap.rssi = (byte) -128;
            ap.sw = sw;
            
            staAP.status = STAStatus.NOT_AUTHENTICATED;
            staAPNew.status = STAStatus.ASSOCIATED;
            
//            STAAPInfo staAPHome = findHomeAP(staAddress);
            
            //Delete tunnel from Home AP to current AP
//            if(staAP != staAPHome)
//            {
//            	OFMatch match = new OFMatch();
//            	match.setDataLayerDestination(staAddress);
//            	match.setWildcards(OFMatch.OFPFW_ALL & ~OFMatch.OFPFW_DL_DST);
//
//        		logger.debug("STA: {} AP: {}", staAddress, staAPHome.apMACAddress);
//        		logger.debug("Delete tunnel from Home AP to current AP");
//        		
//            	sendFlowModMessage(false, OFFlowMod.OFPFC_DELETE, match, 
//            			OFPort.OFPP_NONE, null, staAPHome.sw);
//            }
            
            //Create tunnel from Home AP to New AP
//            if(staAPNew != staAPHome)
//            {
//            	OFMatch match = new OFMatch();
//            	match.setDataLayerDestination(staAddress);
//            	match.setWildcards(OFMatch.OFPFW_ALL & ~OFMatch.OFPFW_DL_DST);
//
//            	String homeIPAddress = staAPHome.sw.getChannel().getRemoteAddress().toString()
//            			.substring(1, staAPHome.sw.getChannel().getRemoteAddress().toString()
//            					.indexOf(':'));
//            	String newIPAddress = staAPNew.sw.getChannel().getRemoteAddress().toString()
//            			.substring(1, staAPNew.sw.getChannel().getRemoteAddress().toString()
//            					.indexOf(':'));
//            	
//            	int tunnelEntry = IPv4.toIPv4Address(homeIPAddress);
//            	int tunnelExit = IPv4.toIPv4Address(newIPAddress);
//            	
//            	OFActionPushIPH pushIPH = new OFActionPushIPH(tunnelEntry, tunnelExit);
//            	
//        		logger.debug("STA: {} AP: {}", staAddress, staAPHome.apMACAddress);
//        		logger.debug("Create tunnel from Home AP to new AP");
//        		
//            	sendFlowModMessage(true, OFFlowMod.OFPFC_ADD, match, null,
//            			Arrays.asList((OFAction)pushIPH), staAPHome.sw);
//            }
            
            ArrayList<NATMap> entries = natTable.get(staAddress);
            if(entries != null)
            {
            	for (NATMap entry : entries) 
            	{
            		OFNATAdd natAdd = new OFNATAdd();
            		natAdd.staMacAddress = wlanAPRSSI.staMacAddress;
            		natAdd.sourceIP = entry.sourceIP;
            		natAdd.destinationIP = entry.destinationIP;
            		natAdd.sourcePort = entry.sourcePort;
            		natAdd.destinationPort = entry.destinationPort;
            		natAdd.newPort = entry.newPort;
            		OFVendor natAddMessage = (OFVendor) floodlightProvider.getOFMessageFactory()
            				.getMessage(OFType.VENDOR);
            		natAddMessage.setVendor(OFWlanHeader.AP_EXPERIMENTER_ID);
            		natAddMessage.setVendorData(natAdd);
            		natAddMessage.setLengthU(OFVendor.MINIMUM_LENGTH +
            				natAdd.getLength());
            		try {
            			staAPNew.sw.write(natAddMessage, null);
            			logger.debug("STA: {} AP: {}", staAddress, apAddress);
            			logger.debug("Adding NAT entries for the STA to the new AP");
            		} catch (IOException e) {
            			logger.error("Failed to write {} to switch {}", 
            					new Object[]{ natAddMessage, staAPNew.sw }, e);
            		}
            	}
            }
            
	        OFWlanRemoveSTA removeSTAData =  new OFWlanRemoveSTA();
	        removeSTAData.staMacAddress = wlanAPRSSI.staMacAddress;
	        OFVendor removeSTA = (OFVendor) floodlightProvider.getOFMessageFactory()
	                .getMessage(OFType.VENDOR);
	        removeSTA.setVendor(OFWlanHeader.AP_EXPERIMENTER_ID);
	        removeSTA.setVendorData(removeSTAData);
            removeSTA.setLengthU(OFVendor.MINIMUM_LENGTH + 
            		removeSTAData.getLength());
            try {
            	sw.write(removeSTA, null);
            	logger.debug("STA: {} AP: {}", staAddress, apAddress);
    	        logger.debug("Removing STA as it has moved towards another AP");
            } catch (IOException e) {
                logger.error("Failed to write {} to switch {}", 
                		new Object[]{ removeSTA, sw }, e);
                return;
            }
            
	        OFWlanUpdateSTAAdd updateSTAAddData =  new OFWlanUpdateSTAAdd(
	        		wlanAPRSSI.staMacAddress, staAP.auth_alg, staAP.aid, staAP.capability,
	        		staAP.listen_interval, staAP.supported_rates,
	        		staAP.supported_rates_len, staAP.qosinfo);
	        OFVendor updateSTAAdd = (OFVendor) floodlightProvider.getOFMessageFactory()
	                .getMessage(OFType.VENDOR);
	        updateSTAAdd.setVendor(OFWlanHeader.AP_EXPERIMENTER_ID);
	        updateSTAAdd.setVendorData(updateSTAAddData);
            updateSTAAdd.setLengthU(OFVendor.MINIMUM_LENGTH + 
            		updateSTAAddData.getLength());
            try {
            	staAPNew.sw.write(updateSTAAdd, null);
            	logger.debug("STA: {} AP: {}", staAddress, apAddress);
    	        logger.debug("Adding STA to the AP in ASSOCIATED state");
            } catch (IOException e) {
                logger.error("Failed to write {} to switch {}", 
                		new Object[]{ updateSTAAdd, staAPNew.sw }, e);
            }
            
            if(entries != null && gwSwitch != null)
            {
            	for (NATMap entry : entries) 
            	{
            		OFNATAdd natAdd = new OFNATAdd();
            		natAdd.staMacAddress = wlanAPRSSI.staMacAddress;
            		String newIPAddress = staAPNew.sw.getChannel().getRemoteAddress().toString()
            				.substring(1, staAPNew.sw.getChannel().getRemoteAddress().toString()
            						.indexOf(':'));
            		natAdd.sourceIP = IPv4.toIPv4Address(newIPAddress);
            		natAdd.destinationIP = entry.destinationIP;
            		natAdd.sourcePort = entry.sourcePort;
            		natAdd.destinationPort = entry.destinationPort;
            		natAdd.newPort = entry.newPort;
            		OFVendor natAddMessage = (OFVendor) floodlightProvider.getOFMessageFactory()
            				.getMessage(OFType.VENDOR);
            		natAddMessage.setVendor(OFWlanHeader.AP_EXPERIMENTER_ID);
            		natAddMessage.setVendorData(natAdd);
            		natAddMessage.setLengthU(OFVendor.MINIMUM_LENGTH +
            				natAdd.getLength());
            		try {
            			gwSwitch.write(natAddMessage, null);
            			logger.debug("STA: {}", staAddress);
            			logger.debug("Updating NAT entries at the Gateway Router for STA");
            		} catch (IOException e) {
            			logger.error("Failed to write {} to switch {}", 
            					new Object[]{ natAddMessage, gwSwitch }, e);
            		}
            	}
            }
        }
	}
	
	private synchronized void handleNATUpdate(IOFSwitch sw, OFNATUpdate natUpdate)
	{
		String staAddress = HexString.toHexString(natUpdate.staMacAddress);
		logger.debug("STA: {}", staAddress);
		logger.debug("Src IP {} Src Port {}",
				IPv4.fromIPv4Address(natUpdate.sourceIP), natUpdate.sourcePort);
		logger.debug("Dst IP {} Dst Port {}",
				IPv4.fromIPv4Address(natUpdate.destinationIP), natUpdate.destinationPort);
		ArrayList<NATMap> entries = natTable.get(staAddress);
		if(entries == null)
		{
			entries = new ArrayList<NATMap>();
			natTable.put(staAddress, entries);
		}
		NATMap map = new NATMap(natUpdate.sourceIP, natUpdate.destinationIP, 
				natUpdate.sourcePort, natUpdate.destinationPort, natUpdate.sourcePort);
		entries.add(map);
	}

	private void handlePortTranslation(IOFSwitch sw, OFPortTranslation portTranslation)
	{
		String staAddress = HexString.toHexString(portTranslation.staMacAddress);
		ArrayList<NATMap> entries = natTable.get(staAddress);
        if(entries != null)
        {
        	for (NATMap entry : entries) 
        	{
        		if(entry.sourceIP == portTranslation.sourceIP
        				&& entry.sourcePort == portTranslation.sourcePort
        				&& entry.destinationIP == portTranslation.destinationIP
        				&& entry.destinationPort == portTranslation.destinationPort)
        		{
        			OFNATAdd natAdd = new OFNATAdd();
        			natAdd.staMacAddress = portTranslation.staMacAddress;
        			natAdd.sourceIP = entry.sourceIP;
        			natAdd.destinationIP = entry.destinationIP;
        			natAdd.sourcePort = entry.sourcePort;
        			natAdd.destinationPort = entry.destinationPort;
        			natAdd.newPort = entry.newPort;
        			OFVendor natAddMessage = (OFVendor) floodlightProvider.getOFMessageFactory()
        					.getMessage(OFType.VENDOR);
        			natAddMessage.setVendor(OFWlanHeader.AP_EXPERIMENTER_ID);
        			natAddMessage.setVendorData(natAdd);
        			natAddMessage.setLengthU(OFVendor.MINIMUM_LENGTH +
        					natAdd.getLength());
        			try {
        				sw.write(natAddMessage, null);
        				logger.debug("STA: {}", staAddress);
//        				logger.debug("Dest. Addr.: {} Dest. Port: {} ", entry.destinationIP, entry.destinationPort);
//        				logger.debug("New Port: {}", entry.newPort);
        				logger.debug("Adding NAT entries for the STA to the new AP");
        			} catch (IOException e) {
        				logger.error("Failed to write {} to switch {}", 
        						new Object[]{ natAddMessage, sw }, e);
        			}
        			return;
        		}
        	}
        }
        else
        {
			entries = new ArrayList<NATMap>();
			natTable.put(staAddress, entries);
		}
        boolean hasKey = portMap.containsKey(new PortPair(portTranslation.destinationIP,
        		portTranslation.destinationPort)); 
        short newPort;
        if(hasKey)
        {
        	Short ret =  portMap.get(new PortPair(portTranslation.destinationIP,
            		portTranslation.destinationPort));
        	newPort = ret.shortValue();
        }
        else
        	newPort = (short)49151;
        newPort++;
        portMap.put(new PortPair(portTranslation.destinationIP, portTranslation.destinationPort), newPort);
		NATMap map = new NATMap(portTranslation.sourceIP, portTranslation.destinationIP, 
				portTranslation.sourcePort, portTranslation.destinationPort, newPort);
		entries.add(map);

		OFNATAdd natAdd = new OFNATAdd();
		natAdd.staMacAddress = portTranslation.staMacAddress;
		natAdd.sourceIP = map.sourceIP;
		natAdd.destinationIP = map.destinationIP;
		natAdd.sourcePort = map.sourcePort;
		natAdd.destinationPort = map.destinationPort;
		natAdd.newPort = map.newPort;
		OFVendor natAddMessage = (OFVendor) floodlightProvider.getOFMessageFactory()
				.getMessage(OFType.VENDOR);
		natAddMessage.setVendor(OFWlanHeader.AP_EXPERIMENTER_ID);
		natAddMessage.setVendorData(natAdd);
		natAddMessage.setLengthU(OFVendor.MINIMUM_LENGTH +
				natAdd.getLength());
		try {
			sw.write(natAddMessage, null);
			logger.debug("STA: {}", staAddress);
//			logger.debug("Dest. Addr.: {} Dest. Port: {} ", map.destinationIP, map.destinationPort);
//			logger.debug("New Port: {}", map.newPort);
			logger.debug("Adding NAT entries for the STA to the new AP");
		} catch (IOException e) {
			logger.error("Failed to write {} to switch {}", 
					new Object[]{ natAddMessage, sw }, e);
		}
	}
	
	private void sendFlowModMessage(boolean isVendor, short command, 
			OFMatch match, OFPort outPort, List<OFAction> actions, IOFSwitch sw)
	{
        OFFlowMod flowMod = (OFFlowMod) floodlightProvider.getOFMessageFactory().getMessage(OFType.FLOW_MOD);
        flowMod.setMatch(match);
        flowMod.setCookie(OF_SWITCH_AP_COOKIE);
        flowMod.setCommand(command);
        flowMod.setIdleTimeout((short) 0);
        flowMod.setHardTimeout((short) 0);
        flowMod.setPriority((short) 0);
        flowMod.setBufferId(-1);
        flowMod.setOutPort((command == OFFlowMod.OFPFC_DELETE) ? outPort.getValue() : OFPort.OFPP_NONE.getValue());
        flowMod.setFlags((command == OFFlowMod.OFPFC_DELETE) ? 0 : (short) (1 << 0)); // OFPFF_SEND_FLOW_REM
        
        if(!isVendor)
        {
        	if(command == OFFlowMod.OFPFC_DELETE)
        	{
        		flowMod.setLength((short) (OFFlowMod.MINIMUM_LENGTH));
        	}
        	else
        	{
        		flowMod.setActions(Arrays.asList((OFAction) new OFActionOutput(
        				outPort.getValue(), (short) 0xffff)));
        		flowMod.setLength((short) (OFFlowMod.MINIMUM_LENGTH + OFActionOutput.MINIMUM_LENGTH));
        	}
        }
        else
        {
            flowMod.setActions(actions);
            int length = 0;
            for (OFAction action : actions)
            {
            	length += action.getLengthU();
            }
            flowMod.setLength((short) (OFFlowMod.MINIMUM_LENGTH + length));
        }
        try {
    		sw.write(flowMod, null);
    	} catch (IOException e) {
    		logger.error("Failed to write {} to switch {}", new Object[]{ flowMod, sw }, e);
    	}
	}

	private boolean findSTAAPPair(String staAddress, String apAddress)
	{
		for(Entry<STAAPPair, STAAPInfo> entry: STAAPList.entrySet())
		{
			String staAdd = entry.getKey().staMACAddress;
	        if(staAdd.equals(staAddress))
	        {
				String apAdd = entry.getKey().apMACAddress;
	            STAAPInfo staAp = entry.getValue();
				if(!apAdd.equals(apAddress) && staAp.status == STAStatus.SELECTED)
				{
					return true;
				}
	        }
	    }
	    return false;
	}
	
	private STAAPInfo lookupSenderSTAAP(String staAddress, String apAddress)
	{
	   return STAAPList.get(new STAAPPair(staAddress, apAddress));
	}
	
	private STAAPInfo findSTAAPInfo(String staAddress)
	{
		for(Entry<STAAPPair, STAAPInfo> entry: STAAPList.entrySet())
		{
			String staAdd = entry.getKey().staMACAddress;
	        if(staAdd.equals(staAddress))
	        {
	            STAAPInfo staAp = entry.getValue();
	            if(staAp.status == STAStatus.ASSOCIATED)
	                return staAp;
	        }
	    }
	    return null;
	}
	
	private STAAPInfo findHomeAP(String staAddress)
	{
		for(Entry<STAAPPair, STAAPInfo> entry: STAAPList.entrySet())
		{
			String staAdd = entry.getKey().staMACAddress;
	        if(staAdd.equals(staAddress))
	        {
	            STAAPInfo staAp = entry.getValue();
	            if(staAp.isHomeAP == true)
	                return staAp;
	        }
	    }
	    return null;
	}
	
	private enum STAStatus {SELECTED, NOT_AUTHENTICATED, AUTHENTICATED, ASSOCIATED};
	
	private class STAAPPair
	{
		private String staMACAddress; 
		private String apMACAddress;
		
		public STAAPPair(String staMACAddress, String apMACAddress) {
			this.staMACAddress = staMACAddress;
			this.apMACAddress = apMACAddress;
		}

		@Override
		public boolean equals(Object obj) 
		{
			if(obj instanceof STAAPPair)
			{
				STAAPPair pair = (STAAPPair)obj;
				if(pair.staMACAddress.equals(this.staMACAddress) &&
						pair.apMACAddress.equals(apMACAddress))
				{
					return true;
				}
			}
			logger.info("STAAPPair equals");
			return false;
		}

		@Override
	    public int hashCode()
	    {
	        int hashcode;
	        hashcode = (int) (HexString.toLong(apMACAddress) ^ HexString.toLong(staMACAddress)); 
	        return hashcode;
	    }
	}
	
	private class STAAPInfo
	{
		private String staMacAddress;
		private String apMACAddress;
		private STAStatus status;
		private boolean isHomeAP;
		private IOFSwitch sw;
		
		private short auth_alg;
		
		private short aid;
		private short capability;
		private short listen_interval;
		private byte[] supported_rates;
		private int supported_rates_len;
		private byte qosinfo;
		
		public STAAPInfo(String staMACAddress, String apMACAddress, STAStatus status,
				boolean isHomeAP, IOFSwitch sw)
		{
			this.staMacAddress = staMACAddress;
			this.apMACAddress = apMACAddress;
			this.status = status;
			this.isHomeAP = isHomeAP;
			this.sw = sw;
		}
	}
	
	private class APInfo
	{
		private String apMACAddress;
		private byte rssi;
		private boolean isSent;
		private IOFSwitch sw;

		public APInfo(String apMACAddress, byte rssi, boolean isSent, IOFSwitch sw) 
		{
			this.apMACAddress = apMACAddress;
			this.rssi = rssi;
			this.isSent = isSent;
			this.sw = sw;
		}
	}
	
	private class NATMap
	{
		private int sourceIP;
		private int destinationIP;
		private short sourcePort;
		private short destinationPort;
		private short newPort;

		public NATMap(int sourceIP, int destinationIP, short sourcePort, short destinationPort) 
		{
			this.sourceIP = sourceIP;
			this.destinationIP = destinationIP;
			this.sourcePort = sourcePort;
			this.destinationPort = destinationPort;
		}

		public NATMap(int sourceIP, int destinationIP, short sourcePort, short destinationPort, short newPort) 
		{
			this(sourceIP, destinationIP, sourcePort, destinationPort);
			this.newPort = newPort;
		}
		
	}
	
	private class PortPair
	{
		private int destinationIP;
		private short destinationPort;
		
		public PortPair(int destinationIP, short destinationPort) 
		{
			this.destinationIP = destinationIP;
			this.destinationPort = destinationPort;
		}

		@Override
		public boolean equals(Object obj) 
		{
			if(obj instanceof PortPair)
			{
				PortPair pair = (PortPair)obj;
				if(pair.destinationIP == this.destinationIP &&
						pair.destinationPort == this.destinationPort)
				{
					return true;
				}
			}
			logger.info("PortPair equals");
			return false;
		}

		@Override
	    public int hashCode()
	    {
	        int hashcode;
	        hashcode = (int) ((destinationIP) ^ (destinationPort)); 
	        return hashcode;
	    }
		
	}

}
