#include "transform_component.h"
#include "core/logging/logging.h"
#include <algorithm>

runtime::CHandle<TransformComponent> create_from_component(runtime::CHandle<TransformComponent> component)
{
	if (!component.expired())
	{
		auto ecs = core::get_subsystem<runtime::EntityComponentSystem>();
		auto entity = ecs->create_from_copy(component.lock()->get_entity());
		return entity.component<TransformComponent>();
	}
	logging::get("Log")->error("trying to clone a null component");
	return runtime::CHandle<TransformComponent>();
}

TransformComponent::TransformComponent()
{
}

TransformComponent::TransformComponent(const TransformComponent& rhs)
{
	//mParent = rhs.mParent;
	_children.reserve(rhs._children.size());
	for (auto& rhsChild : rhs._children)
	{
		_children.push_back(create_from_component(rhsChild));
	}

	_local_transform = rhs._local_transform;
	_world_transform = rhs._world_transform;
	_slow_parenting = rhs._slow_parenting;
	_slow_parenting_speed = rhs._slow_parenting_speed;
}

void TransformComponent::on_entity_set()
{
	// 	if (!mParent.expired())
	// 	{
	// 		mParent.lock()->attachChild(makeHandle());
	// 	}

	for (auto& child : _children)
	{
		child.lock()->_parent = handle();
	}
}

TransformComponent::~TransformComponent()
{
	if (!_parent.expired())
	{
		if (get_entity())
			_parent.lock()->remove_child(handle());
	}
	for (auto& child : _children)
	{
		if (!child.expired())
			child.lock()->get_entity().destroy();
	}

}

TransformComponent& TransformComponent::move(const math::vec3 & amount)
{
	math::vec3 vNewPos = get_position();
	vNewPos += get_x_axis() * amount.x;
	vNewPos += get_y_axis() * amount.y;
	vNewPos += get_z_axis() * amount.z;

	// Pass through to setPosition so that any derived classes need not
	// override the 'move' method in order to catch this position change.
	set_position(vNewPos);
	return *this;
}

TransformComponent& TransformComponent::move_local(const math::vec3 & amount)
{
	math::vec3 vNewPos = get_local_position();
	vNewPos += get_local_x_axis() * amount.x;
	vNewPos += get_local_y_axis() * amount.y;
	vNewPos += get_local_z_axis() * amount.z;

	// Pass through to setPosition so that any derived classes need not
	// override the 'move' method in order to catch this position change.
	set_local_position(vNewPos);
	return *this;
}

TransformComponent& TransformComponent::set_local_position(const math::vec3 & position)
{
	// Set new cell relative position
	_local_transform.setPosition(position);
	set_local_transform(_local_transform);
	return *this;
}

TransformComponent& TransformComponent::set_position(const math::vec3 & position)
{
	// Rotate a copy of the current math::transform_t.
	math::transform_t m = get_transform();
	m.setPosition(position);

	if (!_parent.expired())
	{
		math::transform_t invParentTransform = math::inverse(_parent.lock()->get_transform());
		m = invParentTransform * m;
	}

	set_local_transform(m);
	return *this;
}

math::vec3 TransformComponent::get_local_scale()
{
	return _local_transform.getScale();
}

const math::vec3 & TransformComponent::get_local_position()
{
	return _local_transform.getPosition();
}

math::quat TransformComponent::get_local_rotation()
{
	return _local_transform.getRotation();
}

math::vec3 TransformComponent::get_local_x_axis()
{
	return _local_transform.xUnitAxis();
}

math::vec3 TransformComponent::get_local_y_axis()
{
	return _local_transform.yUnitAxis();

}

math::vec3 TransformComponent::get_local_z_axis()
{
	return _local_transform.zUnitAxis();

}

const math::vec3 & TransformComponent::get_position()
{
	return get_transform().getPosition();
}

math::quat TransformComponent::get_rotation()
{
	return get_transform().getRotation();
}

math::vec3 TransformComponent::get_x_axis()
{
	return get_transform().xUnitAxis();
}

