// Copyright (C) 2009-2015, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#pragma once

#include <anki/Gr.h>
#include <anki/util/Ptr.h>

namespace anki {

// Forward
class Renderer;
class Ms;
class Is;
class Fs;
class Lf;
class Ssao;
class Sslf;
class Tm;
class Bloom;
class Pps;
class Dbg;
class Tiler;
class Ir;
class Refl;

/// Cut the job submition into multiple chains. We want to avoid feeding
/// GL with a huge job chain
const U RENDERER_COMMAND_BUFFERS_COUNT = 2;

} // end namespace anki

