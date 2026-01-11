cls
call ./bs/win_build.bat
if %errorlevel% neq 0 exit /b %errorlevel%
.\build\jeux.exe
