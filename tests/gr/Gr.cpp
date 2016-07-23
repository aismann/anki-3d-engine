// Copyright (C) 2009-2016, Panagiotis Christopoulos Charitos and contributors.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#include <tests/framework/Framework.h>
#include <anki/Gr.h>
#include <anki/core/NativeWindow.h>
#include <anki/core/Config.h>
#include <anki/util/HighRezTimer.h>
#include <anki/collision/Frustum.h>

namespace anki
{

const U WIDTH = 1024;
const U HEIGHT = 768;

static const char* VERT_SRC = R"(
out gl_PerVertex
{
	vec4 gl_Position;
};

void main()
{
	const vec2 POSITIONS[3] =
		vec2[](vec2(-1.0, 1.0), vec2(0.0, -1.0), vec2(1.0, 1.0));

	gl_Position = vec4(POSITIONS[gl_VertexID % 3], 0.0, 1.0);
#if defined(ANKI_VK)
	gl_Position.y = -gl_Position.y;
#endif
})";

static const char* VERT_UBO_SRC = R"(
out gl_PerVertex
{
	vec4 gl_Position;
};

layout(ANKI_UBO_BINDING(0, 0)) uniform u0_
{
	vec4 u_color[3];
};

layout(ANKI_UBO_BINDING(0, 1)) uniform u1_
{
	vec4 u_rotation2d;
};

layout(location = 0) out vec3 out_color;

void main()
{
	out_color = u_color[gl_VertexID].rgb;

	const vec2 POSITIONS[3] =
		vec2[](vec2(-1.0, 1.0), vec2(0.0, -1.0), vec2(1.0, 1.0));
		
	mat2 rot = mat2(
		u_rotation2d.x, u_rotation2d.y, u_rotation2d.z, u_rotation2d.w);
	vec2 pos = rot * POSITIONS[gl_VertexID % 3];

	gl_Position = vec4(pos, 0.0, 1.0);
#if defined(ANKI_VK)
	gl_Position.y = -gl_Position.y;
#endif
})";

static const char* VERT_INP_SRC = R"(
layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color0;
layout(location = 2) in vec3 in_color1;

out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out vec3 out_color0;
layout(location = 1) out vec3 out_color1;

void main()
{
	gl_Position = vec4(in_position, 1.0);
#if defined(ANKI_VK)
	gl_Position.y = -gl_Position.y;
#endif

	out_color0 = in_color0;
	out_color1 = in_color1;
})";

static const char* VERT_QUAD_SRC = R"(
out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) out vec2 out_uv;

void main()
{
	const vec2 POSITIONS[6] =
		vec2[](vec2(-1.0, 1.0), vec2(-1.0, -1.0), vec2(1.0, -1.0),
		vec2(1.0, -1.0), vec2(1.0, 1.0), vec2(-1.0, 1.0));

	gl_Position = vec4(POSITIONS[gl_VertexID], 0.0, 1.0);
#if defined(ANKI_VK)
	gl_Position.y = -gl_Position.y;
#endif
	out_uv = POSITIONS[gl_VertexID] / 2.0 + 0.5;
})";

static const char* VERT_MRT_SRC = R"(
out gl_PerVertex
{
	vec4 gl_Position;
};

layout(location = 0) in vec3 in_pos;

layout(ANKI_UBO_BINDING(0, 0), std140, row_major) uniform u0_
{
	mat4 u_mvp;
};

void main()
{
	gl_Position = u_mvp * vec4(in_pos, 1.0);
#if defined(ANKI_VK)
	gl_Position.y = -gl_Position.y;
#endif	
})";

static const char* FRAG_SRC = R"(layout (location = 0) out vec4 out_color;

void main()
{
	out_color = vec4(0.5);
})";

static const char* FRAG_UBO_SRC = R"(layout (location = 0) out vec4 out_color;

layout(location = 0) in vec3 in_color;

void main()
{
	out_color = vec4(in_color, 1.0);
})";

static const char* FRAG_INP_SRC = R"(layout (location = 0) out vec4 out_color;

layout(location = 0) in vec3 in_color0;
layout(location = 1) in vec3 in_color1;

void main()
{
	out_color = vec4(in_color0 + in_color1, 1.0);
})";

static const char* FRAG_TEX_SRC = R"(layout (location = 0) out vec4 out_color;

layout(location = 0) in vec2 in_uv;

layout(ANKI_UBO_BINDING(0, 0)) uniform u0_
{
	vec4 u_factor;
};

layout(ANKI_TEX_BINDING(0, 0)) uniform sampler2D u_tex0;
layout(ANKI_TEX_BINDING(0, 1)) uniform sampler2D u_tex1;

ANKI_USING_FRAG_COORD(768)

void main()
{
	if(anki_fragCoord.x < 1024 / 2)
	{
		if(anki_fragCoord.y < 768 / 2)
		{
			vec2 uv = in_uv * 2.0;
			out_color = textureLod(u_tex0, uv, 0.0);
		}
		else
		{
			vec2 uv = in_uv * 2.0 - vec2(0.0, 1.0);
			out_color = textureLod(u_tex0, uv, 1.0);
		}
	}
	else
	{
		if(anki_fragCoord.y < 768 / 2)
		{
			vec2 uv = in_uv * 2.0 - vec2(1.0, 0.0);
			out_color = textureLod(u_tex1, uv, 0.0);
		}
		else
		{
			vec2 uv = in_uv * 2.0 - vec2(1.0, 1.0);
			out_color = textureLod(u_tex1, uv, 1.0);
		}
	}
})";

