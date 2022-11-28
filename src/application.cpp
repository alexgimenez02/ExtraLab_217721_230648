#include "application.h"
#include "utils.h"
#include "mesh.h"
#include "texture.h"
#include "volume.h"
#include "fbo.h"
#include "shader.h"
#include "input.h"
#include "animation.h"
#include "extra/hdre.h"
#include "extra/imgui/imgui.h"
#include "extra/imgui/imgui_impl_sdl.h"
#include "extra/imgui/imgui_impl_opengl3.h"

#include <cmath>

bool render_wireframe = false;
Camera* Application::camera = nullptr;
Application* Application::instance = NULL;

Application::Application(int window_width, int window_height, SDL_Window* window)
{
	this->window_width = window_width;
	this->window_height = window_height;
	this->window = window;
	instance = this;
	must_exit = false;

	fps = 0;
	frame = 0;
	time = 0.0f;
	elapsed_time = 0.0f;
	mouse_locked = false;

	// OpenGL flags
	glEnable( GL_CULL_FACE ); //render both sides of every triangle
	glEnable( GL_DEPTH_TEST ); //check the occlusions using the Z buffer

	// Create camera
	camera = new Camera();
	camera->lookAt(Vector3(5.f, 5.f, 5.f), Vector3(0.f, 0.0f, 0.f), Vector3(0.f, 1.f, 0.f));
	camera->setPerspective(45.f, window_width/(float)window_height, .1f, 1000.f); //set the projection, we want to be perspectiv
	current_scene = 0;
	{
		// EXAMPLE OF HOW TO CREATE A SCENE NODE
		SceneNode* node = new SceneNode();
		/*node->mesh = new Mesh();
		node->mesh->createQuad(0, 0, 2, 2, false);*/
		node->mesh = new Mesh();
		node->mesh->createQuad(0, 0, 2, 2, false);
		StandardMaterial* mat = new StandardMaterial();
		node->material = mat;
		mat->shader = Shader::Get("data/shaders/basic.vs", "data/shaders/Scene1.fs");
		node_list.push_back(node);
		
		
		SceneNode* node1 = new SceneNode();
		/*node->mesh = new Mesh();
		node->mesh->createQuad(0, 0, 2, 2, false);*/
		node1->mesh = new Mesh();
		node1->mesh->createQuad(0, 0, 2, 2, false);
		StandardMaterial* mat1 = new StandardMaterial();
		node1->material = mat1;
		mat1->shader = Shader::Get("data/shaders/basic2.vs", "data/shaders/Scene2.fs");
		node_list.push_back(node1);
		
		SceneNode* node2 = new SceneNode();
		/*node->mesh = new Mesh();
		node->mesh->createQuad(0, 0, 2, 2, false);*/
		node2->mesh = new Mesh();
		node2->mesh->createQuad(0, 0, 2, 2, false);
		StandardMaterial* mat2 = new StandardMaterial();
		node2->material = mat2;
		mat2->shader = Shader::Get("data/shaders/basic2.vs", "data/shaders/Scene3.fs");
		node_list.push_back(node2);

	}	
	for (size_t i = 0; i < node_list.size(); i++) {
		if (i == 0) node_list[i]->enabled = true;
		else node_list[i]->enabled = false;
	}
	//hide the cursor
	SDL_ShowCursor(!mouse_locked); //hide or show the mouse
}

// what to do when the image has to be draw
void Application::render(void)
{
	// set the clear color (the background color)
	glClearColor(0.1, 0.1, 0.1, 1.0);

	// Clear the window and the depth buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// set the camera as default
	camera->enable();

	// set flags
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);

	for (size_t i = 0; i < node_list.size(); i++) {
		if(node_list[i]->enabled) node_list[i]->render(camera);
	}

}

