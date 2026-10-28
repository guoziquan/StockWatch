@echo off
setlocal enabledelayedexpansion
chcp 936 >nul
echo ========================================
echo   冰川 · A股行情监控  打包脚本
echo ========================================
echo.

:: -------- 配置 --------
set "PROJECT_DIR=%~dp0"
set "QT_DIR=F:\Qt\Qt5.9.7"
set "MINGW_BIN=%QT_DIR%\Tools\mingw530_32\bin"

:: 默认输出目录，可传参覆盖
if "%~1"=="" (
    set "OUT_DIR=%PROJECT_DIR%\package_out"
) else (
    set "OUT_DIR=%~1"
)

:: -------- 编译 --------
echo [1/4] 编译项目...
pushd "%PROJECT_DIR%"
call "%QT_DIR%\5.9.7\mingw53_32\bin\qmake.exe" StockWatch.pro >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] qmake 失败
    popd
    exit /b 1
)
call "%MINGW_BIN%\mingw32-make.exe" -j4 >nul 2>&1
if %errorlevel% neq 0 (
    echo [错误] 编译失败
    popd
    exit /b 1
)
popd
echo       编译完成

:: -------- 部署 --------
echo [2/4] 收集依赖...
if exist "%OUT_DIR%" (
    rd /s /q "%OUT_DIR%" >nul 2>&1
)
mkdir "%OUT_DIR%" >nul 2>&1

:: Qt 核心文件
copy /y "%PROJECT_DIR%release\StockWatch.exe" "%OUT_DIR%\" >nul
call "%QT_DIR%\5.9.7\mingw53_32\bin\windeployqt.exe" "%OUT_DIR%\StockWatch.exe" --no-translations --no-compiler-runtime --no-system-d3d-compiler --no-quick-import >nul 2>&1

:: MinGW 运行时
copy /y "%MINGW_BIN%\libgcc_s_dw2-1.dll"   "%OUT_DIR%\" >nul 2>&1
copy /y "%MINGW_BIN%\libstdc++-6.dll"      "%OUT_DIR%\" >nul 2>&1
copy /y "%MINGW_BIN%\libwinpthread-1.dll"  "%OUT_DIR%\" >nul 2>&1

:: OpenSSL（windeployqt 不处理，需手动打包）
:: 查找顺序：release 目录 -> MinGW 工具链自带 opt\bin（Qt 安装时自带，版本匹配）
copy /y "%PROJECT_DIR%release\libeay32.dll" "%OUT_DIR%\" >nul 2>&1
copy /y "%PROJECT_DIR%release\ssleay32.dll" "%OUT_DIR%\" >nul 2>&1
if not exist "%OUT_DIR%\libeay32.dll" (
    copy /y "%QT_DIR%\Tools\mingw530_32\opt\bin\libeay32.dll" "%OUT_DIR%\" >nul 2>&1
    copy /y "%QT_DIR%\Tools\mingw530_32\opt\bin\ssleay32.dll" "%OUT_DIR%\" >nul 2>&1
)
if not exist "%OUT_DIR%\libeay32.dll" (
    copy /y "%QT_DIR%\Tools\QtCreator\bin\libeay32.dll" "%OUT_DIR%\" >nul 2>&1
    copy /y "%QT_DIR%\Tools\QtCreator\bin\ssleay32.dll" "%OUT_DIR%\" >nul 2>&1
)
if exist "%OUT_DIR%\libeay32.dll" (
    echo       OpenSSL DLL 已打包
) else (
    echo       [WARN] 未找到 OpenSSL DLL，HTTPS 不可用（程序会自动改用腾讯 HTTP 源）
)

:: 项目文件
copy /y "%PROJECT_DIR%stocks.json" "%OUT_DIR%\" >nul
copy /y "%PROJECT_DIR%resource\Image\enter_icon.ico" "%OUT_DIR%\" >nul 2>&1

:: qt.conf
(
echo [Paths]
echo Plugins = .
) > "%OUT_DIR%\qt.conf"

:: 清理多余文件
if exist "%OUT_DIR%\translations" rd /s /q "%OUT_DIR%\translations" >nul 2>&1
if exist "%OUT_DIR%\D3Dcompiler_47.dll" del /q "%OUT_DIR%\D3Dcompiler_47.dll" >nul 2>&1
if exist "%OUT_DIR%\opengl32sw.dll"     del /q "%OUT_DIR%\opengl32sw.dll" >nul 2>&1
if exist "%OUT_DIR%\Qt5Svg.dll"         del /q "%OUT_DIR%\Qt5Svg.dll" >nul 2>&1

:: -------- 验证 --------
echo [3/4] 验证完整性...
set "MISSING="
if not exist "%OUT_DIR%\StockWatch.exe"       set "MISSING=StockWatch.exe %MISSING%"
if not exist "%OUT_DIR%\Qt5Core.dll"          set "MISSING=Qt5Core.dll %MISSING%"
if not exist "%OUT_DIR%\Qt5Gui.dll"           set "MISSING=Qt5Gui.dll %MISSING%"
if not exist "%OUT_DIR%\Qt5Widgets.dll"       set "MISSING=Qt5Widgets.dll %MISSING%"
if not exist "%OUT_DIR%\Qt5Network.dll"       set "MISSING=Qt5Network.dll %MISSING%"
if not exist "%OUT_DIR%\platforms\qwindows.dll" set "MISSING=qwindows.dll %MISSING%"
if not exist "%OUT_DIR%\libeay32.dll"         set "MISSING=libeay32.dll %MISSING%"
if not exist "%OUT_DIR%\ssleay32.dll"         set "MISSING=ssleay32.dll %MISSING%"
if not exist "%OUT_DIR%\stocks.json"          set "MISSING=stocks.json %MISSING%"

if defined MISSING (
    echo       [警告] 缺失文件: %MISSING%
) else (
    echo       核心文件完整
)

:: -------- 统计 --------
echo [4/4] 统计大小...
for /f %%b in ('powershell -NoProfile -Command "[math]::Round((Get-ChildItem -LiteralPath '%OUT_DIR%' -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB, 1)"') do set "MB=%%b"
echo       输出目录: %OUT_DIR%
echo       总大小:   %MB% MB
echo.
echo ========================================
echo   打包完成！
echo ========================================
endlocal
