#include "avm/bootstrap_runtime.h"
#include "avm/execution_trace.h"
#include "avm/triune_primitives.h"

#include "calculator_view.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstddef>
#include <iostream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace
{

constexpr std::size_t no_trace_selection = std::numeric_limits<std::size_t>::max();

unsigned long long show_id(avm::LinkId id)
{
	return static_cast<unsigned long long>(id);
}

const char *event_kind_name(avm::ExecutionEventKind kind)
{
	switch (kind)
	{
	case avm::ExecutionEventKind::Enter:
		return "Enter";
	case avm::ExecutionEventKind::Return:
		return "Return";
	case avm::ExecutionEventKind::Fail:
		return "Fail";
	}
	return "Unknown";
}

struct ShowcaseModel
{
	avm::InMemoryLinkStore store;
	avm::BootstrapRuntime runtime;
	avm::DirectTriuneVocabulary direct;
	avm::BoundedExecutionTrace trace;
	avm::showcase::CalculatorViewport calculator_view;

	avm::LinkId subject_a = avm::invalid_link_id;
	avm::LinkId subject_b = avm::invalid_link_id;
	avm::LinkId object = avm::invalid_link_id;
	avm::LinkId receiver = avm::invalid_link_id;
	avm::LinkId target_begin = avm::invalid_link_id;
	avm::LinkId target_end = avm::invalid_link_id;
	avm::LinkId target_descriptor = avm::invalid_link_id;
	avm::LinkId subject_entity_a = avm::invalid_link_id;
	avm::LinkId subject_entity_b = avm::invalid_link_id;
	avm::LinkId find_entity = avm::invalid_link_id;
	avm::LinkId realize_entity = avm::invalid_link_id;
	avm::SemanticContextView semantic_root;

	avm::LinkId selected_entity = avm::invalid_link_id;
	std::optional<avm::LinkId> last_result;
	std::string last_error;
	std::size_t selected_trace = no_trace_selection;
	bool use_semantic_context = true;

	ShowcaseModel()
	    : runtime(store), direct(avm::DirectTriuneVocabulary::create(store)), trace(256), calculator_view(store, runtime)
	{
		avm::register_direct_triune_primitives(runtime.executor(), direct);
		runtime.executor().set_observer(&trace);

		subject_a = store.create_point();
		subject_b = store.create_point();
		object = store.create_point();
		receiver = store.create_point();
		target_begin = store.create_point();
		target_end = store.create_point();

		subject_entity_a =
		    avm::encode_relation_entity(store, avm::RelationEntity{direct.subject_value_relation, subject_a, object});
		subject_entity_b =
		    avm::encode_relation_entity(store, avm::RelationEntity{direct.subject_value_relation, subject_b, object});

		target_descriptor = avm::materialize_pair_target(store, direct, target_begin, target_end);
		find_entity = avm::encode_relation_entity(
		    store, avm::RelationEntity{direct.pair_find_relation, receiver, target_descriptor});
		realize_entity = avm::encode_relation_entity(
		    store, avm::RelationEntity{direct.pair_realize_relation, receiver, target_descriptor});

		const avm::SemanticContextFrame root_frame{
		    store.create_point(),
		    store.create_point(),
		    store.create_point(),
		    store.create_point(),
		};
		semantic_root = avm::SemanticContextView::root(root_frame);
		selected_entity = subject_entity_a;
	}

	void execute(avm::LinkId entity)
	{
		selected_entity = entity;
		selected_trace = no_trace_selection;
		last_error.clear();
		last_result.reset();
		try
		{
			if (use_semantic_context)
				last_result = runtime.executor().execute_in_context(entity, semantic_root);
			else
				last_result = runtime.execute(entity);
		}
		catch (const std::exception &error)
		{
			last_error = error.what();
		}
	}
};

void draw_node(ImDrawList &draw_list, ImVec2 center, const char *role, avm::LinkId id, ImU32 fill)
{
	constexpr float width = 132.0F;
	constexpr float height = 52.0F;
	const ImVec2 min(center.x - width / 2.0F, center.y - height / 2.0F);
	const ImVec2 max(center.x + width / 2.0F, center.y + height / 2.0F);
	draw_list.AddRectFilled(min, max, fill, 7.0F);
	draw_list.AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), 7.0F, 0, 1.5F);
	draw_list.AddText(ImVec2(min.x + 8.0F, min.y + 6.0F), ImGui::GetColorU32(ImGuiCol_Text), role);
	const std::string id_text = "#" + std::to_string(show_id(id));
	draw_list.AddText(ImVec2(min.x + 8.0F, min.y + 27.0F), ImGui::GetColorU32(ImGuiCol_Text), id_text.c_str());
}

