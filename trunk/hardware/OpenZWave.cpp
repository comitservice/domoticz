#include "stdafx.h"
#ifdef WITH_OPENZWAVE
#include "OpenZWave.h"

#include <sstream>      // std::stringstream
#include <vector>
#include <ctype.h>
#include <boost/lexical_cast.hpp>
#include <algorithm>

#include "../main/Helper.h"
#include "../main/RFXtrx.h"
#include "../main/Logger.h"
#include "../main/SQLHelper.h"
#include "../webserver/Base64.h"
#include "hardwaretypes.h"

#include "../json/json.h"
#include "../main/localtime_r.h"

//OpenZWave includes
#include "openzwave/Options.h"
#include "openzwave/Manager.h"
#include "openzwave/Driver.h"
#include "openzwave/Node.h"
#include "openzwave/Group.h"
#include "openzwave/Notification.h"
#include "openzwave/value_classes/ValueStore.h"
#include "openzwave/value_classes/Value.h"
#include "openzwave/value_classes/ValueBool.h"
#include "openzwave/platform/Log.h"

#include "ZWaveCommands.h"

//Note!, Some devices uses the same instance for multiple values,
//to solve this we are going to use the Index value!, Except for COMMAND_CLASS_MULTI_INSTANCE

//Scale ID's
enum _eSensorScaleID
{
	SCALEID_UNUSED = 0,
	SCALEID_ENERGY,
	SCALEID_POWER,
	SCALEID_VOLTAGE,
	SCALEID_CURRENT,
	SCALEID_POWERFACTOR,
	SCALEID_GAS
};

struct _tAlarmNameToIndexMapping
{
	std::string sLabel;
	unsigned char iIndex;
};

static const _tAlarmNameToIndexMapping AlarmToIndexMapping[] = {
		{ "General",			0x28 },
		{ "Smoke",				0x29 },
		{ "Carbon Monoxide",	0x2A },
		{ "Carbon Dioxide",		0x2B },
		{ "Heat",				0x2C },
		{ "Flood",				0x2D },
		{ "Alarm Level",		0x32 },
		{ "Alarm Type",			0x33 },
		{ "", 0 }
};

unsigned char GetIndexFromAlarm(const std::string &sLabel)
{
	int ii = 0;
	while (AlarmToIndexMapping[ii].iIndex != 0)
	{
		if (AlarmToIndexMapping[ii].sLabel == sLabel)
			return AlarmToIndexMapping[ii].iIndex;
		ii++;
	}
	return 0;
}

#pragma warning(disable: 4996)

extern std::string szStartupFolder;

#define round(a) ( int ) ( a + .5 )

#ifdef _DEBUG
#define DEBUG_ZWAVE_INT
#endif

const char *cclassStr(uint8 cc)
{
	switch (cc) {
	default:
	case 0x00:
		return "NO OPERATION";
	case 0x20:
		return "BASIC";
	case 0x21:
		return "CONTROLLER REPLICATION";
	case 0x22:
		return "APPLICATION STATUS";
	case 0x23:
		return "ZIP SERVICES";
	case 0x24:
		return "ZIP SERVER";
	case 0x25:
		return "SWITCH BINARY";
	case 0x26:
		return "SWITCH MULTILEVEL";
	case 0x27:
		return "SWITCH ALL";
	case 0x28:
		return "SWITCH TOGGLE BINARY";
	case 0x29:
		return "SWITCH TOGGLE MULTILEVEL";
	case 0x2A:
		return "CHIMNEY FAN";
	case 0x2B:
		return "SCENE ACTIVATION";
	case 0x2C:
		return "SCENE ACTUATOR CONF";
	case 0x2D:
		return "SCENE CONTROLLER CONF";
	case 0x2E:
		return "ZIP CLIENT";
	case 0x2F:
		return "ZIP ADV SERVICES";
	case 0x30:
		return "SENSOR BINARY";
	case 0x31:
		return "SENSOR MULTILEVEL";
	case 0x32:
		return "METER";
	case 0x33:
		return "COLOR CONTROL";
	case 0x34:
		return "ZIP ADV CLIENT";
	case 0x35:
		return "METER PULSE";
	case 0x38:
		return "THERMOSTAT HEATING";
	case 0x40:
		return "THERMOSTAT MODE";
	case 0x42:
		return "THERMOSTAT OPERATING STATE";
	case 0x43:
		return "THERMOSTAT SETPOINT";
	case 0x44:
		return "THERMOSTAT FAN MODE";
	case 0x45:
		return "THERMOSTAT FAN STATE";
	case 0x46:
		return "CLIMATE CONTROL SCHEDULE";
	case 0x47:
		return "THERMOSTAT SETBACK";
	case 0x4C:
		return "DOOR LOCK LOGGING";
	case 0x4E:
		return "SCHEDULE ENTRY LOCK";
	case 0x50:
		return "BASIC WINDOW COVERING";
	case 0x51:
		return "MTP WINDOW COVERING";
	case 0x60:
		return "MULTI INSTANCE";
	case 0x62:
		return "DOOR LOCK";
	case 0x63:
		return "USER CODE";
	case 0x70:
		return "CONFIGURATION";
	case 0x71:
		return "ALARM";
	case 0x72:
		return "MANUFACTURER SPECIFIC";
	case 0x73:
		return "POWERLEVEL";
	case 0x75:
		return "PROTECTION";
	case 0x76:
		return "LOCK";
	case 0x77:
		return "NODE NAMING";
	case 0x7A:
		return "FIRMWARE UPDATE MD";
	case 0x7B:
		return "GROUPING NAME";
	case 0x7C:
		return "REMOTE ASSOCIATION ACTIVATE";
	case 0x7D:
		return "REMOTE ASSOCIATION";
	case 0x80:
		return "BATTERY";
	case 0x81:
		return "CLOCK";
	case 0x82:
		return "HAIL";
	case 0x84:
		return "WAKE UP";
	case 0x85:
		return "ASSOCIATION";
	case 0x86:
		return "VERSION";
	case 0x87:
		return "INDICATOR";
	case 0x88:
		return "PROPRIETARY";
	case 0x89:
		return "LANGUAGE";
	case 0x8A:
		return "TIME";
	case 0x8B:
		return "TIME PARAMETERS";
	case 0x8C:
		return "GEOGRAPHIC LOCATION";
	case 0x8D:
		return "COMPOSITE";
	case 0x8E:
		return "MULTI INSTANCE ASSOCIATION";
	case 0x8F:
		return "MULTI CMD";
	case 0x90:
		return "ENERGY PRODUCTION";
	case 0x91:
		return "MANUFACTURER PROPRIETARY";
	case 0x92:
		return "SCREEN MD";
	case 0x93:
		return "SCREEN ATTRIBUTES";
	case 0x94:
		return "SIMPLE AV CONTROL";
	case 0x95:
		return "AV CONTENT DIRECTORY MD";
	case 0x96:
		return "AV RENDERER STATUS";
	case 0x97:
		return "AV CONTENT SEARCH MD";
	case 0x98:
		return "SECURITY";
	case 0x99:
		return "AV TAGGING MD";
	case 0x9A:
		return "IP CONFIGURATION";
	case 0x9B:
		return "ASSOCIATION COMMAND CONFIGURATION";
	case 0x9C:
		return "SENSOR ALARM";
	case 0x9D:
		return "SILENCE ALARM";
	case 0x9E:
		return "SENSOR CONFIGURATION";
	case 0xEF:
		return "MARK";
	case 0xF0:
		return "NON INTEROPERABLE";
	}
	return "UNKNOWN";
}

COpenZWave::COpenZWave(const int ID, const std::string& devname):
m_szSerialPort(devname)
{
	m_HwdID = ID;
	m_controllerID = 0;
	m_controllerNodeId = 0;
	m_bIsShuttingDown = false;
	m_initFailed = false;
	m_allNodesQueried = false;
	m_awakeNodesQueried = false;
	m_bInUserCodeEnrollmentMode = false;
	m_bNightlyNetworkHeal = false;
	m_pManager = NULL;
	m_bNeedSave = false;
}


COpenZWave::~COpenZWave(void)
{
	CloseSerialConnector();
}

//-----------------------------------------------------------------------------
// <GetNodeInfo>
// Return the NodeInfo object associated with this notification
//-----------------------------------------------------------------------------
COpenZWave::NodeInfo* COpenZWave::GetNodeInfo(OpenZWave::Notification const* _notification)
{
	unsigned int const homeId = _notification->GetHomeId();
	unsigned char const nodeId = _notification->GetNodeId();
	for (std::list<NodeInfo>::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		if ((it->m_homeId == homeId) && (it->m_nodeId == nodeId))
		{
			return &(*it);
		}
	}

	return NULL;
}