math::vec3 TransformComponent::get_y_axis()
{
	return get_transform().yUnitAxis();
}

math::vec3 TransformComponent::get_z_axis()
{
	return get_transform().zUnitAxis();
}

math::vec3 TransformComponent::get_scale()
{
	return get_transform().getScale();
}

const math::transform_t & TransformComponent::get_transform()
{
	resolve();
	return _world_transform;
}

const math::transform_t & TransformComponent::get_local_transform() const
{
	// Return reference to our internal matrix
	return _local_transform;
}

TransformComponent& TransformComponent::look_at(float x, float y, float z)
{
	//TODO("General", "These lookAt methods need to consider the pivot and the currently applied math::transform_t method!!!");
	look_at(get_position(), math::vec3(x, y, z));
	return *this;
}

TransformComponent& TransformComponent::look_at(const math::vec3 & point)
{
	look_at(get_position(), point);
	return *this;
}

TransformComponent& TransformComponent::look_at(const math::vec3 & eye, const math::vec3 & at)
{
	math::transform_t m;
	m.lookAt(eye, at);

	// Update the component position / orientation through the common base method.
	math::vec3 translation;
	math::quat orientation;

	if (m.decompose(orientation, translation))
	{
		set_rotation(orientation);
		set_position(translation);
	}
	return *this;
}

TransformComponent& TransformComponent::look_at(const math::vec3 & eye, const math::vec3 & at, const math::vec3 & up)
{
	math::transform_t m;
	m.lookAt(eye, at, up);

	// Update the component position / orientation through the common base method.
	math::vec3 translation;
	math::quat orientation;

	if (m.decompose(orientation, translation))
	{
		set_rotation(orientation);
		set_position(translation);
	}
	return *this;
}

TransformComponent& TransformComponent::rotate_local(float x, float y, float z)
{
	// Do nothing if rotation is disallowed.
	if (!can_rotate())
		return *this;

	// No-op?
	if (!x && !y && !z)
		return *this;

	_local_transform.rotateLocal(math::radians(x), math::radians(y), math::radians(z));
	set_local_transform(_local_transform);
	return *this;
}

TransformComponent& TransformComponent::rotate_axis(float degrees, const math::vec3 & axis)
{
	// No - op?
	if (!degrees)
		return *this;

	// If rotation is disallowed, only process position change. Otherwise
	// perform full rotation.
	if (!can_rotate())
	{
		// Scale the position, but do not allow axes to scale.
		math::vec3 vPos = get_position();
		math::transform_t t;
		t.rotateAxis(math::radians(degrees), axis);
		t.transformCoord(vPos, vPos);
		set_position(vPos);

	} // End if !canRotate()
	else
	{
		// Rotate a copy of the current math::transform_t.
		math::transform_t m = get_transform();
		m.rotateAxis(math::radians(degrees), axis);

		if (!_parent.expired())
		{
			math::transform_t invParentTransform = math::inverse(_parent.lock()->get_transform());
			m = invParentTransform * m;
		}

		set_local_transform(m);

	} // End if canRotate()
	return *this;
}

TransformComponent& TransformComponent::rotate(float x, float y, float z)
{
	// No - op?
	if (!x && !y && !z)
		return *this;

	// If rotation is disallowed, only process position change. Otherwise
	// perform full rotation.
	if (!can_rotate())
	{
		// Scale the position, but do not allow axes to scale.
		math::transform_t t;
		math::vec3 position = get_position();
		t.rotate(math::radians(x), math::radians(y), math::radians(z));
		t.transformCoord(position, position);
		set_position(position);

	} // End if !canRotate()
	else
	{
		// Scale a copy of the cell math::transform_t
		// Set orientation of new math::transform_t
		math::transform_t m = get_transform();
		m.rotate(math::radians(x), math::radians(y), math::radians(z));

		if (!_parent.expired())
		{
			math::transform_t invParentTransform = math::inverse(_parent.lock()->get_transform());
			m = invParentTransform * m;
		}

		set_local_transform(m);

	} // End if canRotate()
	return *this;
}

