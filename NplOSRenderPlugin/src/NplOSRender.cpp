#include "NplOSRender.h"
#ifdef __linux__
#include "libpng/png.h"
#elif _WIN32
#include "png.h"
#endif

using namespace ParaEngine;

struct RenderParams
{
	std::string modelName;
	int width = 128;
	int height = 128;
	int frame = 8;
	NPLInterface::NPLObjectProxy renderList;

	RenderParams(const std::string& name, const NPLInterface::NPLObjectProxy& r)
		:modelName(name), renderList(r) {}
};

NplOSRender* NplOSRender::m_pInstance = nullptr;
NplOSRender::NplOSRender()
	:m_pThread(nullptr)
	, m_start(false)
	, m_context(OSMesaCreateContextExt(GL_RGBA, 32, 0, 0, nullptr))
{
	if (!m_context) {
		printf("OSMesaCreateContext failed!\n");
	}
}

NplOSRender::~NplOSRender()
{
	if (m_pThread != nullptr)
	{
		m_start = false;
		lock_guard<mutex> lock(m_mutex);
		m_queue.push(nullptr);
		m_condition.notify_one();
		m_pThread->join();
		
		RenderParams* params;
		while (!m_queue.empty())
		{
			params = m_queue.front();
			m_queue.pop();
			delete params;
		}
		params = nullptr;
		
		delete m_pThread;
		m_pThread = nullptr;
	}

	if (m_context != nullptr)
	{
		OSMesaDestroyContext(m_context);
		m_context = nullptr;
	}
}

void NplOSRender::PostTask(const char* msg, int length)
{
	NPLInterface::NPLObjectProxy tabMsg = NPLInterface::NPLHelper::MsgStringToNPLTable(msg, length);
	if (nullptr == m_pThread)
	{
		m_start = true;
		m_pThread = new std::thread(&NplOSRender::DoTask, this);
	}

	string fileName = tabMsg["model"];
	size_t pos = fileName.find_last_of('.');
	if (pos != string::npos)
		fileName = fileName.substr(0, pos - 1);
	fileName.append("_");
	RenderParams* params = new RenderParams(fileName, tabMsg["render"]);
	double w = tabMsg["width"];
	double h = tabMsg["height"];
	double f = tabMsg["frame"];
	if (w > 0) params->width = (int)w;
	if (h > 0) params->height = (int)h;
	if (f > 0) params->frame = (int)f;

	std::unique_lock<std::mutex> lk(m_mutex);
	m_queue.push(params);
	m_condition.notify_one();
}

void NplOSRender::DoTask()
{
	while (m_start)
	{
		RenderParams* params = nullptr;
		{
			std::unique_lock<std::mutex> lk(m_mutex);
			while (m_queue.empty())
				m_condition.wait(lk);

			if (m_queue.empty())
				continue;

			params = m_queue.front();
			m_queue.pop();
		}
		
		if (nullptr == params) continue;

		GLubyte* buffer = new GLubyte[params->width * params->height * 4];
		if (buffer != nullptr)
			OSMesaMakeCurrent(m_context, buffer, GL_UNSIGNED_BYTE, params->width, params->height);
		InitGL();

		Vector3 center, extents;
		GLuint listId = CreateDisplayList(params->renderList, center, extents);
		GLfloat scale = std::max(std::max(extents.x, extents.y), extents.z);
		ResizeView(params->width, params->height, scale);

		float degree = 360.0f / params->frame;
		for (int i = 0; i < params->frame; i++)
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			glPushMatrix();
			glRotatef(-60.0f, 1, 0, 0);
			glRotatef(degree * i, 0, 0, 1);
			glTranslatef(-center.x, -center.y, -center.z);
			glCallList(listId);
			glPopMatrix();
			glFinish();
			WritePng(params->modelName + std::to_string(i) + ".png", buffer, params->width, params->height);
		}

		glDeleteLists(listId, 1);
		delete[] buffer;
		buffer = nullptr;

		delete params;
		params = nullptr;
	}
}

