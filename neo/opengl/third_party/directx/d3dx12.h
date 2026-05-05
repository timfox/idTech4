//*********************************************************
// Thin wrapper: full d3dx12.h pulls many optional modules.
// IceBridge only needs pipeline state stream helpers for mesh PSOs.
//*********************************************************
#pragma once

#include "d3dx12_pipeline_state_stream.h"