static const char* FRAG_MRT_SRC = R"(layout (location = 0) out vec4 out_color0;
layout (location = 1) out vec4 out_color1;

layout(ANKI_UBO_BINDING(0, 1), std140) uniform u1_
{
	vec4 u_color0;
	vec4 u_color1;
};

void main()
{
	out_color0 = u_color0;
	out_color1 = u_color1;
})";

static const char* FRAG_MRT2_SRC = R"(layout (location = 0) out vec4 out_color;

layout(location = 0) in vec2 in_uv;

layout(ANKI_TEX_BINDING(0, 0)) uniform sampler2D u_tex0;
layout(ANKI_TEX_BINDING(0, 1)) uniform sampler2D u_tex1;

void main()
{
	float factor = in_uv.x;
	vec3 col0 = texture(u_tex0, in_uv).rgb;
	vec3 col1 = texture(u_tex1, in_uv).rgb;
	
	out_color = vec4(col1 + col0, 1.0);
})";

#define COMMON_BEGIN()                                                         \
	NativeWindow* win = nullptr;                                               \
	GrManager* gr = nullptr;                                                   \
	createGrManager(win, gr)

#define COMMON_END()                                                           \
	delete gr;                                                                 \
	delete win

//==============================================================================
static NativeWindow* createWindow()
{
	HeapAllocator<U8> alloc(allocAligned, nullptr);

	NativeWindowInitInfo inf;
	inf.m_width = WIDTH;
	inf.m_height = HEIGHT;
	NativeWindow* win = new NativeWindow();

	ANKI_TEST_EXPECT_NO_ERR(win->init(inf, alloc));

	return win;
}

//==============================================================================
static void createGrManager(NativeWindow*& win, GrManager*& gr)
{
	win = createWindow();
	gr = new GrManager();

	Config cfg;
	cfg.set("debugContext", 1);
	GrManagerInitInfo inf;
	inf.m_allocCallback = allocAligned;
	inf.m_cacheDirectory = "./";
	inf.m_config = &cfg;
	inf.m_window = win;
	ANKI_TEST_EXPECT_NO_ERR(gr->init(inf));
}

//==============================================================================
static PipelinePtr createSimplePpline(
	CString vertSrc, CString fragSrc, GrManager& gr)
{
	ShaderPtr vert = gr.newInstance<Shader>(ShaderType::VERTEX, vertSrc);
	ShaderPtr frag = gr.newInstance<Shader>(ShaderType::FRAGMENT, fragSrc);

	PipelineInitInfo init;
	init.m_shaders[ShaderType::VERTEX] = vert;
	init.m_shaders[ShaderType::FRAGMENT] = frag;
	init.m_color.m_drawsToDefaultFramebuffer = true;
	init.m_color.m_attachmentCount = 1;
	init.m_depthStencil.m_depthWriteEnabled = false;
	init.m_depthStencil.m_depthCompareFunction = CompareOperation::ALWAYS;

	return gr.newInstance<Pipeline>(init);
}

//==============================================================================
static FramebufferPtr createDefaultFb(GrManager& gr)
{
	FramebufferInitInfo fbinit;
	fbinit.m_colorAttachmentCount = 1;
	fbinit.m_colorAttachments[0].m_clearValue.m_colorf = {1.0, 0.0, 1.0, 1.0};

	return gr.newInstance<Framebuffer>(fbinit);
}

//==============================================================================
static void createCube(GrManager& gr, BufferPtr& verts, BufferPtr& indices)
{
	static const Array<F32, 8 * 3> pos = {{1,
		1,
		1,
		-1,
		1,
		1,
		-1,
		-1,
		1,
		1,
		-1,
		1,
		1,
		1,
		-1,
		-1,
		1,
		-1,
		-1,
		-1,
		-1,
		1,
		-1,
		-1}};

	static const Array<U16, 6 * 2 * 3> idx = {{0,
		1,
		3,
		3,
		1,
		2,
		1,
		5,
		6,
		1,
		6,
		2,
		7,
		4,
		0,
		7,
		0,
		3,
		6,
		5,
		7,
		7,
		5,
		4,
		0,
		4,
		5,
		0,
		5,
		1,
		3,
		2,
		6,
		3,
		6,
		7}};

	verts = gr.newInstance<Buffer>(
		sizeof(pos), BufferUsageBit::VERTEX, BufferMapAccessBit::WRITE);

	void* mapped = verts->map(0, sizeof(pos), BufferMapAccessBit::WRITE);
	memcpy(mapped, &pos[0], sizeof(pos));
	verts->unmap();

	indices = gr.newInstance<Buffer>(
		sizeof(idx), BufferUsageBit::INDEX, BufferMapAccessBit::WRITE);
	mapped = indices->map(0, sizeof(idx), BufferMapAccessBit::WRITE);
	memcpy(mapped, &idx[0], sizeof(idx));
	indices->unmap();
}