COpenZWave::NodeInfo* COpenZWave::GetNodeInfo(const unsigned int homeID, const int nodeID)
{
	for (std::list<NodeInfo>::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
	{
		if ((it->m_homeId == homeID) && (it->m_nodeId == nodeID))
		{
			return &(*it);
		}
	}

	return NULL;
}

std::string COpenZWave::GetNodeStateString(const unsigned int homeID, const int nodeID)
{
	std::string strState = "Unknown";
	COpenZWave::NodeInfo *pNode = GetNodeInfo(homeID, nodeID);
	if (!pNode)
		return strState;
	switch (pNode->eState)
	{
	case NTSATE_UNKNOWN:
		strState = "Unknown";
		break;
	case NSTATE_AWAKE:
		strState = "Awake";
		break;
	case NSTATE_SLEEP:
		strState = "Sleep";
		break;
	case NSTATE_DEAD:
		strState = "Dead";
		break;
	}
	return strState;
}

unsigned char COpenZWave::GetInstanceFromValueID(const OpenZWave::ValueID &vID)
{
	unsigned char instance;

	int commandClass = vID.GetCommandClassId();
	unsigned char vInstance = vID.GetInstance();//(See note on top of this file) GetInstance();
	unsigned char vIndex = vID.GetIndex();
	//uint8 vNodeId = vID.GetNodeId();

	if (
		(commandClass == COMMAND_CLASS_MULTI_INSTANCE) ||
		(commandClass == COMMAND_CLASS_SENSOR_MULTILEVEL) ||
		(commandClass == COMMAND_CLASS_THERMOSTAT_SETPOINT) ||
		(commandClass == COMMAND_CLASS_SENSOR_BINARY)
		)
	{
		instance = vIndex;
		//special case for sensor_multilevel
		if (commandClass == COMMAND_CLASS_SENSOR_MULTILEVEL)
		{
			unsigned char rIndex = instance;
			if (rIndex != vInstance)
			{
				if (rIndex == 1)
					instance = vInstance;
			}
		}
		else
		{
			if ((instance == 0) && (vInstance > 1))
				instance = vInstance;
		}
	}
	else
	{
		instance = vInstance;
		//if (commandClass == COMMAND_CLASS_SWITCH_MULTILEVEL)
		//{
		//	unsigned char rIndex = vInstance;
		//	if (rIndex != vIndex)
		//	{
		//		if (vInstance == 1)
		//		{
		//			instance = vIndex;
		//		}
		//	}
		//}
	}

	return instance;
}

void COpenZWave::WriteControllerConfig()
{
	if (m_controllerID == 0)
		return;

	OpenZWave::Manager::Get()->WriteConfig(m_controllerID);
	m_LastControllerConfigWrite = mytime(NULL);
}

void OnDeviceStatusUpdate(OpenZWave::Driver::ControllerState cs, OpenZWave::Driver::ControllerError err, void *_context)
{
	COpenZWave *pClass = static_cast<COpenZWave*>(_context);
	pClass->OnZWaveDeviceStatusUpdate(cs, err);
}

//-----------------------------------------------------------------------------
// <OnNotification>
// Callback that is triggered when a value, group or node changes
//-----------------------------------------------------------------------------
void OnNotification(OpenZWave::Notification const* _notification, void* _context)
{
	COpenZWave *pClass = static_cast<COpenZWave*>(_context);
	pClass->OnZWaveNotification(_notification);
}

void COpenZWave::OnZWaveNotification(OpenZWave::Notification const* _notification)
{
	// Must do this inside a critical section to avoid conflicts with the main thread
	boost::lock_guard<boost::mutex> l(m_NotificationMutex);

	if (m_bIsShuttingDown)
		return;

	//Send 2 OZW control panel
	OnCPNotification(_notification);

	OpenZWave::Manager* pManager = OpenZWave::Manager::Get();
	if (!pManager)
		return;
	if (m_bIsShuttingDown)
		return;
	m_updateTime = mytime(NULL);

	OpenZWave::ValueID vID = _notification->GetValueID();
	int commandClass = vID.GetCommandClassId();
	unsigned int _homeID = _notification->GetHomeId();
	unsigned char _nodeID = _notification->GetNodeId();

	unsigned char instance = GetInstanceFromValueID(vID);

	OpenZWave::Notification::NotificationType nType = _notification->GetType();
	switch (nType)
	{
	case OpenZWave::Notification::Type_DriverReady:
	{
		m_controllerID = _notification->GetHomeId();
		m_controllerNodeId = _notification->GetNodeId();
		_log.Log(LOG_STATUS, "OpenZWave: Driver Ready");
	}
		break;
	case OpenZWave::Notification::Type_NodeNew:
		_log.Log(LOG_STATUS, "OpenZWave: New Node added. HomeID: %u, NodeID: %d (0x%02x)", _homeID, _nodeID, _nodeID);
		m_bNeedSave = true;
		break;
	case OpenZWave::Notification::Type_NodeAdded:
		{
			// Add the new node to our list
			NodeInfo nodeInfo;
			nodeInfo.m_homeId = _homeID;
			nodeInfo.m_nodeId = _nodeID;
			nodeInfo.m_polled = false;
			nodeInfo.HaveUserCodes = false;
			nodeInfo.szType = pManager->GetNodeType(_homeID, _nodeID);
			nodeInfo.iVersion = pManager->GetNodeVersion(_homeID, _nodeID);
			nodeInfo.Manufacturer_id = pManager->GetNodeManufacturerId(_homeID, _nodeID);
			nodeInfo.Manufacturer_name = pManager->GetNodeManufacturerName(_homeID, _nodeID);
			nodeInfo.Product_type = pManager->GetNodeProductType(_homeID, _nodeID);
			nodeInfo.Product_id = pManager->GetNodeProductId(_homeID, _nodeID);
			nodeInfo.Product_name = pManager->GetNodeProductName(_homeID, _nodeID);

			nodeInfo.tClockDay = -1;
			nodeInfo.tClockHour = -1;
			nodeInfo.tClockMinute = -1;
			nodeInfo.tMode = -1;
			nodeInfo.tFanMode = -1;

			if ((_homeID == m_controllerID) && (_nodeID == m_controllerNodeId))
				nodeInfo.eState = NSTATE_AWAKE;	//controller is always awake
			else
				nodeInfo.eState = NTSATE_UNKNOWN;

			nodeInfo.m_LastSeen = m_updateTime;
			m_nodes.push_back(nodeInfo);
			m_LastIncludedNode = _nodeID;
			AddNode(_homeID, _nodeID, &nodeInfo);
			m_bNeedSave = true;
		}
		break;
	case OpenZWave::Notification::Type_NodeRemoved:
		{
			_log.Log(LOG_STATUS, "OpenZWave: Node Removed. HomeID: %u, NodeID: %d (0x%02x)", _homeID, _nodeID,_nodeID);
			// Remove the node from our list
			for (std::list<NodeInfo>::iterator it = m_nodes.begin(); it != m_nodes.end(); ++it)
			{
				if ((it->m_homeId == _homeID) && (it->m_nodeId == _nodeID))
				{
					m_nodes.erase(it);
					DeleteNode(_homeID, _nodeID);
					break;
				}
			}
			m_bNeedSave = true;
		}
		break;
	case OpenZWave::Notification::Type_NodeProtocolInfo:
		m_bNeedSave = true;
		break;
	case OpenZWave::Notification::Type_DriverReset:
		m_bNeedSave = true;
		break;
	case OpenZWave::Notification::Type_ValueAdded:
		if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
		{
			// Add the new value to our list
			nodeInfo->Instances[instance][commandClass].Values.push_back(vID);
			nodeInfo->m_LastSeen = m_updateTime;
			nodeInfo->Instances[instance][commandClass].m_LastSeen = m_updateTime;
			if (commandClass == COMMAND_CLASS_USER_CODE)
			{
				nodeInfo->HaveUserCodes = true;
			}
			AddValue(vID);
		}
		break;
	case OpenZWave::Notification::Type_SceneEvent:
		if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
		{
			nodeInfo->eState = NSTATE_AWAKE;

			// Add the new value to our list
			UpdateNodeScene(vID, static_cast<int>(_notification->GetSceneId()));
			nodeInfo->m_LastSeen = m_updateTime;
			nodeInfo->Instances[instance][commandClass].m_LastSeen = m_updateTime;
		}
		break;
	case OpenZWave::Notification::Type_ValueRemoved:
		if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
		{
			// Remove the value from out list
			for (std::list<OpenZWave::ValueID>::iterator it = nodeInfo->Instances[instance][commandClass].Values.begin(); it != nodeInfo->Instances[instance][commandClass].Values.end(); ++it)
			{
				if ((*it) == vID)
				{
					nodeInfo->Instances[instance][commandClass].Values.erase(it);
					nodeInfo->Instances[instance][commandClass].m_LastSeen = m_updateTime;
					nodeInfo->m_LastSeen = m_updateTime;
					break;
				}
			}
		}
		break;
	case OpenZWave::Notification::Type_ValueChanged:
		// One of the node values has changed
		if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
		{
			nodeInfo->eState = NSTATE_AWAKE;
			nodeInfo->m_LastSeen = m_updateTime;
			UpdateValue(vID);
			nodeInfo->Instances[instance][commandClass].m_LastSeen = m_updateTime;
		}
		break;
	case OpenZWave::Notification::Type_ValueRefreshed:
		// One of the node values has changed
		if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
		{
			nodeInfo->eState = NSTATE_AWAKE;
			//UpdateValue(vID);
			nodeInfo->Instances[instance][commandClass].m_LastSeen = m_updateTime;
		}
		break;
	case OpenZWave::Notification::Type_Notification:
		{
			uint8 subType = _notification->GetNotification();
			switch (subType)
			{
			case OpenZWave::Notification::Code_MsgComplete:
				if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
				{
					nodeInfo->eState = NSTATE_AWAKE;
					nodeInfo->Instances[instance][commandClass].m_LastSeen = m_updateTime;
				}
				break;
			case OpenZWave::Notification::Code_Awake:
				if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
				{
					nodeInfo->eState = NSTATE_AWAKE;
					nodeInfo->Instances[instance][commandClass].m_LastSeen = m_updateTime;
				}
				break;
			case OpenZWave::Notification::Code_Sleep:
				if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
				{
					nodeInfo->eState = NSTATE_SLEEP;
				}
				break;
			case OpenZWave::Notification::Code_Dead:
				if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
				{
					nodeInfo->eState = NSTATE_DEAD;
				}
				break;
			case OpenZWave::Notification::Code_Alive:
				if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
				{
					nodeInfo->eState = NSTATE_AWAKE;
				}
				break;
			case OpenZWave::Notification::Code_Timeout:
#ifdef _DEBUG
				_log.Log(LOG_STATUS, "OpenZWave: Received timeout notification from HomeID: %u, NodeID: %d (0x%02x)", _homeID, _nodeID, _nodeID);
#endif
				break;
			case OpenZWave::Notification::Code_NoOperation:
				//Code_NoOperation send to node
				break;
			default:
				_log.Log(LOG_STATUS, "OpenZWave: Received unknown notification type (%d) from HomeID: %u, NodeID: %d (0x%02x)", subType, _homeID, _nodeID, _nodeID);
				break;
			}
		}
		break;
	case OpenZWave::Notification::Type_Group:
		// One of the node's association groups has changed
		if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
		{
			nodeInfo->m_LastSeen = m_updateTime;
		}
		break;
	case OpenZWave::Notification::Type_NodeEvent:
		// We have received an event from the node, caused by a
		// basic_set or hail message.
		if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
		{
			nodeInfo->eState = NSTATE_AWAKE;
			UpdateNodeEvent(vID, static_cast<int>(_notification->GetEvent()));
			nodeInfo->Instances[instance][commandClass].m_LastSeen = m_updateTime;
			nodeInfo->m_LastSeen = m_updateTime;
		}
		break;
	case OpenZWave::Notification::Type_PollingDisabled:
		if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
		{
			nodeInfo->m_polled = false;
			nodeInfo->m_LastSeen = m_updateTime;
		}
		break;
	case OpenZWave::Notification::Type_PollingEnabled:
		if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
		{
			nodeInfo->m_polled = true;
			nodeInfo->m_LastSeen = m_updateTime;
		}
		break;
	case OpenZWave::Notification::Type_DriverFailed:
		m_initFailed = true;
		_log.Log(LOG_ERROR, "OpenZWave: Driver Failed!!");
		break;
	case OpenZWave::Notification::Type_DriverRemoved:
		_log.Log(LOG_ERROR, "OpenZWave: Driver Removed!!");
		m_bIsShuttingDown = true;
		break;
	case OpenZWave::Notification::Type_AwakeNodesQueried:
		_log.Log(LOG_STATUS, "OpenZWave: Awake Nodes queried");
		m_awakeNodesQueried = true;
		m_bNeedSave = true;
		NodesQueried();
		break;
	case OpenZWave::Notification::Type_AllNodesQueried:
	case OpenZWave::Notification::Type_AllNodesQueriedSomeDead:
		m_awakeNodesQueried = true;
		m_allNodesQueried = true;
		if (nType == OpenZWave::Notification::Type_AllNodesQueried)
			_log.Log(LOG_STATUS, "OpenZWave: All Nodes queried");
		else
			_log.Log(LOG_STATUS, "OpenZWave: All Nodes queried (Some Dead)");
		NodesQueried();
		m_bNeedSave = true;
		break;
	case OpenZWave::Notification::Type_NodeNaming:
		if (NodeInfo* nodeInfo = GetNodeInfo(_notification))
		{
			std::string product_name = pManager->GetNodeProductName(_homeID, _nodeID);
			if (nodeInfo->Product_name != product_name)
			{
				nodeInfo->Manufacturer_id = pManager->GetNodeManufacturerId(_homeID, _nodeID);
				nodeInfo->Manufacturer_name = pManager->GetNodeManufacturerName(_homeID, _nodeID);
				nodeInfo->Product_type = pManager->GetNodeProductType(_homeID, _nodeID);
				nodeInfo->Product_id = pManager->GetNodeProductId(_homeID, _nodeID);
				nodeInfo->Product_name = product_name;
				AddNode(_homeID, _nodeID, nodeInfo);
			}
			nodeInfo->m_LastSeen = m_updateTime;
			m_bNeedSave = true;
		}
		break;
	case OpenZWave::Notification::Type_NodeQueriesComplete:
		m_bNeedSave = true;
		break;
	case OpenZWave::Notification::Type_EssentialNodeQueriesComplete:
		//The queries on a node that are essential to its operation have been completed. The node can now handle incoming messages.
		break;
	default:
		_log.Log(LOG_STATUS, "OpenZWave: Received unhandled notification type (%d) from HomeID: %u, NodeID: %d (0x%02x)", nType, _homeID, _nodeID,_nodeID);
		break;
	}

	//Force configuration flush every hour
	bool bWriteControllerConfig = false;
	if (m_bNeedSave)
	{
		bWriteControllerConfig = (m_updateTime - m_LastControllerConfigWrite > 60);
	}
	else
	{
		bWriteControllerConfig = (m_updateTime - m_LastControllerConfigWrite > 3600);
	}
	if (bWriteControllerConfig)
	{
		m_bNeedSave = false;
		WriteControllerConfig();
	}
}

void COpenZWave::StopHardwareIntern()
{
	//CloseSerialConnector();
}

void COpenZWave::EnableDisableDebug()
{
	int debugenabled = 0;
#ifdef _DEBUG
	debugenabled = 1;
#else
	m_sql.GetPreferencesVar("ZWaveEnableDebug", debugenabled);
#endif

	if (debugenabled)
	{
		OpenZWave::Options::Get()->AddOptionInt("SaveLogLevel", OpenZWave::LogLevel_Detail);
		OpenZWave::Options::Get()->AddOptionInt("QueueLogLevel", OpenZWave::LogLevel_Debug);
		OpenZWave::Options::Get()->AddOptionInt("DumpTrigger", OpenZWave::LogLevel_Error);
	}
	else
	{
		OpenZWave::Options::Get()->AddOptionInt("SaveLogLevel", OpenZWave::LogLevel_Error);
		OpenZWave::Options::Get()->AddOptionInt("QueueLogLevel", OpenZWave::LogLevel_Error);
		OpenZWave::Options::Get()->AddOptionInt("DumpTrigger", OpenZWave::LogLevel_Error);
	}
	//if (!(OpenZWave::Options::Get()->GetOptionAsInt("DumpTriggerLevel",&optInt))) {
	//	OpenZWave::Options::Get()->AddOptionInt( "DumpTriggerLevel", OpenZWave::LogLevel_Error );
	//}

}

bool COpenZWave::OpenSerialConnector()
{
	_log.Log(LOG_STATUS, "OpenZWave: Starting...");

	m_allNodesQueried = false;
	m_updateTime = mytime(NULL);
	CloseSerialConnector();
	m_bNeedSave = false;
	std::string ConfigPath = szStartupFolder + "Config/";
	// Create the OpenZWave Manager.
	// The first argument is the path to the config files (where the manufacturer_specific.xml file is located
	// The second argument is the path for saved Z-Wave network state and the log file.  If you leave it NULL 
	// the log file will appear in the program's working directory.
	_log.Log(LOG_STATUS, "OpenZWave: using config in: %s", ConfigPath.c_str());
	OpenZWave::Options::Create(ConfigPath.c_str(), ConfigPath.c_str(), "--SaveConfiguration=true ");
	EnableDisableDebug();
	OpenZWave::Options::Get()->AddOptionInt("PollInterval", 60000); //enable polling each 60 seconds
	OpenZWave::Options::Get()->AddOptionBool("IntervalBetweenPolls", true);
	OpenZWave::Options::Get()->AddOptionBool("ValidateValueChanges", true);
	OpenZWave::Options::Get()->AddOptionBool("Associate", true);

	//Set network key for security devices
	std::string sValue = "0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10";
	m_sql.GetPreferencesVar("ZWaveNetworkKey", sValue);
	OpenZWave::Options::Get()->AddOptionString("NetworkKey", sValue, false);

	OpenZWave::Options::Get()->Lock();

	OpenZWave::Manager::Create();
	m_pManager = OpenZWave::Manager::Get();
	if (m_pManager == NULL)
		return false;

	m_bIsShuttingDown = false;
	// Add a callback handler to the manager.  The second argument is a context that
	// is passed to the OnNotification method.  If the OnNotification is a method of
	// a class, the context would usually be a pointer to that class object, to
	// avoid the need for the notification handler to be a static.
	OpenZWave::Manager::Get()->AddWatcher(OnNotification, this);

	// Add a Z-Wave Driver
	// Modify this line to set the correct serial port for your PC interface.
#ifdef WIN32
	if (m_szSerialPort.size() < 4)
		return false;
	std::string szPort = m_szSerialPort.substr(3);
	int iPort = atoi(szPort.c_str());
	char szComm[15];
	if (iPort < 10)
		sprintf(szComm, "COM%d", iPort);
	else
		sprintf(szComm, "\\\\.\\COM%d", iPort);
	OpenZWave::Manager::Get()->AddDriver(szComm);
#else
	OpenZWave::Manager::Get()->AddDriver(m_szSerialPort.c_str());
#endif
	m_LastControllerConfigWrite = mytime(NULL);

	int nightly_heal = 0;
	m_sql.GetPreferencesVar("ZWaveEnableNightlyNetworkHeal", nightly_heal);
	m_bNightlyNetworkHeal = (nightly_heal != 0);

	//Manager::Get()->AddDriver( "HID Controller", Driver::ControllerInterface_Hid );
	return true;
}

void COpenZWave::CloseSerialConnector()
{
	// program exit (clean up)
	m_bIsShuttingDown = true;
	boost::lock_guard<boost::mutex> l(m_NotificationMutex);

	OpenZWave::Manager* pManager = OpenZWave::Manager::Get();
	if (pManager)
	{
		WriteControllerConfig();
		_log.Log(LOG_STATUS, "OpenZWave: Closed");

		try
		{
			OpenZWave::Manager::Get()->RemoveWatcher(OnNotification, this);
			sleep_seconds(1);
			//OpenZWave::Manager::Get()->RemoveDriver(m_szSerialPort.c_str()); //not needed according the documentation, destroy will take card of this
			OpenZWave::Manager::Destroy();
			OpenZWave::Options::Destroy();
		}
		catch (...)
		{

		}
		m_pManager = NULL;
	}
}

bool COpenZWave::GetInitialDevices()
{
	m_controllerID = 0;
	m_controllerNodeId = 0;
	m_initFailed = false;
	m_awakeNodesQueried = false;
	m_allNodesQueried = false;
	m_bInUserCodeEnrollmentMode = false;

	//Connect and request initial devices
	OpenSerialConnector();

	return true;
}

bool COpenZWave::GetFailedState()
{
	return m_initFailed;
}

bool COpenZWave::GetUpdates()
{
	if (m_pManager == NULL)
		return false;

	return true;
}

bool COpenZWave::GetValueByCommandClass(const int nodeID, const int instanceID, const int commandClass, OpenZWave::ValueID &nValue)
{
	COpenZWave::NodeInfo *pNode = GetNodeInfo(m_controllerID, nodeID);
	if (!pNode)
		return false;

	for (std::list<OpenZWave::ValueID>::iterator itt = pNode->Instances[instanceID][commandClass].Values.begin(); itt != pNode->Instances[instanceID][commandClass].Values.end(); ++itt)
	{
		unsigned char cmdClass = itt->GetCommandClassId();
		if (cmdClass == commandClass)
		{
			nValue = *itt;
			return true;
		}
	}
	return false;
}

bool COpenZWave::GetValueByCommandClassLabel(const int nodeID, const int instanceID, const int commandClass, const std::string &vLabel, OpenZWave::ValueID &nValue)
{
	COpenZWave::NodeInfo *pNode = GetNodeInfo(m_controllerID, nodeID);
	if (!pNode)
		return false;

	for (std::list<OpenZWave::ValueID>::iterator itt = pNode->Instances[instanceID][commandClass].Values.begin(); itt != pNode->Instances[instanceID][commandClass].Values.end(); ++itt)
	{
		unsigned char cmdClass = itt->GetCommandClassId();
		if (cmdClass == commandClass)
		{
			std::string cvLabel = m_pManager->GetValueLabel(*itt);
			if (cvLabel == vLabel)
			{
				nValue = *itt;
				return true;
			}
		}
	}
	return false;
}

bool COpenZWave::GetNodeConfigValueByIndex(const NodeInfo *pNode, const int index, OpenZWave::ValueID &nValue)
{
	for (std::map<int, std::map<int, NodeCommandClass> >::const_iterator ittInstance = pNode->Instances.begin(); ittInstance != pNode->Instances.end(); ++ittInstance)
	{
		for (std::map<int, NodeCommandClass>::const_iterator ittCmds = ittInstance->second.begin(); ittCmds != ittInstance->second.end(); ++ittCmds)
		{
			for (std::list<OpenZWave::ValueID>::const_iterator ittValue = ittCmds->second.Values.begin(); ittValue != ittCmds->second.Values.end(); ++ittValue)
			{
				int vindex = ittValue->GetIndex();
				int vinstance = ittValue->GetInstance();
				unsigned char commandclass = ittValue->GetCommandClassId();
				if (
					(commandclass == COMMAND_CLASS_CONFIGURATION) &&
					(vindex == index)
					)
				{
					nValue = *ittValue;
					return true;
				}
				else if (
					(commandclass == COMMAND_CLASS_WAKE_UP) &&
					(vindex == index - 2000) //spacial case
					)
				{
					nValue = *ittValue;
					return true;
				}
			}
		}
	}
	return false;
}

void COpenZWave::SwitchLight(const int nodeID, const int instanceID, const int commandClass, const int value)
{
	if (m_pManager == NULL)
		return;
	boost::lock_guard<boost::mutex> l(m_NotificationMutex);

	NodeInfo *pNode = GetNodeInfo(m_controllerID, nodeID);
	if (!pNode)
	{
		if (!m_awakeNodesQueried)
		{
			_log.Log(LOG_ERROR, "OpenZWave: Switch command not sent because not all Awake Nodes have been Queried!");
		}
		else {
			_log.Log(LOG_ERROR, "OpenZWave: Node not found! (NodeID: %d, 0x%02x)", nodeID, nodeID);
		}
		return;
	}

	OpenZWave::ValueID vID(0, 0, OpenZWave::ValueID::ValueGenre_Basic, 0, 0, 0, OpenZWave::ValueID::ValueType_Bool);
	unsigned char svalue = (unsigned char)value;

	bool bHaveSendSwitch = false;

	bool bIsDimmer = (GetValueByCommandClassLabel(nodeID, instanceID, COMMAND_CLASS_SWITCH_MULTILEVEL, "Level", vID) == true);
	if (bIsDimmer == false)
	{
		if (GetValueByCommandClass(nodeID, instanceID, COMMAND_CLASS_SWITCH_BINARY, vID) == true)
		{
			unsigned int _homeID = vID.GetHomeId();
			unsigned char _nodeID = vID.GetNodeId();
			if (m_pManager->IsNodeFailed(_homeID, _nodeID))
			{
				_log.Log(LOG_ERROR, "OpenZWave: Node has failed (or is not alive), Switch command not sent!");
				return;
			}

			OpenZWave::ValueID::ValueType vType = vID.GetType();
			_log.Log(LOG_NORM, "OpenZWave: Domoticz has send a Switch command! HomeID: %u, NodeID: %d (0x%02x)",_homeID,_nodeID,_nodeID);
			bHaveSendSwitch = true;
			if (vType == OpenZWave::ValueID::ValueType_Bool)
			{
				if (svalue == 0) {
					//Off
					m_pManager->SetValue(vID, false);
				}
				else {
					//On
					m_pManager->SetValue(vID, true);
				}
			}
			else
			{
				if (svalue == 0) {
					//Off
					m_pManager->SetValue(vID, 0);
				}
				else {
					//On
					m_pManager->SetValue(vID, 255);
				}
			}
		}
		else if ((GetValueByCommandClass(nodeID, instanceID, COMMAND_CLASS_SENSOR_BINARY, vID) == true) ||
			(GetValueByCommandClass(nodeID, instanceID, COMMAND_CLASS_SENSOR_MULTILEVEL, vID) == true))
		{
			unsigned int _homeID = vID.GetHomeId();
			unsigned char _nodeID = vID.GetNodeId();
			if (m_pManager->IsNodeFailed(_homeID, _nodeID))
			{
				_log.Log(LOG_ERROR, "OpenZWave: Node has failed (or is not alive), Switch command not sent!");
				return;
			}

			OpenZWave::ValueID::ValueType vType = vID.GetType();
			_log.Log(LOG_NORM, "OpenZWave: Domoticz has sent a Switch command!");
			bHaveSendSwitch = true;
			if (vType == OpenZWave::ValueID::ValueType_Bool)
			{
				if (svalue == 0) {
					//Off
					m_pManager->SetValue(vID, false);
				}
				else {
					//On
					m_pManager->SetValue(vID, true);
				}
			}
			else
			{
				if (svalue == 0) {
					//Off
					m_pManager->SetValue(vID, 0);
				}
				else {
					//On
					m_pManager->SetValue(vID, 255);
				}
			}
		}
		else {

		}
	}
	else
	{
		unsigned int _homeID = vID.GetHomeId();
		unsigned char _nodeID = vID.GetNodeId();
		if (m_pManager->IsNodeFailed(_homeID, _nodeID))
		{
			_log.Log(LOG_ERROR, "OpenZWave: Node has failed (or is not alive), Switch command not sent!");
			return;
		}

		/*
				if (vType == OpenZWave::ValueID::ValueType_Decimal)
				{
				float pastLevel;
				if (m_pManager->GetValueAsFloat(vID,&pastLevel)==true)
				{
				if (svalue==pastLevel)
				{
				if (GetValueByCommandClassLabel(nodeID, instanceID, COMMAND_CLASS_SWITCH_MULTILEVEL,"Level",vID)==true);
				}
				}
				}
				*/
		if ((svalue > 99) && (svalue != 255))
			svalue = 99;
		_log.Log(LOG_NORM, "OpenZWave: Domoticz has send a Switch command!, Level: %d, HomeID: %u, NodeID: %d (0x%02x)", svalue,_homeID,_nodeID,_nodeID);
		bHaveSendSwitch = true;
		if (!m_pManager->SetValue(vID, svalue))
		{
			_log.Log(LOG_ERROR, "OpenZWave: Error setting Switch Value! HomeID: %u, NodeID: %d (0x%02x)",_homeID,_nodeID,_nodeID);
		}
	}

	//bHaveSendSwitch = false; //solves the state problem?

	if (bHaveSendSwitch)
	{
		unsigned char commandclass = vID.GetCommandClassId();
		unsigned char NodeID = vID.GetNodeId();

		//unsigned char vInstance = vID.GetInstance();//(See note on top of this file) GetInstance();
		//unsigned char vIndex = vID.GetIndex();

		unsigned char instance = GetInstanceFromValueID(vID);

		std::stringstream sstr;
		sstr << int(NodeID) << ".instances." << int(instance) << ".commandClasses." << int(commandclass) << ".data";
		std::string path = sstr.str();

		_tZWaveDevice *pDevice = NULL;
		std::map<std::string, _tZWaveDevice>::iterator itt;
		for (itt = m_devices.begin(); itt != m_devices.end(); ++itt)
		{
			std::string::size_type loc = path.find(itt->second.string_id, 0);
			if (loc != std::string::npos)
			{
				pDevice = &itt->second;
				break;
			}
		}
		if (pDevice != NULL)
		{
			pDevice->intvalue = value;
		}

	}
	m_updateTime = mytime(NULL);
}

void COpenZWave::SwitchColor(const int nodeID, const int instanceID, const int commandClass, const unsigned char *colvalues, const unsigned char valuelen)
{
	if (m_pManager == NULL)
		return;
	boost::lock_guard<boost::mutex> l(m_NotificationMutex);

	NodeInfo *pNode = GetNodeInfo(m_controllerID, nodeID);
	if (!pNode)
	{
		if (!m_awakeNodesQueried)
		{
			_log.Log(LOG_ERROR, "OpenZWave: Switch command not sent because not all Awake Nodes have been Queried!");
		}
		else {
			_log.Log(LOG_ERROR, "OpenZWave: Node not found! (NodeID: %d, 0x%02x)", nodeID, nodeID);
		}
		return;
	}

	OpenZWave::ValueID vID(0, 0, OpenZWave::ValueID::ValueGenre_Basic, 0, 0, 0, OpenZWave::ValueID::ValueType_Bool);

	bool bHaveSendSwitch = false;

	if (GetValueByCommandClassLabel(nodeID, instanceID, COMMAND_CLASS_COLOR_CONTROL, "Color", vID) == true)
	{
		if (!m_pManager->SetValue(vID, colvalues, valuelen))
		{
			_log.Log(LOG_ERROR, "OpenZWave: Error setting Switch Value! HomeID: %u, NodeID: %d (0x%02x)",m_controllerID,nodeID,nodeID);
		}
	}
	m_updateTime = mytime(NULL);
}

void COpenZWave::SetThermostatSetPoint(const int nodeID, const int instanceID, const int commandClass, const float value)
{
	if (m_pManager == NULL)
		return;
	boost::lock_guard<boost::mutex> l(m_NotificationMutex);
	OpenZWave::ValueID vID(0, 0, OpenZWave::ValueID::ValueGenre_Basic, 0, 0, 0, OpenZWave::ValueID::ValueType_Bool);
	if (GetValueByCommandClass(nodeID, instanceID, COMMAND_CLASS_THERMOSTAT_SETPOINT, vID) == true)
	{
		m_pManager->SetValue(vID, value);
	}
	m_updateTime = mytime(NULL);
}

void COpenZWave::AddValue(const OpenZWave::ValueID &vID)
{
	if (m_pManager == NULL)
		return;
	if (m_controllerID == 0)
		return;
	unsigned char commandclass = vID.GetCommandClassId();

	//Ignore some command classes, values are already added before calling this function
	if (
		(commandclass == COMMAND_CLASS_BASIC) ||
		(commandclass == COMMAND_CLASS_SWITCH_ALL) ||
		(commandclass == COMMAND_CLASS_CONFIGURATION) ||
		(commandclass == COMMAND_CLASS_VERSION) ||
		(commandclass == COMMAND_CLASS_POWERLEVEL)
		)
		return;

	unsigned int HomeID = vID.GetHomeId();
	unsigned char NodeID = vID.GetNodeId();

	unsigned char vInstance = vID.GetInstance();//(See note on top of this file) GetInstance();
	unsigned char vIndex = vID.GetIndex();

	unsigned char vOrgInstance = vInstance;
	unsigned char vOrgIndex = vIndex;

	OpenZWave::ValueID::ValueType vType = vID.GetType();
	OpenZWave::ValueID::ValueGenre vGenre = vID.GetGenre();

	//Ignore non-user types
	if (vGenre != OpenZWave::ValueID::ValueGenre_User)
		return;

	std::string vLabel = m_pManager->GetValueLabel(vID);

	unsigned char instance = GetInstanceFromValueID(vID);


	if (
		(vLabel == "Exporting") ||
		(vLabel == "Interval") ||
		(vLabel == "Previous Reading")
		)
		return;

	std::string vUnits = m_pManager->GetValueUnits(vID);
	_log.Log(LOG_NORM, "OpenZWave: Value_Added: Node: %d (0x%02x), CommandClass: %s, Label: %s, Instance: %d", static_cast<int>(NodeID), static_cast<int>(NodeID), cclassStr(commandclass), vLabel.c_str(), instance);

	if ((instance == 0) && (NodeID == m_controllerID))
		return;// We skip instance 0 if there are more, since it should be mapped to other instances or their superposition

	_tZWaveDevice _device;
	_device.nodeID = NodeID;
	_device.commandClassID = commandclass;
	_device.scaleID = -1;
	_device.instanceID = instance;
	_device.indexID = 0;
	_device.hasWakeup = m_pManager->IsNodeAwake(HomeID, NodeID);
	_device.isListening = m_pManager->IsNodeListeningDevice(HomeID, NodeID);

	if (vLabel != "")
		_device.label = vLabel;

	float fValue = 0;
	int iValue = 0;
	bool bValue = false;
	unsigned char byteValue = 0;

	// We choose SwitchMultilevel first, if not available, SwhichBinary is chosen
	if (commandclass == COMMAND_CLASS_SWITCH_BINARY)
	{
		if (
			(vLabel == "Switch") ||
			(vLabel == "Sensor") ||
			(vLabel == "Motion Sensor") ||
			(vLabel == "Door/Window Sensor") ||
			(vLabel == "Tamper Sensor") ||
			(vLabel == "Magnet open")
			)
		{
			if (m_pManager->GetValueAsBool(vID, &bValue) == true)
			{
				_device.devType = ZDTYPE_SWITCH_NORMAL;
				if (bValue == true)
					_device.intvalue = 255;
				else
					_device.intvalue = 0;
				InsertDevice(_device);
			}
			else if (m_pManager->GetValueAsByte(vID, &byteValue) == true)
			{
				_device.devType = ZDTYPE_SWITCH_NORMAL;
				if (byteValue == 0)
					_device.intvalue = 0;
				else
					_device.intvalue = 255;
				InsertDevice(_device);
			}
		}
	}
	else if (commandclass == COMMAND_CLASS_SWITCH_MULTILEVEL)
	{
		if (vLabel == "Level")
		{
			if (m_pManager->GetValueAsByte(vID, &byteValue) == true)
			{
				_device.devType = ZDTYPE_SWITCH_DIMMER;
				_device.intvalue = byteValue;
				InsertDevice(_device);
				if (instance == 1)
				{
					if (IsNodeRGBW(HomeID, NodeID))
					{
						_device.label = "Fibaro RGBW";
						_device.devType = ZDTYPE_SWITCH_FGRGBWM441;
						_device.instanceID = 100;
						InsertDevice(_device);
					}
				}
			}
		}
	}
	else if (commandclass == COMMAND_CLASS_COLOR_CONTROL)
	{
		if (vLabel == "Color")
		{
			if (vType == OpenZWave::ValueID::ValueType_Raw)
			{
				_device.devType = ZDTYPE_SWITCH_DIMMER;
				_device.intvalue = 0;
				InsertDevice(_device);
				_device.label = "RGBW";
				_device.devType = ZDTYPE_SWITCH_COLOR;
				_device.instanceID = 101;
				InsertDevice(_device);
			}
		}
	}
	else if (commandclass == COMMAND_CLASS_SENSOR_BINARY)
	{
		if (
			(vLabel == "Switch") ||
			(vLabel == "Sensor") ||
			(vLabel == "Motion Sensor") ||
			(vLabel == "Door/Window Sensor") ||
			(vLabel == "Tamper Sensor") ||
			(vLabel == "Magnet open")
			)
		{
			if (m_pManager->GetValueAsBool(vID, &bValue) == true)
			{
				_device.devType = ZDTYPE_SWITCH_NORMAL;
				if (bValue == true)
					_device.intvalue = 255;
				else
					_device.intvalue = 0;
				InsertDevice(_device);
			}
			else if (m_pManager->GetValueAsByte(vID, &byteValue) == true)
			{
				_device.devType = ZDTYPE_SWITCH_NORMAL;
				if (byteValue == 0)
					_device.intvalue = 0;
				else
					_device.intvalue = 255;
				InsertDevice(_device);
			}
		}
	}
	else if (commandclass == COMMAND_CLASS_USER_CODE)
	{
		/*
		//We already stored our codes in the nodeinfo
		if (vLabel.find("Code ")==0)
		{
		if ((vType == OpenZWave::ValueID::ValueType_Raw) || (vType == OpenZWave::ValueID::ValueType_String))
		{
		std::string strValue;
		if (m_pManager->GetValueAsString(vID, &strValue) == true)
		{
		while (1==0);
		}
		}
		}
		*/
	}
	else if (commandclass == COMMAND_CLASS_BASIC_WINDOW_COVERING)
	{
		if (vLabel == "Open")
		{
			_device.devType = ZDTYPE_SWITCH_NORMAL;
			_device.intvalue = 255;
			InsertDevice(_device);
		}
		else if (vLabel == "Close")
		{
			_device.devType = ZDTYPE_SWITCH_NORMAL;
			_device.intvalue = 0;
			InsertDevice(_device);
		}
	}
	else if ((commandclass == COMMAND_CLASS_ALARM) || (commandclass == COMMAND_CLASS_SENSOR_ALARM))
	{
		int newInstance = GetIndexFromAlarm(vLabel);
		if (newInstance != 0)
		{
			_device.instanceID = newInstance;
			if (m_pManager->GetValueAsByte(vID, &byteValue) == true)
			{
				_device.devType = ZDTYPE_SWITCH_NORMAL;
				if (byteValue == 0)
					_device.intvalue = 0;
				else
					_device.intvalue = 255;
				InsertDevice(_device);
			}
		}
		else
		{
			_log.Log(LOG_STATUS, "OpenZWave: Value_Added: Unhandled Label: %s, Unit: %s", vLabel.c_str(), vUnits.c_str());
		}
	}
	else if (commandclass == COMMAND_CLASS_METER)
	{
		//Meter device
		if (
			(vLabel == "Energy") ||
			(vLabel == "Power")
			)
		{
			if (vType == OpenZWave::ValueID::ValueType_Decimal)
			{
				if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
				{
					if (vLabel == "Energy")
						_device.scaleID = SCALEID_ENERGY;
					else
						_device.scaleID = SCALEID_POWER;
					_device.floatValue = fValue;
					_device.scaleMultiply = 1;
					if (vUnits == "kWh")
					{
						_device.scaleMultiply = 1000;
						_device.devType = ZDTYPE_SENSOR_POWERENERGYMETER;
					}
					else
					{
						_device.devType = ZDTYPE_SENSOR_POWER;
					}
					InsertDevice(_device);
				}
			}
		}
		else if (vLabel == "Voltage")
		{
			if (vType == OpenZWave::ValueID::ValueType_Decimal)
			{
				if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
				{
					_device.floatValue = fValue;
					_device.scaleID = SCALEID_VOLTAGE;
					_device.scaleMultiply = 1;
					_device.devType = ZDTYPE_SENSOR_VOLTAGE;
					InsertDevice(_device);
				}
			}

		}
		else if (vLabel == "Current")
		{
			if (vType == OpenZWave::ValueID::ValueType_Decimal)
			{
				if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
				{
					_device.floatValue = fValue;
					_device.scaleID = SCALEID_CURRENT;
					_device.scaleMultiply = 1;
					_device.devType = ZDTYPE_SENSOR_AMPERE;
					InsertDevice(_device);
				}
			}
		}
		else if (vLabel == "Power Factor")
		{
			if (vType == OpenZWave::ValueID::ValueType_Decimal)
			{
				if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
				{
					_device.floatValue = fValue;
					_device.scaleID = SCALEID_POWERFACTOR;
					_device.scaleMultiply = 1;
					_device.devType = ZDTYPE_SENSOR_PERCENTAGE;
					InsertDevice(_device);
				}
			}
		}
		else if (vLabel == "Gas")
		{
			if (vType == OpenZWave::ValueID::ValueType_Decimal)
			{
				if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
				{
					_device.floatValue = fValue;
					_device.scaleID = SCALEID_GAS;
					_device.scaleMultiply = 1;
					_device.devType = ZDTYPE_SENSOR_GAS;
					InsertDevice(_device);
				}
			}
		}
	}
	else if (commandclass == COMMAND_CLASS_SENSOR_MULTILEVEL)
	{
		if (vLabel == "Temperature")
		{
			if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
			{
				if (vUnits == "F")
				{
					//Convert to celcius
					fValue = float((fValue - 32)*(5.0 / 9.0));
				}
				_device.floatValue = fValue;
				_device.commandClassID = 49;
				_device.devType = ZDTYPE_SENSOR_TEMPERATURE;
				InsertDevice(_device);
			}
		}
		else if (vLabel == "Luminance")
		{
			if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
			{
				if (vUnits != "lux")
				{
					//convert from % to Lux (where max is 1000 Lux)
					fValue = (1000.0f / 100.0f)*fValue;
					if (fValue > 1000.0f)
						fValue = 1000.0f;
				}

				_device.floatValue = fValue;
				_device.commandClassID = 49;
				_device.devType = ZDTYPE_SENSOR_LIGHT;
				InsertDevice(_device);
			}
		}
		else if (vLabel == "Relative Humidity")
		{
			if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
			{
				_device.intvalue = round(fValue);
				_device.commandClassID = 49;
				_device.devType = ZDTYPE_SENSOR_HUMIDITY;
				InsertDevice(_device);
			}
		}
		else if (
			(vLabel == "Energy") ||
			(vLabel == "Power")
			)
		{
			if (vType == OpenZWave::ValueID::ValueType_Decimal)
			{
				if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
				{
					if (vLabel == "Energy")
						_device.scaleID = SCALEID_ENERGY;
					else
						_device.scaleID = SCALEID_POWER;
					_device.floatValue = fValue;
					_device.scaleMultiply = 1;
					if (vUnits == "kWh")
					{
						_device.scaleMultiply = 1000;
						_device.devType = ZDTYPE_SENSOR_POWERENERGYMETER;
					}
					else
					{
						_device.devType = ZDTYPE_SENSOR_POWER;
					}
					InsertDevice(_device);
				}
			}
		}
		else if (vLabel == "Voltage")
		{
			if (vType == OpenZWave::ValueID::ValueType_Decimal)
			{
				if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
				{
					_device.floatValue = fValue;
					_device.scaleMultiply = 1;
					_device.scaleID = SCALEID_VOLTAGE;
					_device.devType = ZDTYPE_SENSOR_VOLTAGE;
					InsertDevice(_device);
				}
			}
		}
		else if (vLabel == "Current")
		{
			if (vType == OpenZWave::ValueID::ValueType_Decimal)
			{
				if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
				{
					_device.floatValue = fValue;
					_device.scaleMultiply = 1;
					_device.scaleID = SCALEID_CURRENT;
					_device.devType = ZDTYPE_SENSOR_AMPERE;
					InsertDevice(_device);
				}
			}
		}
		else if (vLabel == "Power Factor")
		{
			if (vType == OpenZWave::ValueID::ValueType_Decimal)
			{
				if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
				{
					_device.floatValue = fValue;
					_device.scaleMultiply = 1;
					_device.scaleID = SCALEID_POWERFACTOR;
					_device.devType = ZDTYPE_SENSOR_PERCENTAGE;
					InsertDevice(_device);
				}
			}
		}
		else if (vLabel == "Gas")
		{
			if (vType == OpenZWave::ValueID::ValueType_Decimal)
			{
				if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
				{
					_device.floatValue = fValue;
					_device.scaleID = SCALEID_GAS;
					_device.scaleMultiply = 1;
					_device.devType = ZDTYPE_SENSOR_GAS;
					InsertDevice(_device);
				}
			}
		}
		else if (vLabel == "General")
		{
			if (vType == OpenZWave::ValueID::ValueType_Decimal)
			{
				if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
				{
					_device.floatValue = fValue;
					_device.devType = ZDTYPE_SWITCH_NORMAL;
					InsertDevice(_device);
				}
			}
		}
		else
		{
			_log.Log(LOG_STATUS, "OpenZWave: Value_Added: Unhandled Label: %s, Unit: %s", vLabel.c_str(), vUnits.c_str());
		}
	}
	else if (commandclass == COMMAND_CLASS_BATTERY)
	{
		if (_device.isListening)
		{
			if (vType == OpenZWave::ValueID::ValueType_Byte)
			{
				UpdateDeviceBatteryStatus(NodeID, byteValue);
			}
		}
	}
	else if (commandclass == COMMAND_CLASS_THERMOSTAT_SETPOINT)
	{
		if (m_pManager->GetValueAsFloat(vID, &fValue) == true)
		{
			if (vUnits == "F")
			{
				//Convert to celcius
				fValue = float((fValue - 32)*(5.0 / 9.0));
			}
			_device.floatValue = fValue;
			_device.commandClassID = COMMAND_CLASS_THERMOSTAT_SETPOINT;
			_device.devType = ZDTYPE_SENSOR_SETPOINT;
			InsertDevice(_device);
			SendDevice2Domoticz(&_device);
		}
	}
	else if (commandclass == COMMAND_CLASS_THERMOSTAT_MODE)
	{
		COpenZWave::NodeInfo *pNode = GetNodeInfo(HomeID, NodeID);
		if (!pNode)
			return;
		if (vType == OpenZWave::ValueID::ValueType_List)
		{
			if (vLabel == "Mode") {
				pNode->tModes.clear();
				m_pManager->GetValueListItems(vID, &pNode->tModes);
				std::string vListStr;
				if (m_pManager->GetValueListSelection(vID, &vListStr))
				{
					int32 vMode = Lookup_ZWave_Thermostat_Modes(vListStr);
					if (vMode != -1)
					{
						pNode->tMode = vMode;
						_device.intvalue = vMode;
						_device.commandClassID = COMMAND_CLASS_THERMOSTAT_MODE;
						_device.devType = ZDTYPE_SENSOR_THERMOSTAT_MODE;
						InsertDevice(_device);
						SendDevice2Domoticz(&_device);
					}
				}
			}
		}
	}
	else if (commandclass == COMMAND_CLASS_THERMOSTAT_FAN_MODE)
	{
		COpenZWave::NodeInfo *pNode = GetNodeInfo(HomeID, NodeID);
		if (!pNode)
			return;
		if (vType == OpenZWave::ValueID::ValueType_List)
		{
			if (vLabel == "Fan Mode") {
				pNode->tFanModes.clear();
				m_pManager->GetValueListItems(vID, &pNode->tFanModes);
				std::string vListStr;
				if (m_pManager->GetValueListSelection(vID, &vListStr))
				{
					int32 vMode = Lookup_ZWave_Thermostat_Fan_Modes(vListStr);
					if (vMode != -1)
					{
						pNode->tFanMode = vMode;
						_device.intvalue = vMode;
						_device.commandClassID = COMMAND_CLASS_THERMOSTAT_FAN_MODE;
						_device.devType = ZDTYPE_SENSOR_THERMOSTAT_FAN_MODE;
						InsertDevice(_device);
						SendDevice2Domoticz(&_device);
					}
				}
			}
		}
	}
	else if (commandclass == COMMAND_CLASS_CLOCK)
	{
		COpenZWave::NodeInfo *pNode = GetNodeInfo(HomeID, NodeID);
		if (!pNode)
			return;

		if (vType == OpenZWave::ValueID::ValueType_List)
		{
			if (vLabel == "Day") {
				int32 vDay;
				if (m_pManager->GetValueListSelection(vID, &vDay))
				{
					if (vDay > 0)
					{
						pNode->tClockDay = vDay - 1;
					}
				}
			}
		}
		else if (vType == OpenZWave::ValueID::ValueType_Byte)
		{
			if (m_pManager->GetValueAsByte(vID, &byteValue) == false)
				return;
			else if (vLabel == "Hour") {
				pNode->tClockHour = byteValue;
			}
			else if (vLabel == "Minute") {
				pNode->tClockMinute = byteValue;
				if (
					(pNode->tClockDay != -1) &&
					(pNode->tClockHour != -1) &&
					(pNode->tClockMinute != -1)
					)
				{
					_log.Log(LOG_NORM, "OpenZWave: NodeID: %d (0x%02x), Thermostat Clock: %s %02d:%02d", NodeID, NodeID, ZWave_Clock_Days(pNode->tClockDay), pNode->tClockHour, pNode->tClockMinute);
					_device.intvalue = (pNode->tClockDay*(24 * 60)) + (pNode->tClockHour * 60) + pNode->tClockMinute;
					_device.commandClassID = COMMAND_CLASS_CLOCK;
					_device.devType = ZDTYPE_SENSOR_THERMOSTAT_CLOCK;
					InsertDevice(_device);
					SendDevice2Domoticz(&_device);
				}
			}
		}
	}
	else if (commandclass == COMMAND_CLASS_CLIMATE_CONTROL_SCHEDULE)
	{
		if (vType == OpenZWave::ValueID::ValueType_Byte)
		{
			if (m_pManager->GetValueAsByte(vID, &byteValue) == false)
				return;
		}
		_log.Log(LOG_STATUS, "OpenZWave: Unhandled class: 0x%02X (%s), NodeID: %d (0x%02x), Index: %d, Instance: %d", commandclass, cclassStr(commandclass), NodeID, NodeID, vOrgIndex, vOrgInstance);
	}
	else
	{
		//Unhandled
		_log.Log(LOG_STATUS, "OpenZWave: Unhandled class: 0x%02X (%s), NodeID: %d (0x%02x), Index: %d, Instance: %d", commandclass, cclassStr(commandclass), NodeID, NodeID, vOrgIndex, vOrgInstance);
		if (vType == OpenZWave::ValueID::ValueType_List)
		{
			//std::vector<std::string > vStringList;
			//if (m_pManager->GetValueListItems(vID,&vStringList)==true)
			//{
			//}
		}
	}
}

void COpenZWave::UpdateNodeEvent(const OpenZWave::ValueID &vID, int EventID)
{
	if (m_pManager == NULL)
		return;
	if (m_controllerID == 0)
		return;

	//if (m_nodesQueried==false)
	//return; //only allow updates when node Query is done

	unsigned int HomeID = vID.GetHomeId();
	unsigned char NodeID = vID.GetNodeId();
	unsigned char instance = vID.GetInstance();
	unsigned char index = vID.GetIndex();
	if (instance == 0)
		return;

	instance = vID.GetIndex();
	unsigned char commandclass = vID.GetCommandClassId();
	std::string vLabel = m_pManager->GetValueLabel(vID);

	if ((commandclass == COMMAND_CLASS_ALARM) || (commandclass == COMMAND_CLASS_SENSOR_ALARM))
	{
		instance = GetIndexFromAlarm(vLabel);
		if (instance == 0)
			return;
	}

	_tZWaveDevice *pDevice = FindDevice(NodeID, instance, index, COMMAND_CLASS_SENSOR_BINARY, ZDTYPE_SWITCH_NORMAL);
	if (pDevice == NULL)
	{
		//one more try
		pDevice = FindDevice(NodeID, instance, index, COMMAND_CLASS_SWITCH_BINARY, ZDTYPE_SWITCH_NORMAL);
		if (pDevice == NULL)
		{
			// absolute last try
			instance = vID.GetIndex();
			pDevice = FindDevice(NodeID, -1, -1, COMMAND_CLASS_SENSOR_MULTILEVEL, ZDTYPE_SWITCH_NORMAL);
			if (pDevice == NULL)
			{
				//okey, 1 more
				int tmp_instance = index;
				pDevice = FindDevice(NodeID, tmp_instance, -1, COMMAND_CLASS_SWITCH_MULTILEVEL, ZDTYPE_SWITCH_DIMMER);
				if (pDevice == NULL)
				{
					return;
				}
			}
		}
	}

	int nintvalue = 0;
	if (EventID == 255)
		nintvalue = 255;
	else
		nintvalue = 0;

	if (pDevice->intvalue == nintvalue)
	{
		return; //dont send/update same value
	}
	time_t atime = mytime(NULL);
	pDevice->intvalue = nintvalue;
	pDevice->lastreceived = atime;
	pDevice->sequence_number += 1;
	if (pDevice->sequence_number == 0)
		pDevice->sequence_number = 1;
	if (pDevice->bValidValue)
		SendDevice2Domoticz(pDevice);
}

void COpenZWave::UpdateNodeScene(const OpenZWave::ValueID &vID, int SceneID)
{
	if (m_pManager == NULL)
		return;
	if (m_controllerID == 0)
		return;

	//if (m_nodesQueried==false)
	//return; //only allow updates when node Query is done

	//unsigned int HomeID = vID.GetHomeId();
	unsigned char NodeID = vID.GetNodeId();

	int devID = (SceneID << 8) + NodeID;
	int instanceID = 0;
	int indexID = 0;
	int commandclass = COMMAND_CLASS_SCENE_ACTIVATION;
	_tZWaveDevice *pDevice = FindDevice(devID, instanceID, indexID, commandclass, ZDTYPE_SWITCH_NORMAL);
	if (pDevice == NULL)
	{
		//Add new switch device
		_tZWaveDevice _device;
		_device.nodeID = devID;
		_device.instanceID = instanceID;
		_device.indexID = indexID;

		std::string vLabel = m_pManager->GetValueLabel(vID);
		if (vLabel != "")
			_device.label = vLabel;

		_device.basicType = 1;
		_device.genericType = 1;
		_device.specificType = 1;
		_device.isListening = false;
		_device.sensor250 = false;
		_device.sensor1000 = false;
		_device.isFLiRS = !_device.isListening && (_device.sensor250 || _device.sensor1000);
		_device.hasWakeup = false;
		_device.hasBattery = false;
		_device.scaleID = -1;

		_device.commandClassID = commandclass;
		_device.devType = ZDTYPE_SWITCH_NORMAL;
		_device.intvalue = 255;
		InsertDevice(_device);
		pDevice = FindDevice(devID, instanceID, indexID, commandclass, ZDTYPE_SWITCH_NORMAL);
		if (pDevice == NULL)
			return;
	}
	time_t atime = mytime(NULL);
	pDevice->intvalue = 255;
	pDevice->lastreceived = atime;
	pDevice->sequence_number += 1;
	if (pDevice->sequence_number == 0)
		pDevice->sequence_number = 1;
	if (pDevice->bValidValue)
		SendDevice2Domoticz(pDevice);
}

void COpenZWave::UpdateValue(const OpenZWave::ValueID &vID)
{
	if (m_pManager == NULL)
		return;
	if (m_controllerID == 0)
		return;

	//	if (m_nodesQueried==false)
	//	return; //only allow updates when node Query is done
	unsigned char commandclass = vID.GetCommandClassId();
	unsigned int HomeID = vID.GetHomeId();
	unsigned char NodeID = vID.GetNodeId();

	unsigned char instance = GetInstanceFromValueID(vID);

	OpenZWave::ValueID::ValueType vType = vID.GetType();
	OpenZWave::ValueID::ValueGenre vGenre = vID.GetGenre();
	std::string vLabel = m_pManager->GetValueLabel(vID);
	std::string vUnits = m_pManager->GetValueUnits(vID);

	float fValue = 0;
	int iValue = 0;
	bool bValue = false;
	unsigned char byteValue = 0;
	std::string strValue = "";
	int32 lValue = 0;

	if (vType == OpenZWave::ValueID::ValueType_Decimal)
	{
		if (m_pManager->GetValueAsFloat(vID, &fValue) == false)
			return;
	}
	else if (vType == OpenZWave::ValueID::ValueType_Bool)
	{
		if (m_pManager->GetValueAsBool(vID, &bValue) == false)
			return;
	}
	else if (vType == OpenZWave::ValueID::ValueType_Byte)
	{
		if (m_pManager->GetValueAsByte(vID, &byteValue) == false)
			return;
	}
	else if ((vType == OpenZWave::ValueID::ValueType_Raw) || (vType == OpenZWave::ValueID::ValueType_String))
	{
		if (m_pManager->GetValueAsString(vID, &strValue) == false)
			return;
	}
	else if (vType == OpenZWave::ValueID::ValueType_List)
	{
		if (m_pManager->GetValueListSelection(vID, &lValue) == false)
			return;
	}
	else
	{
		//unhandled value type
		return;
	}


	if (vGenre != OpenZWave::ValueID::ValueGenre_User)
	{
		NodeInfo *pNode = GetNodeInfo(HomeID, NodeID);
		if (pNode)
		{
			/*
				if (pNode->m_WasSleeping)
				{
				pNode->m_WasSleeping=false;
				m_pManager->RefreshNodeInfo(HomeID,NodeID);
				}
				*/
		}
		if ((pNode) && (vLabel == "Wake-up Interval"))
		{
			if (HomeID != m_controllerID)
				m_pManager->AddAssociation(HomeID, NodeID, 1, m_controllerNodeId);
		}
		return;
	}

	if (
		(vLabel == "Exporting") ||
		(vLabel == "Interval") ||
		(vLabel == "Previous Reading")
		)
		return;

	if ((commandclass == COMMAND_CLASS_ALARM) || (commandclass == COMMAND_CLASS_SENSOR_ALARM))
	{
		instance = GetIndexFromAlarm(vLabel);
		if (instance == 0)
			return;
	}

	time_t atime = mytime(NULL);
	std::stringstream sstr;
	sstr << int(NodeID) << ".instances." << int(instance) << ".commandClasses." << int(commandclass) << ".data";

	if (
		(vLabel == "Energy") ||
		(vLabel == "Power") ||
		(vLabel == "Voltage") ||
		(vLabel == "Current") ||
		(vLabel == "Power Factor")||
		(vLabel == "Gas")
		)
	{
		int scaleID = 0;
		if (vLabel == "Energy")
			scaleID = SCALEID_ENERGY;
		else if (vLabel == "Power")
			scaleID = SCALEID_POWER;
		else if (vLabel == "Voltage")
			scaleID = SCALEID_VOLTAGE;
		else if (vLabel == "Current")
			scaleID = SCALEID_CURRENT;
		else if (vLabel == "Power Factor")
			scaleID = SCALEID_POWERFACTOR;
		else if (vLabel == "Gas")
			scaleID = SCALEID_GAS;

		sstr << "." << scaleID;
	}
	std::string path = sstr.str();

#ifdef DEBUG_ZWAVE_INT
	_log.Log(LOG_NORM, "OpenZWave: Value_Changed: Node: %d (0x%02x), CommandClass: %s, Label: %s, Instance: %d, Index: %d", NodeID, NodeID, cclassStr(commandclass), vLabel.c_str(), vID.GetInstance(), vID.GetIndex());
#endif

	if (commandclass == COMMAND_CLASS_USER_CODE)
	{
		if ((instance == 1) && (vGenre == OpenZWave::ValueID::ValueGenre_User) && (vID.GetIndex() == 0) && (vType == OpenZWave::ValueID::ValueType_Raw))
		{
			//Enrollment Code
			COpenZWave::NodeInfo *pNode = GetNodeInfo(HomeID, NodeID);
			if (!pNode)
				return;
			if (pNode->Instances.find(1) == pNode->Instances.end())
				return; //no codes added yet, wake your tag reader

			//Check if we are in Enrollment Mode, if not dont continue

			if (!m_bInUserCodeEnrollmentMode)
			{
				_log.Log(LOG_ERROR, "OpenZWave: Received new User Code/Tag, but we are not in Enrollment mode!");
				return;
			}
			m_bControllerCommandInProgress = false;

			bool bIncludedCode = false;

			for (std::list<OpenZWave::ValueID>::iterator itt = pNode->Instances[1][COMMAND_CLASS_USER_CODE].Values.begin(); itt != pNode->Instances[1][COMMAND_CLASS_USER_CODE].Values.end(); ++itt)
			{
				OpenZWave::ValueID vNode = *itt;
				if ((vNode.GetGenre() == OpenZWave::ValueID::ValueGenre_User) && (vNode.GetInstance() == 1) && (vNode.GetType() == OpenZWave::ValueID::ValueType_Raw))
				{
					int vNodeIndex = vNode.GetIndex();
					if (vNodeIndex >= 1)
					{
						std::string sValue;
						if (m_pManager->GetValueAsString(vNode, &sValue))
						{
							//Find Empty slot
							if (sValue.find("0x00 ") == 0)
							{
								_log.Log(LOG_STATUS, "OpenZWave: Enrolling User Code/Tag to code index: %d", vNodeIndex);
								m_pManager->SetValue(vNode, strValue);
								AddValue(vID);
								bIncludedCode = true;
								break;
							}
						}
					}
				}
			}
			if (!bIncludedCode)
			{
				_log.Log(LOG_ERROR, "OpenZWave: Received new User Code/Tag, but there is no available room for new codes!!");
			}
			m_bInUserCodeEnrollmentMode = false;
			return;
		}
		else
		{
			int cIndex = vID.GetIndex();
			//if ((instance == 1) && (vGenre == OpenZWave::ValueID::ValueGenre_User) && (vID.GetIndex() != 0) && (vType == OpenZWave::ValueID::ValueType_Raw))
			//{
			//	_log.Log(LOG_NORM, "OpenZWave: Received User Code/Tag index: %d (%s)", cIndex, strValue.c_str());
			//}
		}
	}
	else if (commandclass == COMMAND_CLASS_BATTERY)
	{
		//Battery status update
		if (vType == OpenZWave::ValueID::ValueType_Byte)
		{
			UpdateDeviceBatteryStatus(NodeID, byteValue);
		}
		return;
	}
	else if (commandclass == COMMAND_CLASS_CLOCK)
	{
		COpenZWave::NodeInfo *pNode = GetNodeInfo(HomeID, NodeID);
		if (!pNode)
			return;

		if (vType == OpenZWave::ValueID::ValueType_List)
		{
			if (vLabel == "Day") {
				int32 vDay;
				if (m_pManager->GetValueListSelection(vID, &vDay))
				{
					if (vDay > 0)
					{
						pNode->tClockDay = vDay - 1;
						return;
					}
				}
			}
		}
		else if (vType == OpenZWave::ValueID::ValueType_Byte)
		{
			if (m_pManager->GetValueAsByte(vID, &byteValue) == false)
				return;
			else if (vLabel == "Hour") {
				pNode->tClockHour = byteValue;
				return;
			}
			else if (vLabel == "Minute") {
				pNode->tClockMinute = byteValue;
				if (
					(pNode->tClockDay != -1) &&
					(pNode->tClockHour != -1) &&
					(pNode->tClockMinute != -1)
					)
				{
					_log.Log(LOG_NORM, "OpenZWave: NodeID: %d (0x%02x), Thermostat Clock: %s %02d:%02d", NodeID, NodeID, ZWave_Clock_Days(pNode->tClockDay), pNode->tClockHour, pNode->tClockMinute);
				}
			}
		}
	}

	_tZWaveDevice *pDevice = NULL;
	std::map<std::string, _tZWaveDevice>::iterator itt;
	for (itt = m_devices.begin(); itt != m_devices.end(); ++itt)
	{
		std::string::size_type loc = path.find(itt->second.string_id, 0);
		if (loc != std::string::npos)
		{
			pDevice = &itt->second;
			break;
		}
	}
	if (pDevice == NULL)
		return;

	pDevice->bValidValue = true;

	switch (pDevice->devType)
	{
	case ZDTYPE_SWITCH_NORMAL:
	{
		if ((commandclass == COMMAND_CLASS_ALARM) || (commandclass == COMMAND_CLASS_SENSOR_ALARM))
		{
			//Alarm sensors
			int nintvalue = 0;
			if (byteValue == 0)
				nintvalue = 0;
			else
				nintvalue = 255;
			//if (pDevice->intvalue==nintvalue)
			//{
			//	return; //dont send same value
			//}
			pDevice->intvalue = nintvalue;
		}
		else if (vLabel == "Open")
		{
			pDevice->intvalue = 255;
		}
		else if (vLabel == "Close")
		{
			pDevice->intvalue = 0;
		}
		else
		{
			if (
				(vLabel == "Switch") ||
				(vLabel == "Sensor") ||
				(vLabel == "Motion Sensor") ||
				(vLabel == "Door/Window Sensor") ||
				(vLabel == "Tamper Sensor") ||
				(vLabel == "Magnet open")
				)
			{
				int intValue = 0;
				if (vType == OpenZWave::ValueID::ValueType_Bool)
				{
					if (bValue == true)
						intValue = 255;
					else
						intValue = 0;
				}
				else if (vType == OpenZWave::ValueID::ValueType_Byte)
				{
					if (byteValue == 0)
						intValue = 0;
					else
						intValue = 255;
				}
				else
					return;
				if (pDevice->intvalue == intValue)
				{
					return; //dont send same value
				}
				pDevice->intvalue = intValue;
			}
			else
				return;
		}
	}
		break;
	case ZDTYPE_SWITCH_DIMMER:
	{
		if (vLabel != "Level")
			return;
		if (vType != OpenZWave::ValueID::ValueType_Byte)
			return;
		if (byteValue == 99)
			byteValue = 255;
		if (pDevice->intvalue == byteValue)
		{
			return; //dont send same value
		}
		pDevice->intvalue = byteValue;
	}
		break;
	case ZDTYPE_SENSOR_POWER:
		if (
			(vLabel != "Energy") &&
			(vLabel != "Power")
			)
			return;
		if (vType != OpenZWave::ValueID::ValueType_Decimal)
			return;
		pDevice->floatValue = fValue*pDevice->scaleMultiply;
		break;
	case ZDTYPE_SENSOR_POWERENERGYMETER:
		if (vType != OpenZWave::ValueID::ValueType_Decimal)
			return;
		if (
			(vLabel != "Energy") &&
			(vLabel != "Power")
			)
			return;
		pDevice->floatValue = fValue*pDevice->scaleMultiply;
		break;
	case ZDTYPE_SENSOR_TEMPERATURE:
		if (vType != OpenZWave::ValueID::ValueType_Decimal)
			return;
		if (vLabel != "Temperature")
			return;
		if (vUnits == "F")
		{
			//Convert to celcius
			fValue = float((fValue - 32)*(5.0 / 9.0));
		}
		pDevice->bValidValue = (abs(pDevice->floatValue - fValue) < 10);
		pDevice->floatValue = fValue;
		break;
	case ZDTYPE_SENSOR_HUMIDITY:
		if (vType != OpenZWave::ValueID::ValueType_Decimal)
			return;
		if (vLabel != "Relative Humidity")
			return;
		pDevice->intvalue = round(fValue);
		break;
	case ZDTYPE_SENSOR_LIGHT:
		if (vType != OpenZWave::ValueID::ValueType_Decimal)
			return;
		if (vLabel != "Luminance")
			return;
		if (vUnits != "lux")
		{
			//convert from % to Lux (where max is 1000 Lux)
			fValue = (1000.0f / 100.0f)*fValue;
		}
		pDevice->floatValue = fValue;
		break;
	case ZDTYPE_SENSOR_VOLTAGE:
		if (vType != OpenZWave::ValueID::ValueType_Decimal)
			return;
		if (vLabel != "Voltage")
			return;
		pDevice->floatValue = fValue;
		break;
	case ZDTYPE_SENSOR_AMPERE:
		if (vType != OpenZWave::ValueID::ValueType_Decimal)
			return;
		if (vLabel != "Current")
			return;
		pDevice->floatValue = fValue;
		break;
	case ZDTYPE_SENSOR_SETPOINT:
		if (vType != OpenZWave::ValueID::ValueType_Decimal)
			return;
		if (vUnits == "F")
		{
			//Convert to celcius
			fValue = float((fValue - 32)*(5.0 / 9.0));
		}
		pDevice->bValidValue = (abs(pDevice->floatValue - fValue) < 10);
		pDevice->floatValue = fValue;
		break;
	case ZDTYPE_SENSOR_PERCENTAGE:
		if (vType != OpenZWave::ValueID::ValueType_Decimal)
			return;
		if (vLabel != "Power Factor")
			return;
		pDevice->floatValue = fValue;
		break;
	case ZDTYPE_SENSOR_GAS:
		{
			if (vType != OpenZWave::ValueID::ValueType_Decimal)
				return;
			if (vLabel != "Gas")
				return;
			float oldvalue = pDevice->floatValue;
			pDevice->floatValue = fValue; //always set the value
			if ((fValue - oldvalue > 10.0f) || (fValue < oldvalue))
				return;//sanity check, don't report it
		}
		break;
	case ZDTYPE_SENSOR_THERMOSTAT_CLOCK:
		if (vLabel == "Minute")
		{
			COpenZWave::NodeInfo *pNode = GetNodeInfo(HomeID, NodeID);
			if (!pNode)
				return;
			pDevice->intvalue = (pNode->tClockDay*(24 * 60)) + (pNode->tClockHour * 60) + pNode->tClockMinute;
		}
		break;
	case ZDTYPE_SENSOR_THERMOSTAT_MODE:
		if (vType != OpenZWave::ValueID::ValueType_List)
			return;
		if (vLabel == "Mode")
		{
			COpenZWave::NodeInfo *pNode = GetNodeInfo(HomeID, NodeID);
			if (!pNode)
				return;
			pNode->tModes.clear();
			m_pManager->GetValueListItems(vID, &pNode->tModes);

			std::string vListStr;
			if (!m_pManager->GetValueListSelection(vID, &vListStr))
				return;
			int32 lValue = Lookup_ZWave_Thermostat_Modes(vListStr);
			if (lValue == -1)
				return;
			pNode->tMode = lValue;
			pDevice->intvalue = lValue;
		}
		break;
	case ZDTYPE_SENSOR_THERMOSTAT_FAN_MODE:
		if (vType != OpenZWave::ValueID::ValueType_List)
			return;
		if (vLabel == "Fan Mode")
		{
			COpenZWave::NodeInfo *pNode = GetNodeInfo(HomeID, NodeID);
			if (!pNode)
				return;
			pNode->tFanModes.clear();
			m_pManager->GetValueListItems(vID, &pNode->tFanModes);

			std::string vListStr;
			if (!m_pManager->GetValueListSelection(vID, &vListStr))
				return;
			int32 lValue = Lookup_ZWave_Thermostat_Fan_Modes(vListStr);
			if (lValue == -1)
				return;
			pNode->tFanMode = lValue;
			pDevice->intvalue = lValue;
		}
		break;
	}

	pDevice->lastreceived = atime;
	pDevice->sequence_number += 1;
	if (pDevice->sequence_number == 0)
		pDevice->sequence_number = 1;
	if (pDevice->bValidValue)
		SendDevice2Domoticz(pDevice);

}

bool COpenZWave::IncludeDevice()
{
	if (m_pManager == NULL)
		return false;
	CancelControllerCommand();
	m_LastIncludedNode = 0;
	m_ControllerCommandStartTime = mytime(NULL);
	m_bControllerCommandCanceled = false;
	m_bControllerCommandInProgress = true;
	m_pManager->BeginControllerCommand(m_controllerID, OpenZWave::Driver::ControllerCommand_AddDevice, OnDeviceStatusUpdate, this, true);
	_log.Log(LOG_STATUS, "OpenZWave: Node Include command initiated...");
	return true;
}

bool COpenZWave::ExcludeDevice(const int nodeID)
{
	if (m_pManager == NULL)
		return false;
	CancelControllerCommand();
	m_ControllerCommandStartTime = mytime(NULL);
	m_bControllerCommandCanceled = false;
	m_bControllerCommandInProgress = true;
	m_pManager->BeginControllerCommand(m_controllerID, OpenZWave::Driver::ControllerCommand_RemoveDevice, OnDeviceStatusUpdate, this, true);
	_log.Log(LOG_STATUS, "OpenZWave: Node Exclude command initiated...");

	return true;
}

bool COpenZWave::SoftResetDevice()
{
	if (m_pManager == NULL)
		return false;

	std::stringstream szQuery;
	szQuery << "DELETE FROM ZWaveNodes WHERE (HardwareID = '" << m_HwdID << "')";
	m_sql.query(szQuery.str());

	m_pManager->SoftReset(m_controllerID);
	_log.Log(LOG_STATUS, "OpenZWave: Soft Reset device executed...");
	return true;
}

bool COpenZWave::HardResetDevice()
{
	if (m_pManager == NULL)
		return false;

	std::stringstream szQuery;
	szQuery << "DELETE FROM ZWaveNodes WHERE (HardwareID = '" << m_HwdID << "')";
	m_sql.query(szQuery.str());

	m_pManager->ResetController(m_controllerID);
	_log.Log(LOG_STATUS, "OpenZWave: Hard Reset device executed...");

	return true;
}

bool COpenZWave::HealNetwork()
{
	if (m_pManager == NULL)
		return false;

	m_pManager->HealNetwork(m_controllerID, true);
	_log.Log(LOG_STATUS, "OpenZWave: Heal Network command initiated...");
	return true;
}

bool COpenZWave::HealNode(const int nodeID)
{
	if (m_pManager == NULL)
		return false;

	m_pManager->HealNetworkNode(m_controllerID, nodeID, true);
	_log.Log(LOG_STATUS, "OpenZWave: Heal Node command initiated for node: %d (0x%02x)...", nodeID,nodeID);
	return true;
}


bool COpenZWave::NetworkInfo(const int hwID, std::vector< std::vector< int > > &NodeArray)
{

	if (m_pManager == NULL) {
		return false;
	}

	std::stringstream szQuery;
	std::vector<std::vector<std::string> > result;
	szQuery << "SELECT HomeID,NodeID FROM ZWaveNodes WHERE (HardwareID = '" << hwID << "')";
	result = m_sql.query(szQuery.str());
	if (result.size() < 1) {
		return false;
	}
	int rowCnt = 0;
	std::vector<std::vector<std::string> >::const_iterator itt;
	for (itt = result.begin(); itt != result.end(); ++itt)
	{
		std::vector<std::string> sd = *itt;
		int nodeID = atoi(sd[1].c_str());
		unsigned int homeID = boost::lexical_cast<unsigned int>(sd[0]);
		NodeInfo *pNode = GetNodeInfo(homeID, nodeID);
		if (pNode == NULL)
			continue;

		std::vector<int> row;
		NodeArray.push_back(row);
		NodeArray[rowCnt].push_back(nodeID);
		uint8* arr;
		int retval = m_pManager->GetNodeNeighbors(homeID, nodeID, &arr);
		if (retval > 0) {

			for (int i = 0; i < retval; i++) {
				NodeArray[rowCnt].push_back(arr[i]);
			}

			delete[] arr;
		}
		rowCnt++;
	}

	return true;

}

int COpenZWave::ListGroupsForNode(const int nodeID)
{
	if (m_pManager == NULL)
		return 0;

	return m_pManager->GetNumGroups(m_controllerID, nodeID);
}


int COpenZWave::ListAssociatedNodesinGroup(const int nodeID, const int groupID, std::vector<int> &nodesingroup)
{

	if (m_pManager == NULL)
		return 0;

	uint8* arr;
	int retval = m_pManager->GetAssociations(m_controllerID, nodeID, groupID, &arr);
	if (retval > 0) {
		for (int i = 0; i < retval; i++) {
			nodesingroup.push_back(arr[i]);
		}
		delete[] arr;
	}
	return retval;
}

bool COpenZWave::AddNodeToGroup(const int nodeID, const int groupID, const int addID)
{

	if (m_pManager == NULL)
		return false;
	m_pManager->AddAssociation(m_controllerID, nodeID, groupID, addID);
	_log.Log(LOG_STATUS, "OpenZWave: added node: %d (0x%02x) in group: %d of node: %d (0x%02x)", addID, addID, groupID, nodeID, nodeID);
	return true;
}

bool COpenZWave::RemoveNodeFromGroup(const int nodeID, const int groupID, const int removeID)
{
	if (m_pManager == NULL)
		return false;
	m_pManager->RemoveAssociation(m_controllerID, nodeID, groupID, removeID);
	_log.Log(LOG_STATUS, "OpenZWave: removed node: %d (0x%02x) from group: %d of node: %d (0x%02x)", removeID, removeID, groupID, nodeID, nodeID);

	return true;
}

bool COpenZWave::RemoveFailedDevice(const int nodeID)
{
	if (m_pManager == NULL)
		return false;

	m_ControllerCommandStartTime = mytime(NULL);
	m_bControllerCommandCanceled = false;
	m_bControllerCommandInProgress = true;
	m_pManager->BeginControllerCommand(m_controllerID, OpenZWave::Driver::ControllerCommand_RemoveFailedNode, OnDeviceStatusUpdate, this, true, (unsigned char)nodeID);
	_log.Log(LOG_STATUS, "OpenZWave: Remove Failed Device initiated...");
	return true;
}

bool COpenZWave::ReceiveConfigurationFromOtherController()
{
	if (m_pManager == NULL)
		return false;

	m_ControllerCommandStartTime = mytime(NULL) + 10;//30 second timeout
	m_bControllerCommandCanceled = false;
	m_bControllerCommandInProgress = true;
	m_pManager->BeginControllerCommand(m_controllerID, OpenZWave::Driver::ControllerCommand_ReceiveConfiguration, OnDeviceStatusUpdate, this);
	_log.Log(LOG_STATUS, "OpenZWave: Receive Configuration initiated...");
	return true;
}

bool COpenZWave::SendConfigurationToSecondaryController()
{
	if (m_pManager == NULL)
		return false;

	m_ControllerCommandStartTime = mytime(NULL) + 10;//30 second timeout
	m_bControllerCommandCanceled = false;
	m_bControllerCommandInProgress = true;
	m_pManager->BeginControllerCommand(m_controllerID, OpenZWave::Driver::ControllerCommand_ReplicationSend, OnDeviceStatusUpdate, this);
	_log.Log(LOG_STATUS, "OpenZWave: Replication to Secondary Controller initiated...");
	return true;
}

bool COpenZWave::TransferPrimaryRole()
{
	if (m_pManager == NULL)
		return false;

	m_ControllerCommandStartTime = mytime(NULL) + 10;//30 second timeout
	m_bControllerCommandCanceled = false;
	m_bControllerCommandInProgress = true;
	m_pManager->BeginControllerCommand(m_controllerID, OpenZWave::Driver::ControllerCommand_TransferPrimaryRole, OnDeviceStatusUpdate, this);
	_log.Log(LOG_STATUS, "OpenZWave: Transfer Primary Role initiated...");
	return true;
}

bool COpenZWave::CancelControllerCommand()
{
	if (m_bControllerCommandInProgress == false)
		return false;
	if (m_pManager == NULL)
		return false;
	m_bControllerCommandInProgress = false;
	m_bControllerCommandCanceled = true;
	return m_pManager->CancelControllerCommand(m_controllerID);
}

std::string COpenZWave::GetConfigFile(std::string &szConfigFile)
{
	std::string retstring = "";
	if (m_pManager == NULL)
		return retstring;

	boost::lock_guard<boost::mutex> l(m_NotificationMutex);
	WriteControllerConfig();

	char szFileName[255];
	sprintf(szFileName, "%sConfig/zwcfg_0x%08x.xml", szStartupFolder.c_str(), m_controllerID);
	szConfigFile = szFileName;
	std::ifstream testFile(szConfigFile.c_str(), std::ios::binary);
	std::vector<char> fileContents((std::istreambuf_iterator<char>(testFile)),
		std::istreambuf_iterator<char>());
	if (fileContents.size() > 0)
	{
		retstring.insert(retstring.begin(), fileContents.begin(), fileContents.end());
	}
	return retstring;
}

void COpenZWave::OnZWaveDeviceStatusUpdate(int _cs, int _err)
{
	OpenZWave::Driver::ControllerState cs = (OpenZWave::Driver::ControllerState)_cs;
	OpenZWave::Driver::ControllerError err = (OpenZWave::Driver::ControllerError)_err;

	std::string szLog;

	switch (cs)
	{
	case OpenZWave::Driver::ControllerState_Normal:
		m_bControllerCommandInProgress = false;
		szLog = "No Command in progress";
		break;
	case OpenZWave::Driver::ControllerState_Starting:
		szLog = "Starting controller command";
		break;
	case OpenZWave::Driver::ControllerState_Cancel:
		szLog = "The command was canceled";
		m_bControllerCommandInProgress = false;
		break;
	case OpenZWave::Driver::ControllerState_Error:
		szLog = "Command invocation had error(s) and was aborted";
		m_bControllerCommandInProgress = false;
		break;
	case OpenZWave::Driver::ControllerState_Waiting:
		m_bControllerCommandInProgress = true;
		szLog = "Controller is waiting for a user action";
		break;
	case OpenZWave::Driver::ControllerState_Sleeping:
		szLog = "Controller command is on a sleep queue wait for device";
		break;
	case OpenZWave::Driver::ControllerState_InProgress:
		szLog = "The controller is communicating with the other device to carry out the command";
		break;
	case OpenZWave::Driver::ControllerState_Completed:
		m_bControllerCommandInProgress = false;
		if (!m_bControllerCommandCanceled)
		{
			szLog = "The command has completed successfully";
			m_bNeedSave = true;
		}
		else
		{
			szLog = "The command was canceled";
		}
		break;
	case OpenZWave::Driver::ControllerState_Failed:
		szLog = "The command has failed";
		m_bControllerCommandInProgress = false;
		break;
	case OpenZWave::Driver::ControllerState_NodeOK:
		szLog = "Used only with ControllerCommand_HasNodeFailed to indicate that the controller thinks the node is OK";
		break;
	case OpenZWave::Driver::ControllerState_NodeFailed:
		szLog = "Used only with ControllerCommand_HasNodeFailed to indicate that the controller thinks the node has failed";
		break;
	default:
		szLog = "Unknown Device Response!";
		m_bControllerCommandInProgress = false;
		break;
	}
	_log.Log(LOG_STATUS, "OpenZWave: Device Response: %s", szLog.c_str());
}

bool COpenZWave::IsNodeRGBW(const unsigned int homeID, const int nodeID)
{
	std::stringstream szQuery;
	std::vector<std::vector<std::string> > result;
	szQuery << "SELECT ProductDescription FROM ZWaveNodes WHERE (HardwareID==" << m_HwdID << ") AND (HomeID==" << homeID << ") AND (NodeID==" << nodeID << ")";
	result = m_sql.query(szQuery.str());
	if (result.size() < 1)
		return false;
	std::string ProductDescription = result[0][0];
	return (ProductDescription.find("FGRGBWM441") != std::string::npos);
}

void COpenZWave::EnableNodePoll(const unsigned int homeID, const int nodeID, const int pollTime)
{
	NodeInfo *pNode = GetNodeInfo(homeID, nodeID);
	if (pNode == NULL)
		return; //Not found

	bool bSingleIndexPoll = false;

	std::stringstream szQuery;
	std::vector<std::vector<std::string> > result;
	szQuery << "SELECT ProductDescription FROM ZWaveNodes WHERE (HardwareID==" << m_HwdID << ") AND (HomeID==" << homeID << ") AND (NodeID==" << nodeID << ")";
	result = m_sql.query(szQuery.str());
	if (result.size() > 0)
	{
		std::string ProductDescription = result[0][0];
		bSingleIndexPoll = (
			(ProductDescription.find("GreenWave PowerNode 6 port") != std::string::npos)
			);
	}

	for (std::map<int, std::map<int, NodeCommandClass> >::const_iterator ittInstance = pNode->Instances.begin(); ittInstance != pNode->Instances.end(); ++ittInstance)
	{

		for (std::map<int, NodeCommandClass>::const_iterator ittCmds = ittInstance->second.begin(); ittCmds != ittInstance->second.end(); ++ittCmds)
		{
			for (std::list<OpenZWave::ValueID>::const_iterator ittValue = ittCmds->second.Values.begin(); ittValue != ittCmds->second.Values.end(); ++ittValue)
			{
				unsigned char commandclass = ittValue->GetCommandClassId();
				OpenZWave::ValueID::ValueGenre vGenre = ittValue->GetGenre();

				unsigned int _homeID = ittValue->GetHomeId();
				unsigned char _nodeID = ittValue->GetNodeId();
				if (m_pManager->IsNodeFailed(_homeID, _nodeID))
				{
					//do not enable/disable polling on nodes that are failed
					continue;
				}

				//Ignore non-user types
				if (vGenre != OpenZWave::ValueID::ValueGenre_User)
					continue;

				std::string vLabel = m_pManager->GetValueLabel(*ittValue);

				if (
					(vLabel == "Exporting") ||
					(vLabel == "Interval") ||
					(vLabel == "Previous Reading")
					)
					continue;

				if (commandclass == COMMAND_CLASS_SWITCH_BINARY)
				{
					if (
						(vLabel == "Switch") ||
						(vLabel == "Sensor") ||
						(vLabel == "Motion Sensor") ||
						(vLabel == "Door/Window Sensor") ||
						(vLabel == "Tamper Sensor") ||
						(vLabel == "Magnet open")
						)
					{
						m_pManager->EnablePoll(*ittValue, 1);
					}
				}
				else if (commandclass == COMMAND_CLASS_SWITCH_MULTILEVEL)
				{
					if (vLabel == "Level")
					{
						if ((*ittValue).GetIndex() != 0)
						{
							continue;
						}
						m_pManager->EnablePoll(*ittValue, 1);
					}
				}
				else if (commandclass == COMMAND_CLASS_SENSOR_BINARY)
				{
					if (
						(vLabel == "Switch") ||
						(vLabel == "Sensor") ||
						(vLabel == "Motion Sensor") ||
						(vLabel == "Door/Window Sensor") ||
						(vLabel == "Tamper Sensor") ||
						(vLabel == "Magnet open")
						)
					{
						m_pManager->EnablePoll(*ittValue, 1);
					}
				}
				else if (commandclass == COMMAND_CLASS_METER)
				{
					//Meter device
					if (
						(vLabel == "Energy") ||
						(vLabel == "Power")||
						(vLabel == "Gas")
						)
					{
						if (bSingleIndexPoll && (ittValue->GetIndex() != 0))
							continue;
						m_pManager->EnablePoll(*ittValue, 1);
					}
				}
				else if (commandclass == COMMAND_CLASS_SENSOR_MULTILEVEL)
				{
					//if ((*ittValue).GetIndex() != 0)
					//{
					//	continue;
					//}
					m_pManager->EnablePoll(*ittValue, 2);
				}
				else if (commandclass == COMMAND_CLASS_BATTERY)
				{
					m_pManager->EnablePoll(*ittValue, 2);
				}
				else
					m_pManager->DisablePoll(*ittValue);
			}
		}
	}
}

void COpenZWave::DisableNodePoll(const unsigned int homeID, const int nodeID)
{
	NodeInfo *pNode = GetNodeInfo(homeID, nodeID);
	if (pNode == NULL)
		return; //Not found

	for (std::map<int, std::map<int, NodeCommandClass> >::const_iterator ittInstance = pNode->Instances.begin(); ittInstance != pNode->Instances.end(); ++ittInstance)
	{
		for (std::map<int, NodeCommandClass>::const_iterator ittCmds = ittInstance->second.begin(); ittCmds != ittInstance->second.end(); ++ittCmds)
		{
			for (std::list<OpenZWave::ValueID>::const_iterator ittValue = ittCmds->second.Values.begin(); ittValue != ittCmds->second.Values.end(); ++ittValue)
			{
				if (m_pManager->isPolled(*ittValue))
					m_pManager->DisablePoll(*ittValue);
			}
		}
	}
}

void COpenZWave::DeleteNode(const unsigned int homeID, const int nodeID)
{
	std::stringstream szQuery;
	szQuery << "DELETE FROM ZWaveNodes WHERE (HardwareID==" << m_HwdID << ") AND (HomeID==" << homeID << ") AND (NodeID==" << nodeID << ")";
	m_sql.query(szQuery.str());
}

void COpenZWave::AddNode(const unsigned int homeID, const int nodeID, const NodeInfo *pNode)
{
	if (m_controllerID == 0)
		return;
	if (homeID != m_controllerID)
		return;
	//Check if node already exist
	std::stringstream szQuery;
	std::vector<std::vector<std::string> > result;
	szQuery << "SELECT ID FROM ZWaveNodes WHERE (HardwareID==" << m_HwdID << ") AND (HomeID==" << homeID << ") AND (NodeID==" << nodeID << ")";
	result = m_sql.query(szQuery.str());
	szQuery.clear();
	szQuery.str("");
	std::string sProductDescription = pNode->Manufacturer_name + " " + pNode->Product_name;

	if (result.size() < 1)
	{
		//Not Found, Add it to the database
		if (nodeID != m_controllerNodeId)
			szQuery << "INSERT INTO ZWaveNodes (HardwareID, HomeID, NodeID, ProductDescription) VALUES (" << m_HwdID << "," << homeID << "," << nodeID << ",'" << sProductDescription << "')";
		else
			szQuery << "INSERT INTO ZWaveNodes (HardwareID, HomeID, NodeID, Name,ProductDescription) VALUES (" << m_HwdID << "," << homeID << "," << nodeID << ",'Controller','" << sProductDescription << "')";
	}
	else
	{
		if (
			(pNode->Manufacturer_name.size() == 0) ||
			(pNode->Product_name.size() == 0)
			)
			return;
		//Update ProductDescription
		szQuery << "UPDATE ZWaveNodes SET ProductDescription='" << sProductDescription << "' WHERE (HardwareID==" << m_HwdID << ") AND (HomeID==" << homeID << ") AND (NodeID==" << nodeID << ")";
	}
	m_sql.query(szQuery.str());
}

void COpenZWave::EnableDisableNodePolling()
{
	int intervalseconds = 60;
	m_sql.GetPreferencesVar("ZWavePollInterval", intervalseconds);

	m_pManager->SetPollInterval(intervalseconds * 1000, false);

	std::stringstream szQuery;
	std::vector<std::vector<std::string> > result;
	szQuery << "SELECT HomeID,NodeID,PollTime FROM ZWaveNodes WHERE (HardwareID==" << m_HwdID << ")";
	result = m_sql.query(szQuery.str());
	if (result.size() < 1)
		return;

	std::vector<std::vector<std::string> >::const_iterator itt;
	for (itt = result.begin(); itt != result.end(); ++itt)
	{
		std::vector<std::string> sd = *itt;
		unsigned int HomeID = boost::lexical_cast<unsigned int>(sd[0]);
		int NodeID = atoi(sd[1].c_str());
		int PollTime = atoi(sd[2].c_str());

		if (
			(HomeID == m_controllerID) &&
			(NodeID != m_controllerNodeId)
			)
		{
			if (PollTime > 0)
				EnableNodePoll(HomeID, NodeID, PollTime);
			else
				DisableNodePoll(HomeID, NodeID);
		}
	}
}

void COpenZWave::SetClock(const int nodeID, const int instanceID, const int commandClass, const int day, const int hour, const int minute)
{
	//We have to set 3 values here (Later check if we can use COMMAND_CLASS_MULTI_CMD for this to do it in one)

	NodeInfo *pNode = GetNodeInfo(m_controllerID, nodeID);
	if (!pNode)
		return;

	OpenZWave::ValueID vDay(0, 0, OpenZWave::ValueID::ValueGenre_Basic, 0, 0, 0, OpenZWave::ValueID::ValueType_Bool);
	OpenZWave::ValueID vHour(0, 0, OpenZWave::ValueID::ValueGenre_Basic, 0, 0, 0, OpenZWave::ValueID::ValueType_Bool);
	OpenZWave::ValueID vMinute(0, 0, OpenZWave::ValueID::ValueGenre_Basic, 0, 0, 0, OpenZWave::ValueID::ValueType_Bool);


	if (GetValueByCommandClassLabel(nodeID, 1, COMMAND_CLASS_CLOCK, "Day", vDay) == false)
		return;
	if (GetValueByCommandClassLabel(nodeID, 1, COMMAND_CLASS_CLOCK, "Hour", vHour) == false)
		return;
	if (GetValueByCommandClassLabel(nodeID, 1, COMMAND_CLASS_CLOCK, "Minute", vMinute) == false)
		return;

	m_pManager->SetValueListSelection(vDay, ZWave_Clock_Days(day));
	m_pManager->SetValue(vHour, (const uint8)hour);
	m_pManager->SetValue(vMinute, (const uint8)minute);
}

void COpenZWave::SetThermostatMode(const int nodeID, const int instanceID, const int commandClass, const int tMode)
{
	if (m_pManager == NULL)
		return;
	boost::lock_guard<boost::mutex> l(m_NotificationMutex);
	OpenZWave::ValueID vID(0, 0, OpenZWave::ValueID::ValueGenre_Basic, 0, 0, 0, OpenZWave::ValueID::ValueType_Bool);
	if (GetValueByCommandClass(nodeID, instanceID, COMMAND_CLASS_THERMOSTAT_MODE, vID) == true)
	{
		m_pManager->SetValueListSelection(vID, ZWave_Thermostat_Modes[tMode]);
	}
	m_updateTime = mytime(NULL);
}

void COpenZWave::SetThermostatFanMode(const int nodeID, const int instanceID, const int commandClass, const int fMode)
{
	if (m_pManager == NULL)
		return;
	boost::lock_guard<boost::mutex> l(m_NotificationMutex);
	OpenZWave::ValueID vID(0, 0, OpenZWave::ValueID::ValueGenre_Basic, 0, 0, 0, OpenZWave::ValueID::ValueType_Bool);
	if (GetValueByCommandClass(nodeID, instanceID, COMMAND_CLASS_THERMOSTAT_FAN_MODE, vID) == true)
	{
		m_pManager->SetValueListSelection(vID, ZWave_Thermostat_Fan_Modes[fMode]);
	}
	m_updateTime = mytime(NULL);
}

std::string COpenZWave::GetSupportedThermostatModes(const unsigned long ID)
{
	std::string retstr = "";
	unsigned char ID1 = (unsigned char)((ID & 0xFF000000) >> 24);
	unsigned char ID2 = (unsigned char)((ID & 0x00FF0000) >> 16);
	unsigned char ID3 = (unsigned char)((ID & 0x0000FF00) >> 8);
	unsigned char ID4 = (unsigned char)((ID & 0x000000FF));

	int nodeID = (ID2 << 8) | ID3;
	int instanceID = ID4;
	int indexID = ID1;

	const _tZWaveDevice* pDevice = FindDevice(nodeID, instanceID, indexID, ZDTYPE_SENSOR_THERMOSTAT_MODE);
	if (pDevice)
	{
		boost::lock_guard<boost::mutex> l(m_NotificationMutex);
		OpenZWave::ValueID vID(0, 0, OpenZWave::ValueID::ValueGenre_Basic, 0, 0, 0, OpenZWave::ValueID::ValueType_Bool);
		if (GetValueByCommandClass(nodeID, instanceID, COMMAND_CLASS_THERMOSTAT_MODE, vID) == true)
		{
			unsigned int homeID = vID.GetHomeId();
			int nodeID = vID.GetNodeId();
			COpenZWave::NodeInfo* pNode = GetNodeInfo(homeID, nodeID);
			if (pNode)
			{
				int smode = 0;
				std::string modes = "";
				char szTmp[200];
				while (ZWave_Thermostat_Modes[smode] != NULL)
				{
					if (std::find(pNode->tModes.begin(), pNode->tModes.end(), ZWave_Thermostat_Modes[smode]) != pNode->tModes.end())
					{
						//Value supported
						sprintf(szTmp, "%d;%s;", smode, ZWave_Thermostat_Modes[smode]);
						modes += szTmp;
					}
					smode++;
				}
				retstr = modes;
			}
		}
	}

	return retstr;
}

std::string COpenZWave::GetSupportedThermostatFanModes(const unsigned long ID)
{
	std::string retstr = "";
	unsigned char ID1 = (unsigned char)((ID & 0xFF000000) >> 24);
	unsigned char ID2 = (unsigned char)((ID & 0x00FF0000) >> 16);
	unsigned char ID3 = (unsigned char)((ID & 0x0000FF00) >> 8);
	unsigned char ID4 = (unsigned char)((ID & 0x000000FF));

	int nodeID = (ID2 << 8) | ID3;
	int instanceID = ID4;
	int indexID = ID1;

	const _tZWaveDevice* pDevice = FindDevice(nodeID, instanceID, indexID, ZDTYPE_SENSOR_THERMOSTAT_FAN_MODE);
	if (pDevice)
	{
		boost::lock_guard<boost::mutex> l(m_NotificationMutex);
		OpenZWave::ValueID vID(0, 0, OpenZWave::ValueID::ValueGenre_Basic, 0, 0, 0, OpenZWave::ValueID::ValueType_Bool);
		if (GetValueByCommandClass(nodeID, instanceID, COMMAND_CLASS_THERMOSTAT_FAN_MODE, vID) == true)
		{
			unsigned int homeID = vID.GetHomeId();
			int nodeID = vID.GetNodeId();
			COpenZWave::NodeInfo* pNode = GetNodeInfo(homeID, nodeID);
			if (pNode)
			{
				int smode = 0;
				char szTmp[200];
				std::string modes = "";
				while (ZWave_Thermostat_Fan_Modes[smode]!=NULL)
				{
					if (std::find(pNode->tFanModes.begin(), pNode->tFanModes.end(), ZWave_Thermostat_Fan_Modes[smode]) != pNode->tFanModes.end())
					{
						//Value supported
						sprintf(szTmp, "%d;%s;", smode, ZWave_Thermostat_Fan_Modes[smode]);
						modes += szTmp;
					}
					smode++;
				}
				retstr = modes;
			}
		}
	}

	return retstr;
}

void COpenZWave::NodesQueried()
{
	//All nodes have been queried, enable/disable node polling
	EnableDisableNodePolling();
}

bool COpenZWave::RequestNodeConfig(const unsigned int homeID, const int nodeID)
{
	NodeInfo *pNode = GetNodeInfo(homeID, nodeID);
	if (pNode == NULL)
		return false;
	m_pManager->RequestAllConfigParams(homeID, nodeID);
	return true;
}

void COpenZWave::GetNodeValuesJson(const unsigned int homeID, const int nodeID, Json::Value &root, const int index)
{
	NodeInfo *pNode = GetNodeInfo(homeID, nodeID);
	if (pNode == NULL)
		return;

	int ivalue = 0;
	std::string szValue;

	if (nodeID == m_controllerNodeId)
	{
		//Main ZWave node

		//Poll Interval
		root["result"][index]["config"][ivalue]["type"] = "short";

		int intervalseconds = 60;
		m_sql.GetPreferencesVar("ZWavePollInterval", intervalseconds);
		root["result"][index]["config"][ivalue]["value"] = intervalseconds;

		root["result"][index]["config"][ivalue]["index"] = 1;
		root["result"][index]["config"][ivalue]["label"] = "Poll Interval";
		root["result"][index]["config"][ivalue]["units"] = "Seconds";
		root["result"][index]["config"][ivalue]["help"] =
			"Set the time period between polls of a node's state. The length of the interval is the same for all devices. "
			"To even out the Z-Wave network traffic generated by polling, OpenZWave divides the polling interval by the number of devices that have polling enabled, and polls each "
			"in turn. It is recommended that if possible, the interval should not be set shorter than the number of polled devices in seconds (so that the network does not have to cope with more than one poll per second).";
		root["result"][index]["config"][ivalue]["LastUpdate"] = "-";
		ivalue++;

		//Debug
		root["result"][index]["config"][ivalue]["type"] = "short";

		int debugenabled = 0;
		m_sql.GetPreferencesVar("ZWaveEnableDebug", debugenabled);
		root["result"][index]["config"][ivalue]["value"] = debugenabled;

		root["result"][index]["config"][ivalue]["index"] = 2;
		root["result"][index]["config"][ivalue]["label"] = "Enable Debug";
		root["result"][index]["config"][ivalue]["units"] = "";
		root["result"][index]["config"][ivalue]["help"] =
			"Enable/Disable debug logging. Disabled=0, Enabled=1 "
			"It is not recommended to enable Debug for a live system as the log files generated will grow large quickly.";
		root["result"][index]["config"][ivalue]["LastUpdate"] = "-";
		ivalue++;

		//Nightly Node Heal
		root["result"][index]["config"][ivalue]["type"] = "short";

		int nightly_heal = 0;
		m_sql.GetPreferencesVar("ZWaveEnableNightlyNetworkHeal", nightly_heal);
		root["result"][index]["config"][ivalue]["value"] = nightly_heal;

		root["result"][index]["config"][ivalue]["index"] = 3;
		root["result"][index]["config"][ivalue]["label"] = "Enable Nightly Heal Network (04:00 am)";
		root["result"][index]["config"][ivalue]["units"] = "";
		root["result"][index]["config"][ivalue]["help"] =
			"Enable/Disable nightly heal network. Disabled=0, Enabled=1 ";
		root["result"][index]["config"][ivalue]["LastUpdate"] = "-";
		ivalue++;

		//Network Key
		root["result"][index]["config"][ivalue]["type"] = "string";

		std::string sValue = "0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10";
		m_sql.GetPreferencesVar("ZWaveNetworkKey", sValue);
		root["result"][index]["config"][ivalue]["value"] = sValue;

		root["result"][index]["config"][ivalue]["index"] = 4;
		root["result"][index]["config"][ivalue]["label"] = "Security Network Key";
		root["result"][index]["config"][ivalue]["units"] = "";
		root["result"][index]["config"][ivalue]["help"] =
			"If you are using any Security Devices, you MUST set a network Key "
			"The length should be 16 bytes!";
		root["result"][index]["config"][ivalue]["LastUpdate"] = "-";
		ivalue++;

		return;
	}

	for (std::map<int, std::map<int, NodeCommandClass> >::const_iterator ittInstance = pNode->Instances.begin(); ittInstance != pNode->Instances.end(); ++ittInstance)
	{
		for (std::map<int, NodeCommandClass>::const_iterator ittCmds = ittInstance->second.begin(); ittCmds != ittInstance->second.end(); ++ittCmds)
		{
			for (std::list<OpenZWave::ValueID>::const_iterator ittValue = ittCmds->second.Values.begin(); ittValue != ittCmds->second.Values.end(); ++ittValue)
			{
				unsigned char commandclass = ittValue->GetCommandClassId();
				if (commandclass == COMMAND_CLASS_CONFIGURATION)
				{
					if (m_pManager->IsValueReadOnly(*ittValue) == true)
						continue;

					OpenZWave::ValueID::ValueType vType = ittValue->GetType();

					if (vType == OpenZWave::ValueID::ValueType_Decimal)
					{
						root["result"][index]["config"][ivalue]["type"] = "float";
					}
					else if (vType == OpenZWave::ValueID::ValueType_Bool)
					{
						root["result"][index]["config"][ivalue]["type"] = "bool";
					}
					else if (vType == OpenZWave::ValueID::ValueType_Byte)
					{
						root["result"][index]["config"][ivalue]["type"] = "byte";
					}
					else if (vType == OpenZWave::ValueID::ValueType_Short)
					{
						root["result"][index]["config"][ivalue]["type"] = "short";
					}
					else if (vType == OpenZWave::ValueID::ValueType_Int)
					{
						root["result"][index]["config"][ivalue]["type"] = "int";
					}
					else if (vType == OpenZWave::ValueID::ValueType_Button)
					{
						//root["result"][index]["config"][ivalue]["type"]="button";
						//Not supported now
						continue;
					}
					else if (vType == OpenZWave::ValueID::ValueType_List)
					{
						root["result"][index]["config"][ivalue]["type"] = "list";
					}
					else
					{
						//not supported
						continue;
					}

					if (m_pManager->GetValueAsString(*ittValue, &szValue) == false)
						continue;
					root["result"][index]["config"][ivalue]["value"] = szValue;
					if (vType == OpenZWave::ValueID::ValueType_List)
					{
						std::vector<std::string> strs;
						m_pManager->GetValueListItems(*ittValue, &strs);
						root["result"][index]["config"][ivalue]["list_items"] = static_cast<int>(strs.size());
						int vcounter = 0;
						for (std::vector<std::string>::const_iterator it = strs.begin(); it != strs.end(); ++it)
						{
							root["result"][index]["config"][ivalue]["listitem"][vcounter++] = *it;
						}
					}
					int i_index = ittValue->GetIndex();
					std::string i_label = m_pManager->GetValueLabel(*ittValue);
					std::string i_units = m_pManager->GetValueUnits(*ittValue);
					std::string i_help = m_pManager->GetValueHelp(*ittValue);
					char *szDate = asctime(localtime(&ittCmds->second.m_LastSeen));
					root["result"][index]["config"][ivalue]["index"] = i_index;
					root["result"][index]["config"][ivalue]["label"] = i_label;
					root["result"][index]["config"][ivalue]["units"] = i_units;
					root["result"][index]["config"][ivalue]["help"] = i_help;
					root["result"][index]["config"][ivalue]["LastUpdate"] = szDate;
					ivalue++;
				}
				else if (commandclass == COMMAND_CLASS_WAKE_UP)
				{
					//Only add the Wake_Up_Interval value here
					if ((ittValue->GetGenre() == OpenZWave::ValueID::ValueGenre_System) && (ittValue->GetInstance() == 1))
					{
						if ((m_pManager->GetValueLabel(*ittValue) == "Wake-up Interval") && (ittValue->GetType() == OpenZWave::ValueID::ValueType_Int))
						{
							if (m_pManager->GetValueAsString(*ittValue, &szValue) == false)
								continue;
							root["result"][index]["config"][ivalue]["type"] = "int";
							root["result"][index]["config"][ivalue]["value"] = szValue;
							int i_index = 2000 + ittValue->GetIndex(); //special case
							std::string i_label = m_pManager->GetValueLabel(*ittValue);
							std::string i_units = m_pManager->GetValueUnits(*ittValue);
							std::string i_help = m_pManager->GetValueHelp(*ittValue);
							char *szDate = asctime(localtime(&ittCmds->second.m_LastSeen));
							root["result"][index]["config"][ivalue]["index"] = i_index;
							root["result"][index]["config"][ivalue]["label"] = i_label;
							root["result"][index]["config"][ivalue]["units"] = i_units;
							root["result"][index]["config"][ivalue]["help"] = i_help;
							root["result"][index]["config"][ivalue]["LastUpdate"] = szDate;
							ivalue++;
						}
					}
				}
			}
		}
	}

}

bool COpenZWave::ApplyNodeConfig(const unsigned int homeID, const int nodeID, const std::string &svaluelist)
{
	NodeInfo *pNode = GetNodeInfo(homeID, nodeID);
	if (pNode == NULL)
		return false;

	std::vector<std::string> results;
	StringSplit(svaluelist, "_", results);
	if (results.size() < 1)
		return false;

	bool bRestartOpenZWave = false;

	size_t vindex = 0;
	while (vindex < results.size())
	{
		int rvIndex = atoi(results[vindex].c_str());
		std::string ValueVal = results[vindex + 1];
		ValueVal = base64_decode(ValueVal);

		if (nodeID == m_controllerNodeId)
		{
			//Main ZWave node (Controller)
			if (rvIndex == 1)
			{
				//PollInterval
				int intervalseconds = atoi(ValueVal.c_str());
				m_sql.UpdatePreferencesVar("ZWavePollInterval", intervalseconds);
				EnableDisableNodePolling();
			}
			else if (rvIndex == 2)
			{
				//Debug mode
				int debugenabled = atoi(ValueVal.c_str());
				int old_debugenabled = 0;
				m_sql.GetPreferencesVar("ZWaveEnableDebug", old_debugenabled);
				if (old_debugenabled != debugenabled)
				{
					m_sql.UpdatePreferencesVar("ZWaveEnableDebug", debugenabled);
					bRestartOpenZWave = true;
				}
			}
			else if (rvIndex == 3)
			{
				//Nightly Node Heal
				int nightly_heal = atoi(ValueVal.c_str());
				int old_nightly_heal = 0;
				m_sql.GetPreferencesVar("ZWaveEnableNightlyNetworkHeal", old_nightly_heal);
				if (old_nightly_heal != nightly_heal)
				{
					m_sql.UpdatePreferencesVar("ZWaveEnableNightlyNetworkHeal", nightly_heal);
					m_bNightlyNetworkHeal = (nightly_heal != 0);
				}
			}
			else if (rvIndex == 4)
			{
				//Security Key
				std::string networkkey = ValueVal;
				std::string old_networkkey = "";
				m_sql.GetPreferencesVar("ZWaveNetworkKey", old_networkkey);
				if (old_networkkey != networkkey)
				{
					m_sql.UpdatePreferencesVar("ZWaveNetworkKey", networkkey.c_str());
					bRestartOpenZWave = true;
				}
			}
		}
		else
		{
			OpenZWave::ValueID vID(0, 0, OpenZWave::ValueID::ValueGenre_Basic, 0, 0, 0, OpenZWave::ValueID::ValueType_Bool);

			if (GetNodeConfigValueByIndex(pNode, rvIndex, vID))
			{
				std::string vstring;
				m_pManager->GetValueAsString(vID, &vstring);

				OpenZWave::ValueID::ValueType vType = vID.GetType();

				if (vstring != ValueVal)
				{
					if (vType == OpenZWave::ValueID::ValueType_List)
					{
						m_pManager->SetValueListSelection(vID, ValueVal);
					}
					else
					{
						m_pManager->SetValue(vID, ValueVal);
					}
				}
			}
		}
		vindex += 2;
	}

	if (bRestartOpenZWave)
	{
		//Restart
		OpenSerialConnector();
	}

	return true;

}

//User Code routines
bool COpenZWave::SetUserCodeEnrollmentMode()
{
	m_ControllerCommandStartTime = mytime(NULL) + 10;//30 second timeout
	m_bControllerCommandInProgress = true;
	m_bControllerCommandCanceled = false;
	m_bInUserCodeEnrollmentMode = true;
	_log.Log(LOG_STATUS, "OpenZWave: User Code Enrollment mode initiated...");
	return false;
}

bool COpenZWave::GetNodeUserCodes(const unsigned int homeID, const int nodeID, Json::Value &root)
{
	int ii = 0;
	COpenZWave::NodeInfo *pNode = GetNodeInfo(homeID, nodeID);
	if (!pNode)
		return false;
	if (pNode->Instances.find(1) == pNode->Instances.end())
		return false; //no codes added yet, wake your tag reader

	for (std::list<OpenZWave::ValueID>::iterator itt = pNode->Instances[1][COMMAND_CLASS_USER_CODE].Values.begin(); itt != pNode->Instances[1][COMMAND_CLASS_USER_CODE].Values.end(); ++itt)
	{
		OpenZWave::ValueID vNode = *itt;
		if ((vNode.GetGenre() == OpenZWave::ValueID::ValueGenre_User) && (vNode.GetInstance() == 1) && (vNode.GetType() == OpenZWave::ValueID::ValueType_Raw))
		{
			int vNodeIndex = vNode.GetIndex();
			if (vNodeIndex >= 1)
			{
				std::string sValue;
				if (m_pManager->GetValueAsString(vNode, &sValue))
				{
					if (sValue.find("0x00 ") != 0)
					{
						root["result"][ii]["index"] = vNodeIndex;
						root["result"][ii]["code"] = sValue;
						ii++;
					}
				}
			}
		}
	}

	return true;
}

bool COpenZWave::RemoveUserCode(const unsigned int homeID, const int nodeID, const int codeIndex)
{
	COpenZWave::NodeInfo *pNode = GetNodeInfo(homeID, nodeID);
	if (!pNode)
		return false;
	if (pNode->Instances.find(1) == pNode->Instances.end())
		return false; //no codes added yet, wake your tag reader

	for (std::list<OpenZWave::ValueID>::iterator itt = pNode->Instances[1][COMMAND_CLASS_USER_CODE].Values.begin(); itt != pNode->Instances[1][COMMAND_CLASS_USER_CODE].Values.end(); ++itt)
	{
		OpenZWave::ValueID vNode = *itt;
		if ((vNode.GetGenre() == OpenZWave::ValueID::ValueGenre_User) && (vNode.GetInstance() == 1) && (vNode.GetType() == OpenZWave::ValueID::ValueType_Raw))
		{
			int vNodeIndex = vNode.GetIndex();
			if (vNodeIndex == codeIndex)
			{
				std::string sValue;
				if (m_pManager->GetValueAsString(vNode, &sValue))
				{
					//Set our code to zero
					//First find code length in bytes
					int cLength = (sValue.size() + 1) / 5;
					//Make an string with zero's
					sValue = "";
					for (int ii = 0; ii < cLength; ii++)
					{
						if (ii != cLength - 1)
							sValue += "0x00 ";
						else
							sValue += "0x00"; //last one
					}
					//Set the new (empty) code
					m_pManager->SetValue(vNode, sValue);
					break;
				}
			}
		}
	}

	return true;
}

void COpenZWave::NightlyNodeHeal()
{
	if (!m_bNightlyNetworkHeal)
		return; //not enabled
	HealNetwork();
}

#endif //WITH_OPENZWAVE

