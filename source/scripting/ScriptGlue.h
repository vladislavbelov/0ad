
#ifndef _SCRIPTGLUE_H_
#define _SCRIPTGLUE_H_

#include "ScriptingHost.h"

JSBool WriteLog(JSContext * context, JSObject * globalObject, unsigned int argc, jsval *argv, jsval *rval);

JSFunctionSpec ScriptFunctionTable[];

#endif