//==============================================================================
ANKI_TEST(Gr, GrManager)
{
	COMMON_BEGIN();
	COMMON_END();
}

//==============================================================================
ANKI_TEST(Gr, Shader)
{
	COMMON_BEGIN();

	{
		ShaderPtr shader =
			gr->newInstance<Shader>(ShaderType::VERTEX, VERT_SRC);
	}

	COMMON_END();
}

//==============================================================================
ANKI_TEST(Gr, Pipeline)
{
	COMMON_BEGIN();

	{
		PipelinePtr ppline = createSimplePpline(VERT_SRC, FRAG_SRC, *gr);
	}

	COMMON_END();
}

//==============================================================================
ANKI_TEST(Gr, SimpleDrawcall)
{
	COMMON_BEGIN();

	{
		PipelinePtr ppline = createSimplePpline(VERT_SRC, FRAG_SRC, *gr);
		FramebufferPtr fb = createDefaultFb(*gr);

		U iterations = 100;
		while(iterations--)
		{
			HighRezTimer timer;
			timer.start();

			gr->beginFrame();

			CommandBufferInitInfo cinit;
			cinit.m_flags =
				CommandBufferFlag::FRAME_FIRST | CommandBufferFlag::FRAME_LAST;
			CommandBufferPtr cmdb = gr->newInstance<CommandBuffer>(cinit);

			cmdb->setViewport(0, 0, WIDTH, HEIGHT);
			cmdb->setPolygonOffset(0.0, 0.0);
			cmdb->bindPipeline(ppline);
			cmdb->beginRenderPass(fb);
			cmdb->drawArrays(3);
			cmdb->endRenderPass();
			cmdb->flush();

			gr->swapBuffers();

			timer.stop();
			const F32 TICK = 1.0 / 30.0;
			if(timer.getElapsedTime() < TICK)
			{
				HighRezTimer::sleep(TICK - timer.getElapsedTime());
			}
		}
	}

	COMMON_END();
}

//==============================================================================
ANKI_TEST(Gr, Buffer)
{
	COMMON_BEGIN();

	{
		BufferPtr a = gr->newInstance<Buffer>(
			512, BufferUsageBit::UNIFORM_ANY_SHADER, BufferMapAccessBit::NONE);

		BufferPtr b = gr->newInstance<Buffer>(64,
			BufferUsageBit::STORAGE_ANY,
			BufferMapAccessBit::WRITE | BufferMapAccessBit::READ);

		void* ptr = b->map(0, 64, BufferMapAccessBit::WRITE);
		ANKI_TEST_EXPECT_NEQ(ptr, nullptr);
		U8 ptr2[64];
		memset(ptr, 0xCC, 64);
		memset(ptr2, 0xCC, 64);
		b->unmap();

		ptr = b->map(0, 64, BufferMapAccessBit::READ);
		ANKI_TEST_EXPECT_NEQ(ptr, nullptr);
		ANKI_TEST_EXPECT_EQ(memcmp(ptr, ptr2, 64), 0);
		b->unmap();
	}

	COMMON_END();
}

//==============================================================================
ANKI_TEST(Gr, ResourceGroup)
{
	COMMON_BEGIN();

	{
		BufferPtr b = gr->newInstance<Buffer>(sizeof(F32) * 4,
			BufferUsageBit::UNIFORM_ANY_SHADER,
			BufferMapAccessBit::WRITE);

		ResourceGroupInitInfo rcinit;
		rcinit.m_uniformBuffers[0].m_buffer = b;
		ResourceGroupPtr rc = gr->newInstance<ResourceGroup>(rcinit);
	}

	COMMON_END();
}

