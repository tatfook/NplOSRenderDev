#pragma once
#include "Core/NPLInterface.hpp"
#include "ParaAngle.h"
#include "ParaMathUtility.h"
#include "ParaMath.h"
#include "ParaVector3.h"
#include "GL/osmesa.h"
#include "gl_wrap.h"
#include "boost/noncopyable.hpp"
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>

struct RenderParams;
class NplOSRender : protected boost::noncopyable
{
public:
	void PostTask(const char* msg, int length, std::function<void(const string&, const string&)> cb);
	static NplOSRender* CreateGetSingleton();

protected:
	NplOSRender();
	~NplOSRender();

private:
	void DoTask();
	void InitGL();
	void InitLights();
	void ResizeView(int w, int h, float scale);
	GLuint CreateDisplayList(NPLInterface::NPLObjectProxy& renderList, ParaEngine::Vector3& center, ParaEngine::Vector3& extents);
	void WritePng(const string& fileName, const GLubyte *buffer, int width, int height);

	std::thread* m_pThread;
	std::queue<RenderParams*> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_condition;
	std::atomic<bool> m_start;

	OSMesaContext m_context;

	static NplOSRender* m_pInstance;
};