void NplOSRender::InitGL()
{
	glShadeModel(GL_SMOOTH);                    // shading mathod: GL_SMOOTH or GL_FLAT
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);      // 4-byte pixel alignment

												// enable /disable features
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_CULL_FACE);

	// track material ambient and diffuse from surface color, call it before glEnable(GL_COLOR_MATERIAL)
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);

	glClearColor(240 / 255.0f, 240 / 255.0f, 240 / 255.0f, 1.0f);	//0xf0f0f0
	glClearStencil(0);
	glClearDepth(1.0f);
	glDepthFunc(GL_LEQUAL);

	InitLights();
}

void NplOSRender::InitLights()
{
	GLfloat lightKa[] = { 68 / 255.0f, 68 / 255.0f, 68 / 255.0f, 1.0f };	//0x444444
	GLfloat lightKd[] = { 1.0f, 1.0f, 1.0f, 1.0f };	//0xffffff
	GLfloat lightKs[] = { 1, 1, 1, 1 };
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightKa);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightKd);
	glLightfv(GL_LIGHT0, GL_SPECULAR, lightKs);

	// position the light
	float lightPos[4] = { 17, 30, 9, 1 }; // positional light
	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

	glEnable(GL_LIGHT0);                        // MUST enable each light source after configuration
}

void NplOSRender::ResizeView(int w, int h, float scale)
{
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (w <= h)
		glOrtho(-scale, scale, -scale*(GLfloat)h / (GLfloat)w, scale*(GLfloat)h / (GLfloat)w, -scale, scale);
	else
		glOrtho(-scale*(GLfloat)w / (GLfloat)h, scale*(GLfloat)w / (GLfloat)h, -scale, scale, -scale, scale);
	glMatrixMode(GL_MODELVIEW);
}

GLuint NplOSRender::CreateDisplayList(NPLInterface::NPLObjectProxy& renderList, Vector3& center, Vector3& extents)
{
	std::vector<Vector3> vertexBuffer;
	std::vector<Vector3> normalBuffer;
	std::vector<Vector3> colorBuffer;
	std::vector<unsigned int> indexBuffer;
	std::vector<int> shapes;

	Vector3 vmax(0, 0, 0);
	Vector3 vmin(0, 0, 0);

	int lastVCount = 0;
	for (NPLInterface::NPLTable::IndexIterator_Type itCur = renderList.index_begin(), itEnd = renderList.index_end(); itCur != itEnd; ++itCur)
	{
		NPLInterface::NPLObjectProxy& value = itCur->second;
		NPLInterface::NPLObjectProxy& vertices = value["vertices"];
		NPLInterface::NPLObjectProxy& normals = value["normals"];
		NPLInterface::NPLObjectProxy& colors = value["colors"];
		NPLInterface::NPLObjectProxy& indices = value["indices"];
		NPLInterface::NPLObjectProxy& matrix = value["world_matrix"];

		for (NPLInterface::NPLTable::IndexIterator_Type vCur = vertices.index_begin(), vEnd = vertices.index_end(); vCur != vEnd; ++vCur)
		{
			NPLInterface::NPLObjectProxy& vertex = vCur->second;
			Vector3 point(Vector3((float)(double)vertex[1], (float)(double)vertex[2], (float)(double)vertex[3]));
			Matrix4 m(1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1);
			int i = 0;
			for (NPLInterface::NPLTable::IndexIterator_Type mCur = matrix.index_begin(), mEnd = matrix.index_end(); mCur != mEnd; ++mCur)
			{
				m._m[i] = (float)(double)mCur->second;
				i++;
			}
			point = point * m;
			vertexBuffer.push_back(point);

			if (point.x > vmax.x) vmax.x = point.x;	if (point.x < vmin.x) vmin.x = point.x;
			if (point.y > vmax.y) vmax.y = point.y;	if (point.y < vmin.x) vmin.y = point.y;
			if (point.z > vmax.z) vmax.z = point.z;	if (point.z < vmin.x) vmin.z = point.z;
		}
		for (NPLInterface::NPLTable::IndexIterator_Type nCur = normals.index_begin(), nEnd = normals.index_end(); nCur != nEnd; ++nCur)
		{
			NPLInterface::NPLObjectProxy& normal = nCur->second;
			normalBuffer.push_back(Vector3((float)(double)normal[1], (float)(double)normal[2], (float)(double)normal[3]));
		}
		for (NPLInterface::NPLTable::IndexIterator_Type cCur = colors.index_begin(), cEnd = colors.index_end(); cCur != cEnd; ++cCur)
		{
			NPLInterface::NPLObjectProxy& color = cCur->second;
			colorBuffer.push_back(Vector3((float)(double)color[1], (float)(double)color[2], (float)(double)color[3]));
		}

		int i = 0;
		for (NPLInterface::NPLTable::IndexIterator_Type iCur = indices.index_begin(), iEnd = indices.index_end(); iCur != iEnd; ++iCur)
		{
			unsigned int index = (unsigned int)(double)iCur->second - 1;
			indexBuffer.push_back(index + lastVCount);
			i++;
		}
		shapes.push_back(i);
		lastVCount = vertexBuffer.size();
	}

	center = (vmax + vmin)*0.5f;
	extents = (vmax - vmin)/**0.5f*/; //Used to scale the model, don't need to div2 

	//MeshPhongMaterial
	float shininess = 5.0f;
	float diffuseColor[3] = { 1.0f, 1.0f, 1.0f };
	float specularColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

	GLuint id = glGenLists(1);
	if (!id) return id;

	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glNormalPointer(GL_FLOAT, 0, &normalBuffer[0]);
	glColorPointer(3, GL_FLOAT, 0, &colorBuffer[0]);
	glVertexPointer(3, GL_FLOAT, 0, &vertexBuffer[0]);

	glNewList(id, GL_COMPILE);
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess);
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glColor3fv(diffuseColor);
	GLint start = 0;
	for (auto range : shapes)
	{
		glDrawElements(GL_TRIANGLES, range, GL_UNSIGNED_INT, &indexBuffer[start]);
		start += range;
	}
	glEndList();

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);

	return id;
}

