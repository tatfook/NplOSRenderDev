#pragma once
#include "Core/NPLInterface.hpp"
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
	void PostTask(const char* msg, int length);
	static NplOSRender* CreateGetSingleton();

protected:
	NplOSRender();
	~NplOSRender();

private:
	void DoTask();
	void InitGL();
	void InitLights();
	void ResizeView(int w, int h);
	GLuint CreateDisplayList(NPLInterface::NPLObjectProxy& renderList);
	void WritePng(const string& fileName, const GLubyte *buffer, int width, int height);

	std::thread* m_pThread;
	std::queue<RenderParams*> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_condition;
	std::atomic<bool> m_start;

	OSMesaContext m_context;

	static NplOSRender* m_pInstance;
};
