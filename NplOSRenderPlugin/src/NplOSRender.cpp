#include "NplOSRender.h"
#include "Core/NPLInterface.hpp"
#include "ParaVector3.h"
#include "ShapeAABB.h"
#ifdef __linux__
#include "libpng/png.h"
#elif _WIN32
#include "png.h"
#endif

using namespace ParaEngine;

struct RenderParams
{
	std::string modelName;
	NPLInterface::NPLObjectProxy renderList;

	RenderParams(const std::string& name, const NPLInterface::NPLObjectProxy& r)
		:modelName(name), renderList(r) {}
};

NplOSRender* NplOSRender::m_pInstance = nullptr;
NplOSRender::NplOSRender()
	:m_pThread(nullptr)
	, m_start(false)
	, m_context(OSMesaCreateContextExt(GL_RGBA, 32, 0, 0, nullptr))
	, m_buffer(new GLfloat[WIDTH * HEIGHT * 4])
{
	if (nullptr == m_context)
	{
		// create osmesa context failed
	}
	m_buffer = new GLfloat[WIDTH * HEIGHT * 4];
	if (m_buffer != nullptr)\
		OSMesaMakeCurrent(m_context, m_buffer, GL_FLOAT, WIDTH, HEIGHT);
}

NplOSRender::~NplOSRender()
{
	if (m_pThread != nullptr)
	{
		m_start = false;
		m_pThread->join();
		
		RenderParams* params;
		std::unique_lock<std::mutex> lk(m_mutex);
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

	if (m_buffer != nullptr)
	{
		delete[] m_buffer;
		m_buffer = nullptr;
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

	RenderParams* params = new RenderParams(tabMsg["model"], tabMsg["render"]);
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

		RenderImage(params->renderList);
		WritePng(params->modelName.append(""));
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
	//glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	//glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	// track material ambient and diffuse from surface color, call it before glEnable(GL_COLOR_MATERIAL)
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);

	glClearColor(0, 0, 0, 0);                   // background color
	glClearStencil(0);                          // clear stencil buffer
	glClearDepth(1.0f);                         // 0 is near, 1 is far
	glDepthFunc(GL_LEQUAL);

	InitLights();
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (w <= h)
		glOrtho(0.0, 16.0, 0.0, 16.0*(GLfloat)h / (GLfloat)w,
			-10.0, 10.0);
	else
		glOrtho(0.0, 16.0*(GLfloat)w / (GLfloat)h, 0.0, 16.0,
			-10.0, 10.0);
	glMatrixMode(GL_MODELVIEW);
}

void NplOSRender::InitLights()
{
	// set up light colors (ambient, diffuse, specular)
	GLfloat lightKa[] = { .2f, .2f, .2f, 1.0f };  // ambient light
	GLfloat lightKd[] = { .7f, .7f, .7f, 1.0f };  // diffuse light
	GLfloat lightKs[] = { 1, 1, 1, 1 };           // specular light
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightKa);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightKd);
	glLightfv(GL_LIGHT0, GL_SPECULAR, lightKs);

	// position the light
	float lightPos[4] = { 0, 0, 20, 1 }; // positional light
	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

	glEnable(GL_LIGHT0);                        // MUST enable each light source after configuration
}

void NplOSRender::SetCamera(float posX, float posY, float posZ, float targetX, float targetY, float targetZ)
{
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (w <= h)
		glOrtho(0.0, 16.0, 0.0, 16.0*(GLfloat)h / (GLfloat)w,
			-10.0, 10.0);
	else
		glOrtho(0.0, 16.0*(GLfloat)w / (GLfloat)h, 0.0, 16.0,
			-10.0, 10.0);
	glMatrixMode(GL_MODELVIEW);
}