TransformComponent& TransformComponent::rotate(float x, float y, float z, const math::vec3 & center)
{
	// No - op?
	if (!x && !y && !z)
		return *this;

	// If rotation is disallowed, only process position change. Otherwise
	// perform full rotation.
	if (!can_rotate())
	{
		// Scale the position, but do not allow axes to scale.
		math::transform_t t;
		math::vec3 position = get_position() - center;
		t.rotate(math::radians(x), math::radians(y), math::radians(z));
		t.transformCoord(position, position);
		set_position(position + center);

	} // End if !canRotate()
	else
	{

		// Scale a copy of the cell math::transform_t
		// Set orientation of new math::transform_t
		math::transform_t m = get_transform();
		m.rotate(math::radians(x), math::radians(y), math::radians(z));

		if (!_parent.expired())
		{
			math::transform_t invParentTransform = math::inverse(_parent.lock()->get_transform());
			m = invParentTransform * m;
		}

		set_local_transform(m);

	} // End if canRotate()
	return *this;
}

TransformComponent& TransformComponent::set_scale(const math::vec3& s)
{
	// If scaling is disallowed, only process position change. Otherwise
	// perform full scale.
	if (!can_scale())
		return *this;

	// Scale a copy of the cell math::transform_t
	// Set orientation of new math::transform_t
	math::transform_t m = get_transform();
	m.setScale(s);

	if (!_parent.expired())
	{
		math::transform_t invParentTransform = math::inverse(_parent.lock()->get_transform());
		m = invParentTransform * m;
	}

	set_local_transform(m);
	return *this;
}

TransformComponent& TransformComponent::set_local_scale(const math::vec3 & scale)
{
	// Do nothing if scaling is disallowed.
	if (!can_scale())
		return *this;
	_local_transform.setScale(scale);
	set_local_transform(_local_transform);
	return *this;
}

TransformComponent& TransformComponent::set_rotation(const math::quat & rotation)
{
	// Do nothing if rotation is disallowed.
	if (!can_rotate())
		return *this;

	// Set orientation of new math::transform_t
	math::transform_t m = get_transform();
	m.setRotation(rotation);

	if (!_parent.expired())
	{
		math::transform_t invParentTransform = math::inverse(_parent.lock()->get_transform());
		m = invParentTransform * m;
	}

	set_local_transform(m);
	return *this;
}

TransformComponent& TransformComponent::set_local_rotation(const math::quat & rotation)
{
	// Do nothing if rotation is disallowed.
	if (!can_rotate())
		return *this;

	// Set orientation of new math::transform_t
	_local_transform.setRotation(rotation);
	set_local_transform(_local_transform);

	return *this;
}

TransformComponent& TransformComponent::reset_rotation()
{
	// Do nothing if rotation is disallowed.
	if (!can_rotate())
		return *this;
	set_rotation(math::quat{});
	return *this;
}

TransformComponent& TransformComponent::reset_scale()
{
	// Do nothing if scaling is disallowed.
	if (!can_scale())
		return *this;

	set_scale(math::vec3{ 1.0f, 1.0f, 1.0f });
	return *this;
}

TransformComponent& TransformComponent::reset_local_rotation()
{
	// Do nothing if rotation is disallowed.
	if (!can_rotate())
		return *this;

	set_local_rotation(math::quat{});

	return *this;
}

TransformComponent& TransformComponent::reset_local_scale()
{
	// Do nothing if scaling is disallowed.
	if (!can_scale())
		return *this;

	set_local_scale(math::vec3{ 1.0f, 1.0f, 1.0f });

	return *this;
}

TransformComponent& TransformComponent::reset_pivot()
{
	// Do nothing if pivot adjustment is disallowed.
	if (!can_adjust_pivot())
		return *this;

	return *this;
}

bool TransformComponent::can_scale() const
{
	// Default is to allow scaling.
	return true;
}

bool TransformComponent::can_rotate() const
{
	// Default is to allow rotation.
	return true;
}

bool TransformComponent::can_adjust_pivot() const
{
	// Default is to allow pivot adjustment.
	return true;
}

