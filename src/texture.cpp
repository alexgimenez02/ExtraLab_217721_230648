#include "texture.h"
#include "fbo.h"
#include "utils.h"

#include "extra/hdre.h"
#include "volume.h"

#include <iostream> //to output
#include <cmath>

#include "mesh.h"
#include "shader.h"
#include "extra/picopng.h"
#include <cassert>

//bilinear interpolation
Color Image::getPixelInterpolated(float x, float y, bool repeat) {
	int ix = repeat ? fmod(x,width) : clamp(x,0,width-1);
	int iy = repeat ? fmod(y,height) : clamp(y, 0, height - 1);
	if (ix < 0) ix += width;
	if (iy < 0) iy += height;
	float fx = (x - (int)x);
	float fy = (y - (int)y);
	int ix2 = ix < width - 1 ? ix + 1 : 0;
	int iy2 = iy < height - 1 ? iy + 1 : 0;
	Color top = lerp( getPixel(ix, iy), getPixel(ix2, iy), fx );
	Color bottom = lerp( getPixel(ix, iy2), getPixel(ix2, iy2), fx);
	return lerp(top, bottom, fy);
};

Vector4 Image::getPixelInterpolatedHigh(float x, float y, bool repeat) {
	int ix = repeat ? fmod(x, width) : clamp(x, 0, width - 1);
	int iy = repeat ? fmod(y, height) : clamp(y, 0, height - 1);
	if (ix < 0) ix += width;
	if (iy < 0) iy += height;
	float fx = (x - (int)x);
	float fy = (y - (int)y);
	int ix2 = ix < width - 1 ? ix + 1 : 0;
	int iy2 = iy < height - 1 ? iy + 1 : 0;
	Vector4 top = lerp( getPixel(ix, iy).toVector4(), getPixel(ix2, iy).toVector4(), fx);
	Vector4 bottom = lerp(getPixel(ix, iy2).toVector4(), getPixel(ix2, iy2).toVector4(), fx);
	return lerp(top, bottom, fy);
};


std::map<std::string, Texture*> Texture::sTexturesLoaded;
int Texture::default_mag_filter = GL_LINEAR;
int Texture::default_min_filter = GL_LINEAR_MIPMAP_LINEAR;
FBO* Texture::global_fbo = NULL;

Texture::Texture()
{
	width = 0;
	height = 0;
	depth = 0;
	texture_id = 0;
	mipmaps = false;
	format = 0;
	type = 0;
	texture_type = GL_TEXTURE_2D;
}

Texture::Texture(unsigned int width, unsigned int height, unsigned int format, unsigned int type, bool mipmaps, Uint8* data, unsigned int internal_format)
{
	texture_id = 0;
	create(width, height, format, type, mipmaps, data, internal_format);
}

Texture::Texture(Image* img)
{
	texture_id = 0;
	create(img->width, img->height, img->bytes_per_pixel == 3 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, true, img->data);
}

Texture::~Texture()
{
	clear();
}

void Texture::clear()
{
	glDeleteTextures(1, &texture_id);
	glBindTexture(this->texture_type, 0);
	texture_id = 0;
}

void Texture::debugInMenu()
{
	if (this != NULL)
	{
		this->bind();
		ImGui::Image((void*)(intptr_t)texture_id, ImVec2(50, 50));
	}
}

void Texture::create(unsigned int width, unsigned int height, unsigned int format, unsigned int type, bool mipmaps, Uint8* data, unsigned int internal_format, unsigned int wrap)
{
	assert(width && height && "texture must have a size");

	this->width = (float)width;
	this->height = (float)height;
	this->depth = 0;
	this->format = format;
	this->internal_format = internal_format;
	this->type = type;
	this->mipmaps = mipmaps && isPowerOfTwo(width) && isPowerOfTwo(height) && format != GL_DEPTH_COMPONENT;
	this->wrapS = this->wrapT = wrap;

	//Delete previous texture and ensure that previous bounded texture_id is not of another texture type
	if (this->texture_id != 0)
		clear();

	this->texture_type = GL_TEXTURE_2D;

	if(texture_id == 0)
		glGenTextures(1, &texture_id); //we need to create an unique ID for the texture

	assert(checkGLErrors() && "Error creating texture");

	if (data != NULL)
		upload(format, type, mipmaps, data, internal_format);
}