void NplOSRender::RenderImage(NPLInterface::NPLObjectProxy& renderList)
{
	std::vector<Vector3> vertexBuffer;
	std::vector<Vector3> normalBuffer;
	std::vector<Vector3> colorBuffer;
	std::vector<unsigned int> indexBuffer;
	std::vector<int> shapes;

	for (NPLInterface::NPLTable::IndexIterator_Type itCur = renderList.index_begin(), itEnd = renderList.index_end(); itCur != itEnd; ++itCur)
	{
		NPLInterface::NPLObjectProxy& value = itCur->second;
		NPLInterface::NPLObjectProxy& vertices = value["vertices"];
		NPLInterface::NPLObjectProxy& normals = value["normals"];
		NPLInterface::NPLObjectProxy& colors = value["colors"];
		NPLInterface::NPLObjectProxy& indices = value["indices"];

		for (NPLInterface::NPLTable::IndexIterator_Type vCur = vertices.index_begin(), vEnd = vertices.index_end(); vCur != vEnd; ++vCur)
		{
			NPLInterface::NPLObjectProxy& vertex = vCur->second;
			vertexBuffer.push_back(Vector3((double)vertex[0], (double)vertex[1], (double)vertex[2]));
		}
		for (NPLInterface::NPLTable::IndexIterator_Type nCur = normals.index_begin(), nEnd = normals.index_end(); nCur != nEnd; ++nCur)
		{
			NPLInterface::NPLObjectProxy& normal = nCur->second;
			normalBuffer.push_back(Vector3((double)normal[0], (double)normal[1], (double)normal[2]));
		}
		for (NPLInterface::NPLTable::IndexIterator_Type cCur = colors.index_begin(), cEnd = colors.index_end(); cCur != cEnd; ++cCur)
		{
			NPLInterface::NPLObjectProxy& color = cCur->second;
			colorBuffer.push_back(Vector3((double)color[0], (double)color[1], (double)color[2]));
		}

		int i = 0;
		for (NPLInterface::NPLTable::IndexIterator_Type iCur = colors.index_begin(), iEnd = colors.index_end(); iCur != iEnd; ++iCur)
		{
// 			indexBuffer.push_back(iCur->second);
			i++;
		}
		shapes.push_back(i);
	}

	CShapeAABB aabb(&vertexBuffer[0], vertexBuffer.size());
	Vector3 center = aabb.GetCenter();
	Vector3 extents = aabb.GetExtents();

	for (int i = 0; i < 6; i++)
	{
		glEnableClientState(GL_NORMAL_ARRAY);
		glEnableClientState(GL_COLOR_ARRAY);
		glEnableClientState(GL_VERTEX_ARRAY);
		glNormalPointer(GL_FLOAT, 0, &normalBuffer[0]);
		glColorPointer(3, GL_FLOAT, 0, &colorBuffer[0]);
		glVertexPointer(3, GL_FLOAT, 0, &vertexBuffer[0]);

		glPushMatrix();
		glTranslatef(-center.x, -center.y, -center.z);

		GLint start = 0;
		for each (auto range in shapes)
		{
			GLint end = indexBuffer[start + range - 1];
			glDrawRangeElements(GL_TRIANGLES, start, end, range, GL_UNSIGNED_BYTE, reinterpret_cast<void*>(indexBuffer[start]));
			start += range;
		}

		glPopMatrix();

		glDisableClientState(GL_VERTEX_ARRAY);
		glDisableClientState(GL_COLOR_ARRAY);
		glDisableClientState(GL_NORMAL_ARRAY);
	}

	GLuint listId = glGenLists(1);

	float shininess = 15.0f;
	float diffuseColor[3] = { 0.929524f, 0.796542f, 0.178823f };
	float specularColor[4] = { 1.00000f, 0.980392f, 0.549020f, 1.0f };

	if (listId == 0)
	{
	}

	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glNormalPointer(GL_FLOAT, 0, &normalBuffer[0]);
	glColorPointer(3, GL_FLOAT, 0, &colorBuffer[0]);
	glVertexPointer(3, GL_FLOAT, 0, &vertexBuffer[0]);

	// store drawing function in the display list =============================
	glNewList(listId, GL_COMPILE);

	// set specular and shiniess using glMaterial (gold-yellow)
	glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess); // range 0 ~ 128
	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);

	// set ambient and diffuse color using glColorMaterial (gold-yellow)
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glColor3fv(diffuseColor);

	// start to render polygons
	glDrawElements(GL_TRIANGLES, 12, GL_UNSIGNED_SHORT, &indexBuffer[0]);

	glEndList();	//=========================================================

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glDisableClientState(GL_NORMAL_ARRAY);
}

void NplOSRender::WritePng(const string& fileName)
{

}

NplOSRender* NplOSRender::CreateGetSingleton()
{
	if (m_pInstance == nullptr)
	{
		m_pInstance = new NplOSRender();
	}
	return m_pInstance;
}
