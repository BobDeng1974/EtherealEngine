#include "transform_system.h"
#include "../components/transform_component.h"
#include "../../system/engine.h"
namespace runtime
{
	void update_transform(CHandle<TransformComponent> hTransform, std::chrono::duration<float> dt)
	{
		auto pTransform = hTransform.lock();
		if (pTransform)
		{
			pTransform->resolve(true, dt.count());

			auto& children = pTransform->get_children();
			for (auto& child : children)
			{
				update_transform(child, dt);
			}
		}
	}

	void TransformSystem::frame_begin(std::chrono::duration<float> dt)
	{
		auto ecs = core::get_subsystem<runtime::EntityComponentSystem>();
		_roots.clear();
		ecs->each<TransformComponent>([this](runtime::Entity e, TransformComponent& transformComponent)
		{
			auto parent = transformComponent.get_parent();
			if (parent.expired())
			{
				_roots.push_back(transformComponent.handle());
			}
		});

		for (auto& hComponent : _roots)
		{
			update_transform(hComponent, dt);
		}
	}

	bool TransformSystem::initialize()
	{
		runtime::on_frame_begin.addListener(this, &TransformSystem::frame_begin);

		return true;
	}

	void TransformSystem::dispose()
	{
		runtime::on_frame_begin.removeListener(this, &TransformSystem::frame_begin);
	}
}