void Texture::createCubemap(unsigned int width, unsigned int height, Uint8** data, unsigned int format, unsigned int type, bool mipmaps, unsigned int internal_format)
{
	assert(width && height && "texture must have a size");

	this->width = (float)width;
	this->height = (float)height;
	this->depth = 0;
	this->format = format;
	this->internal_format = internal_format;
	this->type = type;
	this->texture_type = GL_TEXTURE_CUBE_MAP;
	this->mipmaps = mipmaps && isPowerOfTwo(width) && isPowerOfTwo(height) && format != GL_DEPTH_COMPONENT;

	this->wrapS = GL_CLAMP_TO_EDGE;
	this->wrapT = GL_CLAMP_TO_EDGE;

	if (texture_id == 0)
		glGenTextures(1, &texture_id); //we need to create an unique ID for the texture

	glBindTexture(this->texture_type, texture_id);	//we activate this id to tell opengl we are going to use this texture

	if (data != NULL)
		uploadCubemap(format, type, mipmaps, data, internal_format);
}

bool Texture::cubemapFromHDRE(HDRE* hdre, unsigned int mipLevel)
{
	if (!hdre)
		return false;

	if (mipLevel < 0 || mipLevel > 5)
		return false;

	sHDRELevel level = hdre->getLevel(mipLevel);

	unsigned int format = hdre->numChannels == 3 ? GL_RGB : GL_RGBA;
	unsigned int internal_format = hdre->numChannels == 3 ? GL_RGB32F : GL_RGBA32F;
	
	createCubemap(level.width, level.height, (Uint8**)level.faces, format, GL_FLOAT, true, internal_format);
	return true;
}

// skyboxes (TGA): https://utfiles.lagout.org/UEditor_Developing/skybox/

bool Texture::cubemapFromImages(const char * folder)
{
	uint8* faces[6];
	std::string imgs[6] = {
		std::string(folder) + "/rt.tga",
		std::string(folder) + "/lf.tga",
		std::string(folder) + "/dn.tga",
		std::string(folder) + "/up.tga",
		std::string(folder) + "/bk.tga",
		std::string(folder) + "/ft.tga"
	};

	Image img;

	for (int i = 0; i < 6; ++i)
	{
		if (!img.loadTGA(imgs[i].c_str()))
		{
			std::cout << imgs[i].c_str() << " not loaded" << std::endl;
			return false;
		}

		faces[i] = img.data;
	}

	createCubemap(img.width, img.height, faces);
	setName(folder);
	return true;
}

void Texture::create3D(unsigned int width, unsigned int height, unsigned int depth, unsigned int format, unsigned int type, bool mipmaps, Uint8* data, unsigned int internal_format, unsigned int wrap)
{
	assert(width && height && depth && "texture must have a size");

	this->width = (float)width;
	this->height = (float)height;
	this->depth = (float)depth;
	this->format = format;
	this->internal_format = internal_format;
	this->type = type;
	this->mipmaps = mipmaps && isPowerOfTwo(width) && isPowerOfTwo(height) && format != GL_DEPTH_COMPONENT && isPowerOfTwo(this->depth);
	this->wrapR = this->wrapS = this->wrapT = wrap;

	//Delete previous texture and ensure that previous bounded texture_id is not of another texture type
	if (this->texture_id != 0)
		clear();

	this->texture_type = GL_TEXTURE_3D;

	if (texture_id == 0)
		glGenTextures(1, &texture_id); //we need to create an unique ID for the texture

	assert(checkGLErrors() && "Error creating texture");

	if (data != NULL)
		upload3D(format, type, mipmaps, data, internal_format);
}

void Texture::create3DFromVolume(Volume* volume, unsigned int wrap)
{
	create3D(volume->width, volume->height, volume->depth, volume->getTextureFormat(), volume->getTextureType(), false, volume->data, volume->getTextureInternalFormat(), wrap);
}

Texture* Texture::Get(const char* filename, bool mipmaps, unsigned int wrap)
{
	assert(filename);

	//check if loaded
	auto it = sTexturesLoaded.find(filename);
	if (it != sTexturesLoaded.end())
		return it->second;

	//load it
	Texture* texture = new Texture();
	if (!texture->load(filename, mipmaps, wrap))
	{
		delete texture;
		return NULL;
	}

	return texture;
}

