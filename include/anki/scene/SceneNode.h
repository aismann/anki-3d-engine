// Copyright (C) 2014, Panagiotis Christopoulos Charitos.
// All rights reserved.
// Code licensed under the BSD License.
// http://www.anki3d.org/LICENSE

#ifndef ANKI_SCENE_SCENE_NODE_H
#define ANKI_SCENE_SCENE_NODE_H

#include "anki/scene/Common.h"
#include "anki/scene/SceneObject.h"
#include "anki/scene/SceneComponent.h"

namespace anki {

class SceneGraph; // Don't include
class ResourceManager;

/// @addtogroup scene
/// @{

/// Interface class backbone of scene
class SceneNode: public SceneObject
{
public:
	/// The one and only constructor
	SceneNode(SceneGraph* scene);

	/// Unregister node
	virtual ~SceneNode();

	/// @param name The unique name of the node. If it's nullptr the the node
	///             is not searchable.
	ANKI_USE_RESULT Error create(const CString& name);

	/// Return the name. It may be empty for nodes that we don't want to track
	CString getName() const
	{
		return (!m_name.isEmpty()) ? m_name.toCString() : CString();
	}

	U32 getComponentsCount() const
	{
		return m_componentsCount;
	}

	/// This is called by the scene every frame after logic and before
	/// rendering. By default it does nothing
	/// @param prevUpdateTime Timestamp of the previous update
	/// @param crntTime Timestamp of this update
	virtual ANKI_USE_RESULT Error frameUpdate(F32 prevUpdateTime, F32 crntTime)
	{
		(void)prevUpdateTime;
		(void)crntTime;
		return ErrorCode::NONE;
	}

	/// Return the last frame the node was updated. It checks all components
	U32 getLastUpdateFrame() const;

	/// Iterate all components
	template<typename Func>
	ANKI_USE_RESULT Error iterateComponents(Func func) const
	{
		Error err = ErrorCode::NONE;
		U count = m_componentsCount;
		while(!err && count-- != 0)
		{
			err = func(*m_components[count]);
		}

		return err;
	}

	/// Iterate all components of a specific type
	template<typename Component, typename Func>
	ANKI_USE_RESULT Error iterateComponentsOfType(Func func)
	{
		Error err = ErrorCode::NONE;
		SceneComponent::Type type = Component::getClassType();
		U count = m_componentsCount;
		while(!err && count-- != 0)
		{
			if(m_components[count]->getType() == type)
			{
				err = func(m_components[count]->downCast<Component>());
			}
		}

		return err;
	}

	/// Try geting a pointer to the first component of the requested type
	template<typename Component>
	Component* tryGetComponent()
	{
		SceneComponent::Type type = Component::getClassType();
		U count = m_componentsCount;
		while(count-- != 0)
		{
			if(m_components[count]->getType() == type)
			{
				return &m_components[count]->downCast<Component>();
			}
		}
		return nullptr;
	}

	/// Try geting a pointer to the first component of the requested type
	template<typename Component>
	const Component* tryGetComponent() const
	{
		SceneComponent::Type type = Component::getClassType();
		U count = m_componentsCount;
		while(count-- != 0)
		{
			if(m_components[count]->getType() == type)
			{
				return &m_components[count]->downCast<Component>();
			}
		}
		return nullptr;
	}

	/// Get a pointer to the first component of the requested type
	template<typename Component>
	Component& getComponent()
	{
		Component* out = tryGetComponent<Component>();
		ANKI_ASSERT(out != nullptr);
		return *out;
	}

	/// Get a pointer to the first component of the requested type
	template<typename Component>
	const Component& getComponent() const
	{
		Component* out = tryGetComponent<Component>();
		ANKI_ASSERT(out != nullptr);
		return *out;
	}

	static constexpr SceneObject::Type getClassType()
	{
		return SceneObject::Type::SCENE_NODE;
	}

protected:
	/// Append a component to the components container. The SceneNode will not
	/// take ownership
	ANKI_USE_RESULT Error addComponent(SceneComponent* comp);

	/// Remove a component from the container
	void removeComponent(SceneComponent* comp);

	ResourceManager& getResourceManager();

private:
	SceneString m_name; ///< A unique name
	SceneDArray<SceneComponent*> m_components;
	U8 m_componentsCount = 0;
};

/// @}

} // end namespace anki

#endif
