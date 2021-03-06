#include <AEvents.h>
#include <AEventHandler.h>
#include <BufferSegment.h>
#include <CommManagerInterface.h>
#include <CommTool.h>
#include <SerialOptions.h>
#include <CommOptions.h>
#include <Endpoint.h>
#include <UserTasks.h>
#include <NewtonScript.h>
#include <stdarg.h>

#include <CMService.h>
#include <CommServices.h>
#include <OptionArray.h>
#include <Protocols.h>

#include "RFCOMMTool.h"
#include "RFCOMMService.impl.h"

PROTOCOL_IMPL_SOURCE_MACRO(TRFCOMMService);

TCMService *TRFCOMMService::New ()
{
	return this;
}

void TRFCOMMService::Delete (void)
{
}

NewtonErr TRFCOMMService::Start(TOptionArray* options, ULong serviceId, TServiceInfo* serviceInfo)
{
	TUPort *port;
	TRFCOMMTool *tool;
	NewtonErr r;
	int fLogLevel = 0;
	
	r = ServiceToPort (serviceId, port);
	LOG ("-------------------------------------\nServiceToPort: %d\n", r);
	if (r == -10067) {
		tool = new TRFCOMMTool (serviceId);
		if (tool->fServerPort != 0) {
			LOG ("  New Tool: %08x\n", tool);
			r = StartCommTool (tool, serviceId, serviceInfo);
			LOG ("  StartCommTool: %d\n", r);
			r = OpenCommTool (serviceInfo->GetPortId (), options, this);
			LOG ("  OpenCommTool: %d (Port: %04x)\n", r, serviceInfo->GetPortId ());
		} else {
			r = kCommErrResourceNotAvailable;
			delete tool;
		}
	} else {
		serviceInfo->SetPortId (port->fId);
		serviceInfo->SetServiceId (serviceId);
	}
	return r;
}

NewtonErr TRFCOMMService::DoneStarting(TAEvent* event, ULong size, TServiceInfo* serviceInfo)
{
	return noErr;
}