bool Texture::load(const char* filename, bool mipmaps, unsigned int wrap, unsigned int type)
{
	std::string str = filename;
	std::string ext = str.substr(str.size() - 4, 4);
	Image* image = NULL;
	long time = getTime();

	std::cout << " + Texture loading: " << filename << " ... ";

	image = new Image();
	bool found = false;

	if (ext == ".tga" || ext == ".TGA")
		found = image->loadTGA(filename);
	else if (ext == ".png" || ext == ".PNG")
		found = image->loadPNG(filename, true);
	else
	{
		std::cout << "[ERROR]: unsupported format" << std::endl;
		return false; //unsupported file type
	}

	if (!found) //file not found
	{
		std::cout << " [ERROR]: Texture not found " << std::endl;
		return false;
	}

	this->filename = filename;

	unsigned int internal_format = 0;

	if (type == GL_FLOAT)
		internal_format = (image->bytes_per_pixel == 3 ? GL_RGB32F : GL_RGBA32F);

	//upload to VRAM
	create(image->width, image->height, (image->bytes_per_pixel == 3 ? GL_RGB : GL_RGBA), type, mipmaps, image->data, 0, wrap);

	if (mipmaps)
		generateMipmaps();

	this->image.clear();
	std::cout << "[OK] Size: " << width << "x" << height << " Time: " << (getTime() - time) * 0.001 << "sec" << std::endl;
	setName(filename);
	return true;
}

void Texture::upload(Image* img)
{
	create(img->width, img->height, img->bytes_per_pixel == 3 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, true, img->data);
}

//uploads the bytes of a texture to the VRAM
void Texture::upload( unsigned int format, unsigned int type, bool mipmaps, Uint8* data, unsigned int internal_format)
{
	assert(texture_id && "Must create texture before uploading data.");
	assert(texture_type == GL_TEXTURE_2D && "Texture type does not match.");

	glBindTexture(this->texture_type, texture_id);	//we activate this id to tell opengl we are going to use this texture

	glTexImage2D(this->texture_type, 0, internal_format == 0 ? format : internal_format, width, height, 0, format, type, data);

	glTexParameteri(this->texture_type, GL_TEXTURE_MAG_FILTER, Texture::default_mag_filter);	//set the min filter
	glTexParameteri(this->texture_type, GL_TEXTURE_MIN_FILTER, this->mipmaps ? Texture::default_min_filter : GL_LINEAR);   //set the mag filter
	glTexParameteri(this->texture_type, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(this->texture_type, GL_TEXTURE_WRAP_T, wrapT);
	//glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4); //better quality but takes more resources

	if (data && this->mipmaps)
		generateMipmaps(); //glGenerateMipmapEXT(GL_TEXTURE_2D); 

	glBindTexture(this->texture_type, 0);
	assert(checkGLErrors() && "Error uploading texture");
}

void Texture::upload3D(unsigned int format, unsigned int type, bool mipmaps, Uint8* data, unsigned int internal_format) {
	assert(texture_id && "Must create texture before uploading data.");
	assert(texture_type == GL_TEXTURE_3D && "Texture type does not match.");

	glBindTexture(this->texture_type, texture_id);	//we activate this id to tell opengl we are going to use this texture

	glTexImage3D(this->texture_type, 0, internal_format == 0 ? format : internal_format, width, height, depth, 0, format, type, data);

	glTexParameteri(this->texture_type, GL_TEXTURE_MAG_FILTER, Texture::default_mag_filter);	//set the min filter
	glTexParameteri(this->texture_type, GL_TEXTURE_MIN_FILTER, this->mipmaps ? Texture::default_min_filter : GL_LINEAR);   //set the mag filter
	glTexParameteri(this->texture_type, GL_TEXTURE_WRAP_S, wrapS);
	glTexParameteri(this->texture_type, GL_TEXTURE_WRAP_T, wrapT);
	glTexParameteri(this->texture_type, GL_TEXTURE_WRAP_R, wrapR);

	if (data && this->mipmaps)
		generateMipmaps(); //glGenerateMipmapEXT(GL_TEXTURE_2D); 

	glBindTexture(this->texture_type, 0);
	assert(checkGLErrors() && "Error uploading texture");
}

void Texture::uploadCubemap(unsigned int format, unsigned int type, bool mipmaps, Uint8** data, unsigned int internal_format) {
	
	assert(texture_id && "Must create texture before uploading data.");
	assert(texture_type == GL_TEXTURE_CUBE_MAP && "Texture type does not match.");

	glBindTexture(this->texture_type, texture_id);	//we activate this id to tell opengl we are going to use this texture

	assert(data && "cubemap must have faces data");

	for (int i = 0; i < 6; i++)
	{
		glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, internal_format == 0 ? format : internal_format, width, height, 0, format, type, data[i]);
	}

	glTexParameteri(this->texture_type, GL_TEXTURE_MAG_FILTER, Texture::default_mag_filter);	//set the min filter
	glTexParameteri(this->texture_type, GL_TEXTURE_MIN_FILTER, this->mipmaps ? Texture::default_min_filter : GL_LINEAR);   //set the mag filter
	glTexParameteri(this->texture_type, GL_TEXTURE_WRAP_S, this->wrapS);
	glTexParameteri(this->texture_type, GL_TEXTURE_WRAP_T, this->wrapT);

	if (data && this->mipmaps)
		generateMipmaps();

	glBindTexture(this->texture_type, 0);
	assert(glGetError() == GL_NO_ERROR && "Error creating texture");
}

