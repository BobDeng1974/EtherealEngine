#pragma once
#include "core/subsystem.h"
#include "interface/docks/imguidock.h"
#include "interface/gizmos/imguizmo.h"
#include "runtime/ecs/ecs.h"
#include "runtime/assets/asset_handle.h"


struct Texture;
namespace editor
{
	struct EditState : core::Subsystem
	{
		struct DragData
		{
			rttr::variant object;
			std::string description;
		};
		struct SelectionData
		{
			rttr::variant object;
		};

		bool initialize();
		void dispose();
		void select(rttr::variant object);
		void unselect();
		void drag(rttr::variant object, const std::string& description = "");
		void drop();
		void frameEnd();

		runtime::Entity camera;
		std::string scene;
		bool showGrid = true;
		bool wireframeSelection = true;
		imguizmo::OPERATION operation = imguizmo::TRANSLATE;
		imguizmo::MODE mode = imguizmo::LOCAL;
		DragData drag_data;
		SelectionData selection_data;
		std::unordered_map<std::string, AssetHandle<Texture>> icons;
	};

}