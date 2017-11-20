# NplOSRenderDev
Off-screen rendering dll used on the NPLRuntime server

### Build NplCefBrowser on Windows
- Set BOOST_ROOT environment
- Create a folder which named "NplOSRender"
- Download [NPLRuntime](https://github.com/LiXizhi/NPLRuntime.git) and upzip it into [NplOSRender/NPLRuntime]
- Download [NplOSRenderDev](https://github.com/tatfook/NplOSRenderDev.git) and upzip it into [NplOSRender/NplOSRenderDev]
- Build NPLRumtime(Optional):Please see [NplOSRender/NPLRuntime/build_win32.bat]
- Run NplOSRender/NplOSRenderDev/create-solution.bat
- Msbuild sln/Win32/NplOSRender.sln
- After build successfully, all binary files will locate at: NplOSRender/NPLRuntime/ParaWorld/mesa