//special function to upload texture arrays, a special type of texture that has layers
void Texture::uploadAsArray(unsigned int texture_size, bool mipmaps)
{
	assert((image.height % texture_size) == 0); //size doesnt match
	assert(image.data);//no image in memory
	int num_columns = image.width / texture_size;
	int num_rows = image.height / texture_size;
	int num_textures = num_columns * num_rows;
	int width = texture_size;
	int height = texture_size;

	int max_layers;
	glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
	if (max_layers < num_textures)
	{
		std::cout << "GPU does not support " << std::endl;
		return;
	}

	texture_type = GL_TEXTURE_2D_ARRAY;
	type = GL_UNSIGNED_BYTE;
	int dataFormat = (image.bytes_per_pixel == 3 ? GL_RGB : GL_RGBA);
	format = (image.bytes_per_pixel == 3 ? GL_RGB8 : GL_RGBA8);
	this->width = (float)width;
	this->height = (float)height;
	int bytes_per_pixel = image.bytes_per_pixel;
	this->mipmaps = mipmaps && isPowerOfTwo((int)width) && isPowerOfTwo((int)height);
	uint8* data = NULL;

	//if texture is a grid, linearize the data so it can be uploaded in a single call
	if (num_columns > 1)
	{
		data = new uint8[num_textures * width * height * bytes_per_pixel];
		int offset = width * bytes_per_pixel;
		int offset_row = width * bytes_per_pixel * num_columns;
		int offset_image = width * height * bytes_per_pixel * num_columns;
		int offset_image_linear = width * height * bytes_per_pixel;
		for (int i = 0; i < num_rows; ++i)
			for (int j = 0; j < num_columns; ++j)
			{
				int start = (i * offset_image) + (j * offset);
				int start_linear = (i * num_columns + j) * offset_image_linear;
				for (int k = 0; k < height; ++k)
					memcpy(data + start_linear + (height - k - 1) * offset, image.data + start + k * offset_row, offset);
			}
	}
	else
		data = image.data;

	//How to store a texture in VRAM
	assert(glGetError() == GL_NO_ERROR);
	if (texture_id == 0)
		glGenTextures(1, &texture_id); //we need to create an unique ID for the texture
	glBindTexture( this->texture_type, texture_id);	//we activate this id to tell opengl we are going to use this texture
	glTexImage3D( this->texture_type, 0, format, width, height, num_textures, 0, dataFormat, type, data);
	assert(glGetError() == GL_NO_ERROR);

	glTexParameteri(this->texture_type, GL_TEXTURE_MAG_FILTER, Texture::default_mag_filter);	//set the min filter
	glTexParameteri(this->texture_type, GL_TEXTURE_MIN_FILTER, this->mipmaps ? Texture::default_min_filter : GL_LINEAR); //set the mag filter
	glTexParameteri(this->texture_type, GL_TEXTURE_WRAP_S, this->mipmaps ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	glTexParameteri(this->texture_type, GL_TEXTURE_WRAP_T, this->mipmaps ? GL_REPEAT : GL_CLAMP_TO_EDGE);
	glTexParameterf(this->texture_type, GL_TEXTURE_MAX_ANISOTROPY_EXT, 4); //better quality but takes more resources
	assert(glGetError() == GL_NO_ERROR);
	if (mipmaps)
		generateMipmaps();
	assert(glGetError() == GL_NO_ERROR);

	if (num_columns > 1)
		delete[] data;
}

void Texture::bind()
{
	//glEnable(this->texture_type); //enable the textures 
	glBindTexture(this->texture_type, texture_id );	//enable the id of the texture we are going to use
}

void Texture::unbind()
{
	//glDisable(this->texture_type); //disable the textures 
	glBindTexture(this->texture_type, 0 );	//disable the id of the texture we are going to use
}

void Texture::UnbindAll()
{
	glDisable( GL_TEXTURE_CUBE_MAP );
	glDisable( GL_TEXTURE_2D );
	glDisable(GL_TEXTURE_3D);
	glBindTexture( GL_TEXTURE_2D, 0 );
	glBindTexture( GL_TEXTURE_CUBE_MAP, 0 );
	glBindTexture(GL_TEXTURE_3D, 0);
}

void Texture::generateMipmaps()
{
	if(!glGenerateMipmapEXT)
		return;

	glBindTexture(this->texture_type, texture_id );	//enable the id of the texture we are going to use
	glTexParameteri(this->texture_type, GL_TEXTURE_MIN_FILTER, Texture::default_min_filter ); //set the mag filter
	glGenerateMipmapEXT(this->texture_type);
}


void Texture::toViewport(Shader* shader)
{
	Mesh* quad = Mesh::getQuad();
	if (!shader)
		shader = Shader::getDefaultShader("screen");
	shader->enable();
	shader->setUniform("u_texture", this);
	quad->render(GL_TRIANGLES);
	shader->disable();
}

FBO* Texture::getGlobalFBO(Texture* texture)
{
	if (!global_fbo)
		global_fbo = new FBO();
	global_fbo->createFromTextures(texture);
	return global_fbo;
}

Texture* Texture::getBlackTexture()
{
	static Texture* black = NULL;
	if (black)
		return black;
	const Uint8 data[3] = { 0,0,0 };
	black = new Texture(1, 1, GL_RGB, GL_UNSIGNED_BYTE, true, (Uint8*)data);
	return black;
}

void Texture::blit(Texture* destination, Shader* shader)
{
	FBO* fbo = getGlobalFBO(destination);
	fbo->bind();
	toViewport(shader);
	fbo->unbind();
}

void Image::fromScreen(int width, int height)
{
	if (data && (width != this->width || height != this->height))
		clear();

	if (!data)
	{
		this->width = width;
		this->height = height;
		data = new uint8[width * height * 4];
	}

	glReadPixels(0,0,width, height, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

void Image::fromTexture(Texture* texture)
{
	assert(texture);

	if(data && (width != texture->width || height != texture->height ))
		clear();

	if (!data)
	{
		width = texture->width;
		height = texture->height;
		data = new uint8[width * height * 4];
	}
	
	glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}

//TGA format from: http://www.paulbourke.net/dataformats/tga/
bool Image::loadTGA(const char* filename)
{
    GLubyte TGAheader[12] = {0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    GLubyte TGAcompare[12];
    GLubyte header[6];
    GLuint bytesPerPixel;
    GLuint imageSize;
    //GLuint type = GL_RGBA;

    FILE * file = fopen(filename, "rb");
    
    if ( file == NULL || fread(TGAcompare, 1, sizeof(TGAcompare), file) != sizeof(TGAcompare) ||
        memcmp(TGAheader, TGAcompare, sizeof(TGAheader)) != 0 ||
        fread(header, 1, sizeof(header), file) != sizeof(header))
    {
        if (file == NULL)
            return NULL;
        else
        {
            fclose(file);
            return NULL;
        }
    }

    width = header[1] * 256 + header[0];
    height = header[3] * 256 + header[2];
	bytes_per_pixel = header[4] / 8;

	bool error = false;

	if (bytes_per_pixel != 3 && bytes_per_pixel != 4)
	{
		error = true;
		std::cerr << "File format not supported: " << bytes_per_pixel << " bytes per pixel" << std::endl;
	}
    
    if (width <= 0 || height <= 0)
	{
		error = true;
		std::cerr << "Wrong texture size: " << width << "x" << height << " pixels" << std::endl;
	}

	if(error)
    {
        fclose(file);
        return false;
    }

    imageSize = width * height * bytes_per_pixel;
    
    data = new GLubyte[imageSize];
    if (data == NULL || fread(data, 1, imageSize, file) != imageSize)
    {
        if (data != NULL)
            delete []data;
        fclose(file);
        return NULL;
    }

	if (header[5] & (1 << 4)) //flip
		origin_topleft = true;
    
	//flip BGR to RGB pixels
	#pragma omp simd
    for (GLuint i = 0; i < int(imageSize); i += bytes_per_pixel)
    {
        uint8 temp = data[i];
        data[i] = data[i + 2];
        data[i + 2] = temp;
    }
    
    fclose(file);

	return true;
}

#include <iostream>
#include <fstream>

bool Image::loadPNG(const char* filename, bool flip_y)
{
	std::ifstream file( filename, std::ios::in | std::ios::binary | std::ios::ate);

	//get filesize
	std::streamsize size = 0;
	if (file.seekg(0, std::ios::end).good()) size = file.tellg();
	if (file.seekg(0, std::ios::beg).good()) size -= file.tellg();

	if (!size)
		return false;

	std::vector<unsigned char> buffer;

	//read contents of the file into the vector
	if (size > 0)
	{
		buffer.resize((size_t)size);
		file.read((char*)(&buffer[0]), size);
	}
	else 
		buffer.clear();

	std::vector<unsigned char> out_image;

	if (decodePNG( out_image, width, height, buffer.empty() ? 0 : &buffer[0], (unsigned long)buffer.size(), true) != 0)
		return false;

	data = new Uint8[ out_image.size() ];
	memcpy( data, &out_image[0], out_image.size() );
	bytes_per_pixel = 4;

	//flip pixels in Y
	if (flip_y)
		flipY();

	return true;
}

// Saves the image to a TGA file
bool Image::saveTGA(const char* filename, bool flip_y)
{
	unsigned char TGAheader[12] = { 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

	FILE *file = fopen(filename, "wb");
	if (file == NULL)
	{
		fclose(file);
		return false;
	}

	unsigned short header_short[3];
	header_short[0] = width;
	header_short[1] = height;
	unsigned char* header = (unsigned char*)header_short;
	header[4] = 32; //bitsperpixel
	header[5] = 0;

	fwrite(TGAheader, 1, sizeof(TGAheader), file);
	fwrite(header, 1, 6, file);

	//convert pixels to unsigned char
	unsigned char* bytes = new unsigned char[width*height * 4];
	for (unsigned int y = 0; y < height; ++y)
		for (unsigned int x = 0; x < width; ++x)
		{
			Uint8* p = data + (height - y - 1)*width*4 + x*4;
			unsigned int pos = (y*width + x) * 4;
			if(flip_y)
				pos = ((height - y - 1)*width + x) * 4;
			bytes[pos + 2] = *p;
			bytes[pos + 1] = *(p+1);
			bytes[pos] = *(p+2);
			bytes[pos + 3] = *(p+3);
		}

	fwrite(bytes, 1, width*height * 4, file);
	fclose(file);
	return true;
}

void Image::flipY()
{
	assert(data);
	int row_size = 4 * width;
	uint8* temp_row = new uint8[row_size];
#pragma omp simd
	for (int y = 0; y < height*0.5; y += 1)
	{
		uint8* pos = data + y*row_size;
		memcpy(temp_row, pos, row_size);
		uint8* pos2 = data + (height - y - 1)*row_size;
		memcpy(pos, pos2, row_size);
		memcpy(pos2, temp_row, row_size);
	}
	delete[] temp_row;
}

bool isPowerOfTwo( int n )
{
	return (n & (n - 1)) == 0;
}
