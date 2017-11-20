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
			indexBuffer.push_back(iCur->second);
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
