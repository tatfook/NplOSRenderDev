#ifndef NPL_OS_RENDER_H
#define NPL_OS_RENDER_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <thread>
#include <queue>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include "boost/noncopyable.hpp"
#include "Core/NPLInterface.hpp"
#include "GL/osmesa.h"
#include "gl_wrap.h"

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
	void RenderImage(NPLInterface::NPLObjectProxy& renderList);
	void WritePng(const string& fileName);

	std::thread* m_pThread;
	std::queue<RenderParams*> m_queue;
	std::mutex m_mutex;
	std::condition_variable m_condition;
	std::atomic<bool> m_start;

	OSMesaContext m_context;
	GLfloat* m_buffer;

	static NplOSRender* m_pInstance;
	static const int WIDTH = 400;
	static const int HEIGHT = 400;
};

#endif // !NPL_OS_RENDER_H
