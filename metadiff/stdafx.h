#pragma once

#include "targetver.h"

#include <stdio.h>
#include <tchar.h>

#include <string>
#include <sstream>
#include <iomanip>
#include <memory>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <functional>

#include "../witutils/find_files.h"
#include "../witutils/find_files_wcs.h"
#include "../witutils/diff_utils.h"

#define VC_EXTRALEAN
#include <windows.h>
#include <atlbase.h>
//#include <comip.h>

//#include <RoMetadataApi.h>

// already included in RoMetadataApi.h
#include <cor.h>
#include <CorHdr.h>
#include <metahost.h>
