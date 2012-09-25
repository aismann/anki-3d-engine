#include "anki/gl/ShaderProgram.h"
#include "anki/gl/GlException.h"
#include "anki/math/Math.h"
#include "anki/util/Exception.h"
#include "anki/gl/Texture.h"
#include "anki/core/Logger.h"
#include "anki/util/StringList.h"
#include "anki/util/Array.h"
#include <sstream>
#include <iomanip>

namespace anki {

//==============================================================================

static const char* padding = "======================================="
                             "=======================================";

//==============================================================================
// ShaderProgramUniformVariable                                                =
//==============================================================================

//==============================================================================
void ShaderProgramUniformVariable::doCommonSetCode() const
{
	ANKI_ASSERT(getLocation() != -1
		&& "You cannot set variable in uniform block");
	ANKI_ASSERT(ShaderProgram::getCurrentProgramGlId() == 
		getFatherShaderProgram().getGlId());

	/*enableFlag(SPUVF_DIRTY);*/
}

//==============================================================================
void ShaderProgramUniformVariable::set(const F32 x) const
{
	doCommonSetCode();
	ANKI_ASSERT(getGlDataType() == GL_FLOAT);
	ANKI_ASSERT(getSize() == 1);

	glUniform1f(getLocation(), x);
}

//==============================================================================
void ShaderProgramUniformVariable::set(const Vec2& x) const
{
	doCommonSetCode();
	ANKI_ASSERT(getGlDataType() == GL_FLOAT_VEC2);
	ANKI_ASSERT(getSize() == 1);

	glUniform2f(getLocation(), x.x(), x.y());
}

//==============================================================================
void ShaderProgramUniformVariable::set(const F32 x[], uint size) const
{
	doCommonSetCode();
	ANKI_ASSERT(getGlDataType() == GL_FLOAT);
	ANKI_ASSERT(getSize() == size);
	
	glUniform1fv(getLocation(), size, x);
}

//==============================================================================
void ShaderProgramUniformVariable::set(const Vec2 x[], uint size) const
{
	doCommonSetCode();
	ANKI_ASSERT(getGlDataType() == GL_FLOAT_VEC2);
	ANKI_ASSERT(getSize() == size);

	glUniform2fv(getLocation(), size, &(const_cast<Vec2&>(x[0]))[0]);
}

//==============================================================================
void ShaderProgramUniformVariable::set(const Vec3 x[], uint size) const
{
	doCommonSetCode();
	ANKI_ASSERT(getGlDataType() == GL_FLOAT_VEC3);
	ANKI_ASSERT(getSize() == size);

	glUniform3fv(getLocation(), size, &(const_cast<Vec3&>(x[0]))[0]);
}

//==============================================================================
void ShaderProgramUniformVariable::set(const Vec4 x[], uint size) const
{
	doCommonSetCode();
	ANKI_ASSERT(getGlDataType() == GL_FLOAT_VEC4);
	ANKI_ASSERT(getSize() == size);
	
	glUniform4fv(getLocation(), size, &(const_cast<Vec4&>(x[0]))[0]);
}

//==============================================================================
void ShaderProgramUniformVariable::set(const Mat3 x[], uint size) const
{
	doCommonSetCode();
	ANKI_ASSERT(getGlDataType() == GL_FLOAT_MAT3);
	ANKI_ASSERT(getSize() == size);

	glUniformMatrix3fv(getLocation(), size, true, &(x[0])[0]);
}

//==============================================================================
void ShaderProgramUniformVariable::set(const Mat4 x[], uint size) const
{
	doCommonSetCode();
	ANKI_ASSERT(getGlDataType() == GL_FLOAT_MAT4);
	ANKI_ASSERT(getSize() == size);

	glUniformMatrix4fv(getLocation(), size, true, &(x[0])[0]);
}

//==============================================================================
void ShaderProgramUniformVariable::set(const Texture& tex) const
{
	doCommonSetCode();
	ANKI_ASSERT(getGlDataType() == GL_SAMPLER_2D 
		|| getGlDataType() == GL_SAMPLER_2D_SHADOW
		|| getGlDataType() == GL_UNSIGNED_INT_SAMPLER_2D);
	
	glUniform1i(getLocation(), tex.bind());
}

//==============================================================================
// ShaderProgramUniformBlock                                                   =
//==============================================================================

//==============================================================================
ShaderProgramUniformBlock& ShaderProgramUniformBlock::operator=(
	const ShaderProgramUniformBlock& b)
{
	uniforms = b.uniforms;
	index = b.index;
	size = b.size;
	name = b.name;
	bindingPoint = b.bindingPoint;
	progId = b.progId;
	return *this;
}

//==============================================================================
// ShaderProgram                                                               =
//==============================================================================

//==============================================================================

const char* ShaderProgram::stdSourceCode =
	"#version 420 core\n"
	//"precision lowp float;\n"
#if defined(NDEBUG)
	"#pragma optimize(on)\n"
	"#pragma debug(off)\n"
	"#extension GL_ARB_gpu_shader5 : enable\n";
#else
	"#pragma optimize(off)\n"
	"#pragma debug(on)\n"
	"#extension GL_ARB_gpu_shader5 : enable\n";
#endif

thread_local const ShaderProgram* ShaderProgram::current = nullptr;

//==============================================================================
void ShaderProgram::create(const char* vertSource, const char* tcSource, 
	const char* teSource, const char* geomSource, const char* fragSource,
	const char* transformFeedbackVaryings[])
{
	ANKI_ASSERT(!isCreated());

	// 1) create and compile the shaders
	//
	std::string preprocSource = stdSourceCode;

	ANKI_ASSERT(vertSource != nullptr);
	vertShaderGlId = createAndCompileShader(vertSource, preprocSource.c_str(),
		GL_VERTEX_SHADER);

	if(tcSource != nullptr)
	{
		tcShaderGlId = createAndCompileShader(tcSource, preprocSource.c_str(), 
			GL_TESS_CONTROL_SHADER);
	}

	if(teSource != nullptr)
	{
		teShaderGlId = createAndCompileShader(teSource, preprocSource.c_str(), 
			GL_TESS_EVALUATION_SHADER);
	}

	if(geomSource != nullptr)
	{
		geomShaderGlId = createAndCompileShader(geomSource, 
			preprocSource.c_str(), GL_GEOMETRY_SHADER);
	}

	ANKI_ASSERT(fragSource != nullptr);
	fragShaderGlId = createAndCompileShader(fragSource, preprocSource.c_str(),
		GL_FRAGMENT_SHADER);

	// 2) create program and attach shaders
	glId = glCreateProgram();
	if(glId == 0)
	{
		throw ANKI_EXCEPTION("glCreateProgram() failed");
	}
	glAttachShader(glId, vertShaderGlId);
	glAttachShader(glId, fragShaderGlId);

	if(tcSource != nullptr)
	{
		glAttachShader(glId, tcShaderGlId);
	}

	if(teSource != nullptr)
	{
		glAttachShader(glId, teShaderGlId);
	}

	if(geomSource != nullptr)
	{
		glAttachShader(glId, geomShaderGlId);
	}

	// 3) set the TRFFB varyings
	ANKI_ASSERT(transformFeedbackVaryings != nullptr);
	int count = 0;
	while(transformFeedbackVaryings[count] != nullptr)
	{
		++count;
	}

	if(count)
	{
		glTransformFeedbackVaryings(
			glId,
			count, 
			transformFeedbackVaryings,
			GL_SEPARATE_ATTRIBS);
	}

	// 4) link
	link();

	// init the rest
	bind();
	initUniAndAttribVars();
	initUniformBlocks();
}

//==============================================================================
void ShaderProgram::destroy()
{
	unbind();

	if(vertShaderGlId != 0)
	{
		glDeleteShader(vertShaderGlId);
	}

	if(tcShaderGlId != 0)
	{
		glDeleteShader(tcShaderGlId);
	}

	if(teShaderGlId != 0)
	{
		glDeleteShader(teShaderGlId);
	}

	if(geomShaderGlId != 0)
	{
		glDeleteShader(geomShaderGlId);
	}

	if(fragShaderGlId != 0)
	{
		glDeleteShader(fragShaderGlId);
	}

	if(glId != 0)
	{
		glDeleteProgram(glId);
	}

	init();
}

//==============================================================================
GLuint ShaderProgram::createAndCompileShader(const char* sourceCode,
	const char* preproc, GLenum type)
{
	uint glId = 0;
	const char* sourceStrs[1] = {nullptr};

	// create the shader
	glId = glCreateShader(type);

	// attach the source
	std::string fullSrc = preproc;
	fullSrc += sourceCode;
	sourceStrs[0] = fullSrc.c_str();

	// compile
	glShaderSource(glId, 1, sourceStrs, NULL);
	glCompileShader(glId);

	int success;
	glGetShaderiv(glId, GL_COMPILE_STATUS, &success);

	if(!success)
	{
		// Get info log
		int infoLen = 0;
		int charsWritten = 0;
		Vector<char> infoLog;

		glGetShaderiv(glId, GL_INFO_LOG_LENGTH, &infoLen);
		infoLog.resize(infoLen + 1);
		glGetShaderInfoLog(glId, infoLen, &charsWritten, &infoLog[0]);
		infoLog[charsWritten] = '\0';

		std::stringstream err;
		err << "Shader compile failed (0x" << std::hex << type << std::dec
			<< "):\n" << padding << "\n" << &infoLog[0]
			<< "\n" << padding << "\nSource:\n" << padding << "\n";

		// Prettyfy source
		StringList lines = StringList::splitString(fullSrc.c_str(), '\n', true);
		int lineno = 0;
		for(const std::string& line : lines)
		{
			err << std::setw(4) << std::setfill('0') << ++lineno << ": "
				<< line << std::endl;
		}

		err << padding;
		
		// Throw
		throw ANKI_EXCEPTION(err.str());
	}

	return glId;
}

//==============================================================================
void ShaderProgram::link() const
{
	// link
	glLinkProgram(glId);

	// check if linked correctly
	GLint success;
	glGetProgramiv(glId, GL_LINK_STATUS, &success);

	if(!success)
	{
		int info_len = 0;
		int charsWritten = 0;
		std::string infoLogTxt;

		glGetProgramiv(glId, GL_INFO_LOG_LENGTH, &info_len);

		infoLogTxt.resize(info_len + 1);
		glGetProgramInfoLog(glId, info_len, &charsWritten, &infoLogTxt[0]);
		throw ANKI_EXCEPTION("Link error log follows:\n" 
			+ infoLogTxt);
	}
}

//==============================================================================
void ShaderProgram::initUniAndAttribVars()
{
	GLint num;
	Array<char, 256> name_;
	GLsizei length;
	GLint size;
	GLenum type;

	//
	// attrib locations
	//
	U32 attribsCount = (U32)num;
	glGetProgramiv(glId, GL_ACTIVE_ATTRIBUTES, &num);

	// Count the _useful_ attribs
	for(GLint i = 0; i < num; i++)
	{
		// Name
		glGetActiveAttrib(glId, i, sizeof(name_), &length, 
			&size, &type, &name_[0]);
		name_[length] = '\0';

		// check if its FFP location
		GLint loc = glGetAttribLocation(glId, &name_[0]);

		if(loc == -1)
		{
			// if -1 it means that its an FFP var or a weird crap like
			// gl_InstanceID
			--attribsCount;
		}
	}

	attribs.resize(attribsCount);
	attribs.shrink_to_fit();
	for(int i = 0; i < num; i++) // loop all attributes
	{
		ShaderProgramAttributeVariable& var = attribs[i];

		// Name
		glGetActiveAttrib(glId, i, sizeof(name_), &length,
			&size, &type, &name_[0]);
		name_[length] = '\0';

		// check if its FFP location
		GLint loc = glGetAttribLocation(glId, &name_[0]);
		if(loc == -1)
		{
			// if -1 it means that its an FFP var or a weird crap like
			// gl_InstanceID
			continue;
		}

		var.loc = loc;
		var.name = &name_[0];
		var.glDataType = type;
		var.size = size;
		var.fatherSProg = this;

		nameToAttribVar[var.name.c_str()] = &var;
	}

	//
	// uni locations
	//
	glGetProgramiv(glId, GL_ACTIVE_UNIFORMS, &num);
	unis.resize(num);
	unis.shrink_to_fit();
	for(GLint i = 0; i < num; i++) // loop all uniforms
	{
		ShaderProgramUniformVariable& var = unis[i];

		glGetActiveUniform(glId, i, sizeof(name_), &length,
			&size, &type, &name_[0]);
		name_[length] = '\0';

		// -1 means in uniform block
		GLint loc = glGetUniformLocation(glId, &name_[0]);

		var.loc = loc;
		var.name = &name_[0];
		var.glDataType = type;
		var.size = size;
		var.fatherSProg = this;

		var.index = (GLuint)i;

		nameToUniVar[var.name.c_str()] = &var;
	}
}

//==============================================================================
void ShaderProgram::initUniformBlocks()
{
	GLint blocksCount;
	glGetProgramiv(glId, GL_ACTIVE_UNIFORM_BLOCKS, &blocksCount);

	blocks.resize(blocksCount);
	blocks.shrink_to_fit();

	GLuint i = 0;
	for(ShaderProgramUniformBlock& block : blocks)
	{
		GLint gli; // General purpose int

		// Name
		char name[256];
		GLsizei len;
		glGetActiveUniformBlockName(glId, i, sizeof(name), &len, name);

		block.name = name;
		block.name.shrink_to_fit();

		// Index
		ANKI_ASSERT(glGetUniformBlockIndex(glId, name) == i);
		block.index = i;

		// Size
		glGetActiveUniformBlockiv(glId, i, GL_UNIFORM_BLOCK_DATA_SIZE, &gli);
		block.size = gli;

		// Binding point
		glGetActiveUniformBlockiv(glId, i, GL_UNIFORM_BLOCK_BINDING, &gli);
		block.bindingPoint = gli;

		// Prog id
		block.progId = glId;

		// Other update
		nameToBlock[block.name.c_str()] = &block;
		++i;
	}

	// Connect uniforms and blocks
	for(ShaderProgramUniformVariable& uni : unis)
	{
		/* Block index */
		GLint blockIndex;
		glGetActiveUniformsiv(glId, 1, &(uni.index),  GL_UNIFORM_BLOCK_INDEX, 
			&blockIndex);

		if(blockIndex == -1)
		{
			continue;
		}

		uni.block = &blocks[blockIndex];
		blocks[blockIndex].uniforms.push_back(&uni);

		/* Offset in block */
		GLint offset;
		glGetActiveUniformsiv(glId, 1, &(uni.index),  GL_UNIFORM_OFFSET, 
			&blockIndex);
		ANKI_ASSERT(offset != -1); // If -1 then it should break before
		uni.offset = offset;
	}
}

//==============================================================================
const ShaderProgramAttributeVariable*
	ShaderProgram::tryFindAttributeVariable(const char* name) const
{
	NameToAttribVarHashMap::const_iterator it = nameToAttribVar.find(name);
	return (it == nameToAttribVar.end()) ? nullptr : it->second;
}

//==============================================================================
const ShaderProgramAttributeVariable&
	ShaderProgram::findAttributeVariable(const char* name) const
{
	const ShaderProgramAttributeVariable* var = tryFindAttributeVariable(name);
	if(var == nullptr)
	{
		throw ANKI_EXCEPTION("Attribute variable not found: " + name);
	}
	return *var;
}

//==============================================================================
const ShaderProgramUniformVariable* ShaderProgram::tryFindUniformVariable(
	const char* name) const
{
	NameToUniVarHashMap::const_iterator it = nameToUniVar.find(name);
	if(it == nameToUniVar.end())
	{
		return nullptr;
	}
	return it->second;
}

//==============================================================================
const ShaderProgramUniformVariable& ShaderProgram::findUniformVariable(
	const char* name) const
{
	const ShaderProgramUniformVariable* var = tryFindUniformVariable(name);
	if(var == nullptr)
	{
		throw ANKI_EXCEPTION("Uniform variable not found: " + name);
	}
	return *var;
}

//==============================================================================
const ShaderProgramUniformBlock* ShaderProgram::tryFindUniformBlock(
	const char* name) const
{
	NameToUniformBlockHashMap::const_iterator it = nameToBlock.find(name);
	return (it == nameToBlock.end()) ? nullptr : it->second;
}

//==============================================================================
const ShaderProgramUniformBlock& ShaderProgram::findUniformBlock(
	const char* name) const
{
	const ShaderProgramUniformBlock* block = tryFindUniformBlock(name);
	if(block == nullptr)
	{
		throw ANKI_EXCEPTION("Block not found: " + name);
	}
	return *block;
}

//==============================================================================
std::ostream& operator<<(std::ostream& s, const ShaderProgram& x)
{
	s << "ShaderProgram\n";
	s << "Uniform variables:\n";
	for(auto var : x.unis)
	{
		s << var.getName() << " " << var.getLocation() <<  '\n';
	}
	s << "Attrib variables:\n";
	for(auto var : x.attribs)
	{
		s << var.getName() << " " << var.getLocation() <<  '\n';
	}
	return s;
}

} // end namespace anki