TransformComponent& TransformComponent::set_parent(runtime::CHandle<TransformComponent> parent)
{
	set_parent(parent, true, false);

	return *this;
}

TransformComponent& TransformComponent::set_parent(runtime::CHandle<TransformComponent> parent, bool worldPositionStays, bool localPositionStays)
{
	auto parentPtr = parent.lock().get();
	auto thisParentPtr = _parent.lock().get();
	// Skip if this is a no-op.
	if (thisParentPtr == parentPtr || this == parentPtr)
		return *this;
	if (parentPtr && (parentPtr->_parent.lock().get() == this))
		return *this;
	// Before we do anything, make sure that all pending math::transform_t 
	// operations are resolved (including those applied to our parent).
	math::transform_t cachedWorldTranform;
	if (worldPositionStays)
	{
		resolve(true);
		cachedWorldTranform = get_transform();
	}

	if (!_parent.expired())
	{
		_parent.lock()->remove_child(handle());
	}

	_parent = parent;

	if (!parent.expired())
	{
		auto shParent = parent.lock();
		// We're now attached / detached as required.	
		shParent->attach_child(handle());
	}


	if (worldPositionStays)
	{
		resolve(true);
		set_transform(cachedWorldTranform);
	}
	else
	{
		if (!localPositionStays)
			set_local_transform(math::transform_t::Identity);
	}


	// Success!
	return *this;
}

const runtime::CHandle<TransformComponent>& TransformComponent::get_parent() const
{
	return _parent;
}

void TransformComponent::attach_child(runtime::CHandle<TransformComponent> child)
{
	_children.push_back(child);
}

void TransformComponent::remove_child(runtime::CHandle<TransformComponent> child)
{
	_children.erase(std::remove_if(std::begin(_children), std::end(_children),
		[&child](runtime::CHandle<TransformComponent> other) { return child.lock() == other.lock(); }
	), std::end(_children));
}

TransformComponent& TransformComponent::set_transform(const math::transform_t & tr)
{
	if (_world_transform.compare(tr, 0.0001f) == 0)
		return *this;

	math::vec3 position, scaling;
	math::quat orientation;
	tr.decompose(scaling, orientation, position);

	math::transform_t m = get_transform();
	m.setScale(scaling);
	m.setRotation(orientation);
	m.setPosition(position);

	if (!_parent.expired())
	{
		math::transform_t invParentTransform = math::inverse(_parent.lock()->get_transform());
		m = invParentTransform * m;
	}

	set_local_transform(m);
	return *this;
}

TransformComponent& TransformComponent::set_local_transform(const math::transform_t & trans)
{
	if (_local_transform.compare(trans, 0.0001f) == 0)
		return *this;

	static const std::string strContext = "LocalTransform";
	touch(strContext);
	_local_transform = trans;
	return *this;
}

void TransformComponent::resolve(bool force, float dt)
{
	bool dirty = is_dirty();

	if (force || dirty)
	{
		if (!_parent.expired())
		{
			auto target = _parent.lock()->get_transform() * _local_transform;

			if (_slow_parenting)
			{
				float t = math::clamp(_slow_parenting_speed * dt, 0.0f, 1.0f);
				_world_transform.setPosition(math::lerp(_world_transform.getPosition(), target.getPosition(), t));
				_world_transform.setScale(math::lerp(_world_transform.getScale(), target.getScale(), t));
				_world_transform.setRotation(math::slerp(_world_transform.getRotation(), target.getRotation(), t));
			}
			else
			{
				_world_transform = target;
			}
		}
		else
		{
			_world_transform = _local_transform;
		}
	}

	_dirty = false;
}

bool TransformComponent::is_dirty() const
{
	bool bDirty = Component::is_dirty();
	if (!bDirty && !_parent.expired())
	{
		bDirty |= _parent.lock()->is_dirty();
	}

	return bDirty;
}


const std::vector<runtime::CHandle<TransformComponent>>& TransformComponent::get_children() const
{
	return _children;
}
