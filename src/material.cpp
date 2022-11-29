#include "material.h"
#include "texture.h"
#include "application.h"
#include "extra/hdre.h"

StandardMaterial::StandardMaterial()
{
	color = vec4(1.f, 1.f, 1.f, 1.f);
	ball_color = vec4(1.f, 1.f, 1.f, 1.f);
	shader = Shader::Get("data/shaders/basic.vs", "data/shaders/flat.fs");
}

StandardMaterial::~StandardMaterial()
{

}

void StandardMaterial::setUniforms(Camera* camera, Matrix44 model)
{

	Matrix44 inv_view = camera->viewprojection_matrix;
	inv_view.inverse();


	//Compute positions
	shader->setUniform("u_viewprojection", camera->viewprojection_matrix);
	shader->setUniform("u_camera_pos", camera->eye);
	Vector4 local_cam = Vector4(camera->eye, 1.0);
	Matrix44 inv_model = model;
	inv_model.inverse();
	local_cam = inv_model * local_cam;
	shader->setUniform("u_local_camera_position", local_cam.xyz);
	shader->setUniform("u_inverse_viewprojection", inv_view);
	shader->setUniform("u_iRes",Vector2(1.0/(float)Application::instance->window_width, 1.0/(float)Application::instance->window_height));

	//SDF attributes	
	shader->setUniform("u_color", color);
	shader->setUniform("u_color_1", ball_color);

	//Extra
	shader->setUniform("u_time", Application::instance->light_int);
	shader->setUniform("u_time", Application::instance->time);
	shader->setUniform("u_pos_x", Application::instance->pos.x);
	shader->setUniform("u_pos_y", Application::instance->pos.y);
	shader->setUniform("u_pos_z", Application::instance->pos.z);
	shader->setUniform("u_light_pos_x", Application::instance->light_pos.x);
	shader->setUniform("u_light_pos_y", Application::instance->light_pos.y);
	shader->setUniform("u_light_pos_z", Application::instance->light_pos.z);
	if(texture) shader->setUniform("u_texture", texture, 0);
}

void StandardMaterial::render(Mesh* mesh, Matrix44 model, Camera* camera)
{
	if (mesh && shader)
	{
		//enable shader
		shader->enable();

		//upload uniforms
		setUniforms(camera, model);

		//do the draw call		
		mesh->render(GL_TRIANGLES);

		//disable shader
		shader->disable();
	}
}

void StandardMaterial::renderInMenu()
{
	ImGui::ColorEdit3("Color", (float*)&color); // Edit 3 floats representing a color
	ImGui::ColorEdit3("Ball Color", (float*)&ball_color);
}