void draw_edge(ImDrawList &draw_list, ImVec2 from, ImVec2 to)
{
	draw_list.AddLine(from, to, ImGui::GetColorU32(ImGuiCol_TextDisabled), 2.0F);
}

void draw_relation_graph(ShowcaseModel &model)
{
	ImGui::TextUnformatted("Canonical entity = Link(relation, Link(subject, object))");
	ImGui::Separator();

	if (model.selected_entity == avm::invalid_link_id)
	{
		ImGui::TextUnformatted("No selected relation entity.");
		return;
	}

	try
	{
		const avm::RelationEntity decoded = avm::decode_relation_entity(model.store, model.selected_entity);
		const avm::Link raw_entity = model.store.get(model.selected_entity);
		const avm::Link raw_arguments = model.store.get(raw_entity.end);

		ImGui::Text("entity #%llu", show_id(model.selected_entity));
		ImGui::Text("rel #%llu   sub #%llu   obj #%llu", show_id(decoded.relation), show_id(decoded.subject),
		            show_id(decoded.object));
		ImGui::Text("raw: #%llu = Link(#%llu, #%llu)", show_id(model.selected_entity), show_id(raw_entity.begin),
		            show_id(raw_entity.end));
		ImGui::Text("args: #%llu = Link(#%llu, #%llu)", show_id(raw_entity.end), show_id(raw_arguments.begin),
		            show_id(raw_arguments.end));

		const ImVec2 available = ImGui::GetContentRegionAvail();
		const float canvas_width = std::max(available.x, 520.0F);
		constexpr float canvas_height = 330.0F;
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("relation_graph_canvas", ImVec2(canvas_width, canvas_height));
		ImDrawList &draw_list = *ImGui::GetWindowDrawList();

		const ImVec2 entity_pos(origin.x + canvas_width * 0.5F, origin.y + 45.0F);
		const ImVec2 relation_pos(origin.x + canvas_width * 0.27F, origin.y + 145.0F);
		const ImVec2 arguments_pos(origin.x + canvas_width * 0.72F, origin.y + 145.0F);
		const ImVec2 subject_pos(origin.x + canvas_width * 0.58F, origin.y + 260.0F);
		const ImVec2 object_pos(origin.x + canvas_width * 0.86F, origin.y + 260.0F);

		draw_edge(draw_list, entity_pos, relation_pos);
		draw_edge(draw_list, entity_pos, arguments_pos);
		draw_edge(draw_list, arguments_pos, subject_pos);
		draw_edge(draw_list, arguments_pos, object_pos);

		draw_node(draw_list, entity_pos, "entity", model.selected_entity, ImGui::GetColorU32(ImGuiCol_Header));
		draw_node(draw_list, relation_pos, "relation", decoded.relation, ImGui::GetColorU32(ImGuiCol_Button));
		draw_node(draw_list, arguments_pos, "args pair", raw_entity.end, ImGui::GetColorU32(ImGuiCol_FrameBgActive));
		draw_node(draw_list, subject_pos, "subject", decoded.subject, ImGui::GetColorU32(ImGuiCol_FrameBg));
		draw_node(draw_list, object_pos, "object", decoded.object, ImGui::GetColorU32(ImGuiCol_FrameBg));

		if (decoded.relation == model.direct.pair_find_relation ||
		    decoded.relation == model.direct.pair_realize_relation)
		{
			const avm::PairTarget target = avm::decode_pair_target(model.store, model.direct, decoded.object);
			ImGui::SeparatorText("PairTarget");
			ImGui::Text("descriptor #%llu", show_id(target.descriptor));
			ImGui::Text("target = Link(#%llu, #%llu)", show_id(target.begin), show_id(target.end));
			const auto pair = model.store.find(target.begin, target.end);
			if (pair)
				ImGui::Text("materialized as #%llu", show_id(*pair));
			else
				ImGui::TextUnformatted("not materialized");
		}
	}
	catch (const std::exception &error)
	{
		ImGui::TextWrapped("Graph decode failed: %s", error.what());
	}
}

