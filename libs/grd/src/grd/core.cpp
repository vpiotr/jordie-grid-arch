/////////////////////////////////////////////////////////////////////////////
// Name:        core.cpp
// Project:     scLib
// Purpose:     Multi-node scheduler infrastructure. Core elements.
// Author:      Piotr Likus
// Modified by:
// Created:     23/09/2008
// Licence:     BSD
/////////////////////////////////////////////////////////////////////////////

//#define SC_CORE_LOG_MSGS

// define to debug msg handler creation / destroy
//#define SC_DEBUG_HANDLERS

//std
#include <stdlib.h> 
#include <sstream>
#include <climits>
#include <memory>
#include <set>
#include <cassert>

//wx
#include "wx/utils.h"

//dtp
#include "base/date.h"
#include "base/rand.h"
#include "base/wildcard.h"

//sc
#include "sc/utils.h"

//perf
#include "perf/Log.h"
#include "perf/time_utils.h"

//grd
#include "grd/core.h"
#include "grd/EnvSerializerJsonYajl.h"

#ifdef DEBUG_MEM
#include "sc/DebugMem.h"
#endif

using namespace dtp;
using namespace perf;