void Application::update(double seconds_elapsed)
{
	float speed = seconds_elapsed * 10; //the speed is defined by the seconds_elapsed so it goes constant
	float orbit_speed = seconds_elapsed * 1;

	// example
	float angle = seconds_elapsed * 10.f * DEG2RAD;
	/*for (int i = 0; i < root.size(); i++) {
		root[i]->model.rotate(angle, Vector3(0,1,0));
	}*/

	// mouse input to rotate the cam
	if ((Input::mouse_state & SDL_BUTTON_LEFT && !ImGui::IsAnyWindowHovered()
		&& !ImGui::IsAnyItemHovered() && !ImGui::IsAnyItemActive())) //is left button pressed?
	{
		camera->orbit(-Input::mouse_delta.x * orbit_speed, Input::mouse_delta.y * orbit_speed);
	}

	// async input to move the camera around
	if (Input::isKeyPressed(SDL_SCANCODE_LSHIFT)) speed *= 10; //move faster with left shift
	if (Input::isKeyPressed(SDL_SCANCODE_W) || Input::isKeyPressed(SDL_SCANCODE_UP)) camera->move(Vector3(0.0f, 0.0f, 1.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_S) || Input::isKeyPressed(SDL_SCANCODE_DOWN)) camera->move(Vector3(0.0f, 0.0f, -1.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_A) || Input::isKeyPressed(SDL_SCANCODE_LEFT)) camera->move(Vector3(1.0f, 0.0f, 0.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_D) || Input::isKeyPressed(SDL_SCANCODE_RIGHT)) camera->move(Vector3(-1.0f, 0.0f, 0.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_SPACE)) camera->moveGlobal(Vector3(0.0f, -1.0f, 0.0f) * speed);
	if (Input::isKeyPressed(SDL_SCANCODE_LCTRL)) camera->moveGlobal(Vector3(0.0f, 1.0f, 0.0f) * speed);

	// to navigate with the mouse fixed in the middle
	if (mouse_locked)
		Input::centerMouse();

	node_list[0]->model.translateGlobal(camera->eye.x, camera->eye.y, camera->eye.z);

}

// Keyboard event handler (sync input)
void Application::onKeyDown(SDL_KeyboardEvent event)
{
	switch (event.keysym.sym)
	{
	case SDLK_ESCAPE: must_exit = true; break; //ESC key, kill the app
	case SDLK_F5: Shader::ReloadAll(); break;
	}
}

void Application::onKeyUp(SDL_KeyboardEvent event)
{
}

void Application::onGamepadButtonDown(SDL_JoyButtonEvent event)
{

}

void Application::onGamepadButtonUp(SDL_JoyButtonEvent event)
{

}

void Application::onMouseButtonDown(SDL_MouseButtonEvent event)
{
	if (event.button == SDL_BUTTON_MIDDLE) //middle mouse
	{
		mouse_locked = !mouse_locked;
		SDL_ShowCursor(!mouse_locked);
	}
}

void Application::onMouseButtonUp(SDL_MouseButtonEvent event)
{
}

void Application::onMouseWheel(SDL_MouseWheelEvent event)
{
	ImGuiIO& io = ImGui::GetIO();
	switch (event.type)
	{
	case SDL_MOUSEWHEEL:
	{
		if (event.x > 0) io.MouseWheelH += 1;
		if (event.x < 0) io.MouseWheelH -= 1;
		if (event.y > 0) io.MouseWheel += 1;
		if (event.y < 0) io.MouseWheel -= 1;
	}
	}

	if (!ImGui::IsAnyWindowHovered() && event.y)
		camera->changeDistance(event.y * 0.5);
}

void Application::onResize(int width, int height)
{
	std::cout << "window resized: " << width << "," << height << std::endl;
	glViewport(0, 0, width, height);
	camera->aspect = width / (float)height;
	window_width = width;
	window_height = height;
}

void Application::onFileChanged(const char* filename)
{
	Shader::ReloadAll();
}

void Application::renderInMenu() {

	if (ImGui::TreeNode("Scene")) {
		//ImGui::Checkbox("Set enable/disable", &enabled);
		ImGui::Text("Scene selector: ");
		bool changed = false;
		changed |= ImGui::Combo("Scene", &current_scene, "Ring With Sphere\0Bouncing Ball\0Extraordinary gear\0");
		if (changed) {
			for (size_t i = 0; i < node_list.size(); i++) {
				if (i == current_scene) node_list[i]->enabled = true;
				else {
					node_list[i]->enabled = false;
				}
			}
		}
		ImGui::TreePop();
	}

	if (ImGui::TreeNode("Camera")) {
		camera->renderInMenu();
		ImGui::TreePop();
	}

	//Scene graph
	if (ImGui::TreeNode("Entities"))
	{
		unsigned int count = 0;
		std::stringstream ss;
		for (auto& node : node_list)
		{
			ss << count;
			if (ImGui::TreeNode(node->name.c_str()))
			{
				node->renderInMenu();
				ImGui::TreePop();
			}
			++count;
			ss.str("");
		}
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Debug")) {
		//
		ImGui::Checkbox("Control time", &controlTime);
		if(controlTime) ImGui::SliderFloat("Time metric", &elapsed_time, 0.0, 2.0);
	}
}