//==============================================================================
ANKI_TEST(Gr, DrawWithUniforms)
{
	COMMON_BEGIN();

	{
		// A non-uploaded buffer
		BufferPtr b = gr->newInstance<Buffer>(sizeof(Vec4) * 3,
			BufferUsageBit::UNIFORM_ANY_SHADER,
			BufferMapAccessBit::WRITE);

		Vec4* ptr = static_cast<Vec4*>(
			b->map(0, sizeof(Vec4) * 3, BufferMapAccessBit::WRITE));
		ANKI_TEST_EXPECT_NEQ(ptr, nullptr);
		ptr[0] = Vec4(1.0, 0.0, 0.0, 0.0);
		ptr[1] = Vec4(0.0, 1.0, 0.0, 0.0);
		ptr[2] = Vec4(0.0, 0.0, 1.0, 0.0);
		b->unmap();

		// Resource group
		ResourceGroupInitInfo rcinit;
		rcinit.m_uniformBuffers[0].m_buffer = b;
		rcinit.m_uniformBuffers[1].m_uploadedMemory = true;
		ResourceGroupPtr rc = gr->newInstance<ResourceGroup>(rcinit);

		// Ppline
		PipelinePtr ppline =
			createSimplePpline(VERT_UBO_SRC, FRAG_UBO_SRC, *gr);

		// FB
		FramebufferPtr fb = createDefaultFb(*gr);

		const U ITERATION_COUNT = 100;
		U iterations = ITERATION_COUNT;
		while(iterations--)
		{
			HighRezTimer timer;
			timer.start();
			gr->beginFrame();

			// Uploaded buffer
			TransientMemoryInfo transientInfo;
			Error err = ErrorCode::NONE;
			Vec4* rotMat = static_cast<Vec4*>(
				gr->allocateFrameTransientMemory(sizeof(Vec4),
					BufferUsageBit::UNIFORM_ANY_SHADER,
					transientInfo.m_uniformBuffers[1],
					&err));
			ANKI_TEST_EXPECT_NO_ERR(err);
			F32 angle = toRad(360.0f / ITERATION_COUNT * iterations);
			(*rotMat)[0] = cos(angle);
			(*rotMat)[1] = -sin(angle);
			(*rotMat)[2] = sin(angle);
			(*rotMat)[3] = cos(angle);

			CommandBufferInitInfo cinit;
			cinit.m_flags =
				CommandBufferFlag::FRAME_FIRST | CommandBufferFlag::FRAME_LAST;
			CommandBufferPtr cmdb = gr->newInstance<CommandBuffer>(cinit);

			cmdb->setViewport(0, 0, WIDTH, HEIGHT);
			cmdb->setPolygonOffset(0.0, 0.0);
			cmdb->bindPipeline(ppline);
			cmdb->beginRenderPass(fb);
			cmdb->bindResourceGroup(rc, 0, &transientInfo);
			cmdb->drawArrays(3);
			cmdb->endRenderPass();
			cmdb->flush();

			gr->swapBuffers();

			timer.stop();
			const F32 TICK = 1.0 / 30.0;
			if(timer.getElapsedTime() < TICK)
			{
				HighRezTimer::sleep(TICK - timer.getElapsedTime());
			}
		}
	}

	COMMON_END();
}

//==============================================================================
ANKI_TEST(Gr, DrawWithVertex)
{
	COMMON_BEGIN();

	{
		// The buffers
		struct Vert
		{
			Vec3 m_pos;
			Array<U8, 4> m_color;
		};
		static_assert(sizeof(Vert) == sizeof(Vec4), "See file");

		BufferPtr b = gr->newInstance<Buffer>(sizeof(Vert) * 3,
			BufferUsageBit::VERTEX,
			BufferMapAccessBit::WRITE);

		Vert* ptr = static_cast<Vert*>(
			b->map(0, sizeof(Vert) * 3, BufferMapAccessBit::WRITE));
		ANKI_TEST_EXPECT_NEQ(ptr, nullptr);

		ptr[0].m_pos = Vec3(-1.0, 1.0, 0.0);
		ptr[1].m_pos = Vec3(0.0, -1.0, 0.0);
		ptr[2].m_pos = Vec3(1.0, 1.0, 0.0);

		ptr[0].m_color = {255, 0, 0};
		ptr[1].m_color = {0, 255, 0};
		ptr[2].m_color = {0, 0, 255};
		b->unmap();

		BufferPtr c = gr->newInstance<Buffer>(sizeof(Vec3) * 3,
			BufferUsageBit::VERTEX,
			BufferMapAccessBit::WRITE);

		Vec3* otherColor = static_cast<Vec3*>(
			c->map(0, sizeof(Vec3) * 3, BufferMapAccessBit::WRITE));

		otherColor[0] = Vec3(0.0, 1.0, 1.0);
		otherColor[1] = Vec3(1.0, 0.0, 1.0);
		otherColor[2] = Vec3(1.0, 1.0, 0.0);
		c->unmap();

		// Resource group
		ResourceGroupInitInfo rcinit;
		rcinit.m_vertexBuffers[0].m_buffer = b;
		rcinit.m_vertexBuffers[1].m_buffer = c;
		ResourceGroupPtr rc = gr->newInstance<ResourceGroup>(rcinit);

		// Shaders
		ShaderPtr vert =
			gr->newInstance<Shader>(ShaderType::VERTEX, VERT_INP_SRC);
		ShaderPtr frag =
			gr->newInstance<Shader>(ShaderType::FRAGMENT, FRAG_INP_SRC);

		// Ppline
		PipelineInitInfo init;
		init.m_shaders[ShaderType::VERTEX] = vert;
		init.m_shaders[ShaderType::FRAGMENT] = frag;
		init.m_color.m_drawsToDefaultFramebuffer = true;
		init.m_color.m_attachmentCount = 1;
		init.m_depthStencil.m_depthWriteEnabled = false;

		init.m_vertex.m_attributeCount = 3;
		init.m_vertex.m_attributes[0].m_format =
			PixelFormat(ComponentFormat::R32G32B32, TransformFormat::FLOAT);
		init.m_vertex.m_attributes[1].m_format =
			PixelFormat(ComponentFormat::R8G8B8, TransformFormat::UNORM);
		init.m_vertex.m_attributes[1].m_offset = sizeof(Vec3);
		init.m_vertex.m_attributes[2].m_format =
			PixelFormat(ComponentFormat::R32G32B32, TransformFormat::FLOAT);
		init.m_vertex.m_attributes[2].m_binding = 1;

		init.m_vertex.m_bindingCount = 2;
		init.m_vertex.m_bindings[0].m_stride = sizeof(Vert);
		init.m_vertex.m_bindings[1].m_stride = sizeof(Vec3);

		PipelinePtr ppline = gr->newInstance<Pipeline>(init);

		// FB
		FramebufferPtr fb = createDefaultFb(*gr);

		U iterations = 100;
		while(iterations--)
		{
			HighRezTimer timer;
			timer.start();

			gr->beginFrame();

			CommandBufferInitInfo cinit;
			cinit.m_flags =
				CommandBufferFlag::FRAME_FIRST | CommandBufferFlag::FRAME_LAST;
			CommandBufferPtr cmdb = gr->newInstance<CommandBuffer>(cinit);

			cmdb->setViewport(0, 0, WIDTH, HEIGHT);
			cmdb->setPolygonOffset(0.0, 0.0);
			cmdb->bindPipeline(ppline);
			cmdb->beginRenderPass(fb);
			cmdb->bindResourceGroup(rc, 0, nullptr);
			cmdb->drawArrays(3);
			cmdb->endRenderPass();
			cmdb->flush();

			gr->swapBuffers();

			timer.stop();
			const F32 TICK = 1.0 / 30.0;
			if(timer.getElapsedTime() < TICK)
			{
				HighRezTimer::sleep(TICK - timer.getElapsedTime());
			}
		}
	}

	COMMON_END();
}

