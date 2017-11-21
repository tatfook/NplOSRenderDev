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
		ResizeView(params->width, params->height);

		Vector3 center, extents;
		GLuint listId = CreateDisplayList(params->renderList, center, extents);
		GLfloat scale = 1.0f / std::max(std::max(extents.x, extents.y), extents.z);

		float degree = 360.0f / params->frame;
		for (int i = 0; i < params->frame; i++)
		{
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

			glPushMatrix();
			glRotatef(-90.0f, 1, 0, 0);
			glRotatef(degree * i, 0, 0, 1);
			glTranslatef(-center.x, -center.y, -center.z);
			glScalef(scale, scale, scale);
			glCallList(listId);
			glPopMatrix();
			glFinish();
			WritePng(params->modelName + std::to_string(i) + ".tga", buffer, params->width, params->height);
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

void NplOSRender::ResizeView(int w, int h)
{
	glViewport(0, 0, (GLsizei)w, (GLsizei)h);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	if (w <= h)
		glOrtho(-1.0, 1.0, -1.0*(GLfloat)h / (GLfloat)w, 1.0*(GLfloat)h / (GLfloat)w, -10.0, 10.0);
	else
		glOrtho(-1.0*(GLfloat)w / (GLfloat)h, 1.0*(GLfloat)w / (GLfloat)h, -1.0, 1.0, -10.0, 10.0);
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
			Vector3 point(Vector3((float)(double)vertex[1], (float)(double)vertex[2], (float)(double)vertex[3]));
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
			indexBuffer.push_back(index);
			i++;
		}
		shapes.push_back(i);
	}

	center = (vmax + vmin)*0.5f;
	extents = (vmax - vmin)/**0.5f*/; //Used to scale the model, don't need to div2 

	//MeshPhongMaterial
	float shininess = 200.0f;
// 	float diffuseColor[3] = { 1.0f, 1.0f, 1.0f };
// 	float specularColor[4] = { 1.00000f, 0.980392f, 0.549020f, 1.0f };

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
// 	glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, specularColor);
// 	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
// 	glColor3fv(diffuseColor);
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