void draw_context(const avm::ExecutionContext &context)
{
	ImGui::Text("dispatch entity   #%llu", show_id(context.entity));
	ImGui::Text("controller rel    #%llu", show_id(context.relation));
	ImGui::Text("dispatch subject  #%llu", show_id(context.subject));
	ImGui::Text("dispatch object   #%llu", show_id(context.object));
	if (context.parent)
		ImGui::Text("parent            #%llu", show_id(*context.parent));
	else
		ImGui::TextUnformatted("parent            -");
	if (context.frame)
		ImGui::Text("call frame        #%llu", show_id(*context.frame));
	else
		ImGui::TextUnformatted("call frame        -");

	ImGui::SeparatorText("Semantic context");
	if (!context.semantic.has_value())
	{
		ImGui::TextUnformatted("semantic context: absent");
		return;
	}

	const avm::SemanticContextFrame &frame = context.semantic.current();
	ImGui::Text("$ent  #%llu", show_id(frame.entity));
	ImGui::Text("$rel  #%llu", show_id(frame.relation_state));
	ImGui::Text("$sub  #%llu", show_id(frame.subject));
	ImGui::Text("$obj  #%llu", show_id(frame.object));
	ImGui::Text("depth %zu", context.semantic.depth());
	if (frame.relation_state != context.relation)
		ImGui::TextUnformatted("controller relation != semantic $rel");
}

void draw_trace(ShowcaseModel &model)
{
	if (ImGui::Button("Clear trace"))
	{
		model.trace.reset();
		model.selected_trace = no_trace_selection;
	}
	ImGui::SameLine();
	ImGui::Text("%zu / %zu events", model.trace.size(), model.trace.max_events());
	if (model.trace.truncated())
		ImGui::TextUnformatted("Trace truncated.");

	const auto events = model.trace.events();
	ImGui::BeginChild("trace_events", ImVec2(0.0F, 245.0F), true);
	for (std::size_t index = 0; index < events.size(); ++index)
	{
		const avm::ExecutionEvent &event = events[index];
		const std::string label = std::to_string(index) + "  " + event_kind_name(event.kind) + "  entity #" +
		                          std::to_string(show_id(event.context.entity));
		const bool selected = model.selected_trace == index;
		if (ImGui::Selectable(label.c_str(), selected))
		{
			model.selected_trace = index;
			model.selected_entity = event.context.entity;
		}
	}
	ImGui::EndChild();

	if (events.empty())
	{
		ImGui::TextUnformatted("Execute an entity to populate the trace.");
		return;
	}

	const std::size_t selected_index = model.selected_trace < events.size() ? model.selected_trace : events.size() - 1;
	const avm::ExecutionEvent &event = events[selected_index];
	ImGui::SeparatorText("Selected event");
	ImGui::Text("%s", event_kind_name(event.kind));
	draw_context(event.context);
	if (event.result)
		ImGui::Text("result #%llu", show_id(*event.result));
	if (event.failure_phase)
		ImGui::Text("failure phase %d", static_cast<int>(*event.failure_phase));
}

