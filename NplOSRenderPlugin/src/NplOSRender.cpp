#include "NplOSRender.h"
#include "Core/NPLInterface.hpp"
#include "ParaAngle.h"
#include "ParaMathUtility.h"
#include "ParaMath.h"
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
	if (nullptr == m_context)
	{
		// create osmesa context failed
	}
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

		GLubyte* buffer = new GLubyte[params->width * params->height * 4];
		if (buffer != nullptr)\
			OSMesaMakeCurrent(m_context, buffer, GL_FLOAT, params->width, params->height);
		InitGL();
		ResizeView(params->width, params->height);

		GLuint listId = CreateDisplayList(params->renderList);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
		glPushMatrix();
		glTranslatef(0, -1.6f, 0);
		glRotatef(10.0f, 1, 0, 0);
		glRotatef(30.0f, 0, 1, 0);
		glCallList(listId);
		glPopMatrix();
		glFinish();
		WritePng(params->modelName.append(""), buffer, params->width, params->height);

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

void NplOSRender::ResizeView(int w, int h)
{
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (w <= h)
		glOrtho(-10.0, 10.0, -10.0*(GLfloat)h / (GLfloat)w, 10.0*(GLfloat)h / (GLfloat)w, -10.0, 10.0);
	else
		glOrtho(-1.0*(GLfloat)w / (GLfloat)h, 1.0*(GLfloat)w / (GLfloat)h, -1.0, 1.0, -10.0, 10.0);
	glMatrixMode(GL_MODELVIEW);
}

GLuint NplOSRender::CreateDisplayList(NPLInterface::NPLObjectProxy& renderList)
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
			vertexBuffer.push_back(Vector3((float)(double)vertex[0], (float)(double)vertex[1], (float)(double)vertex[2]));
		}
		for (NPLInterface::NPLTable::IndexIterator_Type nCur = normals.index_begin(), nEnd = normals.index_end(); nCur != nEnd; ++nCur)
		{
			NPLInterface::NPLObjectProxy& normal = nCur->second;
			normalBuffer.push_back(Vector3((float)(double)normal[0], (float)(double)normal[1], (float)(double)normal[2]));
		}
		for (NPLInterface::NPLTable::IndexIterator_Type cCur = colors.index_begin(), cEnd = colors.index_end(); cCur != cEnd; ++cCur)
		{
			NPLInterface::NPLObjectProxy& color = cCur->second;
			colorBuffer.push_back(Vector3((float)(double)color[0], (float)(double)color[1], (float)(double)color[2]));
		}

		int i = 0;
		for (NPLInterface::NPLTable::IndexIterator_Type iCur = colors.index_begin(), iEnd = colors.index_end(); iCur != iEnd; ++iCur)
		{
			indexBuffer.push_back((unsigned int)(double)iCur->second);
			i++;
		}
		shapes.push_back(i);
	}

// 	CShapeAABB aabb(&vertexBuffer[0], vertexBuffer.size());
// 	Vector3 center = aabb.GetCenter();
// 	Vector3 extents = aabb.GetExtents();

	GLuint id = glGenLists(1);
	if (!id) return id;

	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glEnableClientState(GL_VERTEX_ARRAY);
	glNormalPointer(GL_FLOAT, 0, &normalBuffer[0]);
	glColorPointer(3, GL_FLOAT, 0, &colorBuffer[0]);
	glVertexPointer(3, GL_FLOAT, 0, &vertexBuffer[0]);

	glNewList(id, GL_COMPILE);
	GLint start = 0;
	for each (auto range in shapes)
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
	FILE *f = fopen(fileName.c_str(), "w");
	if (f) {
		int i, x, y;
		const GLubyte *ptr = buffer;
		fputc(0x00, f);	/* ID Length, 0 => No ID	*/
		fputc(0x00, f);	/* Color Map Type, 0 => No color map included	*/
		fputc(0x02, f);	/* Image Type, 2 => Uncompressed, True-color Image */
		fputc(0x00, f);	/* Next five bytes are about the color map entries */
		fputc(0x00, f);	/* 2 bytes Index, 2 bytes length, 1 byte size */
		fputc(0x00, f);
		fputc(0x00, f);
		fputc(0x00, f);
		fputc(0x00, f);	/* X-origin of Image	*/
		fputc(0x00, f);
		fputc(0x00, f);	/* Y-origin of Image	*/
		fputc(0x00, f);
		fputc(width & 0xff, f);      /* Image Width	*/
		fputc((width >> 8) & 0xff, f);
		fputc(height & 0xff, f);     /* Image Height	*/
		fputc((height >> 8) & 0xff, f);
		fputc(0x18, f);		/* Pixel Depth, 0x18 => 24 Bits	*/
		fputc(0x20, f);		/* Image Descriptor	*/
		fclose(f);
		f = fopen(fileName.c_str(), "ab");  /* reopen in binary append mode */
		for (y = height - 1; y >= 0; y--) {
			for (x = 0; x < width; x++) {
				i = (y*width + x) * 4;
				fputc(ptr[i + 2], f); /* write blue */
				fputc(ptr[i + 1], f); /* write green */
				fputc(ptr[i], f);   /* write red */
			}
		}
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
