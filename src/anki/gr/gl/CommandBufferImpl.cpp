// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <anki/gr/gl/CommandBufferImpl.h>
#include <anki/gr/GrManager.h>
#include <anki/gr/gl/GrManagerImpl.h>
#include <anki/gr/gl/GlState.h>
#include <anki/gr/gl/Error.h>

#include <anki/gr/OcclusionQuery.h>
#include <anki/gr/gl/OcclusionQueryImpl.h>
#include <anki/gr/Buffer.h>
#include <anki/gr/gl/BufferImpl.h>

#include <anki/util/Logger.h>
#include <anki/core/Trace.h>
#include <cstring>

namespace anki
{

void CommandBufferImpl::init(const CommandBufferInitInfo& init)
{
	auto& pool = m_manager->getAllocator().getMemoryPool();

	m_alloc = CommandBufferAllocator<GlCommand*>(
		pool.getAllocationCallback(), pool.getAllocationCallbackUserData(), init.m_hints.m_chunkSize, 1.0, 0, false);

	m_flags = init.m_flags;
}

void CommandBufferImpl::destroy()
{
	ANKI_TRACE_START_EVENT(GL_CMD_BUFFER_DESTROY);

#if ANKI_DEBUG
	if(!m_executed && m_firstCommand)
	{
		ANKI_LOGW("Chain contains commands but never executed. "
				  "This should only happen on exceptions");
	}
#endif

	GlCommand* command = m_firstCommand;
	while(command != nullptr)
	{
		GlCommand* next = command->m_nextCommand; // Get next before deleting
		m_alloc.deleteInstance(command);
		command = next;
	}

	ANKI_ASSERT(m_alloc.getMemoryPool().getUsersCount() == 1
		&& "Someone is holding a reference to the command buffer's allocator");

	m_alloc = CommandBufferAllocator<U8>();

	ANKI_TRACE_STOP_EVENT(GL_CMD_BUFFER_DESTROY);
}

Error CommandBufferImpl::executeAllCommands()
{
	ANKI_ASSERT(m_firstCommand != nullptr && "Empty command buffer");
	ANKI_ASSERT(m_lastCommand != nullptr && "Empty command buffer");
#if ANKI_DEBUG
	m_executed = true;
#endif

	Error err = ErrorCode::NONE;
	GlState& state = m_manager->getImplementation().getState();

	GlCommand* command = m_firstCommand;

	while(command != nullptr && !err)
	{
		err = (*command)(state);
		ANKI_CHECK_GL_ERROR();

		command = command->m_nextCommand;
	}

	return err;
}

CommandBufferImpl::InitHints CommandBufferImpl::computeInitHints() const
{
	InitHints out;
	out.m_chunkSize = m_alloc.getMemoryPool().getMemoryCapacity();

	return out;
}

GrAllocator<U8> CommandBufferImpl::getAllocator() const
{
	return m_manager->getAllocator();
}

void CommandBufferImpl::flushDrawcall(CommandBuffer& cmdb)
{
	ANKI_ASSERT(!!(m_flags & CommandBufferFlag::GRAPHICS_WORK));

	//
	// Set default state
	//
	if(ANKI_UNLIKELY(m_state.m_mayContainUnsetState))
	{
		m_state.m_mayContainUnsetState = false;

		if(m_state.m_primitiveRestart == 2)
		{
			cmdb.setPrimitiveRestart(false);
		}

		if(m_state.m_fillMode == FillMode::COUNT)
		{
			cmdb.setFillMode(FillMode::SOLID);
		}

		if(m_state.m_cullMode == static_cast<FaceSelectionMask>(0))
		{
			cmdb.setCullMode(FaceSelectionMask::BACK);
		}

		if(m_state.m_polyOffsetFactor == -1.0)
		{
			cmdb.setPolygonOffset(0.0, 0.0);
		}

		for(U i = 0; i < 2; ++i)
		{
			FaceSelectionMask face = (i == 0) ? FaceSelectionMask::FRONT : FaceSelectionMask::BACK;

			if(m_state.m_stencilFail[i] == StencilOperation::COUNT)
			{
				cmdb.setStencilOperations(face, StencilOperation::KEEP, StencilOperation::KEEP, StencilOperation::KEEP);
			}

			if(m_state.m_stencilCompare[i] == CompareOperation::COUNT)
			{
				cmdb.setStencilCompareFunction(face, CompareOperation::ALWAYS);
			}

			if(m_state.m_stencilCompareMask[i] == StateTracker::DUMMY_STENCIL_MASK)
			{
				cmdb.setStencilCompareMask(face, MAX_U32);
			}

			if(m_state.m_stencilWriteMask[i] == StateTracker::DUMMY_STENCIL_MASK)
			{
				cmdb.setStencilWriteMask(face, MAX_U32);
			}

			if(m_state.m_stencilRef[i] == StateTracker::DUMMY_STENCIL_MASK)
			{
				cmdb.setStencilReference(face, 0);
			}
		}

		if(m_state.m_depthWrite == 2)
		{
			cmdb.setDepthWrite(true);
		}

		if(m_state.m_depthOp == CompareOperation::COUNT)
		{
			cmdb.setDepthCompareFunction(CompareOperation::LESS);
		}

		for(U i = 0; i < MAX_COLOR_ATTACHMENTS; ++i)
		{
			if(m_state.m_colorWriteMasks[i] == StateTracker::INVALID_COLOR_MASK)
			{
				cmdb.setColorChannelWriteMask(i, ColorBit::ALL);
			}

			if(m_state.m_blendSrcMethod[i] == BlendMethod::COUNT)
			{
				cmdb.setBlendMethods(i, BlendMethod::ONE, BlendMethod::ZERO);
			}

			if(m_state.m_blendFuncs[i] == BlendFunction::COUNT)
			{
				cmdb.setBlendFunction(i, BlendFunction::ADD);
			}
		}
	}

	//
	// Fire commands to change some state
	//
	class Cmd final : public GlCommand
	{
	public:
		GLenum m_face;
		GLenum m_func;
		GLint m_ref;
		GLuint m_compareMask;

		Cmd(GLenum face, GLenum func, GLint ref, GLuint mask)
			: m_face(face)
			, m_func(func)
			, m_ref(ref)
			, m_compareMask(mask)
		{
		}

		Error operator()(GlState&)
		{
			glStencilFuncSeparate(m_face, m_func, m_ref, m_compareMask);
			return ErrorCode::NONE;
		}
	};

	const Array<GLenum, 2> FACE = {{GL_FRONT, GL_BACK}};
	for(U i = 0; i < 2; ++i)
	{
		if(m_state.m_glStencilFuncSeparateDirty[i])
		{
			pushBackNewCommand<Cmd>(FACE[i],
				convertCompareOperation(m_state.m_stencilCompare[i]),
				m_state.m_stencilRef[i],
				m_state.m_stencilCompareMask[i]);

			m_state.m_glStencilFuncSeparateDirty[i] = false;
		}
	}

	class DepthTestCmd final : public GlCommand
	{
	public:
		Bool8 m_enable;

		DepthTestCmd(Bool enable)
			: m_enable(enable)
		{
		}

		Error operator()(GlState&)
		{
			if(m_enable)
			{
				glEnable(GL_DEPTH_TEST);
			}
			else
			{
				glDisable(GL_DEPTH_TEST);
			}
			return ErrorCode::NONE;
		}
	};

	if(m_state.maybeEnableDepthTest())
	{
		pushBackNewCommand<DepthTestCmd>(m_state.m_depthTestEnabled);
	}

	class StencilTestCmd final : public GlCommand
	{
	public:
		Bool8 m_enable;

		StencilTestCmd(Bool enable)
			: m_enable(enable)
		{
		}

		Error operator()(GlState&)
		{
			if(m_enable)
			{
				glEnable(GL_STENCIL_TEST);
			}
			else
			{
				glDisable(GL_STENCIL_TEST);
			}
			return ErrorCode::NONE;
		}
	};

	if(m_state.maybeEnableStencilTest())
	{
		pushBackNewCommand<StencilTestCmd>(m_state.m_stencilTestEnabled);
	}
}

} // end namespace anki