void draw_playground(ShowcaseModel &model)
{
	ImGui::Text("LinkStore size: %zu", model.store.size());
	ImGui::Checkbox("Execute with semantic context", &model.use_semantic_context);

	if (ImGui::Button("subject_value(A)", ImVec2(-1.0F, 0.0F)))
		model.execute(model.subject_entity_a);
	if (ImGui::Button("subject_value(B)", ImVec2(-1.0F, 0.0F)))
		model.execute(model.subject_entity_b);

	ImGui::SeparatorText("Pair target");
	ImGui::Text("begin #%llu", show_id(model.target_begin));
	ImGui::Text("end   #%llu", show_id(model.target_end));
	const auto existing_pair = model.store.find(model.target_begin, model.target_end);
	if (existing_pair)
		ImGui::Text("pair  #%llu", show_id(*existing_pair));
	else
		ImGui::TextUnformatted("pair  <missing>");

	if (ImGui::Button("pair_find", ImVec2(-1.0F, 0.0F)))
		model.execute(model.find_entity);
	if (ImGui::Button("pair_realize", ImVec2(-1.0F, 0.0F)))
		model.execute(model.realize_entity);

	ImGui::SeparatorText("Last execution");
	ImGui::Text("selected entity #%llu", show_id(model.selected_entity));
	if (model.last_result)
		ImGui::Text("result #%llu", show_id(*model.last_result));
	else
		ImGui::TextUnformatted("result -");
	if (!model.last_error.empty())
		ImGui::TextWrapped("failure: %s", model.last_error.c_str());
}

void draw_showcase(ShowcaseModel &model)
{
	ImGui::SetNextWindowSize(ImVec2(1280.0F, 760.0F), ImGuiCond_FirstUseEver);
	ImGui::Begin("AVM Showcase — Relations Model");
	ImGui::TextUnformatted("The UI is only a consumer. Every action below goes through the canonical AVM Executor.");
	ImGui::Separator();

	if (ImGui::BeginTable("showcase_layout", 3, ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV))
	{
		ImGui::TableSetupColumn("Playground", ImGuiTableColumnFlags_WidthFixed, 245.0F);
		ImGui::TableSetupColumn("Relation graph", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Context + trace", ImGuiTableColumnFlags_WidthFixed, 390.0F);
		ImGui::TableHeadersRow();

		ImGui::TableNextColumn();
		draw_playground(model);
		model.calculator_view.draw(model.selected_entity, model.last_result, model.last_error, model.selected_trace);

		ImGui::TableNextColumn();
		draw_relation_graph(model);

		ImGui::TableNextColumn();
		draw_trace(model);

		ImGui::EndTable();
	}
	ImGui::End();
}

void glfw_error_callback(int error, const char *description)
{
	std::cerr << "GLFW error " << error << ": " << description << '\n';
}

} // namespace

int main()
{
	glfwSetErrorCallback(glfw_error_callback);
	if (glfwInit() == GLFW_FALSE)
		return 1;

#if defined(__APPLE__)
	const char *glsl_version = "#version 150";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
	const char *glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif

	GLFWwindow *window = glfwCreateWindow(1440, 860, "AVM Showcase", nullptr, nullptr);
	if (window == nullptr)
	{
		glfwTerminate();
		return 1;
	}

	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO &io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	ImGui::StyleColorsDark();

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);

	int exit_code = 0;
	try
	{
		ShowcaseModel model;
		while (glfwWindowShouldClose(window) == GLFW_FALSE)
		{
			glfwPollEvents();
			if (glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
			{
				glfwWaitEventsTimeout(0.05);
				continue;
			}

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			draw_showcase(model);
			ImGui::Render();

			int display_width = 0;
			int display_height = 0;
			glfwGetFramebufferSize(window, &display_width, &display_height);
			glViewport(0, 0, display_width, display_height);
			glClearColor(0.08F, 0.08F, 0.10F, 1.0F);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
			glfwSwapBuffers(window);
		}
	}
	catch (const std::exception &error)
	{
		std::cerr << "AVM showcase failed: " << error.what() << '\n';
		exit_code = 1;
	}

	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
	glfwDestroyWindow(window);
	glfwTerminate();
	return exit_code;
}