//==============================================================================
ANKI_TEST(Gr, Sampler)
{
	COMMON_BEGIN();

	{
		SamplerInitInfo init;

		SamplerPtr b = gr->newInstance<Sampler>(init);
	}

	COMMON_END();
}

//==============================================================================
ANKI_TEST(Gr, Texture)
{
	COMMON_BEGIN();

	{
		TextureInitInfo init;
		init.m_depth = 1;
		init.m_format =
			PixelFormat(ComponentFormat::R8G8B8, TransformFormat::UNORM);
		init.m_usage = TextureUsageBit::FRAGMENT_SHADER_SAMPLED;
		init.m_height = 4;
		init.m_width = 4;
		init.m_mipmapsCount = 2;
		init.m_depth = 1;
		init.m_layerCount = 1;
		init.m_samples = 1;
		init.m_sampling.m_minMagFilter = SamplingFilter::LINEAR;
		init.m_sampling.m_mipmapFilter = SamplingFilter::LINEAR;
		init.m_type = TextureType::_2D;

		TexturePtr b = gr->newInstance<Texture>(init);
	}

	COMMON_END();
}

//==============================================================================
ANKI_TEST(Gr, DrawWithTexture)
{
	COMMON_BEGIN();

	{
		//
		// Create texture A
		//
		TextureInitInfo init;
		init.m_depth = 1;
		init.m_format =
			PixelFormat(ComponentFormat::R8G8B8A8, TransformFormat::UNORM);
		init.m_usage =
			TextureUsageBit::FRAGMENT_SHADER_SAMPLED | TextureUsageBit::UPLOAD;
		init.m_initialUsage = TextureUsageBit::FRAGMENT_SHADER_SAMPLED;
		init.m_height = 2;
		init.m_width = 2;
		init.m_mipmapsCount = 2;
		init.m_samples = 1;
		init.m_depth = 1;
		init.m_layerCount = 1;
		init.m_sampling.m_repeat = false;
		init.m_sampling.m_minMagFilter = SamplingFilter::NEAREST;
		init.m_sampling.m_mipmapFilter = SamplingFilter::LINEAR;
		init.m_type = TextureType::_2D;

		TexturePtr a = gr->newInstance<Texture>(init);

		//
		// Create texture B
		//
		init.m_width = 4;
		init.m_height = 4;
		init.m_mipmapsCount = 3;
		init.m_usage = TextureUsageBit::FRAGMENT_SHADER_SAMPLED
			| TextureUsageBit::UPLOAD | TextureUsageBit::GENERATE_MIPMAPS;
		init.m_initialUsage = TextureUsageBit::NONE;

		TexturePtr b = gr->newInstance<Texture>(init);

		//
		// Upload all textures
		//
		Array<U8, 2 * 2 * 4> mip0 = {
			{255, 0, 0, 0, 0, 255, 0, 0, 0, 0, 255, 0, 255, 0, 255, 0}};

		Array<U8, 4> mip1 = {{128, 128, 128, 0}};

		Array<U8, 4 * 4 * 4> bmip0 = {{255,
			0,
			0,
			0,
			0,
			255,
			0,
			0,
			0,
			0,
			255,
			0,
			255,
			255,
			0,
			0,
			255,
			0,
			255,
			0,
			0,
			255,
			255,
			0,
			255,
			255,
			255,
			0,
			128,
			0,
			0,
			0,
			0,
			128,
			0,
			0,
			0,
			0,
			128,
			0,
			128,
			128,
			0,
			0,
			128,
			0,
			128,
			0,
			0,
			128,
			128,
			0,
			128,
			128,
			128,
			0,
			255,
			128,
			0,
			0,
			0,
			128,
			255,
			0}};

		CommandBufferInitInfo cmdbinit;
		cmdbinit.m_flags = CommandBufferFlag::TRANSFER_WORK;
		CommandBufferPtr cmdb = gr->newInstance<CommandBuffer>(cmdbinit);

		// Set barriers
		cmdb->setTextureBarrier(a,
			TextureUsageBit::FRAGMENT_SHADER_SAMPLED,
			TextureUsageBit::UPLOAD,
			TextureSurfaceInfo(0, 0, 0, 0));

		cmdb->setTextureBarrier(a,
			TextureUsageBit::FRAGMENT_SHADER_SAMPLED,
			TextureUsageBit::UPLOAD,
			TextureSurfaceInfo(1, 0, 0, 0));

		cmdb->setTextureBarrier(b,
			TextureUsageBit::NONE,
			TextureUsageBit::UPLOAD,
			TextureSurfaceInfo(0, 0, 0, 0));

		Error err = ErrorCode::NONE;
		TransientMemoryToken token;
		void* ptr = gr->allocateFrameTransientMemory(
			sizeof(mip0), BufferUsageBit::TRANSFER_SOURCE, token, &err);
		ANKI_TEST_EXPECT_NEQ(ptr, nullptr);
		ANKI_TEST_EXPECT_NO_ERR(err);
		memcpy(ptr, &mip0[0], sizeof(mip0));

		cmdb->uploadTextureSurface(a, TextureSurfaceInfo(0, 0, 0, 0), token);

		ptr = gr->allocateFrameTransientMemory(
			sizeof(mip1), BufferUsageBit::TRANSFER_SOURCE, token, &err);
		ANKI_TEST_EXPECT_NEQ(ptr, nullptr);
		ANKI_TEST_EXPECT_NO_ERR(err);
		memcpy(ptr, &mip1[0], sizeof(mip1));

		cmdb->uploadTextureSurface(a, TextureSurfaceInfo(1, 0, 0, 0), token);

		ptr = gr->allocateFrameTransientMemory(
			sizeof(bmip0), BufferUsageBit::TRANSFER_SOURCE, token, &err);
		ANKI_TEST_EXPECT_NEQ(ptr, nullptr);
		ANKI_TEST_EXPECT_NO_ERR(err);
		memcpy(ptr, &bmip0[0], sizeof(bmip0));

		cmdb->uploadTextureSurface(b, TextureSurfaceInfo(0, 0, 0, 0), token);

		// Gen mips
		cmdb->setTextureBarrier(b,
			TextureUsageBit::UPLOAD,
			TextureUsageBit::GENERATE_MIPMAPS,
			TextureSurfaceInfo(0, 0, 0, 0));

		cmdb->generateMipmaps(b, 0, 0, 0);

		// Set barriers
		cmdb->setTextureBarrier(a,
			TextureUsageBit::UPLOAD,
			TextureUsageBit::FRAGMENT_SHADER_SAMPLED,
			TextureSurfaceInfo(0, 0, 0, 0));

		cmdb->setTextureBarrier(a,
			TextureUsageBit::UPLOAD,
			TextureUsageBit::FRAGMENT_SHADER_SAMPLED,
			TextureSurfaceInfo(1, 0, 0, 0));

		for(U i = 0; i < 3; ++i)
		{
			cmdb->setTextureBarrier(b,
				TextureUsageBit::GENERATE_MIPMAPS,
				TextureUsageBit::FRAGMENT_SHADER_SAMPLED,
				TextureSurfaceInfo(i, 0, 0, 0));
		}

		cmdb->flush();

		//
		// Create resource group
		//
		ResourceGroupInitInfo rcinit;
		rcinit.m_textures[0].m_texture = a;
		rcinit.m_textures[1].m_texture = b;
		ResourceGroupPtr rc = gr->newInstance<ResourceGroup>(rcinit);

		//
		// Create ppline
		//
		PipelinePtr ppline =
			createSimplePpline(VERT_QUAD_SRC, FRAG_TEX_SRC, *gr);

		//
		// Create FB
		//
		FramebufferPtr fb = createDefaultFb(*gr);

		//
		// Draw
		//
		const U ITERATION_COUNT = 200;
		U iterations = ITERATION_COUNT;
		while(iterations--)
		{
			HighRezTimer timer;
			timer.start();

			gr->beginFrame();

			CommandBufferInitInfo cinit;
			cinit.m_flags =
				CommandBufferFlag::FRAME_FIRST | CommandBufferFlag::FRAME_LAST;
			CommandBufferPtr cmdb = gr->newInstance<CommandBuffer>(cinit);

			cmdb->setViewport(0, 0, WIDTH, HEIGHT);
			cmdb->setPolygonOffset(0.0, 0.0);
			cmdb->bindPipeline(ppline);
			cmdb->beginRenderPass(fb);
			cmdb->bindResourceGroup(rc, 0, nullptr);
			cmdb->drawArrays(6);
			cmdb->endRenderPass();
			cmdb->flush();

			gr->swapBuffers();

			timer.stop();
			const F32 TICK = 1.0 / 30.0;
			if(timer.getElapsedTime() < TICK)
			{
				HighRezTimer::sleep(TICK - timer.getElapsedTime());
			}
		}
	}

	COMMON_END();
}