void NplOSRender::WritePng(const string& fileName, const GLubyte *buffer, int width, int height)
{
	FILE *fp = fopen(fileName.c_str(), "wb");
	if (fp != nullptr)
	{
		png_structp write_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
		png_infop write_info_ptr = png_create_info_struct(write_ptr);
		png_infop write_end_info_ptr = png_create_info_struct(write_ptr);
		if (setjmp(png_jmpbuf(write_ptr)))
		{
			png_destroy_info_struct(write_ptr, &write_end_info_ptr);
			png_destroy_write_struct(&write_ptr, &write_info_ptr);
			fclose(fp);
			return;
		}

		png_init_io(write_ptr, fp);
		png_set_IHDR(write_ptr, write_info_ptr, width, height, 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
		png_colorp palette = (png_colorp)png_malloc(write_ptr, PNG_MAX_PALETTE_LENGTH * sizeof(png_color));
		if (!palette) {
			png_destroy_info_struct(write_ptr, &write_end_info_ptr);
			png_destroy_write_struct(&write_ptr, &write_info_ptr);
			fclose(fp);
			return;
		}
		png_set_PLTE(write_ptr, write_info_ptr, palette, PNG_MAX_PALETTE_LENGTH);
		png_write_info_before_PLTE(write_ptr, write_info_ptr);
		png_write_info(write_ptr, write_info_ptr);
		png_write_info(write_ptr, write_end_info_ptr);

		png_bytepp rows = (png_bytepp)png_malloc(write_ptr, height * sizeof(png_bytep));
		for (int i = 0; i < height; i++)
			rows[i] = (png_bytep)(buffer + (height - i) * width * 4);

		png_write_image(write_ptr, rows);
		png_write_end(write_ptr, write_end_info_ptr);
		png_free(write_ptr, rows);
		png_free(write_ptr, palette);
		png_destroy_info_struct(write_ptr, &write_end_info_ptr);
		png_destroy_write_struct(&write_ptr, &write_info_ptr);

		fclose(fp);
	}
}

NplOSRender* NplOSRender::CreateGetSingleton()
{
	if (m_pInstance == nullptr)
	{
		m_pInstance = new NplOSRender();
	}
	return m_pInstance;
}