//==============================================================================
ANKI_TEST(Gr, DrawOffscreen)
{
	COMMON_BEGIN();
	{
		//
		// Create textures
		//
		const PixelFormat COL_FORMAT =
			PixelFormat(ComponentFormat::R8G8B8A8, TransformFormat::UNORM);
		const PixelFormat DS_FORMAT =
			PixelFormat(ComponentFormat::D24, TransformFormat::UNORM);
		const U TEX_SIZE = 256;

		TextureInitInfo init;
		init.m_depth = 1;
		init.m_format = COL_FORMAT;
		init.m_usage = TextureUsageBit::FRAGMENT_SHADER_SAMPLED
			| TextureUsageBit::FRAMEBUFFER_ATTACHMENT_READ_WRITE;
		init.m_height = TEX_SIZE;
		init.m_width = TEX_SIZE;
		init.m_mipmapsCount = 1;
		init.m_depth = 1;
		init.m_layerCount = 1;
		init.m_samples = 1;
		init.m_sampling.m_minMagFilter = SamplingFilter::LINEAR;
		init.m_sampling.m_mipmapFilter = SamplingFilter::LINEAR;
		init.m_type = TextureType::_2D;

		TexturePtr col0 = gr->newInstance<Texture>(init);
		TexturePtr col1 = gr->newInstance<Texture>(init);

		init.m_format = DS_FORMAT;
		TexturePtr dp = gr->newInstance<Texture>(init);

		//
		// Create FB
		//
		FramebufferInitInfo fbinit;
		fbinit.m_colorAttachmentCount = 2;
		fbinit.m_colorAttachments[0].m_texture = col0;
		fbinit.m_colorAttachments[0].m_clearValue.m_colorf = {
			0.1, 0.0, 0.0, 0.0};
		fbinit.m_colorAttachments[1].m_texture = col1;
		fbinit.m_colorAttachments[1].m_clearValue.m_colorf = {
			0.0, 0.1, 0.0, 0.0};
		fbinit.m_depthStencilAttachment.m_texture = dp;
		fbinit.m_depthStencilAttachment.m_clearValue.m_depthStencil.m_depth =
			1.0;

		FramebufferPtr fb = gr->newInstance<Framebuffer>(fbinit);

		//
		// Create default FB
		//
		FramebufferPtr dfb = createDefaultFb(*gr);

		//
		// Create buffs and rc groups
		//
		BufferPtr verts, indices;
		createCube(*gr, verts, indices);

		ResourceGroupInitInfo rcinit;
		rcinit.m_uniformBuffers[0].m_uploadedMemory = true;
		rcinit.m_uniformBuffers[1].m_uploadedMemory = true;
		rcinit.m_vertexBuffers[0].m_buffer = verts;
		rcinit.m_indexBuffer.m_buffer = indices;
		rcinit.m_indexSize = 2;

		ResourceGroupPtr rc0 = gr->newInstance<ResourceGroup>(rcinit);

		rcinit = {};
		rcinit.m_textures[0].m_texture = col0;
		rcinit.m_textures[1].m_texture = col1;
		ResourceGroupPtr rc1 = gr->newInstance<ResourceGroup>(rcinit);

		//
		// Create pplines
		//
		ShaderPtr vert =
			gr->newInstance<Shader>(ShaderType::VERTEX, VERT_MRT_SRC);
		ShaderPtr frag =
			gr->newInstance<Shader>(ShaderType::FRAGMENT, FRAG_MRT_SRC);

		PipelineInitInfo pinit;
		pinit.m_shaders[ShaderType::VERTEX] = vert;
		pinit.m_shaders[ShaderType::FRAGMENT] = frag;
		pinit.m_color.m_drawsToDefaultFramebuffer = false;
		pinit.m_color.m_attachmentCount = 2;
		pinit.m_color.m_attachments[0].m_format = COL_FORMAT;
		pinit.m_color.m_attachments[1].m_format = COL_FORMAT;
		pinit.m_depthStencil.m_depthWriteEnabled = true;
		pinit.m_depthStencil.m_format = DS_FORMAT;

		pinit.m_vertex.m_attributeCount = 1;
		pinit.m_vertex.m_attributes[0].m_format =
			PixelFormat(ComponentFormat::R32G32B32, TransformFormat::FLOAT);

		pinit.m_vertex.m_bindingCount = 1;
		pinit.m_vertex.m_bindings[0].m_stride = sizeof(Vec3);

		PipelinePtr ppline = gr->newInstance<Pipeline>(pinit);

		PipelinePtr pplineResolve =
			createSimplePpline(VERT_QUAD_SRC, FRAG_MRT2_SRC, *gr);

		//
		// Draw
		//
		Mat4 viewMat(Vec4(0.0, 0.0, 5.0, 1.0), Mat3::getIdentity(), 1.0f);
		viewMat.invert();

		Mat4 projMat;
		PerspectiveFrustum::calculateProjectionMatrix(
			toRad(60.0), toRad(60.0), 0.1f, 100.0f, projMat);

		const U ITERATION_COUNT = 200;
		U iterations = ITERATION_COUNT;
		F32 ang = 0.0;
		while(iterations--)
		{
			HighRezTimer timer;
			timer.start();
			gr->beginFrame();

			TransientMemoryInfo transientInfo;

			CommandBufferInitInfo cinit;
			cinit.m_flags =
				CommandBufferFlag::FRAME_FIRST | CommandBufferFlag::FRAME_LAST;
			CommandBufferPtr cmdb = gr->newInstance<CommandBuffer>(cinit);

			cmdb->setPolygonOffset(0.0, 0.0);

			cmdb->setTextureBarrier(col0,
				TextureUsageBit::NONE,
				TextureUsageBit::FRAMEBUFFER_ATTACHMENT_WRITE,
				TextureSurfaceInfo(0, 0, 0, 0));
			cmdb->setTextureBarrier(col1,
				TextureUsageBit::NONE,
				TextureUsageBit::FRAMEBUFFER_ATTACHMENT_WRITE,
				TextureSurfaceInfo(0, 0, 0, 0));
			cmdb->setTextureBarrier(dp,
				TextureUsageBit::NONE,
				TextureUsageBit::FRAMEBUFFER_ATTACHMENT_READ_WRITE,
				TextureSurfaceInfo(0, 0, 0, 0));
			cmdb->beginRenderPass(fb);

			// First draw
			Mat4 modelMat(Vec4(0.0, 0.0, 0.0, 1.0),
				Mat3(Euler(ang, ang / 2.0f, ang / 3.0f)),
				1.0f);

			Mat4* mvp = static_cast<Mat4*>(
				gr->allocateFrameTransientMemory(sizeof(*mvp),
					BufferUsageBit::UNIFORM_ANY_SHADER,
					transientInfo.m_uniformBuffers[0],
					nullptr));
			*mvp = projMat * viewMat * modelMat;

			Vec4* color = static_cast<Vec4*>(
				gr->allocateFrameTransientMemory(sizeof(*color) * 2,
					BufferUsageBit::UNIFORM_ANY_SHADER,
					transientInfo.m_uniformBuffers[1],
					nullptr));
			*color++ = Vec4(1.0, 0.0, 0.0, 0.0);
			*color = Vec4(0.0, 1.0, 0.0, 0.0);

			cmdb->bindPipeline(ppline);
			cmdb->setViewport(0, 0, TEX_SIZE, TEX_SIZE);
			cmdb->bindResourceGroup(rc0, 0, &transientInfo);
			cmdb->drawElements(6 * 2 * 3);

			// 2nd draw
			modelMat = Mat4(Vec4(0.0, 0.0, 0.0, 1.0),
				Mat3(Euler(ang * 2.0, ang, ang / 3.0f * 2.0)),
				1.0f);

			mvp = static_cast<Mat4*>(
				gr->allocateFrameTransientMemory(sizeof(*mvp),
					BufferUsageBit::UNIFORM_ANY_SHADER,
					transientInfo.m_uniformBuffers[0],
					nullptr));
			*mvp = projMat * viewMat * modelMat;

			color = static_cast<Vec4*>(
				gr->allocateFrameTransientMemory(sizeof(*color) * 2,
					BufferUsageBit::UNIFORM_ANY_SHADER,
					transientInfo.m_uniformBuffers[1],
					nullptr));
			*color++ = Vec4(0.0, 0.0, 1.0, 0.0);
			*color = Vec4(0.0, 1.0, 1.0, 0.0);

			cmdb->bindResourceGroup(rc0, 0, &transientInfo);
			cmdb->drawElements(6 * 2 * 3);

			cmdb->endRenderPass();

			cmdb->setTextureBarrier(col0,
				TextureUsageBit::FRAMEBUFFER_ATTACHMENT_WRITE,
				TextureUsageBit::FRAGMENT_SHADER_SAMPLED,
				TextureSurfaceInfo(0, 0, 0, 0));
			cmdb->setTextureBarrier(col1,
				TextureUsageBit::FRAMEBUFFER_ATTACHMENT_WRITE,
				TextureUsageBit::FRAGMENT_SHADER_SAMPLED,
				TextureSurfaceInfo(0, 0, 0, 0));
			cmdb->setTextureBarrier(dp,
				TextureUsageBit::FRAMEBUFFER_ATTACHMENT_READ_WRITE,
				TextureUsageBit::FRAGMENT_SHADER_SAMPLED,
				TextureSurfaceInfo(0, 0, 0, 0));

			// Draw quad
			cmdb->beginRenderPass(dfb);
			cmdb->bindPipeline(pplineResolve);
			cmdb->setViewport(0, 0, WIDTH, HEIGHT);
			cmdb->bindResourceGroup(rc1, 0, nullptr);
			cmdb->drawArrays(6);
			cmdb->endRenderPass();

			cmdb->flush();

			// End
			ang += toRad(2.5f);
			gr->swapBuffers();

			timer.stop();
			const F32 TICK = 1.0 / 30.0;
			if(timer.getElapsedTime() < TICK)
			{
				HighRezTimer::sleep(TICK - timer.getElapsedTime());
			}
		}
	}
	COMMON_END();
}

} // end namespace anki
